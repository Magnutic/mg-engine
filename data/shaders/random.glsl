// Roughly uniform pseudo-random number generation functions.
//
// Based on the following blog post (and comments there):
// http://amindforeverprogramming.blogspot.com/2013/07/random-floats-in-glsl-330.html
//
// I have not carefully evaluated this myself, but this approach looked the most promising to my
// eyes, when I searched for GLSL random number generation.

uint hash_int(uint v)
{
    uint hash = v;
    hash += hash >> 11;
    hash ^= hash << 7;
    hash += hash >> 15;
    hash ^= hash << 5;
    hash += hash >> 12;
    hash ^= hash << 9;
    return hash;
}

uint hash_int(uvec2 v)
{
    uint hash = v.x;
    hash += hash >> 11;
    hash ^= hash << 7;
    hash += v.y;
    hash ^= hash << 6;
    hash += hash >> 15;
    hash ^= hash << 5;
    hash += hash >> 12;
    hash ^= hash << 9;
    return hash;
}

uint hash_int(uvec3 v)
{
    uint hash = v.x;
    hash += hash >> 11;
    hash ^= hash << 7;
    hash += v.y;
    hash ^= hash << 3;
    hash += v.z ^ (hash >> 14);
    hash ^= hash << 6;
    hash += hash >> 15;
    hash ^= hash << 5;
    hash += hash >> 12;
    hash ^= hash << 9;
    return hash;
}

const uint ieee_754_mantissa_mask = 0x007FFFFFu;
const uint ieee_754_one = 0x3f800000u;

float random(float v)
{
    uint hash = hash_int(floatBitsToUint(v));
    hash &= ieee_754_mantissa_mask;
    hash |= ieee_754_one;
    return uintBitsToFloat(hash) - 1.0;
}

float random(vec2 v)
{
    uint hash = hash_int(uvec2(floatBitsToUint(v.x), floatBitsToUint(v.y)));
    hash &= ieee_754_mantissa_mask;
    hash |= ieee_754_one;
    return uintBitsToFloat(hash) - 1.0;
}

float random(vec3 v)
{
    uint hash = hash_int(uvec3(floatBitsToUint(v.x), floatBitsToUint(v.y), floatBitsToUint(v.z)));
    hash &= ieee_754_mantissa_mask;
    hash |= ieee_754_one;
    return uintBitsToFloat(hash) - 1.0;
}
