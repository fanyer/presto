#ifdef GL_ES
precision highp float;
#endif
uniform float x[3];

varying vec2 TexCoord;
void main()
{
  int idx = 4 * int(max(1.0, TexCoord.s*20.0));
  gl_FragColor = vec4(1.0, 0.0, 0.0, x[idx]);
}
