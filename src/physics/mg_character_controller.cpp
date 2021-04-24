#include "mg/physics/mg_character_controller.h"

#include "mg/gfx/mg_debug_renderer.h" // TODO temp?
#include "mg/utils/mg_math_utils.h"
#include "mg/utils/mg_stl_helpers.h"

#include "mg/core/mg_log.h" // TODO temp

using glm::mat3;
using glm::mat4;
using glm::vec3;
using glm::vec4;
using namespace std::literals;

//#define MG_ENABLE_CHARACTER_CONTROLLER_DEBUG_VISUALIZATION 1

namespace Mg::physics {

namespace {

constexpr vec3 world_up(0.0f, 0.0f, 1.0f);
constexpr int num_penetration_recovery_iterations = 5;
constexpr float penetration_recovery_per_iteration = 1.0f / num_penetration_recovery_iterations;

vec3 normalise_if_nonzero(const vec3& v)
{
    return length(v) > FLT_EPSILON ? normalize(v) : vec3{ 0.0f, 0.0f, 0.0f };
}

vec3 parallel_component(const vec3& direction, const vec3& normal)
{
    return normal * dot(direction, normal);
}

vec3 perpendicular_component(const vec3& direction, const vec3& normal)
{
    return direction - parallel_component(direction, normal);
}

vec3 new_position_based_on_collision(const vec3& current_position,
                                     const vec3& target_position,
                                     const vec3& hit_normal)
{
    const vec3 movement = target_position - current_position;
    const float movement_length = length(movement);

    if (movement_length <= FLT_EPSILON) {
        return target_position;
    }

    const vec3 movement_direction = normalize(movement);
    const vec3 reflect_direction = normalize(reflect(movement_direction, hit_normal));
    const vec3 perpendicular_direction = perpendicular_component(reflect_direction, hit_normal);
    const vec3 perpendicular_movement = perpendicular_direction * movement_length;
    return current_position + perpendicular_movement;
}

} // namespace

class CharacterControllerData {
public:
    void init_body(float radius, float height);

    Opt<RayHit> character_sweep_test(const vec3& start,
                                     const vec3& end,
                                     const vec3& up,
                                     float max_surface_angle_cosine) const;
    bool recover_from_penetration();
    void step_up();
    void horizontal_step(const vec3& step);
    void step_down(float time_step);

    Identifier id;

    World* world = nullptr;

    GhostObjectHandle ghost_object;
    Shape* shape;

    float vertical_velocity;
    float vertical_step;

    float gravity;
    float step_height;
    float max_fall_speed;
    float max_slope_radians; // Slope angle that is set (used for returning the exact value)
    float max_slope_cosine;  // Cosine equivalent of max_slope_radians (calculated once when set)

    /// The desired velocity and its normalised direction, as set by the user.
    vec3 desired_velocity;
    vec3 desired_direction;

    vec3 current_position;
    vec3 last_position;
    float current_step_offset;
    vec3 target_position;

    // Array of collisions. Used in recover_from_penetration but declared here to allow the
    // heap buffer to be re-used between invocations.
    mutable std::vector<Collision> collisions;

    // Declared here to re-use heap buffer.
    mutable std::vector<RayHit> ray_hits;

    float linear_damping;

    bool was_on_ground;
    bool was_jumping;
};

#if MG_ENABLE_CHARACTER_CONTROLLER_DEBUG_VISUALIZATION
static void debug_vis_ray_hit(const RayHit& ray_hit, vec4 colour)
{
    gfx::DebugRenderer::EllipsoidDrawParams params;
    params.dimensions = vec3(0.05f);
    params.centre = vec3(ray_hit.hit_point_worldspace);
    params.colour = colour;
    gfx::get_debug_render_queue().draw_ellipsoid(params);
    gfx::get_debug_render_queue().draw_line(ray_hit.hit_point_worldspace,
                                            ray_hit.hit_point_worldspace +
                                                ray_hit.hit_normal_worldspace * 1.0f,
                                            colour,
                                            2.0f);
}
#endif

void CharacterControllerData::init_body(const float radius, const float height)
{
    const float capsule_height = height - 2.0f * radius - step_height;
    const auto ghost_id = Identifier::from_runtime_string(std::string(id.str_view()) + "_ghost"s);
    shape = world->create_capsule_shape(radius, capsule_height);
    ghost_object = world->create_ghost_object(ghost_id, *shape, glm::mat4(1.0f));
}

Opt<RayHit>
CharacterControllerData::character_sweep_test(const vec3& start,
                                              const vec3& end,
                                              const vec3& up,
                                              const float max_surface_angle_cosine) const
{
    ray_hits.clear();
    world->convex_sweep(*shape, start, end, ray_hits);

    auto reject_condition = [&](const RayHit& hit) {
        return hit.body == ghost_object ||
               dot(up, hit.hit_normal_worldspace) < max_surface_angle_cosine;
    };

#if MG_ENABLE_CHARACTER_CONTROLLER_DEBUG_VISUALIZATION
    for (const RayHit& hit : ray_hits) {
        const vec4 reject_colour = { 1.0f, 0.0f, 0.0f, 0.5f };
        const vec4 approve_colour = { 0.0f, 1.0f, 0.0f, 0.5f };
        const vec4 colour = reject_condition(hit) ? reject_colour : approve_colour;

        if (hit.body != ghost_object) {
            debug_vis_ray_hit(hit, colour);
        }
    }
#endif

    auto compare_hit_fraction = [](const RayHit& l, const RayHit& r) {
        return l.hit_fraction < r.hit_fraction;
    };

    find_and_erase_if(ray_hits, reject_condition);
    auto closest_hit_it = std::min_element(ray_hits.begin(), ray_hits.end(), compare_hit_fraction);

    if (closest_hit_it != ray_hits.end()) {
        return *closest_hit_it;
    }

    return nullopt;
}

CharacterController::CharacterController(Identifier id,
                                         World& world,
                                         const float radius,
                                         const float height,
                                         const float step_height)
{
    auto& m = impl();
    m.id = id;
    m.world = &world;
    m.desired_velocity = vec3(0.0f, 0.0f, 0.0f);
    m.vertical_velocity = 0.0f;
    m.vertical_step = 0.0f;
    m.gravity = 9.8f;
    m.max_fall_speed = 55.0f; // Terminal velocity of a sky diver in m/s.
    m.step_height = step_height;
    m.was_on_ground = false;
    m.was_jumping = false;
    m.current_step_offset = 0.0f;
    m.linear_damping = 0.0f;

    m.init_body(radius, height);

    m.current_position = m.ghost_object.get_position();
    m.last_position = m.current_position;

    this->max_slope(65_degrees);
}

CharacterController::~CharacterController() = default;

bool CharacterControllerData::recover_from_penetration()
{
    bool penetration = false;
    current_position = ghost_object.get_position();

    // Here, we must refresh the set of collisions as the penetrating movement itself or previous
    // iterations of this recovery may have moved the character.
    collisions.clear();
    world->calculate_collisions_for(ghost_object, collisions);

    for (const Collision& collision : collisions) {
        const float direction_sign = (collision.object_a == ghost_object) ? -1.0f : 1.0f;

        if (collision.distance < 0.0f) {
            const vec3 offset = collision.normal_on_b * direction_sign * collision.distance *
                                penetration_recovery_per_iteration;
            current_position += offset;
            penetration = true;
        }
    }

    ghost_object.set_position(current_position);
    return penetration;
}

static constexpr bool collision_enabled = true; // TODO: noclip

void CharacterControllerData::step_up()
{
    target_position = current_position + world_up * max(vertical_step, 0.0f);

    if (collision_enabled) {
        Opt<RayHit> sweep_result =
            character_sweep_test(current_position, target_position, -world_up, max_slope_cosine);

        if (sweep_result) {
            current_step_offset = 0.0f;
            current_position = target_position;

            ghost_object.set_position(current_position);

            // Fix penetration if we hit a ceiling, for example.
            {
                int i = 0;
                while (recover_from_penetration() && ++i < num_penetration_recovery_iterations) {
                }
            }

            if (vertical_step > 0.0f) {
                vertical_step = 0.0f;
                vertical_velocity = 0.0f;
                current_step_offset = 0.0f;
            }

            target_position = ghost_object.get_position();
            current_position = target_position;
            return;
        }
    }

    current_step_offset = 0.0f;
    current_position = target_position;
}

void CharacterControllerData::horizontal_step(const vec3& step)
{
    target_position = current_position + step;

    if (!collision_enabled || length2(step) <= FLT_EPSILON) {
        current_position = target_position;
        return;
    }

    float fraction = 1.0f;
    constexpr size_t max_iterations = 10;

    for (size_t i = 0; i < max_iterations && fraction > 0.01f; ++i) {
        const vec3 sweep_direction_negative = normalize(current_position - target_position);

        Opt<RayHit> sweep_result;
        if (current_position != target_position) {
            // Sweep test with "up-vector" and "max-slope" such that only surfaces facing against
            // the character's movement are considered.
            sweep_result = character_sweep_test(current_position,
                                                target_position,
                                                sweep_direction_negative,
                                                0.0f);
        }

        if (sweep_result) {
            fraction -= sweep_result->hit_fraction;
            target_position = new_position_based_on_collision(current_position,
                                                              target_position,
                                                              sweep_result->hit_normal_worldspace);
            const float step_length_sqr = length2(target_position - current_position);
            const vec3 step_direction = normalize(target_position - current_position);

            // See Quake 2: "If velocity is against original velocity, stop ead to avoid tiny
            // oscilations in sloping corners."
            const bool step_direction_is_against_desired_direction = dot(step_direction,
                                                                         desired_direction) <= 0.0f;

            if (step_length_sqr <= FLT_EPSILON || step_direction_is_against_desired_direction) {
                break;
            }
        }
        else {
            break;
        }
    }

    current_position = target_position;
}

void CharacterControllerData::step_down(float time_step)
{
    if (!collision_enabled) {
        return;
    }

    const vec3 original_position = target_position;

    float drop_height = -min(0.0f, vertical_velocity) * time_step + step_height;
    vec3 step_drop = -world_up * drop_height;
    target_position += step_drop;

    Opt<RayHit> drop_sweep_result;
    Opt<RayHit> double_drop_sweep_result;

    bool has_clamped_to_floor = false;

    for (;;) {
        drop_sweep_result =
            character_sweep_test(current_position, target_position, world_up, max_slope_cosine);

        if (!drop_sweep_result) {
            // Test a double fall height, to see if the character should interpolate its fall (full)
            // or not (partial).
            double_drop_sweep_result = character_sweep_test(current_position,
                                                            target_position + step_drop,
                                                            world_up,
                                                            max_slope_cosine);
        }

#if MG_ENABLE_CHARACTER_CONTROLLER_DEBUG_VISUALIZATION
        if (drop_sweep_result) {
            debug_vis_ray_hit(*drop_sweep_result, { 1.0f, 0.0f, 0.0f, 0.5f });
        }
        if (double_drop_sweep_result) {
            debug_vis_ray_hit(*double_drop_sweep_result, { 0.0f, 1.0f, 0.0f, 0.5f });
        }
#endif

        const bool has_hit = double_drop_sweep_result.has_value();

        // Double step_height to compensate for the character controller floating step_height in the
        // air.
        const float step_down_height = (vertical_velocity < 0.0f) ? 2.0f * step_height : 0.0f;

        // Redo the velocity calculation when falling a small amount, for fast stairs motion.
        // For larger falls, use the smoother/slower interpolated movement by not touching the
        // target position.
        const bool should_clamp_to_floor = drop_height > 0.0f && drop_height < step_down_height &&
                                           has_hit && !has_clamped_to_floor && was_on_ground &&
                                           !was_jumping;

        if (should_clamp_to_floor) {
            target_position = original_position;
            drop_height = step_down_height;
            step_drop = -world_up * (current_step_offset + drop_height);
            target_position += step_drop;
            has_clamped_to_floor = true;
            continue; // re-run previous tests
        }
        break;
    }

    if (drop_sweep_result || has_clamped_to_floor) {
        const float mix_factor = drop_sweep_result ? drop_sweep_result->hit_fraction : 0.0f;
        current_position = mix(current_position, target_position, mix_factor) +
                           vec3(0.0f, 0.0f, step_height);
        vertical_velocity = 0.0f;
        vertical_step = 0.0f;
        was_jumping = false;
    }
    else {
        // we dropped the full height
        current_position = target_position + vec3(0.0f, 0.0f, step_height);
    }

    ghost_object.set_position(current_position);
}

Identifier CharacterController::id() const
{
    return impl().id;
}

void CharacterController::move(const vec3& velocity)
{
    auto& m = impl();
    m.desired_velocity = velocity;
    m.desired_direction = normalise_if_nonzero(velocity);
}

vec3 CharacterController::velocity(float time_step) const
{
    auto& m = impl();
    return (m.current_position - m.last_position) / time_step;
}

void CharacterController::reset()
{
    auto& m = impl();
    m.vertical_velocity = 0.0f;
    m.vertical_step = 0.0f;
    m.was_on_ground = false;
    m.was_jumping = false;
    m.desired_velocity = vec3(0.0f, 0.0f, 0.0f);
}

void CharacterController::max_fall_speed(float speed)
{
    impl().max_fall_speed = speed;
}

void CharacterController::jump(const float velocity)
{
    if (velocity > 0.0f) {
        auto& m = impl();
        m.vertical_velocity = velocity;
        m.was_jumping = true;
    }
}

void CharacterController::gravity(const float gravity)
{
    impl().gravity = gravity;
}

float CharacterController::gravity() const
{
    return impl().gravity;
}

void CharacterController::max_slope(Angle max_slope)
{
    auto& m = impl();
    m.max_slope_radians = max_slope.radians();
    m.max_slope_cosine = std::cos(max_slope.radians());
}

float CharacterController::max_slope() const
{
    return impl().max_slope_radians;
}

bool CharacterController::is_on_ground() const
{
    return std::fabs(impl().vertical_velocity) < FLT_EPSILON &&
           std::fabs(impl().vertical_step) < FLT_EPSILON;
}

vec3 CharacterController::position(const float interpolate) const
{
    return mix(impl().last_position, impl().current_position, interpolate) +
           vec3(0.0f, 0.0f, -0.5f * impl().step_height);
}

void CharacterController::position(const vec3& position)
{
    impl().ghost_object.set_position(position);

    // Prevent interpolation between last position and this one.
    impl().current_position = position;
    impl().last_position = position;
}

void CharacterController::linear_damping(float d)
{
    impl().linear_damping = std::clamp(d, 0.0f, 1.0f);
}

float CharacterController::linear_damping() const
{
    return impl().linear_damping;
}

float CharacterController::max_fall_speed() const
{
    return impl().max_fall_speed;
}

void CharacterController::update(float time_step)
{
    pre_step();
    player_step(time_step);
}

void CharacterController::pre_step()
{
    auto& m = impl();
    m.last_position = m.current_position;
    m.current_position = m.ghost_object.get_position();
    m.target_position = m.current_position;
}

void CharacterController::player_step(float time_step)
{
    auto& m = impl();

    m.was_on_ground = is_on_ground();

    // apply damping
    if (length2(m.desired_velocity) > 0.0f) {
        m.desired_velocity *= std::pow(1.0f - m.linear_damping, time_step);
    }

    m.vertical_velocity *= std::pow(1.0f - m.linear_damping, time_step);

    // Update fall velocity.
    m.vertical_velocity -= m.gravity * time_step;
    if (m.vertical_velocity < 0.0f && std::abs(m.vertical_velocity) > m.max_fall_speed) {
        m.vertical_velocity = -m.max_fall_speed;
    }
    m.vertical_step = m.vertical_velocity * time_step;

    m.step_up();
    m.horizontal_step(m.desired_velocity * time_step);
    m.step_down(time_step);

    {
        int i = 0;
        while (m.recover_from_penetration() && (++i < num_penetration_recovery_iterations)) {
        }
    }
}

} // namespace Mg::physics
