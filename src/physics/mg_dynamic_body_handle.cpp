//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/physics/mg_dynamic_body_handle.h"

#include "mg_dynamic_body.h"

namespace Mg::physics {

float DynamicBodyHandle::mass() const
{
    return data().body.getMass();
}

mat4 DynamicBodyHandle::interpolated_transform() const
{
    // transform is updated in Mg::physics::World::update()
    // interpolated_transform is updated in Mg::physics::World::interpolate()
    return data().interpolated_transform;
}

void DynamicBodyHandle::set_transform(const mat4& transform)
{
    data().body.setWorldTransform(convert_transform(transform));
    data().body.activate(true);

    // Also set result transform, to prevent interpolation with old value.
    data().transform = transform;

    // And set final interpolated transform, so that calls to transform() will immediately see
    // the new value.
    data().interpolated_transform = transform;
}

void DynamicBodyHandle::set_gravity(const vec3& gravity)
{
    data().body.setGravity(convert_vector(gravity));
}

vec3 DynamicBodyHandle::get_gravity() const
{
    return convert_vector(data().body.getGravity());
}

void DynamicBodyHandle::apply_force(const vec3& force, const vec3& relative_position)
{
    data().body.applyForce(convert_vector(force), convert_vector(relative_position));
    data().body.activate(true);
}

void DynamicBodyHandle::apply_central_force(const vec3& force)
{
    data().body.applyCentralForce(convert_vector(force));
    data().body.activate(true);
}

void DynamicBodyHandle::apply_impulse(const vec3& impulse, const vec3& relative_position)
{
    data().body.applyImpulse(convert_vector(impulse), convert_vector(relative_position));
    data().body.activate(true);
}

void DynamicBodyHandle::apply_central_impulse(const vec3& impulse)
{
    data().body.applyCentralImpulse(convert_vector(impulse));
    data().body.activate(true);
}

void DynamicBodyHandle::apply_torque(const vec3& torque)
{
    data().body.applyTorque(convert_vector(torque));
    data().body.activate(true);
}

void DynamicBodyHandle::apply_torque_impulse(const vec3& torque)
{
    data().body.applyTorqueImpulse(convert_vector(torque));
    data().body.activate(true);
}

void DynamicBodyHandle::clear_forces()
{
    data().body.clearForces();
}

vec3 DynamicBodyHandle::velocity() const
{
    return convert_vector(data().body.getLinearVelocity());
}

vec3 DynamicBodyHandle::angular_velocity() const
{
    return convert_vector(data().body.getAngularVelocity());
}

vec3 DynamicBodyHandle::total_force() const
{
    return convert_vector(data().body.getTotalForce());
}

vec3 DynamicBodyHandle::total_torque() const
{
    return convert_vector(data().body.getTotalTorque());
}

void DynamicBodyHandle::velocity(const vec3& velocity)
{
    data().body.setLinearVelocity(convert_vector(velocity));
    data().body.activate(true);
}

void DynamicBodyHandle::angular_velocity(const vec3& angular_velocity)
{
    data().body.setAngularVelocity(convert_vector(angular_velocity));
    data().body.activate(true);
}

void DynamicBodyHandle::move(const vec3& offset)
{
    data().body.translate(convert_vector(offset));
    data().body.activate(true);
}

DynamicBody& DynamicBodyHandle::data()
{
    MG_ASSERT_DEBUG(type() == PhysicsBodyType::dynamic_body);
    return *static_cast<DynamicBody*>(m_data); // NOLINT
}

const DynamicBody& DynamicBodyHandle::data() const
{
    MG_ASSERT_DEBUG(type() == PhysicsBodyType::dynamic_body);
    return *static_cast<const DynamicBody*>(m_data); // NOLINT
}

} // namespace Mg
