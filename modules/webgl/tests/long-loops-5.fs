#ifdef GL_ES
precision highp float;
#endif
varying vec2 TexCoord;
void main()
{
    float z = 1.0;
    while(z > TexCoord.s) { z += 0.1; z *= 0.995; }
    gl_FragColor = vec4(1.0, TexCoord.s, TexCoord.t, z);
}
