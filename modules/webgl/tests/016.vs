// Testing functions
float f1()
{
    float x = 1 + 1.3, y;
    return x;
}

float f2()
{
    return 2;
}

struct t { int x; int y; } f3()
{
    struct t2 { int x; int y} v = t2(2,3);
    return v;
}

vec2 f4(vec2 x)
{
    return x;
}

vec4 f4(vec4 x)
{
    return x;
}

vec3 f4(vec3 x)
{
    return x+x;
}

vec3 greaterThan(vec2 x, vec2 y)
{
    return x;
}

void main()
{
   vec3 x;
   f4(x);
}
