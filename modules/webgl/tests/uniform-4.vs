attribute vec3 Vertex; attribute vec2 Tex;
uniform float x[3];
void main()
{
  int idx = 4 * int(max(1.0, Vertex.x*20.0));
  x[idx] = Vertex.z;
  gl_Position = vec4(Vertex.st, Tex.s, x[idx]);
}
