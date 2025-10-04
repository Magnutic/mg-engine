//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/physics/mg_physics_body_handle.h"

#include "mg/physics/mg_dynamic_body_handle.h"
#include "mg/physics/mg_ghost_object_handle.h"
#include "mg/physics/mg_static_body_handle.h"

namespace Mg::physics {

Opt<DynamicBodyHandle> PhysicsBodyHandle::as_dynamic_body() const
{
    if (type() != PhysicsBodyType::dynamic_body) {
        return nullopt;
    }
    return DynamicBodyHandle::downcast(*this);
}

Opt<StaticBodyHandle> PhysicsBodyHandle::as_static_body() const
{
    if (type() != PhysicsBodyType::static_body) {
        return nullopt;
    }
    return StaticBodyHandle::downcast(*this);
}

Opt<GhostObjectHandle> PhysicsBodyHandle::as_ghost_body() const
{
    if (type() != PhysicsBodyType::ghost_object) {
        return nullopt;
    }
    return GhostObjectHandle::downcast(*this);
}

} // namespace Mg::physics
