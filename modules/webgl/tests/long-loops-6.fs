#ifdef GL_ES
precision highp float;
#endif
varying vec2 TexCoord;
void main()
{
  gl_FragColor = vec4(1.0, TexCoord.s, TexCoord.t, 1.0);
}
