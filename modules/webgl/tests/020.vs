/*
void main()
{
    vec4 v = vec4(1.0);
    float y = v[int(sign(2.)) + int(abs(-3.))];
}
*/
attribute vec3 Vertex;
attribute vec2 Tex;
varying vec2 TexCoord;
void main()
{
    TexCoord = Tex;
    float x[3];
    x[0] = 1.0 + 1;
    x[1] = 2.0;
    x[2] = 3.0;
    
    int idx = 4 * int(max(1.0, Vertex.x*20.0));
    gl_Position = vec4(Vertex, x[idx]);
}
