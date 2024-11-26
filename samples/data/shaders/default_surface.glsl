void final_colour(const SurfaceInput s_in, const SurfaceParams s, inout vec4 colour)
{
    const float fog_density = 0.025;
    float depth = distance(WORLD_POSITION, CAMERA_POSITION);
    // Alternative, non-radial: float depth = linearize_depth(gl_FragCoord.z);
    float fog_factor = 1.0 - exp2(-fog_density * depth);
    colour = mix(colour, vec4(0.5, 0.4, 1.0, 1.0), fog_factor);
}

const float parallax_range = 15.0;
const float parallax_depth = 1.0;

#if PARALLAX
// Apply parallax displacement. Assumes alpha channel of map contains height. View vector should be
// in tangent space.
vec3 parallax(float amount, vec3 view_vector, vec3 tex_coord, SAMPLER_TYPE sampler)
{
    float parallax_bias = -amount * 0.5;
    float z = 0.0;

    for (int i = 0; i < 4; ++i) {
        float height = amount * GET_SAMPLER_VALUE(sampler, tex_coord).a + parallax_bias;
        vec3 f = ((height - z) * view_vector);
        tex_coord += f * vec3(1.0, -1.0, 0.0);
        z += f.z;
    }

    return tex_coord;
}
#endif

void surface(const SurfaceInput s_in, out SurfaceParams s_out)
{
    vec3 tex_coord = TEX_COORD;

#if PARALLAX
    vec3 view_vector = CAMERA_POSITION - WORLD_POSITION;
    vec3 view_tangentspace = normalize(transpose(TANGENT_TO_WORLD) * view_vector);

    float dist_sqr = dot(view_vector, view_vector);
    const float parallax_range_sqr = parallax_range * parallax_range;

    // apply parallax (if near enough)
    if (dist_sqr < parallax_range_sqr) {
        // fade out effect of parallax with distance
        float fade = smoothstep(1.0, 0.0, dist_sqr / parallax_range_sqr);
        tex_coord.xy =
            parallax(parallax_depth * 0.03 * fade, view_tangentspace, tex_coord, sampler_diffuse)
                .xy;
    }
#endif

    MaterialValues material;
    get_material_values(tex_coord, material);

    s_out.albedo = material.albedo;
    s_out.specular = material.specular;
    s_out.gloss = material.gloss;
    s_out.normal = material.normal;
    s_out.emission = material.albedo * parameters.ambient_colour.rgb * material.ambient_occlusion;

#if RIM_LIGHT
    float rim_factor = 1.0 - max(0.0, dot(s_in.view_direction, material.normal));
    rim_factor = pow(rim_factor, parameters.rim_power) * parameters.rim_intensity;
    s_out.emission += rim_factor * material.specular;
#endif

    s_out.occlusion = 0.0; // TODO: material.ambient_occlusion ?
    s_out.alpha = 1.0;
}
