attribute vec4 vPosition;
void main()
{
    int k = 123;
    gl_Position = vec4(vPosition.x, vPosition.y, vPosition.z, float(k));
}

