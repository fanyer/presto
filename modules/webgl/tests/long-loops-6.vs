attribute vec3 Vertex; attribute vec2 Tex;
varying vec2 TexCoord;
void main()
{
    TexCoord = Tex.st;
    gl_Position = vec4(Vertex, 0.0);
}
