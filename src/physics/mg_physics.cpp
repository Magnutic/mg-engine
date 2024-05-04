//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/physics/mg_physics.h"

#include "mg/utils/mg_math_utils.h"
#include "mg_physics_debug_renderer.h"

#include "mg/containers/mg_array.h"
#include "mg/containers/mg_flat_map.h"
#include "mg/containers/mg_small_vector.h"
#include "mg/gfx/mg_debug_renderer.h"
#include "mg/gfx/mg_mesh_data.h"
#include "mg/utils/mg_stl_helpers.h"

#include <BulletCollision/BroadphaseCollision/btCollisionAlgorithm.h>
#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <BulletCollision/BroadphaseCollision/btOverlappingPairCache.h>
#include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
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
#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <LinearMath/btTransform.h>

#include <glm/mat3x4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <plf_colony.h>

#include <atomic>
#include <type_traits>

using glm::mat3;
using glm::mat4;
using glm::vec3;
using glm::vec4;

namespace Mg::physics {

Shape::Shape() = default;

Shape::~Shape() = default;

namespace {

//--------------------------------------------------------------------------------------------------
// Conversion functions for vectors and matrices
// These compile down to just a few instructions in release builds (verified with clang-11).

btTransform convert_transform(const mat4& transform)
{
    btTransform result;
    result.setFromOpenGLMatrix(&transform[0][0]);
    return result;
}

mat4 convert_transform(const btTransform& transform)
{
    mat4 result;
    transform.getOpenGLMatrix(&result[0][0]);
    return result;
}

btVector3 convert_vector(const vec3& vector)
{
    return { vector.x, vector.y, vector.z };
}

vec3 convert_vector(const btVector3& vector)
{
    return { vector.x(), vector.y(), vector.z() };
}

int convert_collision_group(const CollisionGroup group)
{
    int result{};
    static_assert(sizeof(result) == sizeof(group));
    std::memcpy(&result, &group, sizeof(group));
    return result;
}

CollisionGroup convert_collision_group(const int group)
{
    CollisionGroup result{};
    static_assert(sizeof(result) == sizeof(group));
    std::memcpy(&result, &group, sizeof(group));
    return result;
}

//--------------------------------------------------------------------------------------------------
// Decompose matrices, for interpolation

struct DecomposedTransform {
    vec4 position;
    Rotation rotation;
};

// Assumes m has no shearing and no scaling.
DecomposedTransform decompose(const mat4& m)
{
    return { m[3], Rotation{ quat_cast(mat3(m)) } };
}

mat4 interpolate_transforms(const mat4& lhs, const mat4& rhs, const float factor)
{
    const auto l = decompose(lhs);
    const auto r = decompose(rhs);
    mat4 result = Rotation::mix(l.rotation, r.rotation, factor).to_matrix();
    result[3] = mix(l.position, r.position, factor);
    return result;
}

//--------------------------------------------------------------------------------------------------
// Definitions of Shape types

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
ShapeBase* shape_base_cast(Shape* shape)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
    auto* shape_base = static_cast<ShapeBase*>(shape);
    MG_ASSERT_DEBUG(dynamic_cast<ShapeBase*>(shape));
    return shape_base;
}

ShapeBase& shape_base_cast(Shape& shape)
{
    return *shape_base_cast(&shape);
}

class BoxShape : public ShapeBase {
public:
    explicit BoxShape(vec3 extents) : m_bt_shape(convert_vector(extents / 2.0f)) {}

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
    explicit CylinderShape(vec3 extents) : m_bt_shape(convert_vector(extents / 2.0f)) {}

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
    explicit ConvexHullShape(const std::span<const gfx::Mesh::Vertex> vertices,
                             const vec3& centre_of_mass,
                             const vec3& scale)
    {
        m_bt_shape.setLocalScaling(convert_vector(scale));

        for (const gfx::Mesh::Vertex& vertex : vertices) {
            vec3 position = vertex.position;
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
    explicit MeshShape(const gfx::Mesh::MeshDataView& mesh_data)
        : m_vertices(decltype(m_vertices)::make_copy(mesh_data.vertices))
        , m_indices(decltype(m_indices)::make_copy(mesh_data.indices))
        , m_bt_shape(prepare_shape())
    {}

    btBvhTriangleMeshShape& bullet_shape() override { return m_bt_shape; }

    Type type() const override { return Type::Mesh; }

private:
    // TODO maybe reference rather than copy this data, somehow
    Mg::Array<Mg::gfx::Mesh::Vertex> m_vertices;
    Mg::Array<Mg::gfx::Mesh::Index> m_indices;

    btTriangleMesh m_bt_triangle_mesh;
    btBvhTriangleMeshShape m_bt_shape;

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

        static_assert(std::is_same_v<gfx::Mesh::Index, uint32_t>,
                      "Vertex index type must match enum value below.");
        constexpr auto bt_index_type = PHY_INTEGER;

        m_bt_triangle_mesh.addIndexedMesh(bt_indexed_mesh, bt_index_type);
        return btBvhTriangleMeshShape{ &m_bt_triangle_mesh, true, true };
    }
};


class CompoundShape : public ShapeBase {
public:
    explicit CompoundShape(std::span<Shape* const> parts, std::span<const mat4> part_transforms)
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

//--------------------------------------------------------------------------------------------------

// Basic check for collision group filters, to determine whether or not to ignore a collision.
bool needs_collision(const btCollisionObject& body0, const btCollisionObject& body1)
{
    const btBroadphaseProxy* handle0 = body0.getBroadphaseHandle();
    const btBroadphaseProxy* handle1 = body1.getBroadphaseHandle();
    return (handle0->m_collisionFilterGroup & handle1->m_collisionFilterMask) != 0 &&
           (handle1->m_collisionFilterGroup & handle0->m_collisionFilterMask) != 0;
}

} // namespace

// Common base for all physics bodies.
class PhysicsBody {
public:
    explicit PhysicsBody(const PhysicsBodyType type_,
                         const Identifier& id_,
                         ShapeBase& shape_,
                         const mat4& transform_)
        : type(type_), id(id_), transform(transform_), shape(&shape_)
    {
        ++shape->ref_count;
    }

    virtual ~PhysicsBody() = default;

    MG_MAKE_NON_COPYABLE(PhysicsBody);
    MG_MAKE_NON_MOVABLE(PhysicsBody);

    virtual btCollisionObject& get_bt_body() = 0;
    virtual const btCollisionObject& get_bt_body() const = 0;

    PhysicsBodyType type;
    Identifier id;
    mat4 transform;
    ShapeBase* shape;

    // How many handles to this body are there?
    std::atomic_int ref_count = 0;
};

[[maybe_unused]] static PhysicsBody* collision_object_cast(const btCollisionObject* bullet_object)
{
    return static_cast<PhysicsBody*>(bullet_object->getUserPointer());
}

PhysicsBodyHandle::PhysicsBodyHandle(PhysicsBody* data) : m_data(data)
{
    if (data) {
        ++m_data->ref_count;
    }
}

PhysicsBodyHandle::~PhysicsBodyHandle()
{
    if (m_data) {
        --m_data->ref_count;
    }
}

PhysicsBodyHandle::PhysicsBodyHandle(const PhysicsBodyHandle& rhs) : m_data(rhs.m_data)
{
    if (m_data) {
        ++m_data->ref_count;
    }
}

PhysicsBodyHandle& PhysicsBodyHandle::operator=(const PhysicsBodyHandle& rhs)
{
    auto tmp(rhs);
    swap(tmp);
    return *this;
}

Identifier PhysicsBodyHandle::id() const
{
    return m_data->id;
}

PhysicsBodyType PhysicsBodyHandle::type() const
{
    return m_data->type;
}

void PhysicsBodyHandle::has_contact_response(const bool enable)
{
    btCollisionObject& bt_body = m_data->get_bt_body();
    const auto old_collision_flags = bt_body.getCollisionFlags();
    const auto new_collision_flags =
        enable ? (old_collision_flags & ~btCollisionObject::CF_NO_CONTACT_RESPONSE)
               : (old_collision_flags | btCollisionObject::CF_NO_CONTACT_RESPONSE);
    bt_body.setCollisionFlags(new_collision_flags);
}

bool PhysicsBodyHandle::has_contact_response() const
{
    return m_data->get_bt_body().hasContactResponse();
}

mat4 PhysicsBodyHandle::get_transform() const
{
    return m_data->transform;
}

void PhysicsBodyHandle::set_filter_group(const CollisionGroup group)
{
    m_data->get_bt_body().getBroadphaseHandle()->m_collisionFilterGroup =
        convert_collision_group(group);
}

CollisionGroup PhysicsBodyHandle::get_filter_group() const
{
    return convert_collision_group(
        m_data->get_bt_body().getBroadphaseHandle()->m_collisionFilterGroup);
}

void PhysicsBodyHandle::set_filter_mask(const CollisionGroup mask)
{
    m_data->get_bt_body().getBroadphaseHandle()->m_collisionFilterMask =
        convert_collision_group(mask);
}

CollisionGroup PhysicsBodyHandle::get_filter_mask() const
{
    return convert_collision_group(
        m_data->get_bt_body().getBroadphaseHandle()->m_collisionFilterMask);
}

Shape& PhysicsBodyHandle::shape()
{
    return *m_data->shape;
}

const Shape& PhysicsBodyHandle::shape() const
{
    return *m_data->shape;
}

//--------------------------------------------------------------------------------------------------
// DynamicBody implementation

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

void DynamicBodyHandle::set_gravity(const glm::vec3& gravity)
{
    data().body.setGravity(convert_vector(gravity));
}

glm::vec3 DynamicBodyHandle::get_gravity() const
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

//--------------------------------------------------------------------------------------------------
// StaticBody implementation

class StaticBody : public PhysicsBody {
public:
    // Construct a static body using the supplied collision shape and parameters.
    explicit StaticBody(const Identifier& id_, ShapeBase& shape_, const mat4 transform_)
        : PhysicsBody(PhysicsBodyType::static_body, id_, shape_, transform_)
    {
        body.setCollisionShape(&shape_.bullet_shape());
        body.setWorldTransform(convert_transform(transform_));
        body.setUserPointer(this);
        body.setCollisionFlags(body.getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);

        // TODO: configurable friction
        body.setFriction(0.5f);
    }

    btCollisionObject& get_bt_body() override { return body; }
    const btCollisionObject& get_bt_body() const override { return body; }

    // The bullet collision object.
    btCollisionObject body;
};

StaticBody& StaticBodyHandle::data()
{
    MG_ASSERT_DEBUG(type() == PhysicsBodyType::static_body);
    return *static_cast<StaticBody*>(m_data); // NOLINT
}

const StaticBody& StaticBodyHandle::data() const
{
    MG_ASSERT_DEBUG(type() == PhysicsBodyType::static_body);
    return *static_cast<const StaticBody*>(m_data); // NOLINT
}

//--------------------------------------------------------------------------------------------------
// GhostObject implementation

class GhostObject : public PhysicsBody {
public:
    // Construct a ghost body using the supplied collision shape and parameters.
    explicit GhostObject(const Identifier& id_, ShapeBase& shape_, const mat4 transform_)
        : PhysicsBody(PhysicsBodyType::ghost_object, id_, shape_, transform_)
    {
        object.setUserPointer(this);
        object.setWorldTransform(convert_transform(transform_));
        object.setCollisionShape(&(shape_base_cast(shape_).bullet_shape()));
        object.setActivationState(DISABLE_DEACTIVATION);
    }

    btCollisionObject& get_bt_body() override { return object; }
    const btCollisionObject& get_bt_body() const override { return object; }

    btPairCachingGhostObject object;

    // The collisions involving this object in the most recent update. See
    // Mg::physics::World::update.
    std::vector<const Collision*> collisions;
};

void GhostObjectHandle::set_transform(const mat4& transform)
{
    data().object.setWorldTransform(convert_transform(transform));
    data().transform = transform;
}

std::span<const Collision* const> GhostObjectHandle::collisions() const
{
    return data().collisions;
}

GhostObject& GhostObjectHandle::data()
{
    MG_ASSERT_DEBUG(type() == PhysicsBodyType::ghost_object);
    return *static_cast<GhostObject*>(m_data); // NOLINT
}

const GhostObject& GhostObjectHandle::data() const
{
    MG_ASSERT_DEBUG(type() == PhysicsBodyType::ghost_object);
    return *static_cast<const GhostObject*>(m_data); // NOLINT
}

//--------------------------------------------------------------------------------------------------
// World implementation

namespace {

struct CollisionById {
    Identifier id;
    const Collision* collision;
};

// Ordering functions are used for sorting and binary-searching CollisionById objects.
// Marked maybe_unused since my completer plugin does not seem to understand that they are used.
[[maybe_unused]] bool operator<(const CollisionById& l, const CollisionById& r)
{
    return l.id.hash() < r.id.hash();
}

[[maybe_unused]] bool operator<(const CollisionById& l, const Identifier& id)
{
    return l.id.hash() < id.hash();
}

[[maybe_unused]] bool operator<(const Identifier& id, const CollisionById& r)
{
    return id.hash() < r.id.hash();
}

} // namespace

struct World::Impl {
    // It is important that the data below is stored in containers that do not re-allocate or move
    // elements. Pointers to these objects must remain valid until the object in question is
    // destroyed, partially due to Bullet internally storing pointers between objects and partially
    // due to the API in the header using raw observer pointers.

    // Container storing all dynamic bodies in the scene.
    plf::colony<DynamicBody> dynamic_bodies;

    // Container storing all static objects in the scene.
    plf::colony<StaticBody> static_bodies;

    // Container storing all ghost objects in the scene.
    plf::colony<GhostObject> ghost_objects;

    // Storage for collision shapes, separated by type.
    plf::colony<BoxShape> box_shapes;
    plf::colony<CapsuleShape> capsule_shapes;
    plf::colony<CylinderShape> cylinder_shapes;
    plf::colony<SphereShape> sphere_shapes;
    plf::colony<ConeShape> cone_shapes;
    plf::colony<MeshShape> mesh_shapes;
    plf::colony<ConvexHullShape> convex_hull_shapes;
    plf::colony<CompoundShape> compound_shapes;

    // Configuration setting up Bullet physics world.
    std::unique_ptr<btDefaultCollisionConfiguration> collision_configuration;
    std::unique_ptr<btCollisionDispatcher> dispatcher;
    std::unique_ptr<btBroadphaseInterface> broadphase;
    std::unique_ptr<btSequentialImpulseConstraintSolver> solver;
    std::unique_ptr<btGhostPairCallback> ghost_pair_callback;
    std::unique_ptr<btDiscreteDynamicsWorld> dynamics_world;

    // Data on collisions that occurred in the most recent update.
    std::vector<Collision> collisions;

    // Collisions sorted by object id hash, allowing lookup of collisions involving a particular
    // object. In practice, this is a FlatMultiMap, but I do not have a container like that right
    // now.
    std::vector<CollisionById> collisions_by_id;
};

namespace {

void init_physics(World::Impl& world)
{
    world.collision_configuration = std::make_unique<btDefaultCollisionConfiguration>();
    world.dispatcher = std::make_unique<btCollisionDispatcher>(world.collision_configuration.get());
    world.broadphase = std::make_unique<btDbvtBroadphase>();
    world.solver = std::make_unique<btSequentialImpulseConstraintSolver>();
    world.ghost_pair_callback = std::make_unique<btGhostPairCallback>();
    world.dynamics_world =
        std::make_unique<btDiscreteDynamicsWorld>(world.dispatcher.get(),
                                                  world.broadphase.get(),
                                                  world.solver.get(),
                                                  world.collision_configuration.get());
    world.dynamics_world->getPairCache()->setInternalGhostPairCallback(
        world.ghost_pair_callback.get());
}

} // namespace

World::World()
{
    init_physics(*m_impl);
    gravity(vec3(0.0f, 0.0f, -9.82f));
}

World::~World() = default;

void World::gravity(const vec3& gravity)
{
    m_impl->dynamics_world->setGravity(convert_vector(gravity));
}

Shape* World::create_box_shape(const vec3& extents)
{
    return &*m_impl->box_shapes.emplace(extents);
}

Shape* World::create_capsule_shape(const float radius, const float height)
{
    return &*m_impl->capsule_shapes.emplace(radius, height);
}

Shape* World::create_cylinder_shape(const vec3& extents)
{
    return &*m_impl->cylinder_shapes.emplace(extents);
}

Shape* World::create_sphere_shape(const float radius)
{
    return &*m_impl->sphere_shapes.emplace(radius);
}

Shape* World::create_cone_shape(const float radius, const float height)
{
    return &*m_impl->cone_shapes.emplace(radius, height);
}

Shape* World::create_mesh_shape(const gfx::Mesh::MeshDataView& mesh_data)
{
    return &*m_impl->mesh_shapes.emplace(mesh_data);
}

Shape* World::create_convex_hull(const std::span<const gfx::Mesh::Vertex> vertices,
                                 const vec3& centre_of_mass,
                                 const vec3& scale)
{
    return &*m_impl->convex_hull_shapes.emplace(vertices, centre_of_mass, scale);
}

Shape* World::create_compound_shape(std::span<Shape* const> parts,
                                    std::span<const mat4> part_transforms)
{
    return &*m_impl->compound_shapes.emplace(parts, part_transforms);
}


StaticBodyHandle
World::create_static_body(const Identifier& id, Shape& shape, const mat4& transform)
{
    // Create Bullet rigid body object and metadata.
    StaticBody& sb = *m_impl->static_bodies.emplace(id, shape_base_cast(shape), transform);

    // Disable debug rendering for static meshes. Too slow and also not really useful; it makes the
    // screen too crowded.
    if (shape.type() == Shape::Type::Mesh) {
        sb.body.setCollisionFlags(sb.body.getCollisionFlags() |
                                  btCollisionObject::CF_DISABLE_VISUALIZE_OBJECT);
    }

    m_impl->dynamics_world->addCollisionObject(&sb.body);
    return PhysicsBodyHandle{ &sb }.as_static_body().value();
}

DynamicBodyHandle World::create_dynamic_body(const Identifier& id,
                                             Shape& shape,
                                             const DynamicBodyParameters& parameters,
                                             const mat4& transform)
{
    MG_ASSERT(shape.type() != Shape::Type::Mesh &&
              "Mesh Shape cannot be used in a DynamicBody. Use a ConvexHull instead.");

    // Create Bullet rigid body object and metadata.
    DynamicBody& rb =
        *m_impl->dynamic_bodies.emplace(id, shape_base_cast(shape), parameters, transform);

    m_impl->dynamics_world->addRigidBody(&rb.body);
    return PhysicsBodyHandle{ &rb }.as_dynamic_body().value();
}

GhostObjectHandle
World::create_ghost_object(const Identifier& id, Shape& shape, const mat4& transform)
{
    // Create Bullet collision body object and metadata.
    GhostObject& go = *m_impl->ghost_objects.emplace(id, shape_base_cast(shape), transform);

    // TODO: look into the filters passed in these construction functions.
    m_impl->dynamics_world->addCollisionObject(&go.object,
                                               btBroadphaseProxy::KinematicFilter,
                                               btBroadphaseProxy::AllFilter);
    return PhysicsBodyHandle{ &go }.as_ghost_body().value();
}

namespace {
void contact_manifold_to_collisions(const btPersistentManifold& contact_manifold,
                                    std::vector<Collision>& out)
{
    PhysicsBody* first = collision_object_cast(contact_manifold.getBody0());
    PhysicsBody* second = collision_object_cast(contact_manifold.getBody1());

    if (!first || !second) {
        return;
    }

    const int num_contacts = contact_manifold.getNumContacts();

    for (int j = 0; j < num_contacts; ++j) {
        const btManifoldPoint& contact_point = contact_manifold.getContactPoint(j);

        if (contact_point.getDistance() > 0.0f) {
            continue;
        }

        Collision& collision = out.emplace_back();
        collision.object_a = PhysicsBodyHandle{ first };
        collision.object_b = PhysicsBodyHandle{ second };
        collision.contact_point_on_a = convert_vector(contact_point.getPositionWorldOnA());
        collision.contact_point_on_b = convert_vector(contact_point.getPositionWorldOnB());
        collision.applied_impulse = contact_point.getAppliedImpulse();
        collision.distance = contact_point.getDistance();

        collision.normal_on_b = convert_vector(contact_point.m_normalWorldOnB);
    }
}
} // namespace

vec3 World::gravity() const
{
    return convert_vector(m_impl->dynamics_world->getGravity());
}

void World::update(const float time_step)
{
    constexpr int max_sub_steps = 0; // Disable interpolation, we do that ourselves instead.
    m_impl->dynamics_world->stepSimulation(time_step, max_sub_steps);

    // TODO: these loops should be parallelisable

    for (DynamicBody& rb : m_impl->dynamic_bodies) {
        rb.previous_transform = rb.transform;
        rb.transform = convert_transform(rb.body.getWorldTransform());
    }

    // Remove ghost-object collisions from last update.
    for (GhostObject& ghost_object : m_impl->ghost_objects) {
        ghost_object.collisions.clear();
    }

    collect_garbage();

    // Get all collisions that occurred.
    const int num_manifolds = m_impl->dispatcher->getNumManifolds();

    for (int i = 0; i < num_manifolds; ++i) {
        const btPersistentManifold& contact_manifold =
            *m_impl->dispatcher->getManifoldByIndexInternal(i);
        contact_manifold_to_collisions(contact_manifold, m_impl->collisions);
    }

    // For each collision, make it efficiently accessible by id of either involved object.
    m_impl->collisions_by_id.clear();
    for (const Collision& collision : m_impl->collisions) {
        m_impl->collisions_by_id.push_back(CollisionById{ collision.object_a.id(), &collision });
        m_impl->collisions_by_id.push_back(CollisionById{ collision.object_b.id(), &collision });
    }

    // Collisions by id must be sorted for `find_collisions_for` to work, as it uses binary search.
    sort(m_impl->collisions_by_id);

    // Collect all collisions involving ghost objects.
    for (GhostObject& ghost_object : m_impl->ghost_objects) {
        find_collisions_for(ghost_object.id, ghost_object.collisions);
    }
}

void World::interpolate(const float factor)
{
    // TODO: should be parallelisable
    for (DynamicBody& rbd : m_impl->dynamic_bodies) {
        rbd.interpolated_transform =
            interpolate_transforms(rbd.previous_transform, rbd.transform, factor);
    }
}

void World::collect_garbage()
{
    // First, remove all collisions. These contain reference-counted handles to bodies, so they must
    // be removed for the rest to take effect.
    m_impl->collisions.clear();

    // Delete bodies. Bodies have pointers into the collision shapes, so they must be deleted before
    // shapes.
    {
        for (auto it = m_impl->dynamic_bodies.begin(); it != m_impl->dynamic_bodies.end();) {
            if (it->ref_count == 0) {
                m_impl->dynamics_world->removeRigidBody(&it->body);
                it = m_impl->dynamic_bodies.erase(it);
            }
            else {
                ++it;
            }
        }

        for (auto it = m_impl->static_bodies.begin(); it != m_impl->static_bodies.end();) {
            if (it->ref_count == 0) {
                m_impl->dynamics_world->removeCollisionObject(&it->body);
                it = m_impl->static_bodies.erase(it);
            }
            else {
                ++it;
            }
        }

        for (auto it = m_impl->ghost_objects.begin(); it != m_impl->ghost_objects.end();) {
            if (it->ref_count == 0) {
                m_impl->dynamics_world->removeCollisionObject(&it->object);
                it = m_impl->ghost_objects.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    // Delete unused collision shapes.
    {
        // Count containers handled to make sure we did not miss any.
        int num_containers_handled = 0;

        auto delete_unused_shapes = [&num_containers_handled](auto& shape_container) {
            for (auto it = shape_container.begin(); it != shape_container.end();) {
                if (it->ref_count == 0) {
                    it = shape_container.erase(it);
                }
                else {
                    ++it;
                }
            }

            ++num_containers_handled;
        };

        delete_unused_shapes(m_impl->box_shapes);
        delete_unused_shapes(m_impl->capsule_shapes);
        delete_unused_shapes(m_impl->cone_shapes);
        delete_unused_shapes(m_impl->cylinder_shapes);
        delete_unused_shapes(m_impl->sphere_shapes);
        delete_unused_shapes(m_impl->mesh_shapes);
        delete_unused_shapes(m_impl->convex_hull_shapes);
        delete_unused_shapes(m_impl->compound_shapes);

        // Check so that we will not forget to update this code if we add more shape types.
        MG_ASSERT(num_containers_handled == static_cast<int>(Shape::Type::NumEnumValues_));
    }
}

std::span<const Collision> World::collisions() const
{
    return m_impl->collisions;
}

void World::find_collisions_for(Identifier id, std::vector<const Collision*>& out) const
{
    const auto& by_id = m_impl->collisions_by_id;
    auto [it, end] = std::equal_range(by_id.begin(), by_id.end(), id);
    while (it != end) {
        out.push_back(it->collision);
        ++it;
    }
}

void World::calculate_collisions_for(const GhostObjectHandle& ghost_object_handle,
                                     std::vector<Collision>& out)
{
    btCollisionWorld& collision_world = *m_impl->dynamics_world;
    btPairCachingGhostObject& ghost_object =
        static_cast<GhostObject*>(ghost_object_handle.m_data)->object; // NOLINT

    // First, we must update the broad-phase pair cache, in case objects have moved since last
    // update. This is done by updating the ghost object's AABB.
    {
        btVector3 minAabb;
        btVector3 maxAabb;
        ghost_object.getCollisionShape()->getAabb(ghost_object.getWorldTransform(),
                                                  minAabb,
                                                  maxAabb);

        // This should update both the world broad-phase pair cache and the ghost object's internal
        // pair cache.
        collision_world.getBroadphase()->setAabb(ghost_object.getBroadphaseHandle(),
                                                 minAabb,
                                                 maxAabb,
                                                 collision_world.getDispatcher());
    }

    // Now, the broad-phase pair cache is up to date. Dispatch narrow-phase collision detection.
    collision_world.getDispatcher()
        ->dispatchAllCollisionPairs(ghost_object.getOverlappingPairCache(),
                                    collision_world.getDispatchInfo(),
                                    collision_world.getDispatcher());

    auto& overlapping_pair_array =
        ghost_object.getOverlappingPairCache()->getOverlappingPairArray();

    btManifoldArray manifold_array;

    for (int i = 0; i < overlapping_pair_array.size(); ++i) {
        btBroadphasePair* collision_pair = &overlapping_pair_array[i];

        auto* obj0 = static_cast<btCollisionObject*>(collision_pair->m_pProxy0->m_clientObject);
        auto* obj1 = static_cast<btCollisionObject*>(collision_pair->m_pProxy1->m_clientObject);

        auto* other = (obj0 == &ghost_object) ? obj1 : obj0;

        if (!other->hasContactResponse() || !needs_collision(*obj0, *obj1) ||
            !collision_pair->m_algorithm) {
            continue;
        }

        manifold_array.clear();
        collision_pair->m_algorithm->getAllContactManifolds(manifold_array);

        for (int j = 0; j < manifold_array.size(); ++j) {
            contact_manifold_to_collisions(*manifold_array[j], out);
        }
    }
}

namespace {

class RayCallback : public btCollisionWorld::RayResultCallback {
public:
    explicit RayCallback(const vec3& start,
                         const vec3& end,
                         const CollisionGroup filter_mask,
                         std::vector<RayHit>& out)
        : m_start(start), m_end(end), m_out(out)
    {
        m_collisionFilterGroup = convert_collision_group(CollisionGroup::All);
        m_collisionFilterMask = convert_collision_group(filter_mask);
    }

    float addSingleResult(btCollisionWorld::LocalRayResult& result,
                          bool normal_is_in_worldspace) override
    {
        ++m_num_hits;
        RayHit& hit = m_out.emplace_back();

        hit.body = PhysicsBodyHandle{ collision_object_cast(result.m_collisionObject) };

        hit.hit_normal_worldspace = convert_vector(
            normal_is_in_worldspace ? result.m_hitNormalLocal
                                    : (result.m_collisionObject->getWorldTransform().getBasis() *
                                       result.m_hitNormalLocal));

        hit.hit_fraction = result.m_hitFraction;
        hit.hit_point_worldspace = glm::mix(m_start, m_end, hit.hit_fraction);

        return hit.hit_fraction;
    }

    size_t num_hits() const { return m_num_hits; }

private:
    size_t m_num_hits = 0;
    vec3 m_start;
    vec3 m_end;
    std::vector<RayHit>& m_out;
};

class ConvexSweepCallback : public btCollisionWorld::ConvexResultCallback {
public:
    explicit ConvexSweepCallback(const CollisionGroup filter_mask, std::vector<RayHit>& out)
        : m_out(out)
    {
        m_collisionFilterGroup = convert_collision_group(CollisionGroup::All);
        m_collisionFilterMask = convert_collision_group(filter_mask);
    }

    float addSingleResult(btCollisionWorld::LocalConvexResult& result,
                          bool normal_is_in_worldspace) override
    {
        ++m_num_hits;
        RayHit& hit = m_out.emplace_back();

        hit.body = PhysicsBodyHandle{ collision_object_cast(result.m_hitCollisionObject) };

        hit.hit_normal_worldspace = convert_vector(
            normal_is_in_worldspace ? result.m_hitNormalLocal
                                    : (result.m_hitCollisionObject->getWorldTransform().getBasis() *
                                       result.m_hitNormalLocal));

        hit.hit_fraction = result.m_hitFraction;
        hit.hit_point_worldspace = convert_vector(result.m_hitPointLocal);

        return hit.hit_fraction;
    }

    size_t num_hits() const { return m_num_hits; }

private:
    size_t m_num_hits = 0;
    std::vector<RayHit>& m_out;
};

} // namespace

size_t World::raycast(const vec3& start,
                      const vec3& end,
                      CollisionGroup filter_mask,
                      std::vector<RayHit>& out)
{
    RayCallback callback(start, end, filter_mask, out);
    m_impl->dynamics_world->rayTest(convert_vector(start), convert_vector(end), callback);
    return callback.num_hits();
}

size_t World::convex_sweep(Shape& shape,
                           const vec3& start,
                           const vec3& end,
                           CollisionGroup filter_mask,
                           std::vector<RayHit>& out)
{
    MG_ASSERT(shape.is_convex());

    auto* bt_convex_shape =
        static_cast<const btConvexShape*>(&shape_base_cast(shape).bullet_shape()); // NOLINT

    ConvexSweepCallback callback(filter_mask, out);
    btTransform start_transform;
    btTransform end_transform;
    start_transform.setIdentity();
    end_transform.setIdentity();
    start_transform.setOrigin(convert_vector(start));
    end_transform.setOrigin(convert_vector(end));

    const float allowed_ccd_penetration =
        m_impl->dynamics_world->getDispatchInfo().m_allowedCcdPenetration;

    m_impl->dynamics_world->convexSweepTest(bt_convex_shape,
                                            start_transform,
                                            end_transform,
                                            callback,
                                            allowed_ccd_penetration);
    return callback.num_hits();
}

void World::draw_debug(const gfx::IRenderTarget& render_target,
                       gfx::DebugRenderer& debug_renderer,
                       const glm::mat4& view_proj)
{
    // Set up a debug drawer. It will redirect Bullet's debug-rendering calls to debug_renderer.
    PhysicsDebugRenderer physics_debug_renderer{ render_target, debug_renderer, view_proj };

    // Tell Bullet to use the debug drawer until the end of this function.
    auto reset_debug_drawer = finally([&] { m_impl->dynamics_world->setDebugDrawer(nullptr); });
    m_impl->dynamics_world->setDebugDrawer(&physics_debug_renderer);

    m_impl->dynamics_world->debugDrawWorld();
}

} // namespace Mg::physics
