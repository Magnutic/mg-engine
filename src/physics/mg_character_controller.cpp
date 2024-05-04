//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
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

#include "mg/core/mg_runtime_error.h"
#include "mg/utils/mg_math_utils.h"
#include "mg/utils/mg_stl_helpers.h"

// Debug visualization bit flags.
#define MG_DEBUG_VIS_SWEEP_FLAG 1
#define MG_DEBUG_VIS_FORCES_FLAG 2
#define MG_DEBUG_VIS_PENETRATION_RECOVERY_FLAG 4
#define MG_ENABLE_CHARACTER_CONTROLLER_DEBUG_VISUALIZATION 0

#if MG_ENABLE_CHARACTER_CONTROLLER_DEBUG_VISUALIZATION
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

constexpr vec3 world_up(0.0f, 0.0f, 1.0f);
constexpr int num_penetration_recovery_iterations = 5;

vec3 normalize_if_nonzero(const vec3& v)
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

#if (MG_ENABLE_CHARACTER_CONTROLLER_DEBUG_VISUALIZATION) != 0
void debug_vis_vector(const vec3& start, const vec3& end, const vec4& colour)
{
    gfx::DebugRenderer::EllipsoidDrawParams params;
    params.dimensions = vec3(0.025f);
    params.centre = start;
    params.colour = colour;
    gfx::get_debug_render_queue().draw_ellipsoid(params);
    gfx::get_debug_render_queue().draw_line(start, end, colour, 2.0f);
}
#endif

#if (MG_ENABLE_CHARACTER_CONTROLLER_DEBUG_VISUALIZATION & MG_DEBUG_VIS_SWEEP_FLAG) != 0
void debug_vis_ray_hit(const RayHit& ray_hit, vec4 colour)
{
    debug_vis_vector(ray_hit.hit_point_worldspace,
                     ray_hit.hit_point_worldspace + ray_hit.hit_normal_worldspace,
                     colour);
}
#endif

} // namespace

void CharacterController::init(const glm::vec3& initial_position)
{
    m_max_slope_radians = m_settings.max_walkable_slope.radians();
    m_max_slope_cosine = std::cos(m_settings.max_walkable_slope.radians());

    if (m_settings.standing_step_height >= m_settings.standing_height) {
        throw RuntimeError{
            "CharacterController {}: could not set standing_step_height={}. "
            "The value must be smaller than standing_height={}",
            m_id.str_view(),
            m_settings.standing_step_height,
            m_settings.standing_height
        };
    }

    if (m_settings.crouching_step_height >= m_settings.crouching_height) {
        throw RuntimeError{
            "CharacterController {}: could not set crouching_step_height={}. "
            "The value must be smaller than crouching_height={}",
            m_id.str_view(),
            m_settings.crouching_step_height,
            m_settings.crouching_height
        };
    }

    // Standing body
    {
        m_standing_shape = m_world->create_cylinder_shape(
            vec3(2.0f * m_settings.radius,
                 2.0f * m_settings.radius,
                 m_settings.standing_height - m_settings.standing_step_height));
        m_standing_collision_body =
            m_world->create_ghost_object(m_id, *m_standing_shape, mat4(1.0f));

        m_standing_collision_body.set_filter_group(CollisionGroup::Character);
        m_standing_collision_body.set_filter_mask(~CollisionGroup::Character);
        m_standing_collision_body.has_contact_response(true);
    }

    // Crouching body
    {
        const auto ghost_id =
            Identifier::from_runtime_string(std::string(m_id.str_view()) + "_crouching"s);

        m_crouching_shape = m_world->create_cylinder_shape(
            vec3(2.0f * m_settings.radius,
                 2.0f * m_settings.radius,
                 m_settings.crouching_height - m_settings.crouching_step_height));
        m_crouching_collision_body =
            m_world->create_ghost_object(ghost_id, *m_crouching_shape, mat4(1.0f));

        // Disable collision for crouching body by default.
        m_crouching_collision_body.set_filter_group(CollisionGroup::Character);
        m_crouching_collision_body.set_filter_mask(~CollisionGroup::Character);
        m_crouching_collision_body.has_contact_response(false);
    }

    set_position(initial_position);
    reset();
}

Opt<RayHit> CharacterController::character_sweep_test(const vec3& start,
                                                      const vec3& end,
                                                      const vec3& up,
                                                      const float min_normal_angle_cosine,
                                                      const float max_normal_angle_cosine) const
{
    m_ray_hits.clear();
    m_world->convex_sweep(*shape(), start, end, ~CollisionGroup::Character, m_ray_hits);

    auto reject_condition = [&](const RayHit& hit) {
        return hit.body == collision_body() || !hit.body.has_contact_response() ||
               dot(up, hit.hit_normal_worldspace) < min_normal_angle_cosine ||
               dot(up, hit.hit_normal_worldspace) > max_normal_angle_cosine;
    };

#if (MG_ENABLE_CHARACTER_CONTROLLER_DEBUG_VISUALIZATION & MG_DEBUG_VIS_SWEEP_FLAG) != 0
    for (const RayHit& hit : m_ray_hits) {
        const vec4 reject_colour = { 1.0f, 0.0f, 0.0f, 0.5f };
        const vec4 approve_colour = { 0.0f, 1.0f, 0.0f, 0.5f };
        const vec4 colour = reject_condition(hit) ? reject_colour : approve_colour;

        if (hit.body != collision_body()) {
            debug_vis_ray_hit(hit, colour);
        }
    }
#endif

    auto compare_hit_fraction = [](const RayHit& l, const RayHit& r) {
        return l.hit_fraction < r.hit_fraction;
    };

    find_and_erase_if(m_ray_hits, reject_condition);
    auto closest_hit_it =
        std::min_element(m_ray_hits.begin(), m_ray_hits.end(), compare_hit_fraction);

    if (closest_hit_it != m_ray_hits.end()) {
        return *closest_hit_it;
    }

    return nullopt;
}

CharacterController::CharacterController(Identifier id,
                                         World& world,
                                         const CharacterControllerSettings& settings,
                                         const glm::vec3& initial_position)
    : m_settings(settings), m_id(id), m_world(&world)
{
    init(initial_position);
}

vec3 CharacterController::get_position(float interpolate) const
{
    const auto body_centre = mix(m_last_position, m_current_position, interpolate);
    return body_centre + world_up * feet_offset();
}

namespace {

// Helper for recover_from_penetration. Get the offset to move the character to recover from the
// given collision.
vec3 penetration_recovery_offset(const Collision& collision, const bool other_is_b)
{
    const float direction_sign = other_is_b ? -1.0f : 1.0f;

    if (collision.distance < 0.0f) {
        const auto result = collision.normal_on_b * direction_sign * collision.distance;

#if (MG_ENABLE_CHARACTER_CONTROLLER_DEBUG_VISUALIZATION & \
     MG_DEBUG_VIS_PENETRATION_RECOVERY_FLAG) != 0
        const auto start = other_is_b ? collision.contact_point_on_b : collision.contact_point_on_a;
        const auto colour = vec4(0.0f, 0.0f, 1.0f, 0.25f);
        debug_vis_vector(start, start + result * 5.0f, colour);
#endif

        return result;
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

void CharacterController::recover_from_penetration()
{
    for (int i = 0; i < num_penetration_recovery_iterations; ++i) {
        m_current_position = collision_body().get_position();

        // Here, we must refresh the set of collisions as the penetrating movement itself or
        // previous iterations of this recovery may have moved the character.
        m_collisions.clear();
        m_world->calculate_collisions_for(collision_body(), m_collisions);

        auto recovery_offset = vec3(0.0f);
        size_t num_penetrations = 0u;

        for (const Collision& collision : m_collisions) {
            const bool other_is_b = collision.object_a == collision_body();
            const auto recovery_offset_part = penetration_recovery_offset(collision, other_is_b);
            if (recovery_offset_part != vec3(0.0f)) {
                ++num_penetrations;
            }
            recovery_offset += recovery_offset_part;
            push_back_against_penetrating_object(collision,
                                                 other_is_b,
                                                 recovery_offset,
                                                 m_time_step);
        }

        if (num_penetrations == 0u) {
            break;
        }

        // It may well be that all penetration normals point in the same direction, so that they all
        // add up in the recovery offset. To avoid pushing the character too far out, we divide the
        // recovery offset by the number of penetrations.
        m_current_position += recovery_offset * (1.0f / float(num_penetrations));
        collision_body().set_position(m_current_position);
    }
}

static constexpr bool collision_enabled = true; // TODO: noclip

void CharacterController::step_up()
{
    auto target_position = m_current_position +
                           world_up * max(m_vertical_velocity * m_time_step, 0.0f);

    if (collision_enabled) {
        Opt<RayHit> sweep_result =
            character_sweep_test(m_current_position, target_position, -world_up, 0.0f);

        // Fix penetration if we hit a ceiling.
        if (sweep_result) {
            if (m_vertical_velocity > 0.0f) {
                m_vertical_velocity = 0.0f;
            }

            collision_body().set_position(target_position);
            recover_from_penetration();
            return;
        }
    }

    m_current_position = target_position;
}

void CharacterController::horizontal_step(const vec3& step)
{
    auto target_position = m_current_position + step;

    if (!collision_enabled || length2(step) <= FLT_EPSILON) {
        m_current_position = target_position;
        return;
    }


    // Apply a force to hit objects.
    {
        const auto sweep_target = m_current_position + m_desired_direction * 0.2f;
        auto sweep_result = character_sweep_test(m_current_position, sweep_target, world_up, -1.0f);
        auto dynamic_body = sweep_result ? sweep_result->body.as_dynamic_body() : nullopt;
        if (dynamic_body) {
            const vec3 force = m_settings.push_force * m_desired_direction;
            const vec3 relative_position = sweep_result->hit_point_worldspace -
                                           dynamic_body->get_position();
            dynamic_body->apply_force(force, relative_position);

#if MG_ENABLE_CHARACTER_CONTROLLER_DEBUG_VISUALIZATION
            const auto colour = vec4{ 1.0f, 1.0f, 0.0f, 1.0f };
            const auto start = vec3(dynamic_body->get_position() + relative_position);
            debug_vis_vector(start, start + normalize(force), colour);
#endif
        }
    }

    float fraction = 1.0f;
    constexpr size_t max_iterations = 10;

    for (size_t i = 0; i < max_iterations && fraction > 0.01f; ++i) {
        if (m_current_position == target_position) {
            break;
        }

        // Sweep test with "up-vector" and "min_normal_angle" such that only surfaces facing
        // against the character's movement are considered.
        const vec3 sweep_direction_negative = normalize(m_current_position - target_position);
        Opt<RayHit> sweep_result = character_sweep_test(m_current_position,
                                                        target_position,
                                                        sweep_direction_negative,
                                                        0.0f);
        if (!sweep_result) {
            break;
        }

        // First, walk as far as we can before we hit the obstacle.
        m_current_position += step * sweep_result->hit_fraction;

        // Then, try to slide along the obstacle.
        fraction -= sweep_result->hit_fraction;
        target_position = new_position_based_on_collision(m_current_position,
                                                          target_position,
                                                          sweep_result->hit_normal_worldspace);
        const float step_length_sqr = length2(target_position - m_current_position);
        const vec3 step_direction = normalize(target_position - m_current_position);

        // See Quake 2: "If velocity is against original velocity, stop ead to avoid tiny
        // oscilations in sloping corners."
        const bool step_direction_is_against_desired_direction = dot(step_direction,
                                                                     m_desired_direction) <= 0.0f;

        if (step_length_sqr <= FLT_EPSILON || step_direction_is_against_desired_direction) {
            break;
        }
    }

    m_current_position = target_position;
}

namespace {

// Apply an impulse to the object on which we landed.
void apply_impulse_to_object_below(DynamicBodyHandle& dynamic_body,
                                   const vec3& relative_position,
                                   const vec3& hit_normal_worldspace,
                                   const float character_vertical_velocity,
                                   const float character_mass)
{
    // This factor reduces the impulse when the character controller just grazes
    // the side of the body, which could otherwise cause the object to be sent flying.
    const float impulse_factor = max(0.0f, dot(world_up, hit_normal_worldspace));

    const float body_vertical_velocity = dot(dynamic_body.velocity(), world_up);
    const float relative_vertical_velocity = character_vertical_velocity - body_vertical_velocity;
    const float impulse = min(0.0f, relative_vertical_velocity) * character_mass * impulse_factor;
    if (abs(impulse) < FLT_EPSILON) {
        return;
    }
    const auto impulse_vector = world_up * impulse;

    dynamic_body.apply_impulse(impulse_vector, relative_position);

#if (MG_ENABLE_CHARACTER_CONTROLLER_DEBUG_VISUALIZATION & MG_DEBUG_VIS_FORCES_FLAG) != 0
    const auto colour = vec4{ 0.0f, 0.0f, 1.0f, 1.0f };
    const auto start = vec3(dynamic_body.get_position() + relative_position);
    debug_vis_vector(start, start + glm::normalize(impulse_vector), colour);
#endif
}

// Apply a force to the object on which we stand.
void apply_force_to_object_below(DynamicBodyHandle& dynamic_body,
                                 [[maybe_unused]] const vec3& relative_position,
                                 const float force)
{
    const vec3 force_vector = -world_up * force;

#if MG_CHARACTER_APPLY_TORQUE_TO_STOOD_UPON_OBJECT
    dynamic_body.apply_force(force_vector, relative_position);
#else
    // This ought to be a non-central force so that we apply the appropriate torque
    // as well, but that can cause the object to slowly slide away under our feet. I
    // am not sure how other games solve this problem. For now, we work around the
    // problem by ignoring torque.
    dynamic_body.apply_central_force(force_vector);
#endif

#if (MG_ENABLE_CHARACTER_CONTROLLER_DEBUG_VISUALIZATION & MG_DEBUG_VIS_FORCES_FLAG) != 0
    const auto colour = vec4{ 0.0f, 1.0f, 0.0f, 1.0f };
    const auto start = vec3(dynamic_body.get_position() + relative_position);
    debug_vis_vector(start, start + glm::normalize(force_vector), colour);
#endif
}

} // namespace

void CharacterController::step_down()
{
    m_velocity_added_by_moving_surface = vec3(0.0f);

    if (!collision_enabled) {
        return;
    }

    auto step_drop = world_up * (min(0.0f, m_vertical_velocity) * m_time_step - step_height());
    Opt<RayHit> drop_sweep_result = character_sweep_test(m_current_position,
                                                         m_current_position + step_drop,
                                                         world_up,
                                                         m_max_slope_cosine);

    // If we are not moving upwards, and the floor is close beneath us, clamp down to the floor, so
    // that we follow stairs and ledges down.
    const bool try_clamping_to_floor = !drop_sweep_result && m_vertical_velocity <= 0.0f &&
                                       m_is_on_ground && m_jump_velocity == 0.0f;
    if (try_clamping_to_floor) {
        // Double step_height: once to compensate for the character controller floating step_height
        // units in the air, and once again to check for floors up to step_height units beneath.
        const float step_down_height = 2.0f * step_height();

        drop_sweep_result = character_sweep_test(m_current_position,
                                                 m_current_position - world_up * step_down_height,
                                                 world_up,
                                                 m_max_slope_cosine);
        if (drop_sweep_result) {
            step_drop = -world_up * step_down_height;
        }
    }


    if (drop_sweep_result) {
        // There is a floor to stand on. Adjust vertical position accordingly.
        const float mix_factor = drop_sweep_result ? drop_sweep_result->hit_fraction : 1.0f;
        const auto target_position =
            mix(m_current_position, m_current_position + step_drop, mix_factor) +
            world_up * step_height();

        // Interpolate vertical position, for smooth movement up and down stairs.
        const auto factor = m_is_on_ground ? m_settings.vertical_interpolation_factor : 1.0f;
        m_current_position = glm::mix(m_current_position, target_position, factor);

        auto dynamic_body = drop_sweep_result ? drop_sweep_result->body.as_dynamic_body() : nullopt;
        if (dynamic_body) {
            const vec3 relative_position = drop_sweep_result->hit_point_worldspace -
                                           dynamic_body->get_position();

            if (m_is_on_ground) {
                const auto force = m_settings.mass * m_settings.gravity;
                apply_force_to_object_below(dynamic_body.value(), relative_position, force);
            }
            else {
                apply_impulse_to_object_below(dynamic_body.value(),
                                              relative_position,
                                              drop_sweep_result->hit_normal_worldspace,
                                              m_vertical_velocity,
                                              m_settings.mass);
            }

            // Move with the object.
            m_velocity_added_by_moving_surface = dynamic_body->velocity() +
                                                 glm::cross(dynamic_body->angular_velocity(),
                                                            relative_position);
        }

        m_vertical_velocity = 0.0f;
    }
    else {
        // We dropped the full height.
        m_current_position = m_current_position + step_drop + world_up * step_height();
    }

    // Determine whether we are standing on solid ground.
    // A bit of a hack, but also check if we are about to jump. Jumps are deferred one step, to let
    // jump impulses affect dynamic bodies first. Therefore we must consider the jump that is about
    // to happen here, or else it would be possible to jump twice.
    m_is_on_ground = drop_sweep_result.has_value() && m_jump_velocity <= 0.0f;

    // Slide down slopes
    if (!m_is_on_ground) {
        // If the drop sweeps did not find any results before, it may be because the surface below
        // slopes too much. Try another sweep, but this time _only_ look for surfaces that slope too
        // much to stand on.
        auto slope_sweep_result = character_sweep_test(m_current_position,
                                                       m_current_position + step_drop,
                                                       world_up,
                                                       0.0f,
                                                       m_max_slope_cosine);
        if (slope_sweep_result) {
            const auto slope_normal = slope_sweep_result->hit_normal_worldspace;
            const auto slide_direction =
                glm::normalize(perpendicular_component(slope_normal, world_up));
            m_current_position += slide_direction * m_settings.slide_down_acceleration *
                                  m_time_step;
        }
    }

    collision_body().set_position(m_current_position);
}

void CharacterController::move(const vec3& velocity)
{
    m_desired_velocity += velocity;
    m_desired_direction = normalize_if_nonzero(m_desired_direction + velocity);
}

vec3 CharacterController::velocity() const
{
    return (m_current_position - m_last_position) / m_time_step;
}

void CharacterController::reset()
{
    m_current_position = collision_body().get_position();
    m_last_position = m_current_position;
    m_vertical_velocity = 0.0f;
    m_jump_velocity = 0.0f;
    m_desired_velocity = vec3(0.0f, 0.0f, 0.0f);
    set_is_standing(true);
    m_is_on_ground = false;
    m_current_height_interpolated = current_height();
    m_last_height_interpolated = m_current_height_interpolated;
}

bool CharacterController::set_is_standing(bool v)
{
    const float vertical_offset = m_settings.standing_height - m_settings.crouching_height;
    const bool is_rising = v && !m_is_standing;

    // If rising, check if there is a ceiling blocking the character from standing.
    if (is_rising) {
        const bool is_blocked =
            character_sweep_test(m_current_position,
                                 m_current_position + world_up * vertical_offset,
                                 -world_up,
                                 0.0f)
                .has_value();

        if (is_blocked) {
            return false;
        }
    }

    GhostObjectHandle& old_collision_body = collision_body();

    // If we are changing standing state, adjust the controller state and position accordingly.
    const bool value_changed = std::exchange(m_is_standing, v) != v;
    if (value_changed) {
        const vec3 direction = world_up * (v ? 1.0f : -1.0f);
        const vec3 offset = direction * vertical_offset * 0.5f;

        // Disable collisions for old object.
        old_collision_body.has_contact_response(false);

        // And enable for new one.
        collision_body().has_contact_response(true);
        collision_body().set_position(m_current_position + offset);
        return true;
    }

    // No change in standing state.
    return false;
}

void CharacterController::jump(const float velocity)
{
    if (velocity > 0.0f) {
        m_jump_velocity += velocity;
        const auto sweep_target = m_current_position - vec3(0.0f, 0.0f, 2.0f * step_height());
        auto sweep_result = character_sweep_test(m_current_position, sweep_target, world_up, -1.0f);
        if (sweep_result) {
            if (auto dynamic_body = sweep_result->body.as_dynamic_body(); dynamic_body) {
                const vec3 relative_position = sweep_result->hit_point_worldspace -
                                               dynamic_body->get_position();
                apply_impulse_to_object_below(dynamic_body.value(),
                                              relative_position,
                                              sweep_result->hit_normal_worldspace,
                                              -m_jump_velocity,
                                              m_settings.mass);
            }
        }
    }
}

void CharacterController::set_position(const vec3& position)
{
    const vec3 body_centre = position - world_up * feet_offset();

    collision_body().set_position(body_centre);

    // Prevent interpolation between last position and this one.
    m_current_position = body_centre;
    m_last_position = body_centre;
}

void CharacterController::update(const float time_step)
{
    m_time_step = time_step;
    m_current_position = collision_body().get_position();
    m_last_position = m_current_position;

    // Update fall velocity.
    m_vertical_velocity -= m_settings.gravity * m_time_step;
    if (m_vertical_velocity < -m_settings.max_fall_speed) {
        m_vertical_velocity = -m_settings.max_fall_speed;
    }

    step_up();
    horizontal_step((m_desired_velocity + m_velocity_added_by_moving_surface) * m_time_step);
    step_down();

    recover_from_penetration();

    // Prevent gravity from accumulating vertical velocity if we are not actually falling.
    // This can otherwise happen when sliding down into a pit.
    if (m_vertical_velocity < 0.0f) {
        m_vertical_velocity = min(0.0f, (m_current_position.z - m_last_position.z) / time_step);
    }

    // Apply jump. We do this after one completed step to let the impulse applied to the stood-upon
    // object affect the character before we add the vertical velocity, so that we jump less high if
    // the object we stood upon gets pushed down by the impulse (otherwise, we would not be
    // conserving energy).
    if (m_jump_velocity > 0.0f) {
        m_vertical_velocity += std::exchange(m_jump_velocity, 0.0f);
    }

    m_desired_velocity = vec3(0.0f);
    m_desired_direction = vec3(0.0f);

    m_last_height_interpolated = m_current_height_interpolated;
    m_current_height_interpolated = glm::mix(m_current_height_interpolated,
                                             current_height(),
                                             m_settings.vertical_interpolation_factor);
}

} // namespace Mg::physics
