#ifdef GL_ES
precision highp float;
#endif
varying vec2 TexCoord;
void main()
{
    float z = TexCoord.s;
    while(z > 0.0) { z += 0.1; z *= 0.995; }
    gl_FragColor = vec4(1.0, TexCoord.s, TexCoord.t, z);
}
