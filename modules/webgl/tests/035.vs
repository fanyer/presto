// Test type derivation for swizzle expressions

void main()
{
    float f1; vec2 f2; vec3 f3; vec4 f4;
    int i1; ivec2 i2; ivec3 i3; ivec4 i4;
    bool b1; bvec2 b2; bvec3 b3; bvec4 b4;

    f1 = f2.x; f1 = f3.x; f1 = f4.x;
    f2 = f2.xy; f2 = f3.xy; f2 = f4.xy;
    f3 = f3.xyz; f3 = f4.xyz;
    f4 = f4.xyzw;

    i1 = i2.x; i1 = i3.x; i1 = i4.x;
    i2 = i2.xy; i2 = i3.xy; i2 = i4.xy;
    i3 = i3.xyz; i3 = i4.xyz;
    i4 = i4.xyzw;

    b1 = b2.x; b1 = b3.x; b1 = b4.x;
    b2 = b2.xy; b2 = b3.xy; b2 = b4.xy;
    b3 = b3.xyz; b3 = b4.xyz;
    b4 = b4.xyzw;

    gl_Position = f4;
}
