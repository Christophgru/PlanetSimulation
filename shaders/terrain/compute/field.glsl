layout(local_size_x=64) in;
// Explicit scalar output retains the legacy 36-byte interleaved vertex stride.
struct Noise { double amplitude,frequency,persistence,lacunarity; uvec4 settings; dvec2 padding; };
layout(std430,binding=0) readonly buffer Field {
    dvec4 scale,landscapeShape,landscapeMask,material,gradient;
    uvec4 flags,seeds; Noise noises[8];
};
layout(std430,binding=1) readonly buffer Radials { dvec4 inputs[]; };
#ifdef TERRAIN_RETAIN_TARGETS
// Integer storage preserves every double bit, including low words whose float
// interpretation is a NaN. Numeric float loads/stores never carry packed bits.
layout(std430,binding=2) buffer Unique { uint uniqueWords[]; };
float readUnique(uint i) {return uintBitsToFloat(uniqueWords[i]);}
void writeUnique(uint i,float value) {uniqueWords[i]=floatBitsToUint(value);}
#else
layout(std430,binding=2) buffer Unique { float uniqueValues[]; };
float readUnique(uint i) {return uniqueValues[i];}
void writeUnique(uint i,float value) {uniqueValues[i]=value;}
#endif
layout(std430,binding=3) readonly buffer Topology { uint corners[]; };
layout(std430,binding=4) writeonly buffer Vertices { float vertices[]; };
layout(std430,binding=5) writeonly buffer Indices { uint indices[]; };
layout(std430,binding=6) writeonly buffer Heights { double heights[]; };
layout(std430,binding=7) readonly buffer SurfacePolicy {
    dvec4 policyEyeEdge,policyDistances; double policySpacing[8];
#ifdef TERRAIN_DETAIL_MORPH
    dvec4 parentEyeEdge,parentDistances; double parentSpacing[8];
#endif
};
uniform int uFirst,uCount,uPhase,uDiagnostics,uCorners;
#ifdef TERRAIN_DETAIL_MORPH
uniform int uMorph,uSamples;
#endif
uniform dvec2 uRockRange;

// CellIdentity.h oracle: quantized angular footprint and canonical permutation.
bool cellLess(dvec3 a,dvec3 b) {
    return a.x!=b.x?a.x<b.x:a.y!=b.y?a.y<b.y:a.z<b.z;
}
double cellRound(double x) {
    // Explicit nearest-even quantization in double precision; positive uint
    // conversion truncates, with a separate carry for the low word.
    bool negative=x<0.0lf;double a=negative?-x:x;
    uint high=uint(a/4294967296.0lf);
    precise double remainder=a-double(high)*4294967296.0lf;
    uint low=uint(remainder);precise double fraction=remainder-double(low);
    if(fraction>0.5lf || (fraction==0.5lf && (low&1u)!=0u)) {
        ++low;if(low==0u) ++high;
    }
    double rounded=double(high)*4294967296.0lf+double(low);
    return negative?-rounded:rounded;
}
dvec3 cellQuantize(dvec3 p) {
    p*=1099511627776.0lf;
    return dvec3(cellRound(p.x),cellRound(p.y),cellRound(p.z));
}
uvec3 cellHashDouble(uvec3 key,double x) {
    uvec2 bits=x==0.0lf?uvec2(0):unpackDouble2x32(x);
    key=(key^bits.x)*16777619u;return (key^bits.y)*16777619u;
}
uvec3 cellHashVector(uvec3 key,dvec3 p) {
    key=cellHashDouble(key,p.x);key=cellHashDouble(key,p.y);return cellHashDouble(key,p.z);
}
uvec4 cellIdentity(dvec3 a,dvec3 b,dvec3 c) {
    // Keep this unrolled: dynamically indexing local double arrays lost low
    // angular bits on the tested NVIDIA driver. The CPU oracle checks all bits.
    dvec3 p0=cellQuantize(a),p1=cellQuantize(b),p2=cellQuantize(c);
    uvec3 order=uvec3(0,1,2);
    if(cellLess(p1,p0)) {dvec3 t=p0;p0=p1;p1=t;uint i=order.x;order.x=order.y;order.y=i;}
    if(cellLess(p2,p1)) {dvec3 t=p1;p1=p2;p2=t;uint i=order.y;order.y=order.z;order.z=i;}
    if(cellLess(p1,p0)) {dvec3 t=p0;p0=p1;p1=t;uint i=order.x;order.x=order.y;order.y=i;}
    uvec3 key=uvec3(2166136261u,0x9e3779b9u,0x85ebca6bu);
    key=cellHashVector(key,p0);key=cellHashVector(key,p1);key=cellHashVector(key,p2);
    return uvec4(key,order.x|(order.y<<2u)|(order.z<<4u));
}

dvec3 normalized(dvec3 p) { precise dvec3 r=p/sqrt(dot(p,p));return r; }
double fieldSmooth(double lo,double hi,double x) {
    precise double t=clamp((x-lo)/(hi-lo),0.0lf,1.0lf);
    precise double r=t*t*(3.0lf-2.0lf*t);return r;
}
uint hash(ivec3 p,uint seed) {
    uint h=seed;
    h^=uint(p.x)*0x9e3779b1u;h^=uint(p.y)*0x85ebca77u;h^=uint(p.z)*0xc2b2ae3du;
    h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;h^=h>>16;return h;
}
double noise(dvec3 point,uint seed) {
    ivec3 lattice=ivec3(floor(point));
    precise dvec3 f=point-dvec3(lattice);
    f=f*f*(3.0lf-2.0lf*f);
    precise double result=0.0lf;
    for(int z=0;z<=1;++z) for(int y=0;y<=1;++y) for(int x=0;x<=1;++x) {
        double v=double(hash(lattice+ivec3(x,y,z),seed))/4294967295.0lf;
        result+=v*(x!=0?f.x:1.0lf-f.x)*(y!=0?f.y:1.0lf-f.y)*(z!=0?f.z:1.0lf-f.z);
    }
    return result;
}
// GLSL has no double-precision acos overload. Two half-angle reductions
// keep the atan series within tan(pi/16), retaining double policy precision.
double policyAcos(double x) {
    double a=abs(clamp(x,-1.0lf,1.0lf));
    double t=sqrt((1.0lf-a)/(1.0lf+a));
    t=t/(1.0lf+sqrt(1.0lf+t*t));t=t/(1.0lf+sqrt(1.0lf+t*t));
    double term=t,sum=t;
    for(int k=1;k<16;++k) {term*=-t*t;sum+=term/double(2*k+1);}
    double result=8.0lf*sum;
    return x<0.0lf?3.1415926535897932384626433832795lf-result:result;
}
#ifdef TERRAIN_DETAIL_MORPH
dvec3 parentProfile(dvec3 radial) {
    if(all(equal(parentEyeEdge.xyz,dvec3(0)))) return dvec3(parentSpacing[0],parentSpacing[0],0);
    double distance=parentDistances.z*policyAcos(clamp(dot(radial,parentEyeEdge.xyz),-1.0lf,1.0lf));
    double level=7.0lf*(1.0lf-clamp((distance-parentDistances.x)/(parentDistances.y-parentDistances.x),0.0lf,1.0lf));
    int parent=int(floor(level)),fine=min(7,parent+1);
    double blend=1.0lf-(level-double(parent));blend=blend*blend*(3.0lf-2.0lf*blend);
    return dvec3(parentSpacing[fine],parentSpacing[parent],blend);
}
#endif
dvec3 surfaceProfile(dvec3 radial) {
    if(policyDistances.z==0.0lf) return dvec3(0);
    if(all(equal(policyEyeEdge.xyz,dvec3(0)))) return dvec3(policySpacing[0],policySpacing[0],0);
    double distance=policyDistances.z*policyAcos(clamp(dot(radial,policyEyeEdge.xyz),-1.0lf,1.0lf));
    double level=7.0lf*(1.0lf-clamp((distance-policyDistances.x)/(policyDistances.y-policyDistances.x),0.0lf,1.0lf));
    int parent=int(floor(level)),fine=min(7,parent+1);
    double blend=1.0lf-(level-double(parent));blend=blend*blend*(3.0lf-2.0lf*blend);
    return dvec3(policySpacing[fine],policySpacing[parent],blend);
}
double resolvedNoise(double frequency,dvec3 profile) {
    dvec2 t=clamp((frequency*profile.xy/scale.x/scale.y-.25lf)/.25lf,0.0lf,1.0lf);
    dvec2 resolved=1.0lf-t*t*(3.0lf-2.0lf*t);
    return resolved.x*(1.0lf-profile.z)+resolved.y*profile.z;
}
double heightMeters(dvec3 direction,dvec3 profile,out double omitted) {
    omitted=0.0lf;
    precise double height=0.0lf;double detailMask=1.0lf;
    if(flags.z!=0u) {
        double continent=2.0lf*noise(direction*landscapeShape.z,seeds.x)-1.0lf;
        double plain=1.0lf-fieldSmooth(landscapeMask.x-.06lf,landscapeMask.x+.06lf,
            noise(normalized(direction)*2.7lf,seeds.x+1u));
        double cliff=fieldSmooth(landscapeMask.y-.06lf,landscapeMask.y+.06lf,
            noise(normalized(direction)*2.4lf,seeds.x+2u));
        height=landscapeShape.x+landscapeShape.y*continent*(1.0lf-.8lf*plain)*resolvedNoise(landscapeShape.z,profile);
        double signal=2.0lf*noise(direction*landscapeMask.z,seeds.x+3u)-1.0lf;
        precise double distance=abs(signal);
        if(landscapeMask.w>0.0lf) {
            double s=landscapeMask.w;
            distance=(sqrt(signal*signal+s*s)-s)/(sqrt(1.0lf+s*s)-s);
        }
        double ridge=1.0lf-clamp(distance,0.0lf,1.0lf);
        precise double fifth=ridge*ridge*ridge*ridge*ridge;
        height+=landscapeShape.w*cliff*fifth*resolvedNoise(landscapeMask.z,profile);detailMask=1.0lf-.9lf*plain;
        omitted=landscapeShape.y*(1.0lf-resolvedNoise(landscapeShape.z,profile))+
            landscapeShape.w*(1.0lf-resolvedNoise(landscapeMask.z,profile));
    }
    for(uint i=0u;i<flags.y;++i) {
        Noise n=noises[i];if(n.amplitude==0.0lf) continue;
        double frequency=n.frequency,weight=1.0lf,weightSum=0.0lf;
        precise double full=0.0lf;double broad=0.0lf,missing=0.0lf;
        for(uint octave=0u;octave<n.settings.y;++octave) {
            double retained=resolvedNoise(frequency,profile);
            double v=retained>0.0lf?2.0lf*noise(direction*frequency,n.settings.z)-1.0lf:0.0lf;
            double sampled=(n.settings.x==1u ? 1.0lf-2.0lf*abs(v) : v)*retained;
            missing+=weight*(1.0lf-retained);
            if(octave==0u) broad=sampled;
            full+=weight*sampled;weightSum+=weight;
            frequency*=n.lacunarity;weight*=n.persistence;
        }
        precise double mixed=broad+(full/weightSum-broad);
        height+=detailMask*n.amplitude*mixed;omitted+=n.amplitude*missing/weightSum;
    }
    return height;
}
dvec2 heightAt(dvec3 direction) {
    direction=normalized(direction);dvec3 profile=surfaceProfile(direction);double omitted;
    double h=heightMeters(direction,profile,omitted)/scale.y;
    double curvature=max(0.0lf,profile.x*profile.x*(1.0lf-profile.z)+profile.y*profile.y*profile.z-policySpacing[7]*policySpacing[7])/(8.0lf*scale.x*scale.y);
    bool finest=profile.x==policySpacing[7] && profile.y==policySpacing[7];
    double sink=policyDistances.z>0.0lf && !finest?double(float(min(policyDistances.w,omitted+curvature))):0.0lf;
    return dvec2(h,sink);
}
dvec3 colorAt(double heightWorld,double slope) {
    if(flags.z==0u) {
        double h=scale.z==0.0lf?0.0lf:heightWorld/(scale.z/scale.y);
        return dvec3(.72lf+.45lf*h);
    }
    double h=heightWorld*scale.y,water=flags.w!=0u?scale.w:0.0lf;
    double beachWidth=material.z,beachTop=water+beachWidth;
    double usable=max(1.0lf,scale.z-water);
    double snowStart=max(beachTop+1.0lf,water+.25lf*usable);
    double snowEnd=max(snowStart+1.0lf,water+.40lf*usable);
    double above=fieldSmooth(water-max(.05lf,.05lf*beachWidth),water+max(.02lf,.02lf*beachWidth),h);
    double beach=above*(1.0lf-fieldSmooth(beachTop,beachTop+max(.15lf,.15lf*beachWidth),h));
    double snow=fieldSmooth(snowStart,snowEnd,h);
    precise dvec3 land=dvec3(1.10lf,1.30lf,.18lf)*(1.0lf-beach)+dvec3(4.20lf,1.90lf,.18lf)*beach;
    land=land*(1.0lf-snow)+dvec3(4.60lf,2.30lf,.92lf)*snow;
    double steep=fieldSmooth(uRockRange.x,uRockRange.y,1.0lf-1.0lf/sqrt(1.0lf+slope*slope));
    land*=1.0lf*(1.0lf-steep)+.50lf*steep;
    double submerged=1.0lf-fieldSmooth(water-max(.5lf,.05lf*beachWidth),water+max(.02lf,.02lf*beachWidth),h);
    land=land*(1.0lf-submerged)+dvec3(.30lf,.40lf,.19lf)*submerged;return land;
}
#ifdef TERRAIN_DETAIL_MORPH
dvec3 parentPosition(dvec4 descriptor) {
    dvec3 r=descriptor.xyz;double omitted;
    double h=heightMeters(normalized(r),parentProfile(normalized(r)),omitted)/scale.y;
    vec3 rounded=vec3(r*(1.0lf+h/scale.x));
    if(descriptor.w!=0.0lf) rounded=vec3(dvec3(rounded)-normalized(dvec3(rounded))*(descriptor.w/(scale.x*scale.y)));
    return dvec3(rounded);
}
#endif
void main() {
    uint i=uint(uFirst)+gl_GlobalInvocationID.x;if(i>=uint(uCount)) return;
#ifdef TERRAIN_DETAIL_MORPH
#ifdef TERRAIN_RETAIN_TARGETS
    if(uPhase==3) {
        uint base=9u*i,tail=9u*uint(uSamples)+base;
        for(uint j=0u;j<3u;++j) {
            uvec2 bits=unpackDouble2x32(inputs[i][j]);
            uniqueWords[base+2u*j]=bits.x;uniqueWords[base+2u*j+1u]=bits.y;
            uniqueWords[base+6u+j]=uniqueWords[tail+6u+j];
        }
        return;
    }
#endif
    if(uPhase==2) {
        uint base=uint(uSamples)+4u*(i/3u)+i%3u;
        dvec3 parent=parentPosition(inputs[base]);
        for(uint j=0u;j<3u;++j) writeUnique(9u*uint(uSamples)+3u*i+j,float(parent[j]));
        return;
    }
#endif
    if(uPhase==1) {
        uint source=9u*corners[i];
        for(uint j=0u;j<9u;++j) vertices[9u*i+j]=readUnique(source+j);
        indices[i]=i;
        if(i%3u==0u) {
            uvec4 key=cellIdentity(inputs[corners[i]].xyz,inputs[corners[i+1u]].xyz,inputs[corners[i+2u]].xyz);
            uint offset=uint(uCorners)+(i/3u)*4u;
            for(uint j=0u;j<4u;++j) indices[offset+j]=key[j];
        }
        return;
    }
    dvec3 radial=inputs[i].xyz;
    dvec2 sampled=heightAt(radial);double h=sampled.x,hm=h*scale.y;
    dvec3 reference=abs(radial.z)<.8lf?dvec3(0,0,1):dvec3(0,1,0);
    dvec3 a=normalized(cross(reference,radial)),b=normalized(cross(radial,a));
    dvec3 sampleA=normalized(gradient.y*radial+gradient.x*a);
    dvec3 sampleB=normalized(gradient.y*radial+gradient.x*b);
    dvec2 ha=heightAt(sampleA),hb=heightAt(sampleB);
    double surfaceHeight=hm-(policyDistances.z>0.0lf?inputs[i].w:0.0lf);
    precise double ga=(ha.x*scale.y-ha.y-surfaceHeight)/gradient.z;
    precise double gb=(hb.x*scale.y-hb.y-surfaceHeight)/gradient.z;
    precise dvec3 normal=normalized(radial-ga*a-gb*b);
    dvec3 color=colorAt(h,sqrt(ga*ga+gb*gb));
    precise dvec3 p=radial*(1.0lf+h/scale.x);
    vec3 rounded=vec3(p);
    if(inputs[i].w!=0.0lf) {
        // Legacy displacement happens after the first float conversion.
        precise dvec3 sunk=dvec3(rounded)-normalized(dvec3(rounded))*(inputs[i].w/(scale.x*scale.y));
        rounded=vec3(sunk);
    }
#ifdef TERRAIN_DETAIL_MORPH
    if(uMorph!=0) {
        uint base=uint(uSamples)+4u*i;
        double blend=inputs[base+3u].x;
#ifdef TERRAIN_RETAIN_TARGETS
        bool needParent=true;
#else
        bool needParent=blend>0.0lf;
#endif
        if(needParent) {
            uint offset=9u*uint(uSamples)+9u*i;
            dvec3 pa=dvec3(readUnique(offset),readUnique(offset+1u),readUnique(offset+2u));
            dvec3 pb=dvec3(readUnique(offset+3u),readUnique(offset+4u),readUnique(offset+5u));
            dvec3 pc=dvec3(readUnique(offset+6u),readUnique(offset+7u),readUnique(offset+8u));
            dvec3 n=cross(pb-pa,pc-pa);
            dvec3 parent=dot(n,n)==0.0lf?pa:radial*(dot(n,pa)/dot(n,radial));
#ifdef TERRAIN_RETAIN_TARGETS
            for(uint j=0u;j<3u;++j) {
                uvec2 bits=unpackDouble2x32(parent[j]);
                uniqueWords[offset+2u*j]=bits.x;uniqueWords[offset+2u*j+1u]=bits.y;
                uniqueWords[offset+6u+j]=floatBitsToUint(rounded[j]);
            }
#endif
            if(blend>0.0lf) rounded=vec3(dvec3(rounded)+(parent-dvec3(rounded))*blend);
        }
    }
#endif
    for(uint j=0u;j<3u;++j) {
        writeUnique(9u*i+j,rounded[j]);writeUnique(9u*i+j+3u,float(normal[j]));
        writeUnique(9u*i+j+6u,float(color[j]));
    }
    if(uDiagnostics!=0) heights[i]=hm;
}
