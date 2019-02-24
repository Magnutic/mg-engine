void main() {
   frag_colour = fs_in_colour * texture2D(sampler_diffuse, tex_coord);

#ifdef A_TEST
   if (frag_colour.a < 0.5) { discard; }
#else
   if (frag_colour.a == 0.0) { discard; }
#endif
}
