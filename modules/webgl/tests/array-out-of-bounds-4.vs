attribute vec3 Vertex;
attribute vec2 Tex;
varying vec2 TexCoord;
void main()
{
  TexCoord = Tex;
  float x[3];
  int idx = 4 * int(max(1.0, Vertex.x*20.0));
  gl_Position = vec4(Vertex, x[idx]);
}
