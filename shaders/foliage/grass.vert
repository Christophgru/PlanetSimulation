#version 330 core
// Blade shape, palette and lighting adapted from SimonDev Quick_Grass (MIT).
// See external/quick-grass/LICENSE and README.md for the pinned source.
layout(location=0) in vec3 aRoot;
layout(location=1) in vec3 aUp;
layout(location=2) in vec4 aVariation;
uniform mat4 model,view,projection,uShadowMatrix;
uniform vec3 uGrassEyeBody;
uniform float uMetersPerRadius,uTime,uWindStrength,uGrassHeight,uGrassWidth,uDrawDistance;
uniform int uSegments;
out vec3 vBodyPosition,vWorldPosition,vNormal,vUp,vColor;
out vec4 vShadowPosition;
out vec3 vBlade; // height fraction, side, detailed shading weight

// Improved Perlin gradient noise: corner dot products and a quintic fade.
// Algorithm: https://cs.nyu.edu/~perlin/noise/ . Integer hashing replaces
// the reference permutation table; no textures or extra instance data.
float grassGradient(ivec3 cell, vec3 offset) {
    uvec3 c=uvec3(cell & ivec3(255));
    uint h=c.x*1597334677u ^ c.y*3812015801u ^ c.z*2798796415u;
    h^=h>>16; h*=0x7feb352du; h^=h>>15;
    const vec3 gradients[12]=vec3[12](
        vec3(1,1,0),vec3(-1,1,0),vec3(1,-1,0),vec3(-1,-1,0),
        vec3(1,0,1),vec3(-1,0,1),vec3(1,0,-1),vec3(-1,0,-1),
        vec3(0,1,1),vec3(0,-1,1),vec3(0,1,-1),vec3(0,-1,-1));
    return dot(gradients[h%12u],offset);
}
float grassPerlin(vec3 p) {
    // Bound integer conversion as well as the gradient lookup. The field
    // repeats every 256 cells, including at negative coordinates.
    p=mod(p,256.0);
    ivec3 cell=ivec3(floor(p));
    vec3 f=fract(p);
    vec3 blend=f*f*f*(f*(f*6.0-15.0)+10.0);
    return mix(
        mix(mix(grassGradient(cell,f),grassGradient(cell+ivec3(1,0,0),f-vec3(1,0,0)),blend.x),
            mix(grassGradient(cell+ivec3(0,1,0),f-vec3(0,1,0)),grassGradient(cell+ivec3(1,1,0),f-vec3(1,1,0)),blend.x),blend.y),
        mix(mix(grassGradient(cell+ivec3(0,0,1),f-vec3(0,0,1)),grassGradient(cell+ivec3(1,0,1),f-vec3(1,0,1)),blend.x),
            mix(grassGradient(cell+ivec3(0,1,1),f-vec3(0,1,1)),grassGradient(cell+ivec3(1,1,1),f-vec3(1,1,1)),blend.x),blend.y),blend.z);
}

mat3 axisRotation(vec3 a,float angle) {
    float s=sin(angle),c=cos(angle),k=1.0-c;
    return mat3(c+a.x*a.x*k,a.y*a.x*k+a.z*s,a.z*a.x*k-a.y*s,
                a.x*a.y*k-a.z*s,c+a.y*a.y*k,a.z*a.y*k+a.x*s,
                a.x*a.z*k+a.y*s,a.y*a.z*k-a.x*s,c+a.z*a.z*k);
}
float grassLodFadeEnd(float variation) {
    // Shared with GrassLod.cpp: a stable, tint-independent retention tier.
    uint h=uint(variation*65536.0);
    h^=h>>16; h*=0x7feb352du; h^=h>>15; h*=0x846ca68bu; h^=h>>16;
    float nearDistance=min(15.0,uDrawDistance*.25);
    return nearDistance+float((h&7u)+1u)*(uDrawDistance-nearDistance)/8.0;
}
void main() {
    float t=float(gl_VertexID/2)/float(uSegments);
    // The final strip vertex is the shared apex; no duplicate zero-area tip.
    float side=float(gl_VertexID%2);
    float distanceToEye=length(aRoot-uGrassEyeBody)*uMetersPerRadius;
    float nearDistance=min(15.0,uDrawDistance*.25);
    float low=smoothstep(nearDistance*.25,nearDistance*.5,distanceToEye);
    float fadeEnd=grassLodFadeEnd(aVariation.w);
    float tierWidth=(uDrawDistance-nearDistance)/8.0;
    float fade=min(1.0-smoothstep(fadeEnd-tierWidth,fadeEnd,distanceToEye),
                  1.0-smoothstep(uDrawDistance*.75,uDrawDistance,distanceToEye));
    vec3 up=normalize(aUp);
    vec3 right=normalize(cross(abs(up.z)<0.9 ? vec3(0,0,1) : vec3(0,1,0),up));
    vec3 forward=cross(right,up);
    mat3 frame=mat3(right,up,forward);
    vec3 p=aRoot*uMetersPerRadius;
    // Advected 3D fields stay attached to the body without a UV/pole seam.
    // All advection rates traverse whole 256-cell periods in 8192 seconds;
    // GrassWind.h wraps the double clock by that exact common period.
    float gust=clamp(0.5+0.5*grassPerlin(p*.08+uTime*vec3(.25,.125,0)),0.0,1.0);
    // Distant blades become straight before the CPU can lower their segment
    // count. Collinear intermediate vertices then vanish without a shape pop.
    float windAngle=pow(mix(.25,1.0,gust),2.0)*1.25*uWindStrength*mix(t,1.0,low);
    float windDirection=grassPerlin(p*.02+vec3(19.3,7.1,43.7)+uTime*vec3(.03125,0,0));
    mat3 wind=axisRotation(vec3(cos(windDirection),0,sin(windDirection)),windAngle);
    mat3 turn=axisRotation(vec3(0,1,0),aVariation.x);
    float lean=aVariation.y+.1*uWindStrength*grassPerlin(p*.7+vec3(5.2,31.8,11.4)+uTime*vec3(0,.5,0));
    float curve=-lean*mix(t*t,1.0,low);
    float height=uGrassHeight*aVariation.z;
    float width=uGrassWidth*mix(1.0-t*t,1.0-t,low);
    vec3 local=axisRotation(vec3(1,0,0),curve)*vec3((side-.5)*width,t*height,0);
    local*=fade; // Smooth collapse while sinking; retired strips have zero area.
    mat3 bend=frame*wind*turn;
    // Retiring blades sink along local gravity, rather than popping out or
    // shrinking toward a conspicuous bright root on the ground.
    float sink=(height+uGrassWidth+.01)*(1.0-fade);
    vec3 body=aRoot+(bend*local-up*sink)/uMetersPerRadius;
    float derivative=-2.0*lean*t*(1.0-low);
    vec3 tangent=normalize(vec3(0,cos(curve)-t*sin(curve)*derivative,sin(curve)+t*cos(curve)*derivative));
    vec3 bladeNormal=vec3(0,-tangent.z,tangent.y);
    bladeNormal=axisRotation(vec3(0,1,0),mix(.3,-.3,side)*3.14159265)*bladeNormal;
    vNormal=normalize(mat3(model)*normalize(mix(up,bend*bladeNormal,.25*(1.0-low))));
    vUp=normalize(mat3(model)*up);
    vec3 base=mix(vec3(.02,.075,.01),vec3(.025,.1,.01),aVariation.w);
    vec3 tip=mix(vec3(.65,.8,.25),vec3(.8,.9,.4),aVariation.w);
    vColor=mix(mix(base,tip,pow(t,4.0))*mix(.75,1.0,aVariation.w),mix(vec3(.02,.075,.01),vec3(.65,.8,.25),t),low);
    vBlade=vec3(t,side,1.0-low);
    vBodyPosition=body;
    vec4 world=model*vec4(body,1);
    vWorldPosition=world.xyz;
    vShadowPosition=uShadowMatrix*vec4(body,1);
    gl_Position=projection*view*world;
}
