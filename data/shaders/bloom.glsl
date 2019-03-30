float weight[8] = float[] (0.224907, 0.191869, 0.119121, 0.053815, 0.017686, 0.004228, 0.000735, 0.000093);

void main() {
    vec2 tex_offset = 1.0 / textureSize(sampler_colour, 0); // gets size of single texel
    vec3 result = texture(sampler_colour, tex_coord).rgb * weight[0]; // current fragment's contribution

#if HORIZONTAL
#define offset_expr (vec2(tex_offset.x * i, 0.0))
#else
#define offset_expr (vec2(0.0, tex_offset.x * i))
#endif

    for (int i = 1; i < 6; ++i)
    {
        result += texture(sampler_colour, tex_coord + offset_expr).rgb * weight[i];
        result += texture(sampler_colour, tex_coord - offset_expr).rgb * weight[i];
    }

    frag_out = vec4(result, 1.0);
}
