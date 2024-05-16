vec3 lotteTonemap(vec3 colour)
{
    // hard-coded parameters, calculated from contrast, shoulder, hdr_max, mid_in, mid_out.
    // see https://raw.githubusercontent.com/Opioid/tonemapper/master/tonemapper.py
    const float a = 1.1;
    const float d = 0.97;
    const float b = 1.05;
    const float c = 0.43;
    vec3 z = pow(colour, vec3(a));
    return z / vec3(pow(z, vec3(d)) * b + c);
}
