attribute vec3 Vertex; attribute vec2 Tex;
varying vec2 TexCoord;

void main()
{
    float z = 1.0;
    while(z > Vertex.z) { z += 0.1; z *= 0.995; }
    TexCoord = Tex.st;
    gl_Position = vec4(Vertex, z);
}
