// dump_output
#define quat(x,y,z,w) vec4(x,y,z,w)

float julia_dist(vec4 x);

#define OFFS 1e4
vec3 julia_grad(vec4 z)
{
    vec3 grad;
    grad.x = julia_dist(z + quat(OFFS, 0.0, 0.0, 0.0)) - julia_dist(z - quat(OFFS, 0.0, 0.0, OFFS));
    grad.y = julia_dist(z + quat(0.0, OFFS, 0.0, 0.0)) - julia_dist(z - quat(0.0, OFFS, 0.0, 0.0));
    grad.z = julia_dist(z + quat(0.0, 0.0, OFFS, 0.0)) - julia_dist(z - quat(0.0, 0.0, OFFS, 0.0));
    return grad;
}

void
main()
{
}
