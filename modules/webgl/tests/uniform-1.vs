attribute vec3 Vertex; attribute vec2 Tex;
uniform float x[3];
void main()
{
    gl_Position = vec4(Vertex.st, Tex.s, x[4]);
}
