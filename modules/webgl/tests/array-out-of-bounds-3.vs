attribute vec3 Vertex;
attribute vec2 Tex;
varying vec2 TexCoord;
void main()
{
  TexCoord = Tex;
  float x[3];
  x[4] = Vertex.z;
  gl_Position = vec4(Vertex, x[4]);
}
