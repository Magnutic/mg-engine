in v2f {
    vec2 uv0;      // Primary UV coordinate
    vec2 uv1;      // Secondary UV coordinate
    vec3 position; // Position (world space)
    vec3 normal;   // Surface normal (world space)
    mat3 TBN;      // Tangent space basis matrix
} _frag_in;

layout (location = 0) out vec4 _frag_out;

struct ClusterGridParams {
    vec2 z_param;
    float scale;
    float bias;
};

layout(std140) uniform FrameBlock {
    ClusterGridParams cluster_grid_params;
    vec4 camera_position_and_time;
    uvec2 viewport_size;

    float exposure;
} _frame_block;

#define CAMERA_POSITION (_frame_block.camera_position_and_time.xyz)
#define CAMERA_EXPOSURE (_frame_block.exposure)
#define TIME (_frame_block.camera_position_and_time.w)
#define VIEWPORT_SIZE (_frame_block.viewport_size)

#define WORLD_POSITION (_frag_in.position)
#define UV_COORD0 (_frag_in.uv0)
#define UV_COORD1 (_frag_in.uv1)
#define SCREEN_POSITION (gl_FragCoord)
#define TANGENT_TO_WORLD (_frag_in.TBN)

// Definition of light source.
struct Light {
   vec4 vector;
   vec3 colour;
   float range_sqr;
};

layout(std140) uniform LightBlock {
    Light lights[MAX_NUM_LIGHTS];
} _light_block;

struct LightInput {
    vec3 direction;
    vec3 colour;
    float NdotL;
};

struct SurfaceParams {
    vec3 albedo;
    vec3 specular;
    vec3 normal;
    vec3 emission;
    float occlusion;
    float gloss;
    float alpha;
};

float attenuate(float distance_sqr, float range_sqr_reciprocal);

// User-defined light function
vec3 light(const LightInput light, const SurfaceParams surface, const vec3 view_direction);

uniform usamplerBuffer _sampler_tile_data;
uniform usamplerBuffer _sampler_light_index;

// Light loop used for clustered forward rendering
vec3 _light_accumulate(const SurfaceParams surface, const vec3 view_direction)
{
    vec3 result = vec3(0.0);

    vec2 tile_dims = vec2(float(_frame_block.viewport_size.x) / LIGHT_GRID_WIDTH,
                          float(_frame_block.viewport_size.y) / LIGHT_GRID_HEIGHT);

    float z = 2.0 * gl_FragCoord.z - 1.0;

    int slice = int(log2(z * _frame_block.cluster_grid_params.z_param.x
                         + _frame_block.cluster_grid_params.z_param.y)
                    * _frame_block.cluster_grid_params.scale
                    + _frame_block.cluster_grid_params.bias);

    slice = clamp(slice, 0, int(LIGHT_GRID_DEPTH) - 1);

    ivec3 l = ivec3(ivec2(SCREEN_POSITION.xy / tile_dims), slice);

    int tile_index = l.z * int(LIGHT_GRID_WIDTH * LIGHT_GRID_HEIGHT)
                   + l.y * int(LIGHT_GRID_WIDTH)
                   + l.x;

    uint light_offset = texelFetch(_sampler_tile_data, tile_index).r;
    uint light_amount = texelFetch(_sampler_tile_data, tile_index).g;

    for (uint i = 0u; i < light_amount; ++i) {
        uint light_index = texelFetch(_sampler_light_index, int(light_offset + i)).r;
        Light ls         = _light_block.lights[light_index];

        LightInput li;
        li.direction             = ls.vector.xyz - WORLD_POSITION;
        float light_distance_sqr = dot(li.direction, li.direction);

        if (light_distance_sqr >= ls.range_sqr) { continue; }

        li.direction = normalize(li.direction);
        li.NdotL     = max(0.0, dot(surface.normal, li.direction));
        li.colour    = ls.colour * attenuate(light_distance_sqr, 1.0 / ls.range_sqr);

        result += light(li, surface, view_direction);
    }

    return result;
}

// Input to user-defined surface-function, likely to be expanded as needed.
struct SurfaceInput {
    vec3 view_direction;
};

void surface(const SurfaceInput s_in, out SurfaceParams s_out);

void final_colour(const SurfaceInput s_in, const SurfaceParams s, inout vec4 colour);

void main() {
    SurfaceInput s_in;
    s_in.view_direction = normalize(CAMERA_POSITION - WORLD_POSITION);
    SurfaceParams s;
    surface(s_in, s);

    vec4 colour = vec4(s.emission + _light_accumulate(s, s_in.view_direction), s.alpha);
    final_colour(s_in, s, colour);

    // Output colour with alpha = 1.0
    // We do exposure here instead of in post-processing to deal with 16-bit float range
    _frag_out = vec4(pow(2.0, _frame_block.exposure) * colour.rgb, colour.a);
}

