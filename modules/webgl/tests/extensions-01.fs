precision highp float;
#extension all : warn
#extension all : disable
#extension GL_OES_standard_derivatives : enable

#ifdef GL_OES_standard_derivatives
uniform vec2 x;
#endif

void main()
{
    vec2 f = fwidth(x);
    return;
}
