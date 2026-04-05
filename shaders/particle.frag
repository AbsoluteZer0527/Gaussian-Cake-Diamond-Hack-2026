#version 330 core

<<<<<<< Updated upstream
in vec4 vColor;
out vec4 fragColor;

void main() {
    // gl_PointCoord is (0,0) top-left to (1,1) bottom-right of the point sprite
    vec2  coord = gl_PointCoord - vec2(0.5);
    float dist  = length(coord);

    // Discard fragments outside the circle
    if (dist > 0.5) discard;

    // Soft edge: full opacity in centre, fades to 0 at rim
    float alpha = vColor.a * (1.0 - smoothstep(0.3, 0.5, dist));
    fragColor = vec4(vColor.rgb, alpha);
=======
out vec4 fragColor;

void main() {
    vec2  coord = gl_PointCoord - vec2(0.5);
    if (length(coord) > 0.5) discard;
    fragColor = vec4(0.9, 0.9, 0.9, 1.0);
>>>>>>> Stashed changes
}
