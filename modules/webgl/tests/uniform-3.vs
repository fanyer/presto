uniform float x[3];
attribute vec3 Vertex; attribute vec2 Tex;
void main()
{
  float idx = 40.0 * max(1.0, Vertex.x*20.0);
  gl_Position = vec4(Vertex, x[2] + Tex.s + x[int(idx)]);
}
