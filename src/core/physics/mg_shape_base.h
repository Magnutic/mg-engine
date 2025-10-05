//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_shape_base.h
 * .
 */

#pragma once

#include "mg/core/physics/mg_shape.h"

#include "mg/utils/mg_assert.h"

#include <BulletCollision/CollisionShapes/btCollisionShape.h>

#include <atomic>

namespace Mg::physics {

// Base class for collision shapes. This intermediary base (between Shape and the concrete shape
// types) makes it possible to access the Bullet collision shape object for all objects without
// exposing the Bullet API in the header.
class ShapeBase : public Shape {
public:
    ShapeBase() = default;
    ~ShapeBase() override = default;

    MG_MAKE_NON_COPYABLE(ShapeBase);
    MG_MAKE_NON_MOVABLE(ShapeBase);

    virtual btCollisionShape& bullet_shape() = 0;

    // How many bodies are using this shape?
    std::atomic_int ref_count = 0;
};

// Helper function for downcast from Shape* to ShapeBase*.
inline ShapeBase* shape_base_cast(Shape* shape)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
    auto* shape_base = static_cast<ShapeBase*>(shape);
    MG_ASSERT_DEBUG(dynamic_cast<ShapeBase*>(shape));
    return shape_base;
}

inline ShapeBase& shape_base_cast(Shape& shape)
{
    return *shape_base_cast(&shape);
}


} // namespace Mg::physics
