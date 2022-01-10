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
    vec4 albedo_alpha = texture(sampler_diffuse, tex_coord.xy);
    vec4 specular_glossiness = texture(sampler_specular, tex_coord.xy);
    vec3 normal = unpack_normal(texture(sampler_normal, tex_coord.xy).xyz);

    m_out.albedo = albedo_alpha.rgb;
    m_out.normal = normal;
    m_out.specular = specular_glossiness.rgb;
    m_out.alpha = albedo_alpha.a;
    m_out.ambient_occlusion = 1.0;
    m_out.gloss = specular_glossiness.a;
}

