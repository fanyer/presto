#ifdef GL_ES
precision highp float;
#endif
uniform float x[3];

varying vec2 TexCoord;
void main()
{
  gl_FragColor = vec4(1.0, 0.0, 0.0, x[4]);
}
