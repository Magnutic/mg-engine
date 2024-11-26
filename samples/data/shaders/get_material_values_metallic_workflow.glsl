struct MaterialValues {
    vec3 albedo;
    vec3 normal;
    vec3 specular;
    float alpha;
    float ambient_occlusion;
    float gloss;
};

void get_material_values(vec3 tex_coord, out MaterialValues m_out)
{
    vec4 albedo_alpha = GET_SAMPLER_VALUE(sampler_diffuse, tex_coord);
    vec3 arm = GET_SAMPLER_VALUE(sampler_ao_roughness_metallic, tex_coord).rgb;
    vec3 normal = unpack_normal(GET_SAMPLER_VALUE(sampler_normal, tex_coord).xyz);
    float metallic_factor = arm.b;

    m_out.albedo = albedo_alpha.rgb - albedo_alpha.rgb * metallic_factor;
    m_out.normal = normal;
    m_out.specular = mix(vec3(0.04), albedo_alpha.rgb, metallic_factor);
    m_out.alpha = albedo_alpha.a;
    m_out.ambient_occlusion = arm.r;
    m_out.gloss = 1.0 - arm.g;
}
