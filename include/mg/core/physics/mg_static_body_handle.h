//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_static_body_handle.h
 * .
 */

#pragma once

#include "mg/core/physics/mg_physics_body_handle.h"
#include "mg/utils/mg_assert.h"

namespace Mg::physics {

/** Handle to a StaticBody. Lifetime of the referee is automatically managed via reference
 * counting.
 */
class StaticBodyHandle : public PhysicsBodyHandle {
public:
    StaticBodyHandle() = default;

    static StaticBodyHandle downcast(const PhysicsBodyHandle& handle)
    {
        MG_ASSERT(handle.type() == PhysicsBodyType::static_body);
        return StaticBodyHandle(handle.m_data);
    }

private:
    explicit StaticBodyHandle(PhysicsBody* data) : PhysicsBodyHandle(data) {}

    StaticBody& data();
    const StaticBody& data() const;
};

} // namespace Mg::physics
