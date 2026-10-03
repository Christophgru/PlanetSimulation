#version 330 core
uniform vec2 uViewport, uSun;
uniform vec3 uTint;
uniform float uStrength, uRadius;
out vec4 fragColor;
void main() {
    vec2 uv=gl_FragCoord.xy/uViewport;
    vec2 aspect=vec2(uViewport.x/uViewport.y,1);
    vec2 delta=(uv-uSun)*aspect;
    float haloWidth=max(.06,uRadius*.85);
    float halo=exp(-dot(delta,delta)/(haloWidth*haloWidth))*.38;
    float streak=exp(-delta.y*delta.y/.000006)*exp(-abs(delta.x)/.28)*.24;
    vec3 light=uTint*(halo+streak);
    for (int i=0;i<4;++i) {
        // Spread ghosts beyond the bright solar haze; tightly clustered,
        // low-energy ghosts disappeared in the production sunset view.
        float t=1.6+.95*float(i);
        vec2 ghost=mix(uSun,vec2(.5),t);
        float d=length((uv-ghost)*aspect);
        float radius=.022+.014*float(i);
        float ring=exp(-pow((d-radius)/.006,2));
        float fill=exp(-d*d/(radius*radius));
        vec3 tint=mix(vec3(.18,.38,.7),vec3(.7,.34,.16),float(i)/3);
        light+=tint*(ring*.55+fill*.12);
    }
    fragColor=vec4(light*uStrength,0);
}
