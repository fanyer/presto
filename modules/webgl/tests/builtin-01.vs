precision highp float;

/* Resolution of types for vector intrinsics,
   including return types. CORE-44337. */
void main()
{
    vec2 v2 = vec2(1.);
    vec3 v3 = vec3(1.);
    vec4 v4 = vec4(1.);

    ivec2 iv2 = ivec2(1);
    ivec3 iv3 = ivec3(1);
    ivec4 iv4 = ivec4(1);

    bvec2 bv2 = bvec2(true);
    bvec3 bv3 = bvec3(false);
    bvec4 bv4 = bvec4(true);

    if (any(lessThan(v2, v2))) return;
    if (any(lessThan(v3, v3))) return;
    if (any(lessThan(v4, v4))) return;

    if (all(lessThan(iv2, iv2))) return;
    if (all(lessThan(iv3, iv3))) return;
    if (all(lessThan(iv4, iv4))) return;

    if (any(lessThanEqual(v2, v2))) return;
    if (any(lessThanEqual(v3, v3))) return;
    if (any(lessThanEqual(v4, v4))) return;

    if (all(not(lessThanEqual(iv2, iv2)))) return;
    if (all(not(lessThanEqual(iv3, iv3)))) return;
    if (all(not(lessThanEqual(iv4, iv4)))) return;

    if (any(greaterThan(v2, v2))) return;
    if (any(greaterThan(v3, v3))) return;
    if (any(greaterThan(v4, v4))) return;

    if (all(greaterThan(iv2, iv2))) return;
    if (all(greaterThan(iv3, iv3))) return;
    if (all(greaterThan(iv4, iv4))) return;

    if (any(greaterThanEqual(v2, v2))) return;
    if (any(greaterThanEqual(v3, v3))) return;
    if (any(greaterThanEqual(v4, v4))) return;

    if (all(not(greaterThanEqual(iv2, iv2)))) return;
    if (all(not(greaterThanEqual(iv3, iv3)))) return;
    if (all(not(greaterThanEqual(iv4, iv4)))) return;

    if (any(equal(v2, v2))) return;
    if (any(equal(v3, v3))) return;
    if (any(equal(v4, v4))) return;

    if (all(equal(iv2, iv2))) return;
    if (all(equal(iv3, iv3))) return;
    if (all(equal(iv4, iv4))) return;

    if (all(equal(bv2, bv2))) return;
    if (all(equal(bv3, bv3))) return;
    if (all(equal(bv4, bv4))) return;

    if (any(notEqual(v2, v2))) return;
    if (any(notEqual(v3, v3))) return;
    if (any(notEqual(v4, v4))) return;

    if (all(not(notEqual(iv2, iv2)))) return;
    if (all(not(notEqual(iv3, iv3)))) return;
    if (all(not(notEqual(iv4, iv4)))) return;

    if (all(not(notEqual(bv2, bv2)))) return;
    if (all(not(notEqual(bv3, bv3)))) return;
    if (all(not(notEqual(bv4, bv4)))) return;
}
