// hlsl_codegen
precision mediump float;

float f(vec3 v)
{
    return float(v.x);
}

void main()
{
    f(vec3(1,1,1));
    return;
}
