attribute vec3 Vertex; attribute vec2 Tex;
varying vec2 TexCoord;
void main()
{
    int i;
    float z = 1.0;
    for (i = 0; i<1000000000; i++) {
        z += 0.1; z *= 0.995;
    }
    TexCoord = Tex.st;
    gl_Position = vec4(Vertex, z);
}
