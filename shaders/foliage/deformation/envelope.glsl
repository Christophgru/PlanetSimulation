// Each corner independently interpolates between its fine and parent positions.
// Cross products are affine in each corner separately, so convexity of length
// bounds every intermediate area by the maximum of the eight endpoint triples.
void terrainMorphEnvelope(uvec3 corners,uint samples,out dvec3 center,out double reach,out double area) {
    dvec3 endpoint[6];
    dvec3 low=dvec3(1.0e300lf),high=-low;
    double magnitude=0.0lf;
    for(int i=0;i<3;++i) {
        endpoint[2*i]=terrainMorphFinePosition(corners[i]);
        endpoint[2*i+1]=terrainMorphParentPosition(corners[i],samples);
        for(int j=0;j<2;++j) {
            dvec3 p=endpoint[2*i+j];low=min(low,p);high=max(high,p);
            magnitude=max(magnitude,length(p));
        }
    }
    // Covers final float3 conversion, intermediate double arithmetic and tiny
    // endpoints. Edge perturbations are <= 2*error at each side of the cross.
    double error=max(1.0e-30lf,magnitude*(4.0lf*1.1920928955078125e-7lf));
    center=(low+high)*0.5lf;reach=0.0lf;
    for(int i=0;i<6;++i) reach=max(reach,length(endpoint[i]-center));
    reach+=error;
    double crossed=0.0lf,ab=0.0lf,ac=0.0lf;
    for(int a=0;a<2;++a) for(int b=0;b<2;++b) for(int c=0;c<2;++c) {
        dvec3 x=endpoint[2+b]-endpoint[a],y=endpoint[4+c]-endpoint[a];
        crossed=max(crossed,length(cross(x,y)));ab=max(ab,length(x));ac=max(ac,length(y));
    }
    area=0.5lf*(crossed+2.0lf*error*(ab+ac)+4.0lf*error*error);
}
