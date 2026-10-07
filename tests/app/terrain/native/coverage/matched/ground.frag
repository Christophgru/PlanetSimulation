#version 330 core
in vec3 offsetMeters,bodyNormal,bodyColor;
uniform vec3 rootBody,planetColor,landscapeLevels;
uniform float scale,greenRatio,waterClearance;
uniform vec2 rockRange;
uniform bool landscape,water;
layout(location=0) out vec4 ground;
layout(location=1) out vec4 plane;
void main() {
    vec3 p=rootBody+offsetMeters/scale;
    float height=(length(p)-1)*scale;
    float eligible=1;
    if(water && height<=landscapeLevels.x+waterClearance) eligible=0;
    vec3 tint=bodyColor*planetColor;
    if(landscape) {
        float beachTop=landscapeLevels.x+landscapeLevels.y;
        float beachFade=max(.15,.15*landscapeLevels.y);
        float relief=max(1,landscapeLevels.z-landscapeLevels.x);
        float snowStart=max(beachTop+1,landscapeLevels.x+.25*relief);
        float snowEnd=max(snowStart+1,landscapeLevels.x+.40*relief);
        if(height>=snowEnd) eligible=0;
        tint=mix(mix(vec3(1.10,1.30,.18),vec3(4.20,1.90,.18),
            1-smoothstep(beachTop,beachTop+beachFade,height)),vec3(4.60,2.30,.92),
            smoothstep(snowStart,snowEnd,height))*planetColor;
    }
    if(tint.y<=greenRatio*max(tint.x,tint.z)) eligible=0;
    float slope=1-dot(normalize(bodyNormal),normalize(p));
    eligible*=1-smoothstep(rockRange.x,rockRange.y,slope);
    // Expected rock retention, not one random placement realization. No Gaussian
    // density falloff in this independent eligible surface-area denominator.
    vec3 geometricNormal=cross(dFdx(offsetMeters),dFdy(offsetMeters));
    float area=length(geometricNormal);
    ground=vec4(offsetMeters,area*eligible);
    plane=vec4(area>0 ? geometricNormal/area : vec3(0),area);
}
