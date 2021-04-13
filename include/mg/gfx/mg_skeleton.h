//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2021, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_skeleton.h
 * Skeleton for animated meshes.
 */

#pragma once

#include "mg/containers/mg_array.h"
#include "mg/core/mg_identifier.h"
#include "mg/core/mg_rotation.h"
#include "mg/gfx/mg_joint.h"
#include "mg/utils/mg_optional.h"

#include <glm/vec3.hpp>

namespace Mg {
class Transform;
}

namespace Mg::gfx::Mesh {
struct AnimationChannel;
}

namespace Mg::gfx {

struct SkeletonPose;

/** A skeleton is a posable structure that is used to animate a mesh. It is defined as a tree of
 * Mg::gfx::Mesh::Joint instances.
 */
class Skeleton {
public:
    Skeleton() = default;

    explicit Skeleton(const Identifier id, const size_t num_joints)
        : m_id(id), m_joints(Array<Mesh::Joint>::make(num_joints))
    {}

    Identifier id() const { return m_id; }

    span<Mesh::Joint> joints() { return m_joints; }
    span<const Mesh::Joint> joints() const { return m_joints; }

    Opt<Mesh::JointId> find_joint(Identifier joint_name) const;

    SkeletonPose make_new_pose() const;

    SkeletonPose get_bind_pose() const;

private:
    Identifier m_id;
    Array<Mesh::Joint> m_joints;
};

/** A pose for a given joint, relative to its bind pose. */
struct JointPose {
    Rotation rotation;
    glm::vec3 translation = { 0.0f, 0.0f, 0.0f };
    float scale = 1.0f;
};

/** A pose for all joints in a skeleton. */
struct SkeletonPose {
    Identifier skeleton_id{ "" };
    Array<JointPose> joint_poses;
};

/** Evaluate pose for a given skeleton and write the resulting skinning transformation matrices
 * (model space to world space) to skinning_matrices_out. This can fail if pose is impossible to
 * apply to the given skeleton, and if skinning_matrices_out is too small to fit matrices for all
 * joints.
 *
 * @param transform Model-space-to-world-space transformation. This will be baked into the resulting
 * skinning matrices.
 * @param skeleton The skeleton onto which to apply the pose.
 * @param pose The pose to evaluate.
 * @param skinning_matrices_out The output matrices will be written to this buffer, if evaluation is
 * successful.
 *
 * @return Whether the evaluation was successful.
 */
bool calculate_skinning_matrices(const Transform& transform,
                                 const Skeleton& skeleton,
                                 const SkeletonPose& pose,
                                 span<glm::mat4> skinning_matrices_out);

/** Evaluate pose for a given skeleton and write the resulting joint transformation matrices
 * (joint space to parent-joint space) to matrices_out. This can fail if pose is impossible to apply
 * to the given skeleton, and if matrices_out is too small to fit matrices for all joints.
 *
 * @param skeleton The skeleton onto which to apply the pose.
 * @param pose The pose to evaluate.
 * @param matrices_out The output matrices will be written to this buffer, if evaluation is
 * successful.
 *
 * @return Whether the evaluation was successful.
 */
bool calculate_pose_transformations(const Skeleton& skeleton,
                                    const SkeletonPose& pose,
                                    span<glm::mat4> matrices_out);

void evaluate_joint_pose(const Mesh::AnimationChannel& animation_channel,
                         double time,
                         JointPose& joint_pose_out);

} // namespace Mg::gfx
