//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#pragma once

#include "mg/core/containers/mg_small_vector.h"

#include "mg_bullet_utils.h"
#include "mg_shape_base.h"

#include "mg/core/containers/mg_array.h"
#include "mg/core/gfx/mg_mesh_data.h"
#include "mg/core/physics/mg_shape.h"
#include "mg/utils/mg_assert.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#include <BulletCollision/CollisionShapes/btCapsuleShape.h>
#include <BulletCollision/CollisionShapes/btCompoundShape.h>
#include <BulletCollision/CollisionShapes/btConeShape.h>
#include <BulletCollision/CollisionShapes/btConvexHullShape.h>
#include <BulletCollision/CollisionShapes/btCylinderShape.h>
#include <BulletCollision/CollisionShapes/btMultiSphereShape.h>
#include <BulletCollision/CollisionShapes/btSphereShape.h>
#include <BulletCollision/CollisionShapes/btTriangleMesh.h>

#include <atomic>

namespace Mg::physics {

class BoxShape : public ShapeBase {
public:
    explicit BoxShape(glm::vec3 extents) : m_bt_shape(convert_vector(extents / 2.0f)) {}

    btBoxShape& bullet_shape() override { return m_bt_shape; }

    Type type() const override { return Type::Box; }

private:
    btBoxShape m_bt_shape;
};


class CapsuleShape : public ShapeBase {
public:
    explicit CapsuleShape(float radius, float height) : m_bt_shape(radius, height) {}

    btCapsuleShapeZ& bullet_shape() override { return m_bt_shape; }

    Type type() const override { return Type::Capsule; }

private:
    btCapsuleShapeZ m_bt_shape; // Z for Z-up
};


class CylinderShape : public ShapeBase {
public:
    explicit CylinderShape(glm::vec3 extents) : m_bt_shape(convert_vector(extents / 2.0f)) {}

    btCylinderShapeZ& bullet_shape() override { return m_bt_shape; }

    Type type() const override { return Type::Cylinder; }

private:
    btCylinderShapeZ m_bt_shape; // Z for Z-up
};


class SphereShape : public ShapeBase {
public:
    explicit SphereShape(const float radius) : m_bt_shape(radius) {}

    btSphereShape& bullet_shape() override { return m_bt_shape; }

    Type type() const override { return Type::Sphere; }

private:
    btSphereShape m_bt_shape;
};


class ConeShape : public ShapeBase {
public:
    explicit ConeShape(float radius, float height) : m_bt_shape(radius, height) {}

    btConeShapeZ& bullet_shape() override { return m_bt_shape; }

    Type type() const override { return Type::Cone; }

private:
    btConeShapeZ m_bt_shape; // Z for Z-up
};


class ConvexHullShape : public ShapeBase {
public:
    explicit ConvexHullShape(const std::span<const gfx::mesh_data::Vertex> vertices,
                             const glm::vec3& centre_of_mass,
                             const glm::vec3& scale)
    {
        m_bt_shape.setLocalScaling(convert_vector(scale));

        for (const gfx::mesh_data::Vertex& vertex : vertices) {
            glm::vec3 position = vertex.position;
            position -= centre_of_mass;
            m_bt_shape.addPoint(convert_vector(position));
        }

        m_bt_shape.optimizeConvexHull();
    }

    btConvexHullShape& bullet_shape() override { return m_bt_shape; }

    Type type() const override { return Type::ConvexHull; }

private:
    btConvexHullShape m_bt_shape;
};

class MeshShape : public ShapeBase {
public:
    explicit MeshShape(const gfx::mesh_data::MeshDataView& mesh_data)
        : m_vertices{ copy_vertex_positions(mesh_data.vertices) }
        , m_indices{ Array<uint32_t>::make_copy(mesh_data.indices) }
        , m_bt_shape{ prepare_shape() }
    {}

    btBvhTriangleMeshShape& bullet_shape() override { return m_bt_shape; }

    Type type() const override { return Type::Mesh; }

    const Mg::Array<glm::vec3>& vertices() const { return m_vertices; }
    const Mg::Array<Mg::gfx::mesh_data::Index>& indices() const { return m_indices; }

private:
    btBvhTriangleMeshShape prepare_shape()
    {
        btIndexedMesh bt_indexed_mesh = {};
        bt_indexed_mesh.m_vertexType = PHY_FLOAT;
        bt_indexed_mesh.m_vertexStride = sizeof(m_vertices[0]);
        bt_indexed_mesh.m_numVertices = Mg::as<int>(m_vertices.size());
        bt_indexed_mesh.m_vertexBase = reinterpret_cast<const uint8_t*>(m_vertices.data());
        bt_indexed_mesh.m_triangleIndexStride = 3 * sizeof(m_indices[0]);
        bt_indexed_mesh.m_numTriangles = Mg::as<int>(m_indices.size() / 3);
        bt_indexed_mesh.m_triangleIndexBase = reinterpret_cast<const uint8_t*>(m_indices.data());

        constexpr auto bt_index_type = PHY_INTEGER;

        m_bt_triangle_mesh.addIndexedMesh(bt_indexed_mesh, bt_index_type);
        return btBvhTriangleMeshShape{ &m_bt_triangle_mesh, true, true };
    }

    static Array<glm::vec3> copy_vertex_positions(std::span<const gfx::mesh_data::Vertex> vertices)
    {
        auto result = Array<glm::vec3>::make_for_overwrite(vertices.size());
        for (size_t i = 0; i < result.size(); ++i) {
            result[i] = vertices[i].position;
        }
        return result;
    }

    Mg::Array<glm::vec3> m_vertices;
    Mg::Array<uint32_t> m_indices;

    btTriangleMesh m_bt_triangle_mesh;
    btBvhTriangleMeshShape m_bt_shape;
};


class CompoundShape : public ShapeBase {
public:
    explicit CompoundShape(std::span<Shape* const> parts,
                           std::span<const glm::mat4> part_transforms)
    {
        MG_ASSERT(parts.size() == part_transforms.size());

        for (size_t i = 0; i < parts.size(); ++i) {
            ShapeBase* part = shape_base_cast(parts[i]);
            MG_ASSERT(part != nullptr);

            btCollisionShape* bullet_part = &part->bullet_shape();
            m_bt_shape.addChildShape(convert_transform(part_transforms[i]), bullet_part);

            ++part->ref_count;
        }
    }

    ~CompoundShape() override
    {
        for (Shape* shape : m_children) {
            --shape_base_cast(shape)->ref_count;
        }
    }

    MG_MAKE_NON_COPYABLE(CompoundShape);
    MG_MAKE_NON_MOVABLE(CompoundShape);

    btCompoundShape& bullet_shape() override { return m_bt_shape; }

    Type type() const override { return Type::Capsule; }

private:
    btCompoundShape m_bt_shape; // Z for Z-up
    small_vector<Shape*, 10> m_children;
};

} // namespace Mg::physics
