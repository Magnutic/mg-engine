#include "mg_mesh_converter.h"

#include "../shared/mg_file_writer.h"
#include "mg_assimp_utils.h"

#include <mg/resources/mg_mesh_resource_data.h>
#include <mg/utils/mg_assert.h>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <assimp/DefaultLogger.hpp>
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <iostream>
#include <iterator>
#include <map>
#include <numeric>
#include <sstream>
#include <type_traits>

// Note: there are a lot of 'NOLINT' in this file, to suppress warnings about using pointer
// arithmetic. It is very difficult to use AssImp without pointer arithmetic, so I chose to suppress
// those warnings.

// TODO: There is still something wrong about transformations for skinned models, I know it.
// With input GLTF files that are supposed to all face +Z, I get some facing -Y and some facing +X.
// I would expect them to face +Y, so I must be missing some transform I am supposed to apply.
//
// Hypothesis: I am probably not including the right set of nodes in the joint hierarchy. For
// skinned models, I should include those node subtrees that are siblings of or children of the
// mesh, but nothing else. The effect I am seeing might be that the root nodes transform such that
// the mesh will be correctly oriented if rendered without skinning, whereas those transformations
// should be ignored for correct results with skinning. Then again, this is just speculation.

namespace Mg {

using namespace Mg::gfx;
using namespace Mg::MeshResourceData;
using namespace std::literals;
using glm::vec1, glm::vec2, glm::vec3, glm::vec4, glm::mat4, glm::quat;

namespace {

constexpr float k_scaling_factor = 1.00f; // TODO configurable or deduced somehow?

constexpr mat4 rotate_z_180({ -1, 0, 0, 0 }, { 0, -1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 });

constexpr mat4 y_up_to_z_up({ -1, 0, 0, 0 }, { 0, 0, 1, 0 }, { 0, 1, 0, 0 }, { 0, 0, 0, 1 });

// Converts from AssImp's Y-up coordinate system to Mg's Z-up coordinate system and applies scaling
// factor.
constexpr mat4 to_mg_space({ -k_scaling_factor, 0, 0, 0 },
                           { 0, 0, k_scaling_factor, 0 },
                           { 0, k_scaling_factor, 0, 0 },
                           { 0, 0, 0, 1 });

const mat4 from_mg_space = glm::inverse(to_mg_space);

inline mat4 convert_matrix(const aiMatrix4x4& aiMat)
{
    mat4 result;
    for (uint32_t i = 0; i < 4; ++i) {
        for (uint32_t u = 0; u < 4; ++u) {
            result[narrow_cast<int>(i)][narrow_cast<int>(u)] = aiMat[u][i]; // NOLINT
        }
    }
    return to_mg_space * result * from_mg_space;
}

inline vec3 convert_vector(const aiVector3D& ai_vector)
{
    return { -ai_vector.x * k_scaling_factor,
             ai_vector.z * k_scaling_factor,
             ai_vector.y * k_scaling_factor };
}

// TODO verify, very unsure about this
inline quat convert_quaternion(const aiQuaternion& quaternion)
{
    return { quaternion.w, -quaternion.x, quaternion.z, quaternion.y };
}

template<typename... Ts> void notify(const Ts&... what)
{
    (std::cout << ... << what); // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    std::cout << '\n';
}

template<typename... Ts> void warn(const Ts&... what)
{
    std::cerr << "Warning: ";
    (std::cerr << ... << what); // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    std::cerr << '\n';
}

template<typename... Ts> void error(const Ts&... what)
{
    std::cerr << "Error: ";
    (std::cerr << ... << what); // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    std::cerr << '\n';
}

//--------------------------------------------------------------------------------------------------

// Stores all the strings that are to be written to file.
class StringData {
public:
    StringData() { m_strings.reserve(1024); }

    // Add a string to be written to the file and return a StringRange referring to its position in
    // the file.
    StringRange store(const std::string_view& string);

    // Add a string to be written to the file and return a StringRange referring to its position in
    // the file.
    StringRange store(const aiString& string);

    // Get the string that was stored at the given range.
    std::string get(const StringRange& range) const
    {
        return m_strings.substr(range.begin, range.length);
    }

    // Get a string_view to the string that was stored at the given range.
    std::string_view get_view(const StringRange& range) const
    {
        return std::string_view(m_strings).substr(range.begin, range.length);
    }

    // All the strings that are to be written into the file. Null-terminated.
    // Since it can contain multiple strings, there may be multiple null characters.
    const std::string& all_strings() const { return m_strings; }

private:
    std::string m_strings;
};

StringRange StringData::store(const std::string_view& string)
{
    const auto begin = uint32_t(m_strings.size());
    const auto length = uint32_t(string.size());
    m_strings += string;
    m_strings += '\0';
    return StringRange{ begin, length };
}

StringRange StringData::store(const aiString& string)
{
    return store(to_string_view(string));
}

//--------------------------------------------------------------------------------------------------

// All joints needed to represent skeletal animation of the mesh.
class JointData {
public:
    // Construct joint data from scene.
    explicit JointData(const aiScene& scene, StringData& string_data);

    // Get id for joint corresponding to bone.
    JointId get_joint_id(const aiBone& bone) const;

    // Get the name of the joint given by id.
    std::string_view get_joint_name(JointId id) const
    {
        return m_string_data->get_view(m_joints.at(id).name);
    }

    // Get the joint given by id.
    const Joint& get_joint(JointId id) const { return m_joints.at(id); }

    // Find JointId given joint name. If no such joint exists, returns joint_id_none.
    JointId find_joint(const std::string_view name) const
    {
        JointId id = 0;
        for (const Joint& joint : m_joints) {
            if (m_string_data->get(joint.name) == name) {
                return id;
            }
            ++id;
        }
        return joint_id_none;
    }

    // Get all joints.
    span<const Joint> joints() const { return m_joints; }

    // Transformation of the root node of the skeleton. This contains the accumulated
    // transformations of the scene nodes that are parent to the skeleton, but are not include as
    // joints in the skeleton.
    const mat4& skeleton_root_transform() const { return m_skeleton_root_transform; }

private:
    // Add a joint that is directly connected to vertices in the mesh.
    JointId add_joint(const aiBone& bone);

    // Add a joint that itself has no connection to any vertices, but which may have such joints as
    // children.
    JointId add_dummy_joint(std::string_view name);

    std::vector<Joint> m_joints;
    std::map<const aiBone*, JointId> m_joint_id_for_bone;
    StringData* m_string_data = nullptr;

    // Rotating by 180 degrees around the Z-axis seems to make most skinned meshes face the intended
    // direction. I am not sure why. TODO figure out and fix this.
    mat4 m_skeleton_root_transform = rotate_z_180 * y_up_to_z_up;
};

JointId JointData::add_joint(const aiBone& bone)
{
    if (m_joints.size() >= joint_id_none) {
        throw std::runtime_error("Too many joints in model.");
    }

    // Get id (index) for joint and add it to map, so we can look up.
    const auto joint_id = JointId(m_joints.size());
    m_joint_id_for_bone[&bone] = joint_id;

    // Create the joint, using bone data if available.
    Joint& joint = m_joints.emplace_back();
    joint.name = m_string_data->store(to_string_view(bone.mName));
    joint.inverse_bind_matrix = convert_matrix(bone.mOffsetMatrix);
    joint.children.fill(joint_id_none);

    return joint_id;
}

JointId JointData::add_dummy_joint(std::string_view name)
{
    if (m_joints.size() >= joint_id_none) {
        throw std::runtime_error("Too many joints in model.");
    }

    const auto joint_id = JointId(m_joints.size());

    Joint& joint = m_joints.emplace_back();
    joint.name = m_string_data->store(name);
    joint.inverse_bind_matrix =
        mat4(1.0); // Will not be used, as dummy joints are not connected to any vertices.
    joint.children.fill(joint_id_none);

    return joint_id;
}

JointId JointData::get_joint_id(const aiBone& bone) const
{
    const auto jointIndexIt = m_joint_id_for_bone.find(&bone);
    if (jointIndexIt == m_joint_id_for_bone.end()) {
        throw std::logic_error("No joint found for bone "s + to_string(bone.mName));
    }
    return jointIndexIt->second;
}

// A map from AssImp nodes to their corresponding bones (nullptr if the node does not correspond to
// a bone).
using NodeBoneMap = std::map<const aiNode*, const aiBone*>;

// Get all nodes corresponding to a bone in the scene.
void collect_assimp_bone_nodes(const aiScene& scene, const aiNode& node, NodeBoneMap& out)
{
    for_each_mesh(scene, node, [&](const aiMesh& mesh) {
        for_each_bone(mesh, [&](const aiBone& bone) {
            const aiNode* boneNode = scene.mRootNode->FindNode(bone.mName);
            out.insert({ boneNode, &bone });
        });
    });

    for_each_child(node,
                   [&](const aiNode& child) { collect_assimp_bone_nodes(scene, child, out); });
}

JointData::JointData(const aiScene& scene, StringData& string_data) : m_string_data(&string_data)
{
    // There are a few steps to gathering the joints:
    // The AssImp importer library will define a hierarchy of nodes, each of which can be
    // transformed by an animation. Each node that directly affects a vertex will also have a bone,
    // which is just a reference to a node in the hierarchy, and some metadata. But gathering just
    // the bones is not sufficient, since bone nodes might be children of other nodes that
    // themselves do not correspond to any bones. They are still needed for the transformation
    // hierarchy, though, so we must include the whole node hierarchy except for those subtrees that
    // do not contain any bones.

    // Get all nodes that are directly used as joints.
    NodeBoneMap joint_nodes;
    collect_assimp_bone_nodes(scene, *scene.mRootNode, joint_nodes);

    // Recursively include parents of all known joint nodes to complete the hierarchy.
    {
        NodeBoneMap new_joint_nodes;

        for (const auto& [node, bone] : joint_nodes) {
            new_joint_nodes.insert({ node, bone });

            const aiNode* current = node->mParent;
            while (current != nullptr) {
                new_joint_nodes.insert({ current, nullptr });
                current = current->mParent;
            }
        }

        std::swap(joint_nodes, new_joint_nodes);
    }

    // Build final joints list by recursively visiting the hierarchy and copying out information
    // from all subtrees that contain bones. The lambda takes itself as parameter to allow
    // recursion, since lambdas cannot ordinarily be recursive.
    auto create_joint_hierarchy = [&](const aiNode& node, auto&& recurse) -> JointId {
        std::vector<const aiNode*> subtrees_containing_bones;

        for_each_child(node, [&](const aiNode& child) {
            const auto it = joint_nodes.find(&child);
            if (it != joint_nodes.end()) {
                subtrees_containing_bones.push_back(it->first);
            }
        });

        const auto bone_it = joint_nodes.find(&node);
        const aiBone* maybe_bone = bone_it != joint_nodes.end() ? bone_it->second : nullptr;

        const JointId joint_id = maybe_bone ? add_joint(*maybe_bone)
                                            : add_dummy_joint(to_string_view(node.mName));

        JointId current_parent = joint_id;
        size_t insert_index = 0;

        for (const aiNode* child : subtrees_containing_bones) {
            const bool no_space_is_left = insert_index + 1 == max_num_children_per_joint;
            if (no_space_is_left) {
                // No space left in joint, add a dummy joint to fit in the rest.
                const std::string parent_name(get_joint_name(current_parent));
                const JointId dummy_joint_id = add_dummy_joint(parent_name + "_ext");
                m_joints.at(current_parent).children[insert_index] = dummy_joint_id;
                current_parent = dummy_joint_id;
                insert_index = 0;
            }

            m_joints.at(current_parent).children.at(insert_index++) = recurse(*child, recurse);
        }

        return joint_id;
    };

    const auto root_joint_id = create_joint_hierarchy(*scene.mRootNode, create_joint_hierarchy);

    MG_ASSERT(root_joint_id == 0);
}

//--------------------------------------------------------------------------------------------------

class AnimationData {
public:
    using PositionChannel = std::vector<PositionKey>;
    using RotationChannel = std::vector<RotationKey>;
    using ScaleChannel = std::vector<ScaleKey>;

    struct Clip {
        StringRange name;
        double duration_seconds{};

        // The channels are indexed by JointId.
        std::vector<PositionChannel> position_channels;
        std::vector<RotationChannel> rotation_channels;
        std::vector<ScaleChannel> scale_channels;
    };

    explicit AnimationData(const aiScene& scene,
                           const JointData& joint_data,
                           StringData& string_data,
                           bool is_gltf2);

    span<const Clip> clips() const { return m_clips; }

private:
    void add_animation_clip(const JointData& joint_data,
                            StringData& string_data,
                            const aiAnimation& ai_animation,
                            bool is_gltf2);

    std::vector<Clip> m_clips;
};

AnimationData::AnimationData(const aiScene& scene,
                             const JointData& joint_data,
                             StringData& string_data,
                             const bool is_gltf2)
{
    m_clips.reserve(scene.mNumAnimations);

    for_each_animation(scene, [&](const aiAnimation& ai_animation) {
        add_animation_clip(joint_data, string_data, ai_animation, is_gltf2);
    });
}

void AnimationData::add_animation_clip(const JointData& joint_data,
                                       StringData& string_data,
                                       const aiAnimation& ai_animation,
                                       const bool is_gltf2)
{
    const std::string_view animation_name = [&] {
        const auto name = to_string_view(ai_animation.mName);
        return name.empty() ? "unnamed animation" : name;
    }();

    auto log_warning = [&animation_name](const auto&... what) {
        warn("in animation clip '", animation_name, "': ", what...);
    };

    const double ticks_per_second =
        is_gltf2 ? 1000.0 // Workaround for bug in AssImp. GLTF2 tick rate is always 1000.
                 : (ai_animation.mTicksPerSecond > 0.0 ? ai_animation.mTicksPerSecond : 30.0);

    if (ai_animation.mTicksPerSecond <= 0.0) {
        log_warning("Unknown tick rate. Assuming tick rate is ", ticks_per_second, ".");
    }

    auto ticks_to_seconds = [ticks_per_second](const double ticks) {
        return ticks / ticks_per_second;
    };

    Clip& clip = m_clips.emplace_back();
    clip.duration_seconds = ticks_to_seconds(ai_animation.mDuration);
    clip.name = string_data.store(animation_name);
    clip.position_channels.resize(joint_data.joints().size());
    clip.rotation_channels.resize(joint_data.joints().size());
    clip.scale_channels.resize(joint_data.joints().size());

    notify("Processing animation clip '", animation_name, "'. Duration: ", clip.duration_seconds);

    if (ai_animation.mNumMeshChannels > 0) {
        log_warning("clip contains mesh channels, which are currently unsupported.");
    }

    if (ai_animation.mNumMorphMeshChannels > 0) {
        log_warning("clip contains morph channels, which are currently unsupported.");
    }

    for_each_channel(ai_animation, [&](const aiNodeAnim& channel) {
        const JointId joint_id = joint_data.find_joint(to_string_view(channel.mNodeName));

        if (joint_id == joint_id_none) {
            log_warning("clip refers to joint ",
                        to_string_view(channel.mNodeName),
                        ", which was not found in the file.");

            return;
        }

        PositionChannel& position_channel = clip.position_channels[joint_id];
        RotationChannel& rotation_channel = clip.rotation_channels[joint_id];
        ScaleChannel& scale_channel = clip.scale_channels[joint_id];

        for_each_rotation_key(channel, [&](const aiQuatKey& ai_key) {
            RotationKey& key = rotation_channel.emplace_back();
            key.time = ticks_to_seconds(ai_key.mTime);
            key.value = convert_quaternion(ai_key.mValue);
        });

        for_each_position_key(channel, [&](const aiVectorKey& ai_key) {
            PositionKey& key = position_channel.emplace_back();
            key.time = ticks_to_seconds(ai_key.mTime);
            key.value = convert_vector(ai_key.mValue);
        });

        for_each_scaling_key(channel, [&](const aiVectorKey& ai_key) {
            ScaleKey& key = scale_channel.emplace_back();
            key.time = ticks_to_seconds(ai_key.mTime);
            key.value = (ai_key.mValue.x + ai_key.mValue.y + ai_key.mValue.z) / 3.0f;
            // TODO or maybe max of scale components? Or warn if not all equal? Or just
            // implement support for vector scales?
        });
    });
}

//--------------------------------------------------------------------------------------------------

// Data for the mesh itself: the vertices, indices, and submeshes.
class MeshData {
public:
    explicit MeshData(const aiScene& scene, const JointData* joint_data, StringData& string_data)
        : m_scene(&scene), m_joint_data(joint_data), m_string_data(&string_data)
    {
        visit(*scene.mRootNode);
    }

    span<const Submesh> submeshes() const { return m_submeshes; }
    span<const Vertex> vertices() const { return m_vertices; }
    span<const Index> indices() const { return m_indices; }
    span<const Influences> influences() const { return m_influences; }

private:
    void visit(const aiMesh& mesh);
    void visit(const aiNode& node);

    void add_vertex(const aiMesh& mesh, uint32_t index);

    std::vector<Submesh> m_submeshes;
    std::vector<Vertex> m_vertices;
    std::vector<Index> m_indices;
    std::vector<Influences> m_influences;

    const aiScene* m_scene = nullptr;
    const JointData* m_joint_data = nullptr;
    StringData* m_string_data = nullptr;
};

// If any influences have weight less than the given weight, return the one index of the one with
// the smallest weight.
std::optional<size_t> get_influence_index_to_use(const Influences& bindings, const float weight)
{
    const auto normalized_weight = normalize<JointWeights::value_type>(weight);
    auto it = std::min_element(bindings.weights.begin(), bindings.weights.end());
    if (*it < normalized_weight) {
        return size_t(std::distance(bindings.weights.begin(), it));
    }
    return std::nullopt;
}

void MeshData::visit(const aiMesh& mesh)
{
    auto log_error = [&](auto&&... what) {
        error("Mesh \'", to_string_view(mesh.mName), "\': ", what...);
    };

    // Determine whether mesh has all data we need
    const bool hasUv0 = mesh.GetNumUVChannels() > 0;
    const bool hasNormals = mesh.HasNormals();
    const bool hasJoints = mesh.HasBones() && m_joint_data != nullptr;

    const size_t submesh_begin = m_indices.size();

    if (!hasUv0 || !hasNormals) {
        std::string reason;
        if (!hasUv0) {
            reason += "texture coordinates"sv;
        }
        if (!hasNormals) {
            reason += (reason.empty() ? ""s : ", "s) + "normals"s;
        }

        log_error("Missing data: ", reason, ".");
    }

    // Copy all relevant index data
    const auto vertices_begin = uint32_t(m_vertices.size());
    m_vertices.reserve(vertices_begin + mesh.mNumVertices);
    m_indices.reserve(m_indices.size() + 3 * mesh.mNumFaces);

    for_each_face(mesh, [&](const aiFace& face) {
        MG_ASSERT(face.mNumIndices == 3);
        for_each_index(face, [&](const uint32_t index) {
            const auto max_index = std::numeric_limits<Index>::max();
            if (index > max_index) {
                log_error("Vertex index out of bounds (limit: ", max_index, ", was: ", index, ").");
                return;
            }
            m_indices.push_back(narrow_cast<Index>(index));
        });
    });

    // Copy all relevant vertex data, Y-up to Z-up and mirrored X
    for (uint32_t i = 0; i < mesh.mNumVertices; i++) {
        add_vertex(mesh, i);
    }

    // If the mesh contains joint info, also prepare a joint binding for each vertex.
    if (hasJoints) {
        m_influences.resize(m_vertices.size());

        for_each_bone(mesh, [&](const aiBone& bone) {
            for (uint32_t wi = 0; wi < bone.mNumWeights; ++wi) {
                const uint32_t vertex_index = vertices_begin +
                                              bone.mWeights[wi].mVertexId; // NOLINT
                if (vertex_index >= m_vertices.size()) {
                    log_error("Joint weight vertex id out of range in joint: ",
                              to_string(bone.mName));
                    continue;
                }

                const float weight = bone.mWeights[wi].mWeight; // NOLINT
                Influences& influences = m_influences[vertex_index];

                if (std::optional<size_t> index = get_influence_index_to_use(influences, weight);
                    index.has_value()) {
                    influences.ids[index.value()] = m_joint_data->get_joint_id(bone);
                    influences.weights[index.value()] = normalize<JointWeights::value_type>(weight);
                }
            }
        });
    }

    // Normalize influence weights, such that each vertex's weights sum to 1.0.
    for (Influences& influences : m_influences) {
        float total_weight = 0.0f;
        for (const auto weight : influences.weights) {
            total_weight += denormalize(weight);
        }
        if (total_weight <= 0.0f) {
            continue;
        }

        for (auto& weight : influences.weights) {
            weight = normalize<JointWeights::value_type>(denormalize(weight) / total_weight);
        }
    }

    Submesh& submesh = m_submeshes.emplace_back();
    submesh.name = m_string_data->store(mesh.mName);
    submesh.material =
        m_string_data->store(m_scene->mMaterials[mesh.mMaterialIndex]->GetName()); // NOLINT
    submesh.begin = uint16_t(submesh_begin);
    submesh.num_indices = uint32_t(3 * mesh.mNumFaces);
}

// TODO: if the converter needs to be optimized, this function seems like a low-hanging fruit.
void MeshData::add_vertex(const aiMesh& mesh, const uint32_t index)
{
    Vertex& vertex = m_vertices.emplace_back();
    vertex.position = convert_vector(mesh.mVertices[index]); // NOLINT

    if (mesh.HasTextureCoords(0)) {
        vertex.tex_coord.x = mesh.mTextureCoords[0][index].x;        // NOLINT
        vertex.tex_coord.y = 1.0f - mesh.mTextureCoords[0][index].y; // NOLINT
    }

    if (mesh.HasNormals()) {
        vertex.normal = convert_vector(mesh.mNormals[index]); // NOLINT

        if (mesh.HasTangentsAndBitangents()) {
            vertex.tangent = convert_vector(mesh.mTangents[index]);      // NOLINT
            vertex.bitangent = -convert_vector(mesh.mBitangents[index]); // Note: inverted. //NOLINT
        }
    }
}

void MeshData::visit(const aiNode& node)
{
    for_each_mesh(*m_scene, node, [&](const aiMesh& mesh) { visit(mesh); });
    for_each_child(node, [&](const aiNode& child) { visit(child); });
}

//--------------------------------------------------------------------------------------------------

namespace logging {
void print_heading(const std::string& text)
{
    std::cout << "--------------------\n" << text << "\n--------------------\n";
}

template<typename... Ts> void print(const size_t indent, const Ts&... what)
{
    static constexpr std::string_view k_indent = "  ";
    for (size_t i = 0; i < indent; ++i) {
        std::cout << k_indent;
    }
    notify(what...);
}

void dump_mesh(const aiMesh& mesh, const size_t indent)
{
    print(indent, to_string_view(mesh.mName), " {");
    print(indent + 1, "numVertices: ", mesh.mNumVertices);
    print(indent + 1, "numFaces: ", mesh.mNumFaces);
    if (mesh.HasBones()) {
        print(indent + 1, "Joints {");
        for_each_bone(mesh,
                      [&](const aiBone& bone) { print(indent + 2, to_string_view(bone.mName)); });
        print(indent + 1, '}');
    }
    print(indent, '}');
}

void dump_node_data(const aiScene& scene, const aiNode& node, const size_t indent)
{
    if (node.mNumMeshes == 0) {
        return;
    }

    print(indent, "Meshes {");
    for_each_mesh(scene, node, [&](const aiMesh& mesh) { dump_mesh(mesh, indent + 1); });
    print(indent, '}');
}

void dump_node_tree(const aiScene& scene, const aiNode& node, const size_t indent = 0)
{
    print(indent, to_string_view(node.mName), " {");

    dump_node_data(scene, node, indent + 1);
    for_each_child(node, [&](const aiNode& child) { dump_node_tree(scene, child, indent + 1); });

    print(indent, '}');
}

void dump_scene(const aiScene& scene)
{
    print_heading("Input node tree");
    dump_node_tree(scene, *scene.mRootNode);
}

void dump_joints(const JointData& joint_data)
{
    print_heading("Joints in linear layout");
    {
        const auto& joints = joint_data.joints();
        for (const Joint& joint : joints) {
            const auto joint_id = as<JointId>(std::distance(joints.data(), &joint));
            const bool has_inverse_bind_matrix = joint.inverse_bind_matrix != mat4(1.0f);
            print(0, '[', std::to_string(joint_id), "] ", joint_data.get_joint_name(joint_id), ':');
            print(0, "\tHas inverse_bind_matrix: ", has_inverse_bind_matrix ? "true" : "false");
            std::cout << "\tChildren: ";
            for (JointId child_id : joint.children) {
                if (child_id == joint_id_none) {
                    continue;
                }

                std::cout << std::to_string(child_id) << "  ";
            }
            std::cout << "\n";
        }
    }

    print_heading("Joints as hierarchy");
    {
        auto print_hierarchy = [&](JointId id, size_t indent_level, auto&& recurse) -> void {
            const Joint& joint = joint_data.get_joint(id);
            print(indent_level,
                  joint_data.get_joint_name(id),
                  joint.inverse_bind_matrix == glm::mat4(1.0f) ? "[dummy]" : "");

            for (JointId child_id : joint.children) {
                if (child_id != joint_id_none) {
                    recurse(child_id, indent_level + 1, recurse);
                }
            }
        };
        print_hierarchy(0, 0, print_hierarchy);
    }
}
} // namespace logging

const aiScene* load_file(Assimp::Importer& importer, const std::filesystem::path& file_path)
{
    Assimp::DefaultLogger::create(nullptr, Assimp::Logger::NORMAL, aiDefaultLogStream_STDERR);

    // Ignore components that we will not use.
    static const int32_t ai_settings = aiComponent_COLORS | aiComponent_LIGHTS |
                                       aiComponent_CAMERAS;

    // Ignore primitive types that we will not use.
    static const int32_t ai_exclude = aiPrimitiveType_POINT | aiPrimitiveType_LINE;

    importer.SetPropertyInteger(AI_CONFIG_PP_SLM_VERTEX_LIMIT, UINT16_MAX);
    importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS, ai_settings);
    importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, ai_exclude);
    importer.SetPropertyFloat(AI_CONFIG_PP_CT_MAX_SMOOTHING_ANGLE, 90.0f);

    static const unsigned int common_flags =
        aiProcess_ValidateDataStructure | aiProcess_RemoveRedundantMaterials |
        aiProcess_CalcTangentSpace | aiProcess_FindDegenerates | aiProcess_FindInvalidData |
        aiProcess_FindInstances | aiProcess_ImproveCacheLocality | aiProcess_JoinIdenticalVertices |
        aiProcess_OptimizeMeshes | aiProcess_RemoveComponent | aiProcess_SortByPType |
        aiProcess_SplitLargeMeshes | aiProcess_GenUVCoords | aiProcess_GenSmoothNormals |
        aiProcess_Triangulate;

    static const unsigned int skinned_model_flags = common_flags | aiProcess_LimitBoneWeights |
                                                    aiProcess_OptimizeGraph;

    static const unsigned int static_model_flags = common_flags | aiProcess_PreTransformVertices;

    // Use AssImp to load a scene from file.
    const aiScene* scene = nullptr;

    // First, load with settings for animated meshes.
    scene = importer.ReadFile(file_path, skinned_model_flags);
    if (!scene) {
        error("importer error: ", importer.GetErrorString());
        return nullptr;
    }

    // If the scene has no animations, re-load it with settings for static mesh instead.
    if (scene->mNumAnimations == 0) {
        notify("Scene has no animations; re-loading as static model.");
        importer.FreeScene();
        scene = importer.ReadFile(file_path, static_model_flags);
    }

    notify("Loaded file '", file_path.u8string(), "'.");
    return scene;
}

bool write_file(const std::filesystem::path& file_path,
                const MeshData& mesh_data,
                const Opt<JointData>& joint_data,
                const Opt<AnimationData>& animation_data,
                const StringData& string_data)
{
    FileWriter writer;

    Header header = {};
    header.four_cc = fourcc;
    header.version = version;

    {
        const BoundingSphere bounding_sphere = calculate_mesh_bounding_sphere(mesh_data.vertices());
        header.centre = bounding_sphere.centre;
        header.radius = bounding_sphere.radius;
    }

    header.skeleton_root_transform = joint_data.map_or(&JointData::skeleton_root_transform,
                                                       mat4(1.0f));

    // Note: everything that is enqueued in the writer is referenced by address until the writer has
    // finished writing, so be sure that everything is created in the correct scope and that vectors
    // etc. will not be reallocated.

    writer.enqueue(header);
    header.submeshes = writer.enqueue_array(mesh_data.submeshes());
    header.vertices = writer.enqueue_array(mesh_data.vertices());
    header.indices = writer.enqueue_array(mesh_data.indices());
    header.influences = writer.enqueue_array(mesh_data.influences());

    // Must be defined in this scope, see above note.
    std::vector<AnimationClip> animation_clips;
    std::vector<std::vector<AnimationChannel>> channels_per_clip;

    if (joint_data && animation_data) {
        header.joints = writer.enqueue_array(joint_data->joints());

        animation_clips.resize(animation_data->clips().size());
        header.animations = writer.enqueue_array<AnimationClip>(animation_clips);

        channels_per_clip.resize(animation_data->clips().size());

        for (size_t clip_index = 0; clip_index < animation_data->clips().size(); ++clip_index) {
            const size_t num_channels = joint_data->joints().size();
            const auto& position_channels = animation_data->clips()[clip_index].position_channels;
            const auto& rotation_channels = animation_data->clips()[clip_index].rotation_channels;
            const auto& scale_channels = animation_data->clips()[clip_index].scale_channels;

            MG_ASSERT(position_channels.size() == num_channels);
            MG_ASSERT(rotation_channels.size() == num_channels);
            MG_ASSERT(scale_channels.size() == num_channels);

            channels_per_clip[clip_index].resize(num_channels);
            const span<AnimationChannel> channels = channels_per_clip[clip_index];

            AnimationClip& animation_clip = animation_clips[clip_index];
            animation_clip.name = animation_data->clips()[clip_index].name;
            animation_clip.duration = animation_data->clips()[clip_index].duration_seconds;
            animation_clip.channels = writer.enqueue_array(channels);

            for (size_t i = 0; i < position_channels.size(); ++i) {
                channels[i].position_keys = writer.enqueue_array(span(position_channels[i]));
            }

            for (size_t i = 0; i < rotation_channels.size(); ++i) {
                channels[i].rotation_keys = writer.enqueue_array(span(rotation_channels[i]));
            }

            for (size_t i = 0; i < scale_channels.size(); ++i) {
                channels[i].scale_keys = writer.enqueue_array(span(scale_channels[i]));
            }
        }
    }

    header.strings = writer.enqueue_string(string_data.all_strings());

    const bool success = writer.write(file_path);
    if (success) {
        notify("Wrote file '", file_path.u8string(), "'.");
    }

    return success;
}

} // end anonymous namespace

bool convert_mesh(const std::filesystem::path& path_in,
                  const std::filesystem::path& path_out,
                  const bool debug_logging)
{
    const bool is_gltf = path_in.extension() == ".glb" || path_in.extension() == ".gltf";

    try {
        auto importer = std::make_unique<Assimp::Importer>();

        const aiScene* scene = load_file(*importer, path_in);
        if (!scene) {
            return false;
        }

        if (debug_logging) {
            logging::dump_scene(*scene);
        }

        // Process imported data.
        StringData string_data;
        Opt<JointData> joint_data;
        Opt<AnimationData> animation_data;

        if (scene->mNumAnimations > 0) {
            joint_data.emplace(*scene, string_data);

            if (debug_logging) {
                logging::dump_joints(*joint_data);
            }

            animation_data.emplace(*scene, *joint_data, string_data, is_gltf);
        }

        MeshData mesh_data(*scene, (joint_data ? &joint_data.value() : nullptr), string_data);

        // Release imported data now.
        importer.reset();

        // Write processed data.
        return write_file(path_out, mesh_data, joint_data, animation_data, string_data);
    }
    catch (const std::exception& e) {
        error("Failed to process '", path_in.u8string(), "': ", e.what());
        return false;
    }
}

} // namespace Mg
