#ifdef GL_ES
precision highp float;
#endif
uniform float baz;

varying vec2 TexCoord;
void main()
{
  gl_FragColor = vec4(1.0, TexCoord.s, 0.0, 1.0);
}
