const float bloom_strength = 0.0175;

const int bloom_mip_start = 0;
const int bloom_mip_end   = 4;

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

void main() {
    vec3 colour = texture(sampler_colour, tex_coord).rgb;

    for (int i = bloom_mip_start; i < bloom_mip_end; ++i) {
        colour += textureLod(sampler_bloom, tex_coord, i).rgb * bloom_strength;
    }

    frag_out = vec4(lotteTonemap(colour), 1.0);
}
