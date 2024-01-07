#include "mg/mg_actor.hpp"

namespace Mg {

void Actor::update(glm::vec3 acceleration, float jump_impulse)
{
    // Walk slower when crouching.
    acceleration *= character_controller.get_is_standing() ? 1.0f : 0.5f;

    const auto max_speed = max_horizontal_speed * (character_controller.get_is_standing() ||
                                                           !character_controller.is_on_ground()
                                                       ? 1.0f
                                                       : 0.5f);

    const float current_friction = character_controller.is_on_ground() ? friction : 0.0f;

    // Keep old velocity for inertia, but discard velocity added by platform movement, as that makes
    // the actor far too prone to uncontrollably sliding off surfaces.
    auto horizontal_velocity = character_controller.velocity() -
                               character_controller.velocity_added_by_moving_surface();
    horizontal_velocity.x += acceleration.x;
    horizontal_velocity.y += acceleration.y;
    horizontal_velocity.z = 0.0f;

    // Compensate for friction.
    if (glm::length2(acceleration) > 0.0f) {
        horizontal_velocity += glm::normalize(acceleration) * current_friction;
    }

    const float horizontal_speed = glm::length(horizontal_velocity);
    if (horizontal_speed > max_speed) {
        horizontal_velocity.x *= max_speed / horizontal_speed;
        horizontal_velocity.y *= max_speed / horizontal_speed;
    }

    if (glm::length2(horizontal_velocity) >= 0.0f) {
        if (glm::length2(horizontal_velocity) <= current_friction * current_friction) {
            horizontal_velocity = glm::vec3(0.0f);
        }
        else {
            horizontal_velocity -= glm::normalize(horizontal_velocity) * current_friction;
        }
    }

    character_controller.move(horizontal_velocity);
    character_controller.jump(jump_impulse *
                              (character_controller.get_is_standing() ? 1.0f : 0.5f));
}

} // namespace Mg
