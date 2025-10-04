//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#pragma once

#include "mg/physics/mg_dynamic_body_handle.h"

#include "mg_bullet_utils.h"
#include "mg_physics_body.h"
#include "mg_shape_base.h"

#include <BulletDynamics/Dynamics/btRigidBody.h>

namespace Mg::physics {

using glm::mat4;
using glm::vec3;

// Create a Bullet rigid body using the supplied parameters.
static btRigidBody create_bt_rigid_body(ShapeBase& shape, const DynamicBodyParameters& parameters)
{
    const bool is_dynamic = parameters.type == DynamicBodyType::Dynamic;

    const float mass = is_dynamic ? parameters.mass : 0.0f;
    btVector3 local_inertia{ 0.0f, 0.0f, 0.0f };
    shape.bullet_shape().calculateLocalInertia(parameters.mass, local_inertia);

    btRigidBody::btRigidBodyConstructionInfo rb_info(mass,
                                                     nullptr,
                                                     &shape.bullet_shape(),
                                                     local_inertia);

    rb_info.m_linearDamping = parameters.linear_damping;
    rb_info.m_angularDamping = parameters.angular_damping;
    rb_info.m_friction = parameters.friction;
    rb_info.m_rollingFriction = parameters.rolling_friction;
    rb_info.m_spinningFriction = parameters.spinning_friction;

    return { rb_info };
}

class DynamicBody : public PhysicsBody {
public:
    // Construct a dynamic body using the supplied collision shape and parameters.
    explicit DynamicBody(const Identifier& id_,
                         ShapeBase& shape_,
                         const DynamicBodyParameters& dynamic_body_parameters,
                         const mat4 transform_)
        : PhysicsBody(PhysicsBodyType::dynamic_body, id_, shape_, transform_)
        , body(create_bt_rigid_body(shape_, dynamic_body_parameters))
    {
        body.setWorldTransform(convert_transform(transform));
        body.setUserPointer(this);

        switch (dynamic_body_parameters.type) {
        case DynamicBodyType::Dynamic:
            init_dynamic(dynamic_body_parameters);
            break;

        case DynamicBodyType::Kinematic:
            body.setCollisionFlags(body.getCollisionFlags() |
                                   btCollisionObject::CF_KINEMATIC_OBJECT);
            body.setActivationState(DISABLE_DEACTIVATION);
            break;
        }
    }

    // Initialization for dynamic rigid bodies.
    void init_dynamic(const DynamicBodyParameters& parameters)
    {
        body.setCollisionFlags(body.getCollisionFlags() | btCollisionObject::CF_DYNAMIC_OBJECT);

        // TODO Bullet seems too eager to deactivate. Figure out real solution.
        // body.setActivationState(DISABLE_DEACTIVATION);

        if (parameters.continuous_collision_detection) {
            btVector3 centre;
            float radius = 0.0f;
            body.getCollisionShape()->getBoundingSphere(centre, radius);

            body.setCcdMotionThreshold(parameters.continuous_collision_detection_motion_threshold);
            body.setCcdSweptSphereRadius(radius);
        }

        body.setAngularFactor(convert_vector(parameters.angular_factor));
    }

    btCollisionObject& get_bt_body() override { return body; }
    const btCollisionObject& get_bt_body() const override { return body; }

    // The bullet rigid body object.
    btRigidBody body;

    // Interpolated transformation matrix. This is updated from the latest Bullet simulation
    // results in Mg::physics::World::update. Note that we are not using bullet's built-in
    // motionState interpolation, because we have little control over bullet's time-step logic.
    mat4 interpolated_transform = mat4(1.0f);

    // Previous update's transform. Interpolated with `transform` to create interpolated transform.
    mat4 previous_transform = mat4(1.0f);
};

} // namespace Mg::physics
