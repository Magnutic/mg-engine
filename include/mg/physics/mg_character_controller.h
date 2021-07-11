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

/** @file mg_character_controller.h
 * Collision-handling physical body that can be controlled for example by a player or by an AI.
 */

#include "mg/physics/mg_physics.h"

namespace Mg::physics {

class CharacterControllerData;

class CharacterController : PImplMixin<CharacterControllerData> {
public:
    MG_MAKE_NON_COPYABLE(CharacterController);
    MG_MAKE_NON_MOVABLE(CharacterController);

    ~CharacterController();

    Identifier id() const;

    /** Get the current position of the character controller's centre of mass.
     * @param interpolate Factor for interpolating between last update's position and the most
     * recent position. When using a fixed update time step but variable framerate, this can be used
     * to prevent choppy motion. The default value of 1.0f will always return the most recent
     * position.
     */
    glm::vec3 position(float interpolate = 1.0f) const;

    /** Directly set position of character controller's centre of mass, ignoring collisions. For
     * regular movement, use `move` instead. To also clear motion state, call `reset`.
     */
    void position(const glm::vec3& position);

    /** Moves the character with the given velocity. This will be reset after each simulation step.
     */
    void move(const glm::vec3& velocity);

    /** Jump by setting the vertical velocity to the given velocity. Note that this will apply the
     * vertical velocity whether or not the character controller is on the ground. To prevent
     * jumping mid-air, check `is_on_ground()` first.
     */
    void jump(float velocity);

    void is_standing(bool v);
    bool is_standing() const;

    /** Current height, taking into account whether the character is standing or crouching. */
    float current_height() const;

    glm::vec3 velocity(float time_step) const;

    void linear_damping(float d);
    float linear_damping() const;

    /** Clear all motion state. */
    void reset();

    void max_fall_speed(float speed);
    float max_fall_speed() const;

    void gravity(float gravity);
    float gravity() const;

    /** The max slope determines the maximum angle that the controller can walk up. */
    void max_slope(Angle max_slope);
    float max_slope() const;

    /** Get whether the character controller is standing on the ground (as opposed to being in air).
     */
    bool is_on_ground() const;

    // Called by Mg::physics::World
    explicit CharacterController(Identifier id,
                                 World& world,
                                 float radius,
                                 float height,
                                 float step_height);

private:
    friend class World;
    void update(float time_step);

    void pre_step();
    void player_step(float time_step);
};

} // namespace Mg::physics
