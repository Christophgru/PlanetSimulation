#version 330 core
in vec2 vUv;
out vec2 fColumns;
void main() {
    // Horizon-focused mapping: radius through rho, direction through path length.
    vec2 unit = clamp((gl_FragCoord.xy - 0.5) / vec2(255.0,127.0), 0.0,1.0);
    float H = sqrt(uAtmOuter*uAtmOuter-1.0), rho = unit.y*H;
    float r = sqrt(1.0+rho*rho);
    float shortest = uAtmOuter-r, longest = rho+H;
    float distance = mix(shortest,longest,unit.x);
    float mu = distance > 1e-6 ? clamp((H*H-rho*rho-distance*distance)/(2.0*r*distance),-1.0,1.0) : 1.0;
    vec3 point = vec3(0,r,0), direction = vec3(sqrt(max(0.0,1.0-mu*mu)),mu,0);
    fColumns = vec2(0.0);
    // Match the reference light-path integration; store density columns, not
    // colour, so composition, Sun direction and exposure do not invalidate it.
    for (int i=0;i<8;++i)
        fColumns += atmosphereDensity(point+direction*((float(i)+0.5)*distance/8.0)) * distance/8.0;
}
