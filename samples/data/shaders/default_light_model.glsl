// Calculate (light) attenuation
float attenuate(float distance_sqr, float range_sqr_reciprocal)
{
    // Clamps distance to light as used in attenuation calculations. Prevents unbounded luminance.
    const float point_light_radius = 0.05f;

    distance_sqr = max(distance_sqr, point_light_radius * point_light_radius);

    float attenuation = 1.0 - distance_sqr * range_sqr_reciprocal;
    attenuation *= attenuation;
    attenuation /= distance_sqr;

    return max(0.0, attenuation);
}

//--------------------------------------------------------------------------------------------------
// Microfacet normal distribution functions
//--------------------------------------------------------------------------------------------------

// The Blinn-Phong microfacet normal distribution function
float blinn_phong_normal_distribution(float NdotH, float specular_power, float gloss)
{
    float Distribution = pow(NdotH, gloss) * specular_power;
    Distribution *= (2.0 + specular_power) / (2.0 * pi);
    return Distribution;
}

// The GGX microfacet normal distribution function
float ggx_normal_distribution(float roughness, float NdotH)
{
    float roughness_sqr = roughness * roughness;
    float distribution = NdotH * NdotH * (roughness_sqr - 1.0) + 1.0;
    return roughness_sqr / (pi * sqr(distribution));
}

//--------------------------------------------------------------------------------------------------
// Geometric shadowing functions
//--------------------------------------------------------------------------------------------------

float schlick_ggx_geometric_shadowing(float NdotL, float NdotV, float roughness)
{
    float k = roughness / 2;
    float l = (NdotL) / (NdotL * (1.0 - k) + k);
    float v = (NdotV) / (NdotV * (1.0 - k) + k);
    return l * v;
}

//--------------------------------------------------------------------------------------------------
// Fresnel functions
//--------------------------------------------------------------------------------------------------

float pow5(float x)
{
    return (x * x) * (x * x) * x;
}

// Schlick approximation of the fresnel factor
float schlick_fresnel(float i)
{
    float base = 1.0 - clamp(i, 0.0, 1.0);
    return pow5(base);
}

// Diffuse fresnel makes the BRDF more "correct", but has little visible impact.
#if USE_DIFFUSE_FRESNEL
// Normal incidence reflection calculation
float diffuse_schlick_fresnel_f0(float NdotL, float NdotV, float LdotH, float roughness)
{
    float f90 = 0.5 + 2.0 * LdotH * LdotH * roughness;
    float light_scatter = schlick_fresnel(NdotL);
    float view_scatter = schlick_fresnel(NdotV);
    return (f90 * light_scatter + (1.0 - light_scatter)) *
           (f90 * view_scatter + (1.0 - view_scatter));
}
#endif

// Fresnel factor for specularity using the schlick approximation.
vec3 specular_schlick_fresnel(vec3 specular_colour, float LdotH)
{
    return specular_colour + (1.0 - specular_colour) * schlick_fresnel(LdotH);
}

//--------------------------------------------------------------------------------------------------
// Light model function
//--------------------------------------------------------------------------------------------------

vec3 light(const LightInput light, const SurfaceParams surface, const vec3 view_direction)
{
    // TODO This is an arbitrary remapping of gloss response to roughness.
    // Roughness of 0.0 seems to have unexpected results; probably makes no mathematical/physical
    // sense anyway. Roughness too close to 0.0 does not look good with point lights, since you will
    // get only individual pixels lighting up extremely bright, causing flashing bloom artefacts
    // (and not having a visible highlight at all). Capping the roughness could be seen as
    // compensating for the lack of area of point lights.
    float roughness = max(0.01, sqr(sqr(1.0 - surface.gloss)));

    // "Halfway vectors" for lighting calculations.
    vec3 h = normalize(light.direction + view_direction);

    // Lighting factors
    float NdotL = max(0.0, dot(surface.normal, light.direction));
    float NdotH = max(0.0, dot(surface.normal, h));
    float NdotV = max(0.0, dot(surface.normal, view_direction));
    float LdotH = max(0.0, dot(light.direction, h));

    //----------------------------------------------------------------------------------------------
    // Diffuse contribution
    //----------------------------------------------------------------------------------------------

    vec3 diffuse_contribution = surface.albedo;

#if USE_DIFFUSE_FRESNEL
    diffuse_contribution *= diffuse_schlick_fresnel_f0(NdotL, NdotV, LdotH, roughness);
#endif

    //----------------------------------------------------------------------------------------------
    // Specular contribution
    //----------------------------------------------------------------------------------------------

#if USE_BLINN_PHONG
    // Blinn-Phong specular
    float gloss = exp2(10.0 * surface.gloss + 1.0);
    float power = ((gloss + 2.0) / 8.0);
    float distribution = blinn_phong_normal_distribution(NdotH, power, gloss);
#else
    // GGX specular
    float distribution = max(0.0, ggx_normal_distribution(roughness, NdotH));
#endif

    float geometric_shadow = schlick_ggx_geometric_shadowing(NdotL, NdotV, roughness);

    // Fresnel factor
    vec3 fresnel = specular_schlick_fresnel(surface.specular, LdotH);

    // Compose specular contribuation
    const float epsilon = 0.000001; // Avoid division by zero
    vec3 specular_contribution = (distribution * fresnel * geometric_shadow) /
                                 (4.0 * NdotL * NdotV + epsilon);

    //----------------------------------------------------------------------------------------------
    // Final composition
    //----------------------------------------------------------------------------------------------

    vec3 lighting_model = (diffuse_contribution + specular_contribution) * NdotL;
    return lighting_model * light.colour * attenuate(light.distance_sqr, 1.0 / light.range_sqr);
}
