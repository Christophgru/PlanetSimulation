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

mat3 axisRotation(vec3 a,float angle) {
    float s=sin(angle),c=cos(angle),k=1.0-c;
    return mat3(c+a.x*a.x*k,a.y*a.x*k+a.z*s,a.z*a.x*k-a.y*s,
                a.x*a.y*k-a.z*s,c+a.y*a.y*k,a.z*a.y*k+a.x*s,
                a.x*a.z*k+a.y*s,a.y*a.z*k-a.x*s,c+a.z*a.z*k);
}
void main() {
    float t=float(gl_VertexID/2)/float(uSegments);
    float side=float(gl_VertexID%2);
    float distanceToEye=length(aRoot-uGrassEyeBody)*uMetersPerRadius;
    float low=smoothstep(7.5,15.0,distanceToEye);
    float fade=1.0-smoothstep(uDrawDistance*0.75,uDrawDistance,distanceToEye);
    vec3 up=normalize(aUp);
    vec3 right=normalize(cross(abs(up.z)<0.9 ? vec3(0,0,1) : vec3(0,1,0),up));
    vec3 forward=cross(right,up);
    mat3 frame=mat3(right,up,forward);
    vec3 p=aRoot*uMetersPerRadius;
    // Coherent broad gusts plus a small blade-local flutter, in body metres.
    float gust=0.5+0.5*sin(dot(p,vec3(.19,.11,.23))+uTime)*sin(dot(p,vec3(.07,.13,.05))+.7*uTime);
    float windAngle=pow(mix(.25,1.0,gust),2.0)*1.25*uWindStrength*t;
    float windDirection=sin(dot(p,vec3(.03,.05,.02))+.05*uTime);
    mat3 wind=axisRotation(vec3(cos(windDirection),0,sin(windDirection)),windAngle);
    mat3 turn=axisRotation(vec3(0,1,0),aVariation.x);
    float lean=aVariation.y+.1*uWindStrength*sin(dot(p,vec3(13.7,9.1,7.3))+.35*uTime);
    float curve=-lean*mix(t*t,1.0,low);
    float height=uGrassHeight*aVariation.z*fade;
    float width=uGrassWidth*mix(1.0-t*t,1.0-t,low)*fade;
    vec3 local=axisRotation(vec3(1,0,0),curve)*vec3((side-.5)*width,t*height,0);
    mat3 bend=frame*wind*turn;
    vec3 body=aRoot+bend*local/uMetersPerRadius;
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
