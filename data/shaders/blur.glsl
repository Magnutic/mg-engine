float offset[3] = float[](0.0, 1.3846153846, 3.2307692308);
float weight[3] = float[](0.2270270270, 0.3162162162, 0.0702702703);

void main() {
    int source_mip_level = material_params.source_mip_level;
    vec2 in_texel_size = 1.0 / textureSize(sampler_colour, source_mip_level);
    vec3 result = textureLod(sampler_colour, tex_coord, source_mip_level).rgb * weight[0];

#if HORIZONTAL
#define offset_expr (vec2(in_texel_size.x * offset[i], 0.0))
#else
#define offset_expr (vec2(0.0, in_texel_size.y * offset[i]))
#endif

    for (int i = 1; i < 3; ++i)
    {
        result += textureLod(sampler_colour, tex_coord + offset_expr, source_mip_level).rgb * weight[i];
        result += textureLod(sampler_colour, tex_coord - offset_expr, source_mip_level).rgb * weight[i];
    }

    frag_out = vec4(max(vec3(0.0), result), 1.0);
}
