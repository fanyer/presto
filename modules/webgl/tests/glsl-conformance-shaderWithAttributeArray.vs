attribute vec4 vPosition[2];
void main()
{
    gl_Position = vPosition[0] + vPosition[1];
}

