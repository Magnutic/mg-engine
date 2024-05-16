// Unpack normal from normal map
vec3 unpack_normal(const vec3 packed_normal)
{
    vec3 n = packed_normal * (255.0 / 128.0) - 1.0;
    n.z = sqrt(1.0 - dot(n.x, n.x) - dot(n.y, n.y));
    return normalize(TANGENT_TO_WORLD * n);
}

float linearize_depth(float depth)
{
    return ZNEAR * ZFAR / (ZFAR + depth * (ZNEAR - ZFAR));
}

float sqr(float v)
{
    return v * v;
}

const float pi = 3.1415926535;
