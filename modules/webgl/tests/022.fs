#ifdef GL_ES
precision mediump float;
#endif
uniform vec2 color[3];
void main()
{
   gl_FragColor = vec4(color[0][1], color[1][1], color[2][1], 1);
}
