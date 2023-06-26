//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2021, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_skeleton.h"

#include "mg/core/mg_log.h"
#include "mg/core/mg_transform.h"
#include "mg/gfx/mg_animation.h"
#include "mg/utils/mg_iteration_utils.h"

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>

namespace Mg::gfx {

namespace {

glm::mat4 pose_to_matrix(const JointPose& pose)
{
    return glm::translate(pose.translation) * pose.rotation.to_matrix() * glm::mat4(pose.scale);
}

JointPose affine_matrix_to_pose(const glm::mat4x4 m)
{
    if (m[0][3] != 0.0f || m[1][3] != 0.0f || m[2][3] != 0.0f || m[3][3] != 1.0f) {
        log.message("affine_matrix_to_pose: matrix contains shearing factor, ignored.");
    }

    auto sqr = [](float f) { return f * f; };
    glm::mat3 rot(m);
    const float scale_x = std::sqrt(sqr(rot[0][0]) + sqr(rot[1][0]) + sqr(rot[2][0]));
    const float scale_y = std::sqrt(sqr(rot[0][1]) + sqr(rot[1][1]) + sqr(rot[2][1]));
    const float scale_z = std::sqrt(sqr(rot[0][2]) + sqr(rot[1][2]) + sqr(rot[2][2]));
    rot[0][0] /= scale_x;
    rot[1][0] /= scale_x;
    rot[2][0] /= scale_x;
    rot[0][1] /= scale_y;
    rot[1][1] /= scale_y;
    rot[2][1] /= scale_y;
    rot[0][2] /= scale_z;
    rot[1][2] /= scale_z;
    rot[2][2] /= scale_z;

    JointPose pose;
    pose.scale = (scale_x + scale_y + scale_z) / 3.0f;
    pose.translation = m[3];
    pose.rotation = Rotation(glm::quat_cast(rot));

    return pose;
}

void calculate_pose_transformations_impl(const glm::mat4& parent_to_model,
                                         const Mesh::JointId current,
                                         const std::span<const Mesh::Joint> joints,
                                         const std::span<const JointPose> joint_poses,
                                         const std::span<glm::mat4> matrices_out)
{
    if (current == Mesh::joint_id_none) {
        return;
    }

    MG_ASSERT_DEBUG(current <= joints.size());
    MG_ASSERT_DEBUG(current <= matrices_out.size());
    MG_ASSERT_DEBUG(current <= joint_poses.size());

    const glm::mat4 joint_to_parent = pose_to_matrix(joint_poses[current]);
    matrices_out[current] = parent_to_model * joint_to_parent;

    for (const Mesh::JointId& child : joints[current].children) {
        calculate_pose_transformations_impl(matrices_out[current],
                                            child,
                                            joints,
                                            joint_poses,
                                            matrices_out);
    }
}

void get_bind_pose_impl(const glm::mat4& inverse_parent_transformation,
                        const Mesh::JointId current,
                        const std::span<const Mesh::Joint> joints,
                        const std::span<JointPose> joint_poses)
{
    if (current == Mesh::joint_id_none) {
        return;
    }

    MG_ASSERT_DEBUG(current <= joints.size());
    MG_ASSERT_DEBUG(current <= joint_poses.size());

    const glm::mat4& model_to_parent_joint = inverse_parent_transformation;
    const glm::mat4 joint_to_model = glm::inverse(joints[current].inverse_bind_matrix);
    const glm::mat4 joint_to_parent_joint = model_to_parent_joint * joint_to_model;
    joint_poses[current] = affine_matrix_to_pose(joint_to_parent_joint);

    for (const Mesh::JointId& child : joints[current].children) {
        get_bind_pose_impl(joints[current].inverse_bind_matrix, child, joints, joint_poses);
    }
}

} // namespace

Opt<Mesh::JointId> Skeleton::find_joint(Identifier joint_name) const
{
    Mesh::JointId id = 0;
    for (const Mesh::Joint& joint : m_joints) {
        if (joint.name == joint_name) {
            return id;
        }
        ++id;
    }

    return nullopt;
}

SkeletonPose Skeleton::make_new_pose() const
{
    SkeletonPose result;
    result.skeleton_id = id();
    result.joint_poses = Array<JointPose>::make(joints().size());
    return result;
}

SkeletonPose Skeleton::get_bind_pose() const
{
    SkeletonPose result;
    result.skeleton_id = id();
    result.joint_poses = Array<JointPose>::make_for_overwrite(m_joints.size());
    get_bind_pose_impl(glm::mat4(1.0f), 0, joints(), result.joint_poses);
    return result;
}

bool calculate_skinning_matrices(const glm::mat4& transform,
                                 const Skeleton& skeleton,
                                 const SkeletonPose& pose,
                                 const std::span<glm::mat4> skinning_matrices_out)
{
    // First, get the joint transformations (joint-space to model-space).
    const bool success = calculate_pose_transformations(skeleton, pose, skinning_matrices_out);
    if (!success) {
        return false;
    }

    const std::span<const Mesh::Joint> joints = skeleton.joints();

    // At this point, skinning_matrices_out contains the accumulated pose (joint space to
    // model space) transformation, not the skinning (model space to model space) transformation.
    // Applying the corresponding inverse bind-pose matrix results in the skinning matrix for
    // each joint. Then we also bake in the transform (model space to world space) to reduce the
    // number of matrix multiplications needed in the shader.
    for (size_t i = 0; i < joints.size(); ++i) {
        skinning_matrices_out[i] = transform * skinning_matrices_out[i] *
                                   joints[i].inverse_bind_matrix;
    }

    return true;
}

bool calculate_pose_transformations(const Skeleton& skeleton,
                                    const SkeletonPose& pose,
                                    const std::span<glm::mat4> matrices_out)
{
    // Sanity checks.
    if (skeleton.id() != pose.skeleton_id) {
        log.warning(
            "calculate_pose_transformations: "
            "attempting to apply pose for skeleton '{}' to skeleton '{}'.",
            pose.skeleton_id.str_view(),
            skeleton.id().str_view());
        return false;
    }

    if (matrices_out.size() < skeleton.joints().size()) {
        log.error(
            "calculate_pose_transformations: "
            "matrices_out (size: {}) too small for skeleton {} (size: {}).",
            matrices_out.size(),
            skeleton.id().str_view(),
            skeleton.joints().size());
        return false;
    }

    if (skeleton.joints().size() < pose.joint_poses.size()) {
        log.error(
            "calculate_pose_transformations (with skeleton {}): "
            "skeleton.joints (size: {}) too small for pose (size: {}).",
            skeleton.id().str_view(),
            skeleton.joints().size(),
            pose.joint_poses.size());
        return false;
    }

    if (skeleton.joints().empty()) {
        return false;
    }

    calculate_pose_transformations_impl(skeleton.root_transform(),
                                        0,
                                        skeleton.joints(),
                                        pose.joint_poses,
                                        matrices_out);
    return true;
}

void animate_joint(const Mesh::AnimationChannel& animation_channel,
                   double time_seconds,
                   JointPose& joint_pose_out)

{
    auto get_keys = [time_seconds](const auto& keys) {
        const bool last_key_index = keys.size() - 1;

        for (size_t i = 0; i < keys.size(); ++i) {
            if (keys[i].time >= time_seconds) {
                const size_t index_a = (i == 0 ? last_key_index : i - 1);
                const size_t index_b = i;

                return std::make_pair(keys[index_a], keys[index_b]);
            }
        }

        return std::make_pair(keys[0], keys[0]);
    };

    auto interpolation_factor = [time_seconds](const auto& key_a, const auto& key_b) {
        return key_a.time == key_b.time
                   ? 0.0f
                   : narrow_cast<float>((time_seconds - key_a.time) / (key_b.time - key_a.time));
    };

    if (!animation_channel.position_keys.empty()) {
        const auto [key_a, key_b] = get_keys(animation_channel.position_keys);
        joint_pose_out.translation =
            glm::mix(key_a.value, key_b.value, interpolation_factor(key_a, key_b));
    }
    else {
        joint_pose_out.translation = glm::vec3(0.0f);
    }

    if (!animation_channel.rotation_keys.empty()) {
        const auto [key_a, key_b] = get_keys(animation_channel.rotation_keys);
        joint_pose_out.rotation = Rotation::mix(Rotation(key_a.value),
                                                Rotation(key_b.value),
                                                interpolation_factor(key_a, key_b));
    }
    else {
        joint_pose_out.rotation = Rotation();
    }

    if (!animation_channel.scale_keys.empty()) {
        const auto [key_a, key_b] = get_keys(animation_channel.scale_keys);
        joint_pose_out.scale =
            glm::mix(key_a.value, key_b.value, interpolation_factor(key_a, key_b));
    }
    else {
        joint_pose_out.scale = 1.0f;
    }
}

void animate_skeleton(const Mesh::AnimationClip& clip, SkeletonPose& pose, double time_seconds)
{
    MG_ASSERT(clip.channels.size() == pose.joint_poses.size());
    time_seconds = std::fmod(time_seconds, clip.duration_seconds);

    for (size_t i = 0; i < pose.joint_poses.size(); ++i) {
        animate_joint(clip.channels[i], time_seconds, pose.joint_poses[i]);
    }
}

void blend_joint_poses(const JointPose& first,
                       const JointPose& second,
                       JointPose& result,
                       double blend_factor)
{
    result.rotation = Rotation::mix(first.rotation, second.rotation, float(blend_factor));
    result.scale = glm::mix(first.scale, second.scale, blend_factor);
    result.translation = glm::mix(first.translation, second.translation, blend_factor);
}

void blend_skeleton_poses(const SkeletonPose& first,
                          const SkeletonPose& second,
                          SkeletonPose& result,
                          const double blend_factor)
{
    MG_ASSERT(first.skeleton_id == second.skeleton_id);
    MG_ASSERT(first.joint_poses.size() == second.joint_poses.size());

    const auto num_joints = first.joint_poses.size();
    for (size_t i = 0; i < num_joints; ++i) {
        blend_joint_poses(first.joint_poses[i],
                          second.joint_poses[i],
                          result.joint_poses[i],
                          blend_factor);
    }
}

} // namespace Mg::gfx
