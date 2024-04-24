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

#include "mg/core/mg_log.h"
#include "mg/utils/mg_math_utils.h"
#include "mg/utils/mg_stl_helpers.h"

// Debug visualization bit flags.
#define MG_DEBUG_VIS_SWEEP_FLAG 1
#define MG_DEBUG_VIS_FORCES_FLAG 2
/*
#define MG_ENABLE_CHARACTER_CONTROLLER_DEBUG_VISUALIZATION \
    (MG_DEBUG_VIS_SWEEP_FLAG | MG_DEBUG_VIS_FORCES_FLAG)
*/

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

} // namespace

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

void CharacterController::init()
{
    m_max_slope_radians = m_settings.max_walkable_slope.radians();
    m_max_slope_cosine = std::cos(m_settings.max_walkable_slope.radians());

    // Due to a capsule shape being at least 2 * radius tall, and the fact that the body hovers
    // step_height above ground, it might not be possible to get the desired height. This lambda
    // calculates the capsule height (length between the swept spheres making up the capsule) and
    // the resulting height, given the desired height.
    auto capsule_height_and_resulting_height_for =
        [&](const float desired_height, const float step_height) -> std::pair<float, float> {
        const float radius_term = m_settings.radius * 2.0f;
        const float hovering_term = step_height;
        const float diff = radius_term + hovering_term;

        const float capsule_height = max(0.0f, desired_height - diff);
        const float resulting_height = capsule_height + diff;

        return { capsule_height, resulting_height };
    };

    // Standing body
    {
        const auto& [capsule_height, resulting_height] =
            capsule_height_and_resulting_height_for(m_settings.standing_height,
                                                    m_settings.standing_step_height);

        if (resulting_height > m_settings.standing_height + FLT_EPSILON) {
            log.warning(
                "CharacterController {}: could not set standing_height = {} due to 2.0 * radius + "
                "standing_step_height > standing_height [radius = {}, standing_step_height = {}].",
                m_id.str_view(),
                m_settings.standing_height,
                m_settings.radius,
                m_settings.standing_step_height);

            m_settings.standing_height = resulting_height;
        }

        m_standing_shape = m_world->create_capsule_shape(m_settings.radius, capsule_height);
        m_standing_collision_body =
            m_world->create_ghost_object(m_id, *m_standing_shape, glm::mat4(1.0f));

        m_standing_collision_body.set_filter_group(CollisionGroup::Character);
        m_standing_collision_body.set_filter_mask(~CollisionGroup::Character);
        m_standing_collision_body.has_contact_response(true);
    }

    // Crouching body
    {
        const auto& [capsule_height, resulting_height] =
            capsule_height_and_resulting_height_for(m_settings.crouching_height,
                                                    m_settings.crouching_step_height);

        if (resulting_height > m_settings.crouching_height + FLT_EPSILON) {
            log.warning(
                "CharacterController {}: could not set crouching_height = {} due to 2.0 * radius + "
                "crouching_step_height > crouching_height [radius = {}, crouching_step_height = "
                "{}].",
                m_id.str_view(),
                m_settings.crouching_height,
                m_settings.radius,
                m_settings.crouching_step_height);

            m_settings.crouching_height = resulting_height;
        }

        const auto ghost_id =
            Identifier::from_runtime_string(std::string(m_id.str_view()) + "_crouching"s);

        m_crouching_shape = m_world->create_capsule_shape(m_settings.radius, capsule_height);
        m_crouching_collision_body =
            m_world->create_ghost_object(ghost_id, *m_crouching_shape, glm::mat4(1.0f));

        // Disable collision for crouching body by default.
        m_crouching_collision_body.set_filter_group(CollisionGroup::Character);
        m_crouching_collision_body.set_filter_mask(~CollisionGroup::Character);
        m_crouching_collision_body.has_contact_response(false);
    }
}

Opt<RayHit> CharacterController::character_sweep_test(const vec3& start,
                                                      const vec3& end,
                                                      const vec3& up,
                                                      const float max_surface_angle_cosine) const
{
    m_ray_hits.clear();
    m_world->convex_sweep(*shape(), start, end, ~CollisionGroup::Character, m_ray_hits);

    auto reject_condition = [&](const RayHit& hit) {
        return hit.body == collision_body() || !hit.body.has_contact_response() ||
               dot(up, hit.hit_normal_worldspace) < max_surface_angle_cosine;
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
                                         const CharacterControllerSettings& settings)
    : m_settings(settings), m_id(id), m_world(&world)
{
    init();

    m_current_position = collision_body().get_position();
    m_last_position = m_current_position;
}

glm::vec3 CharacterController::get_position(float interpolate) const
{
    const auto capsule_centre = mix(m_last_position, m_current_position, interpolate);
    return capsule_centre + world_up * feet_offset();
}

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

void CharacterController::recover_from_penetration()
{
    for (int i = 0; i < num_penetration_recovery_iterations; ++i) {
        m_current_position = collision_body().get_position();

        // Here, we must refresh the set of collisions as the penetrating movement itself or
        // previous iterations of this recovery may have moved the character.
        m_collisions.clear();
        m_world->calculate_collisions_for(collision_body(), m_collisions);

        bool still_penetrates = false;

        for (const Collision& collision : m_collisions) {
            const bool other_is_b = collision.object_a == collision_body();
            const vec3 recovery_offset = penetration_recovery_offset(collision, other_is_b);

            if (recovery_offset != vec3(0.0f)) {
                still_penetrates = true;
            }

            push_back_against_penetrating_object(collision,
                                                 other_is_b,
                                                 recovery_offset,
                                                 m_time_step);
            m_current_position += recovery_offset;
        }

        if (!still_penetrates) {
            break;
        }

        collision_body().set_position(m_current_position);
    }
}

static constexpr bool collision_enabled = true; // TODO: noclip

void CharacterController::step_up()
{
    m_target_position = m_current_position + world_up * max(m_vertical_step, 0.0f);

    if (collision_enabled) {
        Opt<RayHit> sweep_result = character_sweep_test(m_current_position,
                                                        m_target_position,
                                                        -world_up,
                                                        m_max_slope_cosine);

        if (sweep_result) {
            m_current_step_offset = 0.0f;
            m_current_position = m_target_position;

            collision_body().set_position(m_current_position);

            // Fix penetration if we hit a ceiling, for example.
            recover_from_penetration();

            if (m_vertical_step > 0.0f) {
                m_vertical_step = 0.0f;
                m_vertical_velocity = 0.0f;
                m_current_step_offset = 0.0f;
            }

            m_target_position = collision_body().get_position();
            m_current_position = m_target_position;
            return;
        }
    }

    m_current_step_offset = 0.0f;
    m_current_position = m_target_position;
}

void CharacterController::horizontal_step(const vec3& step)
{
    m_target_position = m_current_position + step;

    if (!collision_enabled || length2(step) <= FLT_EPSILON) {
        m_current_position = m_target_position;
        return;
    }


    // Apply an force to hit objects.
    {
        const auto sweep_target = m_current_position + m_desired_direction * 0.2f;
        auto sweep_result = character_sweep_test(m_current_position, sweep_target, world_up, -1.0f);
        auto dynamic_body = sweep_result ? sweep_result->body.as_dynamic_body() : nullopt;
        if (dynamic_body) {
            const vec3 force = push_force * m_desired_direction;
            const vec3 relative_position = get_position(1.0f) + world_up * current_height() * 0.8f -
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
        const vec3 sweep_direction_negative = normalize(m_current_position - m_target_position);

        Opt<RayHit> sweep_result;
        if (m_current_position != m_target_position) {
            // Sweep test with "up-vector" and "max-slope" such that only surfaces facing
            // against the character's movement are considered.
            sweep_result = character_sweep_test(m_current_position,
                                                m_target_position,
                                                sweep_direction_negative,
                                                0.0f);
        }

        if (sweep_result) {
            fraction -= sweep_result->hit_fraction;
            m_target_position =
                new_position_based_on_collision(m_current_position,
                                                m_target_position,
                                                sweep_result->hit_normal_worldspace);
            const float step_length_sqr = length2(m_target_position - m_current_position);
            const vec3 step_direction = normalize(m_target_position - m_current_position);

            // See Quake 2: "If velocity is against original velocity, stop ead to avoid tiny
            // oscilations in sloping corners."
            const bool step_direction_is_against_desired_direction =
                dot(step_direction, m_desired_direction) <= 0.0f;

            if (step_length_sqr <= FLT_EPSILON || step_direction_is_against_desired_direction) {
                break;
            }
        }
        else {
            break;
        }
    }

    m_current_position = m_target_position;
}

void CharacterController::step_down()
{
    m_velocity_added_by_moving_surface = glm::vec3(0.0f);

    if (!collision_enabled) {
        return;
    }

    const vec3 original_position = m_target_position;

    float drop_height = -min(0.0f, m_vertical_velocity) * m_time_step + step_height();
    vec3 step_drop = -world_up * drop_height;
    m_target_position += step_drop;

    Opt<RayHit> drop_sweep_result;
    Opt<RayHit> double_drop_sweep_result;

    bool has_clamped_to_floor = false;

    for (;;) {
        drop_sweep_result = character_sweep_test(m_current_position,
                                                 m_target_position,
                                                 world_up,
                                                 m_max_slope_cosine);

        if (!drop_sweep_result) {
            // Test a double fall height, to see if the character should interpolate its fall
            // (full) or not (partial).
            double_drop_sweep_result = character_sweep_test(m_current_position,
                                                            m_target_position + step_drop,
                                                            world_up,
                                                            m_max_slope_cosine);
        }

        const bool has_hit = double_drop_sweep_result.has_value();

        // Double step_height to compensate for the character controller floating step_height in
        // the air.
        const float step_down_height = (m_vertical_velocity < 0.0f) ? 2.0f * step_height() : 0.0f;

        // Redo the velocity calculation when falling a small amount, for fast stairs motion.
        // For larger falls, use the smoother/slower interpolated movement by not touching the
        // target position.
        const bool should_clamp_to_floor = drop_height > 0.0f && drop_height < step_down_height &&
                                           has_hit && !has_clamped_to_floor && m_was_on_ground &&
                                           m_jump_velocity == 0.0f;

        if (should_clamp_to_floor) {
            m_target_position = original_position;
            drop_height = step_down_height;
            step_drop = -world_up * (m_current_step_offset + drop_height);
            m_target_position += step_drop;
            has_clamped_to_floor = true;
            continue; // re-run previous tests
        }
        break;
    }

    if (drop_sweep_result || has_clamped_to_floor) {
        const float mix_factor = drop_sweep_result ? drop_sweep_result->hit_fraction : 1.0f;
        m_current_position = mix(m_current_position, m_target_position, mix_factor) +
                             vec3(0.0f, 0.0f, step_height());

        auto dynamic_body = drop_sweep_result ? drop_sweep_result->body.as_dynamic_body() : nullopt;
        if (dynamic_body) {
            // Apply an impulse to the object on which we landed.
            [[maybe_unused]] const vec3 relative_position = get_position(1.0f) -
                                                            dynamic_body->get_position();

            if (m_was_on_ground) {
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
                // This factor reduces the impulse when the character controller just grazes
                // the side of the body, which could otherwise cause the object to be sent flying.
                const float impulse_factor =
                    max(0.0f, dot(world_up, drop_sweep_result->hit_normal_worldspace));

                const vec3 impulse = { 0.0f,
                                       0.0f,
                                       min(0.0f, m_vertical_velocity * mass * impulse_factor) };
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
            m_velocity_added_by_moving_surface = dynamic_body->velocity() +
                                                 glm::cross(dynamic_body->angular_velocity(),
                                                            relative_position);
        }

        m_vertical_velocity = 0.0f;
        m_vertical_step = 0.0f;
    }
    else {
        // we dropped the full height
        m_current_position = m_target_position + vec3(0.0f, 0.0f, step_height());
    }

    collision_body().set_position(m_current_position);

    // Apply jump. We do this after one completed step to let the impulse applied to the stood-upon
    // object affect the character before we add the vertical velocity, so that we jump less high if
    // the object we stood upon gets pushed down by the impulse (otherwise, we would not be
    // conserving energy).
    // TODO move, does not really belong in this function
    if (m_jump_velocity > 0.0f) {
        m_vertical_velocity += std::exchange(m_jump_velocity, 0.0f);
    }
}

void CharacterController::move(const vec3& velocity)
{
    m_desired_velocity = velocity;
    m_desired_direction = normalize_if_nonzero(velocity);
}

vec3 CharacterController::velocity() const
{
    return (m_current_position - m_last_position) / m_time_step;
}

void CharacterController::reset()
{
    m_vertical_velocity = 0.0f;
    m_vertical_step = 0.0f;
    m_was_on_ground = false;
    m_jump_velocity = 0.0f;
    m_desired_velocity = vec3(0.0f, 0.0f, 0.0f);
    set_is_standing(true);
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

        // Disable collisions for old object.
        old_collision_body.has_contact_response(false);

        // And enable for new one.
        old_collision_body.has_contact_response(false);

        collision_body().set_position(m_current_position + direction * vertical_offset * 0.5f);
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
            // This factor reduces the impulse when the character controller just grazes
            // the side of the body, which could otherwise cause the object to be sent flying.
            const float impulse_factor = max(0.0f,
                                             dot(sweep_result->hit_point_worldspace, world_up));
            if (auto dynamic_body = sweep_result->body.as_dynamic_body(); dynamic_body) {
                const vec3 impulse = { 0.0f,
                                       0.0f,
                                       min(0.0f, -m_jump_velocity * mass * impulse_factor) };
                const vec3 relative_position = get_position(1.0f) - dynamic_body->get_position();
                dynamic_body->apply_impulse(impulse, relative_position);
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
    m_last_position = m_current_position;
    m_current_position = collision_body().get_position();
    m_target_position = m_current_position;

    m_was_on_ground = is_on_ground();

    // Update fall velocity.
    m_vertical_velocity -= gravity * m_time_step;
    if (m_vertical_velocity < 0.0f && std::abs(m_vertical_velocity) > max_fall_speed) {
        m_vertical_velocity = -max_fall_speed;
    }
    m_vertical_step = m_vertical_velocity * m_time_step;

    step_up();
    horizontal_step((m_desired_velocity + m_velocity_added_by_moving_surface) * m_time_step);
    step_down();

    recover_from_penetration();
}

} // namespace Mg::physics
