// Unpack normal from normal map
vec3 unpack_normal(const vec3 packed_normal)
{
    vec3 n = packed_normal * (255.0 / 128.0) - 1.0;
    n.z    = sqrt(1.0 - dot(n.x, n.x) - dot(n.y, n.y));
    return normalize(TANGENT_TO_WORLD * n);
}

void final_colour(const SurfaceInput s_in, const SurfaceParams s, inout vec4 colour) {
    const float fog_density = 0.025;
    float depth = distance(WORLD_POSITION, CAMERA_POSITION);
    float fog_factor = 1.0 - exp2(-fog_density * depth);
    colour = mix(colour, vec4(2.7, 2.4, 5.5, 1.0), fog_factor);
}

const float parallax_range = 15.0;
const float parallax_depth = 1.0;

#if PARALLAX
// Apply parallax displacement. Assumes alpha channel of map contains height. View vector should be
// in tangent space.
vec3 parallax(float amount, vec3 view_vector, vec3 tex_coord, sampler2D map)
{
    float parallax_bias = -amount * 0.5;

    for (int i = 0; i < 4; ++i) {
        float height = amount * texture(map, tex_coord.xy).a + parallax_bias;
        tex_coord += ((height - tex_coord.z) * view_vector) * vec3(1.0, -1.0, 1.0);
    }

    return tex_coord;
}
#endif

void surface(const SurfaceInput s_in, out SurfaceParams s_out) {
    // parallax mapping algorithm requires three-dimensional texture coords
    vec3 tex_coord = vec3(TEX_COORD, 0.0);

#if PARALLAX
    vec3 view_vector               = CAMERA_POSITION - WORLD_POSITION;
    vec3 view_tangentspace         = normalize(transpose(TANGENT_TO_WORLD) * view_vector);

    float dist_sqr                 = dot(view_vector, view_vector);
    const float parallax_range_sqr = parallax_range * parallax_range;

    // apply parallax (if near enough)
    if (dist_sqr < parallax_range_sqr) {
        // fade out effect of parallax with distance
        float fade = smoothstep(1.0, 0.0, dist_sqr / parallax_range_sqr);
        tex_coord   = parallax(parallax_depth * 0.03 * fade, view_tangentspace, tex_coord, sampler_diffuse);
    }
#endif

    vec4 spec_gloss = texture(sampler_specular, tex_coord.xy);
    vec4 albedo_alpha = texture(sampler_diffuse, tex_coord.xy);
    vec3 normal = unpack_normal(texture(sampler_normal, tex_coord.xy).xyz);

    s_out.albedo   = albedo_alpha.rgb;
    s_out.specular = spec_gloss.rgb;
    s_out.gloss    = spec_gloss.a;
    s_out.normal   = normal;
    s_out.emission = albedo_alpha.rgb * material_params.ambient_colour.rgb;

#if RIM_LIGHT
    float rim_factor = 1.0 - max(0.0, dot(s_in.view_direction, normal));
    rim_factor       = pow(rim_factor, material_params.rim_power) * material_params.rim_intensity;
    s_out.emission   += rim_factor * spec_gloss.rgb;
#endif

    s_out.occlusion  = 0.0;
    s_out.alpha      = 1.0;
}
