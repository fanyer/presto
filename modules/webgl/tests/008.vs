int i0 = int(2);
int i1 = int(2.2);
int i2 = int(false);

float f0 = float(3.2);
float f1 = float(3);
float f2 = float(true);

bool b0 = bool(false);
bool b1 = bool(3.1);
bool b2 = bool(2);

vec3 v2 = vec3(1, 2, ivec2(3,3));
vec3 v0 = vec3(2);
vec3 v1 = vec3(1,2,3);
// type mismatch; no attempt at type checking.
vec3 v3 = vec4(1,2,3.3,3);

struct foo
{
    int x;
    int y;
};

foo sf1 = foo(2,2.2);
foo sf2 = foo(2,2);
