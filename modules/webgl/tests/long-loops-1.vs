attribute vec3 Vertex;
attribute vec2 Tex;

varying vec2 texCoord0;
void main()
{
  texCoord0 = vec2(Tex.s, Tex.t);
  gl_Position = vec4(Vertex, 1.0);
}
