void main() {
    vs_out_colour = v_colour;
    float radius = v_radius;
    gl_Position = VP * vec4(v_position, 1.0);

#if FADE_WHEN_CLOSE || SHRINK_WHEN_CLOSE
    vec3 diff = v_position - cam_pos;
    float depth_sqr = dot(diff, diff);
    float r_sqr = radius * radius * 4.0;
    float fade = smoothstep(0.0, r_sqr, depth_sqr);
#endif

#if SHRINK_WHEN_CLOSE
    radius *= min(1.0, fade * 2.0);
#endif

#if FADE_WHEN_CLOSE
    vs_out_colour.a *= fade;
#endif

#if FIXED_SIZE
    gl_Position /= gl_Position.w;
    vs_out_size = vec2(radius / aspect_ratio, radius);
#else
    vs_out_size = (P * vec4(radius, radius, 0.0, 1.0)).xy;
#endif
}
