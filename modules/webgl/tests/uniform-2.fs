#ifdef GL_ES
precision highp float;
#endif
uniform float x[3];

varying vec2 TexCoord;
void main()
{
  x[4] = 6.0;
  gl_FragColor = vec4(1.0, 0.0, 0.0, x[4]);
}
