//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2021, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

// This character controller was initially based on the character controller code bundled with the
// Bullet physics library. It has since been heavily modified.
//
// Original license:
//
//    Bullet Continuous Collision Detection and Physics Library Copyright (c) 2003-2008 Erwin
//    Coumans http://bulletphysics.com
//
//    This software is provided 'as-is', without any express or implied warranty.  In no event will
//    the authors be held liable for any damages arising from the use of this software.  Permission
//    is granted to anyone to use this software for any purpose, including commercial applications,
//    and to alter it and redistribute it freely, subject to the following restrictions:
//
//    1. The origin of this software must not be misrepresented; you must not claim that you wrote
//    the original software. If you use this software in a product, an acknowledgment in the product
//    documentation would be appreciated but is not required.
//    2. Altered source versions must be plainly marked as such, and must not be misrepresented as
//    being the original software.
//    3. This notice may not be removed or altered from any source distribution.

#include "mg/physics/mg_character_controller.h"

#include "mg/utils/mg_math_utils.h"
#include "mg/utils/mg_stl_helpers.h"

// Uncomment to enable debug visualisation.
#define MG_DEBUG_VIS_SWEEP_FLAG 1
#define MG_DEBUG_VIS_FORCES_FLAG 2
#define MG_ENABLE_CHARACTER_CONTROLLER_DEBUG_VISUALIZATION (MG_DEBUG_VIS_FORCES_FLAG)

#ifdef MG_ENABLE_CHARACTER_CONTROLLER_DEBUG_VISUALIZATION
#    include "mg/gfx/mg_debug_renderer.h"
#endif

#include <string>

using glm::mat3;
using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;
using namespace std::literals;

namespace Mg::physics {

namespace {

constexpr float crouch_height_factor = 0.5f;

constexpr vec3 world_up(0.0f, 0.0f, 1.0f);
constexpr int num_penetration_recovery_iterations = 5;

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
    void recover_from_penetration();
    void step_up();
    void horizontal_step(const vec3& step);
    void step_down();

    Identifier id;

    World* world = nullptr;

    GhostObjectHandle standing_ghost;
    GhostObjectHandle crouching_ghost;
    Shape* standing_shape;
    Shape* crouching_shape;

    bool set_is_standing(bool v)
    {
        const float vertical_offset = standing_height - crouching_height;
        const bool is_rising = v && !is_standing;

        // If rising, check if there is a ceiling blocking the character from standing.
        if (is_rising) {
            const bool is_blocked =
                character_sweep_test(current_position,
                                     current_position + world_up * vertical_offset,
                                     -world_up,
                                     0.0f)
                    .has_value();

            if (is_blocked) {
                return false;
            }
        }

        GhostObjectHandle& old_ghost_object = ghost_object();

        // If we are changing standing state, adjust the controller state and position accordingly.
        const bool value_changed = std::exchange(is_standing, v) != v;
        if (value_changed) {
            const vec3 direction = world_up * (v ? 1.0f : -1.0f);

            // Disable collisions for old object.
            old_ghost_object.has_contact_response(false);

            // And enable for new one.
            old_ghost_object.has_contact_response(false);

            ghost_object().set_position(current_position + direction * vertical_offset * 0.5f);
            return true;
        }

        // No change in standing state.
        return false;
    }

    bool is_standing = true;

    Shape* shape() const { return is_standing ? standing_shape : crouching_shape; }

    GhostObjectHandle& ghost_object() { return is_standing ? standing_ghost : crouching_ghost; }
    const GhostObjectHandle& ghost_object() const
    {
        return is_standing ? standing_ghost : crouching_ghost;
    }

    float vertical_velocity;
    float vertical_step;

    float gravity;

    float standing_height;
    float crouching_height;
    float height() const { return is_standing ? standing_height : crouching_height; }

    float standing_step_height;
    float step_height() const
    {
        return is_standing ? standing_step_height : crouch_height_factor * standing_step_height;
    }

    float max_fall_speed;
    float max_slope_radians; // Slope angle that is set (used for returning the exact value)
    float max_slope_cosine;  // Cosine equivalent of max_slope_radians (calculated once when set)

    /// The desired velocity and its normalised direction, as set by the user.
    vec3 desired_velocity;
    vec3 desired_direction;

    vec3 velocity_added_by_moving_surface;

    vec3 current_position;
    vec3 last_position;
    float current_step_offset;
    vec3 target_position;

    float time_step = 1.0f; // Placeholder value to prevent division by zero before first update.

    // Vertical offset from current_position (capsule centre) to the character's feet.
    float feet_offset() const
    {
        // Capsule hovers step_height over the ground.
        const float capsule_height = height() - step_height();

        // Offset from capsule_centre to feet.
        return -(step_height() + capsule_height * 0.5f);
    }

    vec3 feet_position(const float interpolate) const
    {
        const vec3 capsule_centre = mix(last_position, current_position, interpolate);
        return capsule_centre + world_up * feet_offset();
    }

    // Array of collisions. Used in recover_from_penetration but declared here to allow the
    // heap buffer to be re-used between invocations.
    std::vector<Collision> collisions;

    // Declared here to re-use heap buffer.
    mutable std::vector<RayHit> ray_hits;

    float linear_damping;

    bool was_on_ground;
    bool was_jumping;
};

#if (MG_ENABLE_CHARACTER_CONTROLLER_DEBUG_VISUALIZATION & MG_DEBUG_VIS_SWEEP_FLAG) != 0
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
    // Standing body
    {
        const float capsule_height = height - 2.0f * radius - standing_step_height;
        standing_height = capsule_height + 2.0f * radius + standing_step_height;
        standing_shape = world->create_capsule_shape(radius, capsule_height);
        standing_ghost = world->create_ghost_object(id, *standing_shape, glm::mat4(1.0f));

        standing_ghost.set_filter_group(CollisionGroup::Character);
        standing_ghost.set_filter_mask(~CollisionGroup::Character);
        standing_ghost.has_contact_response(true);
    }

    // Crouching body
    {
        const float capsule_height = max(0.0f,
                                         (height * crouch_height_factor) - 2.0f * radius -
                                             (standing_step_height * crouch_height_factor));
        crouching_height = capsule_height + 2.0f * radius +
                           standing_step_height * crouch_height_factor;
        const auto ghost_id =
            Identifier::from_runtime_string(std::string(id.str_view()) + "_crouching"s);
        crouching_shape = world->create_capsule_shape(radius, capsule_height);
        crouching_ghost = world->create_ghost_object(ghost_id, *crouching_shape, glm::mat4(1.0f));

        // Disable collision for crouching body by default.
        crouching_ghost.set_filter_group(CollisionGroup::Character);
        crouching_ghost.set_filter_mask(~CollisionGroup::Character);
        crouching_ghost.has_contact_response(false);
    }
}

Opt<RayHit>
CharacterControllerData::character_sweep_test(const vec3& start,
                                              const vec3& end,
                                              const vec3& up,
                                              const float max_surface_angle_cosine) const
{
    ray_hits.clear();
    world->convex_sweep(*shape(), start, end, CollisionGroup::All, ray_hits);

    auto reject_condition = [&](const RayHit& hit) {
        return hit.body == ghost_object() || !hit.body.has_contact_response() ||
               dot(up, hit.hit_normal_worldspace) < max_surface_angle_cosine;
    };

#if (MG_ENABLE_CHARACTER_CONTROLLER_DEBUG_VISUALIZATION & MG_DEBUG_VIS_SWEEP_FLAG) != 0
    for (const RayHit& hit : ray_hits) {
        const vec4 reject_colour = { 1.0f, 0.0f, 0.0f, 0.5f };
        const vec4 approve_colour = { 0.0f, 1.0f, 0.0f, 0.5f };
        const vec4 colour = reject_condition(hit) ? reject_colour : approve_colour;

        if (hit.body != ghost_object()) {
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
    m.standing_step_height = step_height;
    m.was_on_ground = false;
    m.was_jumping = false;
    m.current_step_offset = 0.0f;
    m.linear_damping = 0.0f;

    m.init_body(radius, height);

    m.current_position = m.ghost_object().get_position();
    m.last_position = m.current_position;

    this->max_slope(45_degrees);
}

CharacterController::~CharacterController() = default;

namespace {

// Helper for recover_from_penetration. Get the offset to move the character to recover from the
// given collision.
vec3 penetration_recovery_offset(const Collision& collision, const bool other_is_b)
{
    const float direction_sign = other_is_b ? -1.0f : 1.0f;

    if (collision.distance < 0.0f) {
        return collision.normal_on_b * direction_sign * collision.distance;
    }

    return vec3(0.0f);
}

// Helper for recover_from_penetration.
// Push back against dynamic objects that are moving toward the character. This prevents penetration
// recovery against dynamic objects from pushing the character through static geometry. The most
// likely case where this could happen is if something falls on top of the character. Then, each
// frame, penetration recovery would result in a solution half-way between recovery from penetration
// with the static geometry and penetration with dynamic one, which pushes the character slightly
// through the static geometry. This then gives the dynamic object space to fall down a little bit,
// and the next frame it resolves halfway between again, pushing the character slightly more through
// the static geometry, and so on, until the character has fallen through the floor.
void push_back_against_penetrating_object(const Collision& collision,
                                          const bool other_is_b,
                                          const vec3& recovery_offset,
                                          const float time_step)
{
    const auto& other = other_is_b ? collision.object_b : collision.object_a;
    if (other.type() != PhysicsBodyType::dynamic_body) {
        return;
    }

    auto other_dynamic = other.as_dynamic_body().value();
    const bool other_moves_toward_character = dot(other_dynamic.velocity(), recovery_offset) > 0.0f;

    if (other_moves_toward_character) {
        const vec3 contact_point_on_other = other_is_b ? collision.contact_point_on_b
                                                       : collision.contact_point_on_a;
        const vec3 relative_position = contact_point_on_other - other_dynamic.get_position();
        other_dynamic.apply_impulse(-recovery_offset / time_step * other_dynamic.mass(),
                                    relative_position);
    }
}

} // namespace

void CharacterControllerData::recover_from_penetration()
{
    constexpr float recovery_per_iteration_factor = 1.0f / num_penetration_recovery_iterations;

    for (int i = 0; i < num_penetration_recovery_iterations; ++i) {
        current_position = ghost_object().get_position();

        // Here, we must refresh the set of collisions as the penetrating movement itself or
        // previous iterations of this recovery may have moved the character.
        collisions.clear();
        world->calculate_collisions_for(ghost_object(), collisions);

        bool still_penetrates = false;

        for (const Collision& collision : collisions) {
            const bool other_is_b = collision.object_a == ghost_object();
            const vec3 recovery_offset = penetration_recovery_offset(collision, other_is_b) *
                                         recovery_per_iteration_factor;
            push_back_against_penetrating_object(collision, other_is_b, recovery_offset, time_step);
            current_position += recovery_offset;
        }

        if (!still_penetrates) {
            break;
        }

        ghost_object().set_position(current_position);
    }
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

            ghost_object().set_position(current_position);

            // Fix penetration if we hit a ceiling, for example.
            recover_from_penetration();

            if (vertical_step > 0.0f) {
                vertical_step = 0.0f;
                vertical_velocity = 0.0f;
                current_step_offset = 0.0f;
            }

            target_position = ghost_object().get_position();
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


    // Apply an force to hit objects.
    {
        const auto double_step_target = current_position + desired_direction * 0.2f;
        auto sweep_result =
            character_sweep_test(current_position, double_step_target, world_up, -1.0f);
        auto dynamic_body = sweep_result ? sweep_result->body.as_dynamic_body() : nullopt;
        if (dynamic_body) {
            const vec3 force = 200.0f * desired_direction; // TODO configurable.
            const vec3 relative_position = feet_position(1.0f) + vec3(0.0f, 0.0f, height() * 0.8f) -
                                           dynamic_body->get_position();
            dynamic_body->apply_force(force, relative_position);

#if MG_ENABLE_CHARACTER_CONTROLLER_DEBUG_VISUALIZATION
            gfx::DebugRenderer::EllipsoidDrawParams params;
            params.dimensions = vec3(0.05f);
            params.centre = vec3(dynamic_body->get_position() + relative_position);
            params.colour = { 1.0f, 1.0f, 0.0f, 1.0f };
            gfx::get_debug_render_queue().draw_ellipsoid(params);
            gfx::get_debug_render_queue().draw_line(params.centre,
                                                    params.centre + normalize(force),
                                                    params.colour,
                                                    2.0f);
#endif
        }
    }

    float fraction = 1.0f;
    constexpr size_t max_iterations = 10;

    for (size_t i = 0; i < max_iterations && fraction > 0.01f; ++i) {
        const vec3 sweep_direction_negative = normalize(current_position - target_position);

        Opt<RayHit> sweep_result;
        if (current_position != target_position) {
            // Sweep test with "up-vector" and "max-slope" such that only surfaces facing
            // against the character's movement are considered.
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

void CharacterControllerData::step_down()
{
    if (!collision_enabled) {
        return;
    }

    const vec3 original_position = target_position;

    float drop_height = -min(0.0f, vertical_velocity) * time_step + step_height();
    vec3 step_drop = -world_up * drop_height;
    target_position += step_drop;

    Opt<RayHit> drop_sweep_result;
    Opt<RayHit> double_drop_sweep_result;

    bool has_clamped_to_floor = false;

    for (;;) {
        drop_sweep_result =
            character_sweep_test(current_position, target_position, world_up, max_slope_cosine);

        if (!drop_sweep_result) {
            // Test a double fall height, to see if the character should interpolate its fall
            // (full) or not (partial).
            double_drop_sweep_result = character_sweep_test(current_position,
                                                            target_position + step_drop,
                                                            world_up,
                                                            max_slope_cosine);
        }

#if (MG_ENABLE_CHARACTER_CONTROLLER_DEBUG_VISUALIZATION & MG_DEBUG_VIS_SWEEP_FLAG) != 0
        if (drop_sweep_result) {
            debug_vis_ray_hit(*drop_sweep_result, { 1.0f, 0.0f, 0.0f, 0.5f });
        }
        if (double_drop_sweep_result) {
            debug_vis_ray_hit(*double_drop_sweep_result, { 0.0f, 1.0f, 0.0f, 0.5f });
        }
#endif

        const bool has_hit = double_drop_sweep_result.has_value();

        // Double step_height to compensate for the character controller floating step_height in
        // the air.
        const float step_down_height = (vertical_velocity < 0.0f) ? 2.0f * step_height() : 0.0f;

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
        const float mix_factor = drop_sweep_result ? drop_sweep_result->hit_fraction : 1.0f;
        current_position = mix(current_position, target_position, mix_factor) +
                           vec3(0.0f, 0.0f, step_height());

        auto dynamic_body = drop_sweep_result ? drop_sweep_result->body.as_dynamic_body() : nullopt;
        if (dynamic_body) {
            // Apply an impulse to the object on which we landed.
            // TODO real mass
            const float mass = 70.0f;
            [[maybe_unused]] const vec3 relative_position = feet_position(1.0f) -
                                                            dynamic_body->get_position();

            if (was_on_ground) {
                const vec3 force = { 0.0f, 0.0f, min(0.0f, -mass * gravity) };

#if MG_CHARACTER_APPLY_TORQUE_TO_STOOD_UPON_OBJECT
                dynamic_body->apply_force(force, relative_position);
#else
                // This ought to be a non-central force so that we apply the appropriate torque
                // as well, but that can cause the object to slowly slide away under our feet. I
                // am not sure how other games solve this problem. For now, we work around the
                // problem by ignoring torque.
                dynamic_body->apply_central_force(force);
#endif

#if (MG_ENABLE_CHARACTER_CONTROLLER_DEBUG_VISUALIZATION & MG_DEBUG_VIS_FORCES_FLAG) != 0
                gfx::DebugRenderer::EllipsoidDrawParams params;
                params.dimensions = vec3(0.05f);
                params.centre = vec3(dynamic_body->get_position() + relative_position);
                params.colour = { 0.0f, 1.0f, 0.0f, 1.0f };
                gfx::get_debug_render_queue().draw_ellipsoid(params);
                gfx::get_debug_render_queue().draw_line(params.centre,
                                                        params.centre + glm::normalize(force),
                                                        params.colour,
                                                        2.0f);
#endif
            }
            else {
                const vec3 impulse = { 0.0f, 0.0f, min(0.0f, vertical_velocity * mass) };
                dynamic_body->apply_impulse(impulse, relative_position);

#if (MG_ENABLE_CHARACTER_CONTROLLER_DEBUG_VISUALIZATION & MG_DEBUG_VIS_FORCES_FLAG) != 0
                gfx::DebugRenderer::EllipsoidDrawParams params;
                params.dimensions = vec3(0.05f);
                params.centre = vec3(dynamic_body->get_position() + relative_position);
                params.colour = { 0.0f, 0.0f, 1.0f, 1.0f };
                gfx::get_debug_render_queue().draw_ellipsoid(params);
                gfx::get_debug_render_queue().draw_line(params.centre,
                                                        params.centre + glm::normalize(impulse),
                                                        params.colour,
                                                        2.0f);
#endif
            }

            // Move with the object.
            velocity_added_by_moving_surface = dynamic_body->velocity() +
                                               glm::cross(dynamic_body->angular_velocity(),
                                                          relative_position);
            current_position += time_step * velocity_added_by_moving_surface;
        }

        vertical_velocity = 0.0f;
        vertical_step = 0.0f;
        was_jumping = false;
    }
    else {
        // we dropped the full height
        current_position = target_position + vec3(0.0f, 0.0f, step_height());
    }

    ghost_object().set_position(current_position);
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

vec3 CharacterController::velocity() const
{
    auto& m = impl();
    return (m.current_position - m.last_position) / m.time_step;
}

void CharacterController::reset()
{
    auto& m = impl();
    m.vertical_velocity = 0.0f;
    m.vertical_step = 0.0f;
    m.was_on_ground = false;
    m.was_jumping = false;
    m.desired_velocity = vec3(0.0f, 0.0f, 0.0f);
    m.set_is_standing(true);
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

glm::vec3 CharacterController::velocity_added_by_moving_surface() const
{
    return impl().velocity_added_by_moving_surface;
}

bool CharacterController::set_is_standing(const bool v)
{
    return impl().set_is_standing(v);
}

bool CharacterController::is_standing() const
{
    return impl().is_standing;
}

float CharacterController::current_height() const
{
    return impl().height();
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
    return impl().feet_position(interpolate);
}

void CharacterController::position(const vec3& position)
{
    const vec3 body_centre = position - world_up * impl().feet_offset();

    impl().ghost_object().set_position(body_centre);

    // Prevent interpolation between last position and this one.
    impl().current_position = body_centre;
    impl().last_position = body_centre;
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
    auto& m = impl();
    m.time_step = time_step;
    m.last_position = m.current_position;
    m.current_position = m.ghost_object().get_position();
    m.target_position = m.current_position;
    m.velocity_added_by_moving_surface = vec3(0.0f);
    player_step();
}

void CharacterController::player_step()
{
    auto& m = impl();

    m.was_on_ground = is_on_ground();

    // apply damping
    if (length2(m.desired_velocity) > 0.0f) {
        m.desired_velocity *= std::pow(1.0f - m.linear_damping, m.time_step);
    }

    m.vertical_velocity *= std::pow(1.0f - m.linear_damping, m.time_step);

    // Update fall velocity.
    m.vertical_velocity -= m.gravity * m.time_step;
    if (m.vertical_velocity < 0.0f && std::abs(m.vertical_velocity) > m.max_fall_speed) {
        m.vertical_velocity = -m.max_fall_speed;
    }
    m.vertical_step = m.vertical_velocity * m.time_step;

    m.step_up();
    m.horizontal_step(m.desired_velocity * m.time_step);
    m.step_down();

    m.recover_from_penetration();
}

} // namespace Mg::physics
