#pragma once

#include <assimp/scene.h>
#include <assimp/types.h>

#include <cassert>
#include <string>
#include <string_view>

namespace Mg {

template<typename F> void for_each_child(const aiNode& node, F&& function)
{
    for (uint32_t i = 0; i < node.mNumChildren; ++i) {
        function(*node.mChildren[i]); // NOLINT
    }                                 // NOLINT
}

template<typename F> void for_each_mesh(const aiScene& scene, const aiNode& node, F&& function)
{
    for (uint32_t i = 0; i < node.mNumMeshes; ++i) {
        const auto meshIndex = node.mMeshes[i]; // NOLINT
        assert(meshIndex < scene.mNumMeshes);
        function(*scene.mMeshes[meshIndex]); // NOLINT
    }
}

template<typename F> void for_each_bone(const aiMesh& mesh, F&& function)
{
    for (uint32_t i = 0; i < mesh.mNumBones; ++i) {
        function(*mesh.mBones[i]); // NOLINT
    }
}

template<typename F> void for_each_face(const aiMesh& mesh, F&& function)
{
    for (uint32_t i = 0; i < mesh.mNumFaces; ++i) {
        function(mesh.mFaces[i]); // NOLINT
    }
}

template<typename F> void for_each_index(const aiFace& face, F&& function)
{
    for (uint32_t i = 0; i < face.mNumIndices; ++i) {
        function(face.mIndices[i]); // NOLINT
    }
}

template<typename F> void for_each_animation(const aiScene& scene, F&& function)
{
    for (uint32_t i = 0; i < scene.mNumAnimations; ++i) {
        function(*scene.mAnimations[i]); // NOLINT
    }
}

template<typename F> void for_each_channel(const aiAnimation& animation, F&& function)
{
    for (uint32_t i = 0; i < animation.mNumChannels; ++i) {
        function(*animation.mChannels[i]); // NOLINT
    }
}

template<typename F, typename KeyT>
void for_each_key(const uint32_t num_keys, const KeyT* key_array, F&& function)
{
    for (uint32_t i = 0; i < num_keys; ++i) {
        function(key_array[i]); // NOLINT
    }
}

template<typename F> void for_each_position_key(const aiNodeAnim& channel, F&& function)
{
    for_each_key(channel.mNumPositionKeys, channel.mPositionKeys, function);
}

template<typename F> void for_each_rotation_key(const aiNodeAnim& channel, F&& function)
{
    for_each_key(channel.mNumRotationKeys, channel.mRotationKeys, function);
}

template<typename F> void for_each_scaling_key(const aiNodeAnim& channel, F&& function)
{
    for_each_key(channel.mNumScalingKeys, channel.mScalingKeys, function);
}

inline std::string to_string(const aiString& str)
{
    return { str.C_Str(), str.length };
}
inline std::string_view to_string_view(const aiString& str)
{
    return { str.C_Str(), str.length };
}

} // namespace Mg
