//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/physics/mg_physics_world.h"

#include "mg_bullet_utils.h"
#include "mg_dynamic_body.h"
#include "mg_ghost_object.h"
#include "mg_physics_debug_renderer.h"
#include "mg_shape_types.h"
#include "mg_static_body.h"

#include "mg/gfx/mg_debug_renderer.h"
#include "mg/gfx/mg_mesh_data.h"
#include "mg/utils/mg_stl_helpers.h"

#include <BulletCollision/BroadphaseCollision/btCollisionAlgorithm.h>
#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <BulletCollision/BroadphaseCollision/btOverlappingPairCache.h>
#include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <LinearMath/btTransform.h>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <plf_colony.h>

using glm::mat4;
using glm::vec3;
using glm::vec4;

namespace Mg::physics {

namespace {

// Basic check for collision group filters, to determine whether or not to ignore a collision.
bool needs_collision(const btCollisionObject& body0, const btCollisionObject& body1)
{
    const btBroadphaseProxy* handle0 = body0.getBroadphaseHandle();
    const btBroadphaseProxy* handle1 = body1.getBroadphaseHandle();
    return (handle0->m_collisionFilterGroup & handle1->m_collisionFilterMask) != 0 &&
           (handle1->m_collisionFilterGroup & handle0->m_collisionFilterMask) != 0;
}

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

struct PhysicsWorld::Impl {
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

void init_physics(PhysicsWorld::Impl& world)
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

PhysicsWorld::PhysicsWorld()
{
    init_physics(*m_impl);
    gravity(vec3(0.0f, 0.0f, -9.82f));
}

PhysicsWorld::~PhysicsWorld() = default;

void PhysicsWorld::gravity(const vec3& gravity)
{
    m_impl->dynamics_world->setGravity(convert_vector(gravity));
}

Shape* PhysicsWorld::create_box_shape(const vec3& extents)
{
    return &*m_impl->box_shapes.emplace(extents);
}

Shape* PhysicsWorld::create_capsule_shape(const float radius, const float height)
{
    return &*m_impl->capsule_shapes.emplace(radius, height);
}

Shape* PhysicsWorld::create_cylinder_shape(const vec3& extents)
{
    return &*m_impl->cylinder_shapes.emplace(extents);
}

Shape* PhysicsWorld::create_sphere_shape(const float radius)
{
    return &*m_impl->sphere_shapes.emplace(radius);
}

Shape* PhysicsWorld::create_cone_shape(const float radius, const float height)
{
    return &*m_impl->cone_shapes.emplace(radius, height);
}

Shape* PhysicsWorld::create_mesh_shape(const gfx::mesh_data::MeshDataView& mesh_data)
{
    return &*m_impl->mesh_shapes.emplace(mesh_data);
}

Shape* PhysicsWorld::create_convex_hull(const std::span<const gfx::mesh_data::Vertex> vertices,
                                        const vec3& centre_of_mass,
                                        const vec3& scale)
{
    return &*m_impl->convex_hull_shapes.emplace(vertices, centre_of_mass, scale);
}

Shape* PhysicsWorld::create_compound_shape(std::span<Shape* const> parts,
                                           std::span<const mat4> part_transforms)
{
    return &*m_impl->compound_shapes.emplace(parts, part_transforms);
}


StaticBodyHandle
PhysicsWorld::create_static_body(const Identifier& id, Shape& shape, const mat4& transform)
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

DynamicBodyHandle PhysicsWorld::create_dynamic_body(const Identifier& id,
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
PhysicsWorld::create_ghost_object(const Identifier& id, Shape& shape, const mat4& transform)
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

vec3 PhysicsWorld::gravity() const
{
    return convert_vector(m_impl->dynamics_world->getGravity());
}

void PhysicsWorld::update(const float time_step)
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

void PhysicsWorld::interpolate(const float factor)
{
    // TODO: should be parallelisable
    for (DynamicBody& rbd : m_impl->dynamic_bodies) {
        rbd.interpolated_transform =
            interpolate_transforms(rbd.previous_transform, rbd.transform, factor);
    }
}

void PhysicsWorld::collect_garbage()
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

std::span<const Collision> PhysicsWorld::collisions() const
{
    return m_impl->collisions;
}

void PhysicsWorld::find_collisions_for(Identifier id, std::vector<const Collision*>& out) const
{
    const auto& by_id = m_impl->collisions_by_id;
    auto [it, end] = std::equal_range(by_id.begin(), by_id.end(), id);
    while (it != end) {
        out.push_back(it->collision);
        ++it;
    }
}

void PhysicsWorld::calculate_collisions_for(const GhostObjectHandle& ghost_object_handle,
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
                          const bool normal_is_in_worldspace) override
    {
        ++m_num_hits;
        RayHit& hit = m_out.emplace_back();

        hit.body = PhysicsBodyHandle{ collision_object_cast(result.m_hitCollisionObject) };

        if (auto* mesh_shape = dynamic_cast<MeshShape*>(&hit.body.shape()); mesh_shape) {
            // Bullet reports weird normals for convex sweeps against meshes, especially when
            // sweeping cylinders or boxes. That breaks the character controller, making it fall
            // through floors. To work around this, we calculate the normal of the triangle here.
            const auto ti = size_t(result.m_localShapeInfo->m_triangleIndex);
            const auto i0 = mesh_shape->indices()[ti * 3];
            const auto i1 = mesh_shape->indices()[ti * 3 + 1];
            const auto i2 = mesh_shape->indices()[ti * 3 + 2];
            const auto v0 = mesh_shape->vertices()[i0];
            const auto v1 = mesh_shape->vertices()[i1];
            const auto v2 = mesh_shape->vertices()[i2];
            hit.hit_normal_worldspace = hit.body.get_transform() *
                                        vec4(glm::normalize(glm::cross(v1 - v0, v2 - v0)), 0.0f);
        }
        else {
            hit.hit_normal_worldspace =
                convert_vector(normal_is_in_worldspace
                                   ? result.m_hitNormalLocal
                                   : (result.m_hitCollisionObject->getWorldTransform().getBasis() *
                                      result.m_hitNormalLocal));
        }

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

size_t PhysicsWorld::raycast(const vec3& start,
                             const vec3& end,
                             CollisionGroup filter_mask,
                             std::vector<RayHit>& out)
{
    RayCallback callback(start, end, filter_mask, out);
    m_impl->dynamics_world->rayTest(convert_vector(start), convert_vector(end), callback);
    return callback.num_hits();
}

size_t PhysicsWorld::convex_sweep(Shape& shape,
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

void PhysicsWorld::draw_debug(gfx::DebugRenderQueue& render_queue)
{
    // Set up a debug drawer. It will redirect Bullet's debug-rendering calls to debug_renderer.
    PhysicsDebugRenderer physics_debug_renderer{ render_queue };

    // Tell Bullet to use the debug drawer until the end of this function.
    auto reset_debug_drawer = finally([&] { m_impl->dynamics_world->setDebugDrawer(nullptr); });
    m_impl->dynamics_world->setDebugDrawer(&physics_debug_renderer);

    m_impl->dynamics_world->debugDrawWorld();
}

} // namespace Mg::physics
