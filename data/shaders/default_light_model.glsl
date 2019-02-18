// Clamps distance to light as used in attenuation calculations. Prevents unbounded luminance.
const float point_light_radius = 0.05f;

// Calculate (light) attenuation
float attenuate(float distance_sqr, float range_sqr_reciprocal)
{
    distance_sqr = max(distance_sqr, point_light_radius * point_light_radius);

    float attenuation = 1.0 - distance_sqr * range_sqr_reciprocal;
    attenuation *= attenuation;
    attenuation /= distance_sqr;

    return max(0.0, attenuation);
}

float pow5(float x) {
    return (x * x) * (x * x) * x;
}

// Calculate the Schlick approximation of the fresnel factor
vec3 schlick(vec3 gloss, vec3 E, vec3 H)
{
    float base = 1.0 - clamp(dot(E, H), 0.0, 1.0);
    float exp  = pow5(base);
    return gloss + (1.0 - gloss) * exp;
}

vec3 light(const LightInput light, const SurfaceParams surface, const vec3 view_direction)
{
    // Basic vectors for lighting calculation
    vec3 h = normalize(light.direction + view_direction);

    // Calculate lighting factors
    const float wrap_factor = 0.0;
    float NdotL_unclipped = dot(surface.normal, light.direction);
    float wrap_falloff = (NdotL_unclipped + 1.0) * 0.5;
    float NdotL = max(0.0, NdotL_unclipped);
    float NdotH = max(0.0, dot(surface.normal, h));

    // PBR Blinn-Phong specular
    float pwr              = exp2(10.0 * surface.gloss + 1.0);
    vec3  fresnel          = schlick(surface.specular, view_direction, h);
    vec3  specular_contrib = fresnel * ((pwr + 2.0) / 8.0) * pow(NdotH, pwr);

    float angle_factor = mix(NdotL, wrap_falloff, wrap_factor) / (1.0 + wrap_factor);
    return (surface.albedo * angle_factor + specular_contrib * NdotL) * light.colour * attenuate(light.distance_sqr, 1.0 / light.range_sqr);
}

