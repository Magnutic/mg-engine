//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_shape.h
 * Shape that can be used for a physics body.
 */

#pragma once

#include "mg/utils/mg_macros.h"

namespace Mg::physics {

/** Interface for all collision shapes. Collision shapes are used to give a PhysicsBody objects a
 * shape. A Shape can be used in multiple PhysicsBody objects, and it is recommended to re-use Shape
 * objects whenever possible.
 *
 * Shape objects can be constructed using the member functions on `Mg::physics::World`. Their
 * lifetime is automatically managed; do not delete them.
 */
class Shape {
protected:
    Shape() = default;

public:
    virtual ~Shape() = default;

    MG_MAKE_NON_MOVABLE(Shape);
    MG_MAKE_NON_COPYABLE(Shape);

    /** Supported types of collision shapes. */
    enum class Type {
        Box,
        Capsule,
        Cylinder,
        Sphere,
        Cone,
        ConvexHull,
        Mesh,
        Compound,
        NumEnumValues_
    };

    /** Get what type of collision shape this object is.  */
    virtual Type type() const = 0;

    bool is_convex() const
    {
        const auto t = type();
        return t != Type::Mesh && t != Type::Compound;
    }
};

} // namespace Mg
