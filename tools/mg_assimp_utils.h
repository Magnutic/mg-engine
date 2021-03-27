#pragma once

#include <assimp/scene.h>
#include <assimp/types.h>

#include <cassert>
#include <string>
#include <string_view>

namespace Mg
{

template<typename F> void for_each_child(const aiNode& node, F&& function)
{
    for (uint32_t i = 0; i < node.mNumChildren; ++i)
    {
        function(*node.mChildren[i]);
    }
}

template<typename F> void for_each_mesh(const aiScene& scene, const aiNode& node, F&& function)
{
    for (uint32_t i = 0; i < node.mNumMeshes; ++i)
    {
        const auto meshIndex = node.mMeshes[i];
        assert(meshIndex < scene.mNumMeshes);
        function(*scene.mMeshes[meshIndex]);
    }
}

template<typename F> void for_each_bone(const aiMesh& mesh, F&& function)
{
    for (uint32_t i = 0; i < mesh.mNumBones; ++i)
    {
        function(*mesh.mBones[i]);
    }
}

template<typename F> void for_each_face(const aiMesh& mesh, F&& function)
{
    for (uint32_t i = 0; i < mesh.mNumFaces; ++i)
    {
        function(mesh.mFaces[i]);
    }
}

template<typename F> void for_each_index(const aiFace& face, F&& function)
{
    for (uint32_t i = 0; i < face.mNumIndices; ++i)
    {
        function(face.mIndices[i]);
    }
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
