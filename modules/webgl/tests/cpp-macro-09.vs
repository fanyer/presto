// dump_output
#define LIGHT_COUNT 1
#define TEXTURE_COLOR 1
#define TEXTURE_SPECULAR 0
#define TEXTURE_NORMAL 1
#define TEXTURE_BUMP 1
#define TEXTURE_REFLECT 0
#define TEXTURE_ENVSPHERE 1
#define TEXTURE_AMBIENT 0
#define TEXTURE_ALPHA 0
#define MATERIAL_ALPHA 0
#define LIGHT_IS_POINT 1
#define LIGHT_IS_DIRECTIONAL 0
#define LIGHT_IS_SPOT 0
#define LIGHT_SHADOWED 0
#define LIGHT_IS_PROJECTOR 0
#define LIGHT_SHADOWED_SOFT 0
#define LIGHT_IS_AREA 0
#define LIGHT_DEPTH_PASS 0
#define FX_DEPTH_ALPHA 0
#define VERTEX_MORPH 0
#define VERTEX_COLOR 0
#define LIGHT_PERPIXEL 1


#if TEXTURE_AMBIENT
#if LIGHT_IS_POINT||LIGHT_IS_DIRECTIONAL||LIGHT_IS_SPOT||LIGHT_IS_AREA
uniform int is_fail_1;
#else
uniform int is_wrong_2;
#endif
#else
#if TEXTURE_COLOR
uniform int is_pass;
#else
uniform int is_wrong_3;
#endif
#endif
#endif

void main()
{
    return;
}
