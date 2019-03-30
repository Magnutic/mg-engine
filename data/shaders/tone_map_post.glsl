vec3 lotteTonemap(vec3 colour) {
    // hard-coded parameters, calculated from contrast, shoulder, hdr_max, mid_in, mid_out.
    // see https://raw.githubusercontent.com/Opioid/tonemapper/master/tonemapper.py
    const float a = 1.1;
    const float d = 0.97;
    const float b = 1.05;
    const float c = 0.43;
    vec3 z = pow(colour, vec3(a));
    return z / vec3(pow(z, vec3(d)) * b + c);
}

const float large_factor = 0.02;
const float medium_factor = 0.02;
const float small_factor = 0.03;

void main() {
    vec3 bloom_large = texture(sampler_bloom_large, tex_coord).rgb;
    vec3 bloom_medium = texture(sampler_bloom_medium, tex_coord).rgb;
    vec3 bloom_small = texture(sampler_bloom_small, tex_coord).rgb;

    vec3 colour = vec3(0.0f) + texture(sampler_colour, tex_coord).rgb
        + bloom_large * large_factor
        + bloom_medium * medium_factor
        + bloom_small * small_factor
        ;


    frag_out = vec4(lotteTonemap(colour), 1.0);
}
