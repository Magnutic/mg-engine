#include "mg_mesh_converter.h"

#include "mg_assimp_utils.h"
#include "mg_file_writer.h"
#include "mg_mesh_definitions.h"

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/transform.hpp>

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

namespace Mg {
namespace {
using namespace std::literals;
using glm::vec1, glm::vec2, glm::vec3, glm::vec4, glm::mat4;

mat4 to_glm_matrix(const aiMatrix4x4& aiMat)
{
    mat4 result;
    for (size_t i = 0; i < 4; ++i) {
        for (size_t u = 0; u < 4; ++u) {
            result[i][u] = aiMat[i][u];
        }
    }
    return result;
}

//------------------------------------------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------------------------------------------

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

    // Get all joints.
    const std::vector<Joint>& joints() const { return m_joints; }

private:
    // Add a joint that is directly connected to vertices in the mesh.
    JointId add_joint(const aiBone& bone);

    // Add a joint that itself has no connection to any vertices, but which may have such joints as
    // children.
    JointId add_dummy_joint(std::string_view name);

    std::vector<Joint> m_joints;
    std::map<const aiBone*, JointId> m_joint_id_for_bone;
    StringData* m_string_data = nullptr;
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
    joint.inverse_bind_matrix = to_glm_matrix(bone.mOffsetMatrix);
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
    // from all subtrees that contain bones. Takes itself as parameter to allow recursion.
    auto create_joint_hierarchy =
        [&](const aiNode& node, const aiBone* maybe_bone, auto&& recurse) -> JointId {
        const JointId joint_id = maybe_bone ? add_joint(*maybe_bone)
                                            : add_dummy_joint(to_string_view(node.mName));

        JointId current = joint_id;
        size_t child_index = 0;

        for_each_child(node, [&](const aiNode& child) {
            const auto it = joint_nodes.find(&child);
            const bool subtree_contains_bones = it != joint_nodes.end();
            if (subtree_contains_bones) {
                const bool no_space_is_left = child_index + 1 == k_max_num_children_per_joint;
                if (no_space_is_left) {
                    // No space left in joint, add a dummy joint to fit in the rest.
                    const std::string parent_name(get_joint_name(current));
                    const JointId dummy_joint_id = add_dummy_joint(parent_name + "_ext");
                    m_joints.at(current).children[child_index] = dummy_joint_id;
                    current = dummy_joint_id;
                    child_index = 0;
                }

                const aiBone* child_bone = it->second;
                m_joints.at(current).children.at(child_index++) =
                    recurse(child, child_bone, recurse);
            }
        });

        return joint_id;
    };

    const auto root_it = joint_nodes.find(scene.mRootNode);
    assert(root_it != joint_nodes.end());

    const auto& [root_node, maybe_bone] = *root_it;
    const auto root_joint_id =
        create_joint_hierarchy(*root_node, maybe_bone, create_joint_hierarchy);
    assert(root_joint_id == 0);
}

//------------------------------------------------------------------------------------------------------------------

// Data for the mesh itself: the vertices, indices, and submeshes.
class MeshData {
public:
    static constexpr float scale = 1.00f; // TODO configurable or deduced somehow?

    explicit MeshData(const aiScene& scene, const JointData& joint_data, StringData& string_data)
        : m_scene(&scene), m_joint_data(&joint_data), m_string_data(&string_data)
    {
        visit(*scene.mRootNode, aiMatrix4x4());
    }


    const std::vector<Submesh>& submeshes() const { return m_submeshes; }
    const std::vector<Vertex>& vertices() const { return m_vertices; }
    const std::vector<VertexIndex>& indices() const { return m_indices; }

private:
    void visit(const aiMesh& mesh, const aiMatrix4x4& accumulate_transform);
    void visit(const aiNode& node, const aiMatrix4x4& accumulate_transform);

    void add_vertex(const aiMesh& mesh,
                    uint32_t index,
                    const aiMatrix4x4& transform4x4,
                    const aiMatrix3x3& transform3x3);

    std::vector<Submesh> m_submeshes;
    std::vector<Vertex> m_vertices;
    std::vector<VertexIndex> m_indices;

    const aiScene* m_scene = nullptr;
    const JointData* m_joint_data = nullptr;
    StringData* m_string_data = nullptr;
};

std::optional<size_t> get_joint_index_to_use(const Vertex& vertex, const float weight)
{
    const auto normalised_weight = normalise<JointWeights::value_type>(weight);
    auto it = std::min_element(vertex.joint_weights.begin(), vertex.joint_weights.end());
    if (*it < normalised_weight) {
        return size_t(std::distance(vertex.joint_weights.begin(), it));
    }
    return std::nullopt;
}

void MeshData::visit(const aiMesh& mesh, const aiMatrix4x4& accumulate_transform)
{
    auto log_error = [&](auto&&... what) {
        std::cerr << "Error in mesh '" << to_string_view(mesh.mName) << "': ";
        (std::cerr << ... << what); // NOLINT
        std::cerr << '\n';
    };

    // Determine whether mesh has all data we need
    const bool hasUv0 = mesh.GetNumUVChannels() > 0;
    const bool hasNormals = mesh.HasNormals();
    const bool hasJoints = mesh.HasBones();

    const size_t submesh_begin = m_indices.size();

    if (!hasUv0 || !hasNormals) {
        std::string reason;
        if (!hasUv0) {
            reason += "\n\tUV coordinates";
        }
        if (!hasNormals) {
            reason += "\n\tNormals";
        }

        log_error("Missing data: ", reason, ", skipping submesh...");
        return;
    }

    // Copy all relevant index data
    const auto vertices_begin = uint32_t(m_vertices.size());
    m_vertices.reserve(vertices_begin + mesh.mNumVertices);
    m_indices.reserve(m_indices.size() + 3 * mesh.mNumFaces);

    for_each_face(mesh, [&](const aiFace& face) {
        assert(face.mNumIndices == 3);
        for_each_index(face, [&](const uint32_t index) {
            const auto max_index = std::numeric_limits<VertexIndex>::max();
            if (index > max_index) {
                log_error("Vertex index out of bounds (limit: ", max_index, ", was: ", index, ").");
                return;
            }
            m_indices.push_back(VertexIndex(index));
        });
    });

    // Copy all relevant vertex data, Y-up to Z-up and mirrored X
    const auto normal_transform = aiMatrix3x3(accumulate_transform);
    for (uint32_t i = 0; i < mesh.mNumVertices; i++) {
        add_vertex(mesh, i, accumulate_transform, normal_transform);
    }

    for_each_bone(mesh, [&](const aiBone& bone) {
        for (uint32_t wi = 0; wi < bone.mNumWeights; ++wi) {
            const uint32_t vertex_index = vertices_begin + bone.mWeights[wi].mVertexId;
            if (vertex_index >= m_vertices.size()) {
                log_error("Joint weight vertex id out of range in joint: ", to_string(bone.mName));
                continue;
            }

            const float weight = bone.mWeights[wi].mWeight;
            Vertex& vertex = m_vertices[vertex_index];

            if (std::optional<size_t> index = get_joint_index_to_use(vertex, weight);
                index.has_value()) {
                vertex.joint_id[index.value()] = m_joint_data->get_joint_id(bone);
                vertex.joint_weights[index.value()] = normalise<JointWeights::value_type>(weight);
            }
        }
    });

    Submesh& submesh = m_submeshes.emplace_back();
    submesh.name = m_string_data->store(mesh.mName);
    submesh.material = m_string_data->store(m_scene->mMaterials[mesh.mMaterialIndex]->GetName());
    submesh.begin = uint16_t(submesh_begin);
    submesh.num_indices = uint32_t(3 * mesh.mNumFaces);
}

// TODO: if the converter needs to be optimised, this function seems like a low-hanging fruit.
void MeshData::add_vertex(const aiMesh& mesh,
                          const uint32_t index,
                          const aiMatrix4x4& transform4x4,
                          const aiMatrix3x3& transform3x3)
{
    aiVector3D position = mesh.mVertices[index];
    position *= transform4x4;

    Vertex& vertex = m_vertices.emplace_back();
    vertex.position.x = -position.x * scale;
    vertex.position.y = position.z * scale;
    vertex.position.z = position.y * scale;

    vertex.uv0.x = mesh.mTextureCoords[0][index].x;
    vertex.uv0.y = 1.0f - mesh.mTextureCoords[0][index].y;

    const bool hasUv1 = mesh.GetNumUVChannels() > 1;
    if (hasUv1) {
        vec2 uv1{ mesh.mTextureCoords[1][index].x, mesh.mTextureCoords[1][index].y };
    }

    auto normal = mesh.mNormals[index];
    auto tangent = mesh.mTangents[index];
    auto bitangent = mesh.mBitangents[index];
    normal *= transform3x3;
    tangent *= transform3x3;
    bitangent *= transform3x3;

    vertex.normal = vec3{ -normal.x, normal.z, normal.y };
    vertex.tangent = vec3{ -tangent.x, tangent.z, tangent.y };
    vertex.bitangent = vec3{ bitangent.x, -bitangent.z, -bitangent.y };
}

void MeshData::visit(const aiNode& node, const aiMatrix4x4& accumulate_transform)
{
    const auto transform = node.mTransformation * accumulate_transform;
    for_each_mesh(*m_scene, node, [&](const aiMesh& mesh) { visit(mesh, transform); });
    for_each_child(node, [&](const aiNode& child) { visit(child, transform); });
}

// Returns {min, max} bounding box.
std::pair<vec3, vec3> calculate_bounding_box(const std::vector<Vertex>& vertices)
{
    // Calculate bounding box
    vec3 abb_min(std::numeric_limits<float>::max());
    vec3 abb_max(std::numeric_limits<float>::min());

    for (const auto& v : vertices) {
        abb_min.x = std::min(v.position.x, abb_min.x);
        abb_min.y = std::min(v.position.y, abb_min.y);
        abb_min.z = std::min(v.position.z, abb_min.z);

        abb_max.x = std::max(v.position.x, abb_max.x);
        abb_max.y = std::max(v.position.y, abb_max.y);
        abb_max.z = std::max(v.position.z, abb_max.z);
    }

    return { abb_min, abb_max };
}

float calculate_radius(const vec3 centre, const std::vector<Vertex>& vertices)
{
    auto distance_to_centre = [&centre](const Vertex& lhs, const Vertex& rhs) {
        return distance2(lhs.position, centre) < distance2(rhs.position, centre);
    };
    auto farthes_vertex_id = std::max_element(vertices.begin(), vertices.end(), distance_to_centre);
    return distance(centre, farthes_vertex_id->position);
}

//------------------------------------------------------------------------------------------------------------------

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
    (std::cout << ... << what); // NOLINT
    std::cout << '\n';
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
            const auto joint_id = std::distance(joints.data(), &joint);
            const bool has_inverse_bind_matrix = joint.inverse_bind_matrix != mat4(1.0f);
            print(0, '[', joint_id, "] ", joint_data.get_joint_name(joint_id), ':');
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
            print(indent_level, joint_data.get_joint_name(id));

            const Joint& joint = joint_data.get_joint(id);
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

    // Use the AssImp to load a scene from file
    static const unsigned int pFlags =
        aiProcess_ValidateDataStructure | aiProcess_RemoveRedundantMaterials |
        aiProcess_CalcTangentSpace | aiProcess_FindDegenerates | aiProcess_FindInvalidData |
        aiProcess_FindInstances | aiProcess_ImproveCacheLocality | aiProcess_JoinIdenticalVertices |
        aiProcess_OptimizeGraph | aiProcess_OptimizeMeshes | aiProcess_RemoveComponent |
        aiProcess_SortByPType | aiProcess_SplitLargeMeshes | aiProcess_GenUVCoords |
        aiProcess_GenSmoothNormals | aiProcess_Triangulate;

    const auto scene = importer.ReadFile(file_path, pFlags);
    if (!scene) {
        std::cerr << importer.GetErrorString();
        return nullptr;
    }

    std::cout << "Loaded file '" << file_path.u8string() << "'.\n";
    return scene;
}

bool write_file(const std::filesystem::path& file_path,
                const MeshData& mesh_data,
                const JointData& joint_data,
                const StringData& string_data)
{
    FileWriter writer;

    MeshHeader header = {};
    header.four_cc = k_four_cc;
    header.version = k_mesh_format_version;

    {
        std::tie(header.abb_min, header.abb_max) = calculate_bounding_box(mesh_data.vertices());
        header.centre = (header.abb_min + header.abb_max) / 2.0f;
        header.radius = calculate_radius(header.centre, mesh_data.vertices());
    }

    writer.enqueue(header);
    header.submeshes = writer.enqueue(mesh_data.submeshes());
    header.vertices = writer.enqueue(mesh_data.vertices());
    header.indices = writer.enqueue(mesh_data.indices());
    header.joints = writer.enqueue(joint_data.joints());
    header.strings = writer.enqueue(string_data.all_strings());

    const bool success = writer.write(file_path);
    if (success) {
        std::cout << "Wrote file '" << file_path.u8string() << "'.\n";
    }

    return success;
}
} // namespace

bool convert_mesh(const std::filesystem::path& path_in, const std::filesystem::path& path_out)
{
    try {
        auto importer = std::make_unique<Assimp::Importer>();

        const aiScene* scene = load_file(*importer, path_in);
        if (!scene) {
            return false;
        }

        logging::dump_scene(*scene);

        // Process imported data.
        StringData string_data;
        JointData joint_data(*scene, string_data);
        logging::dump_joints(joint_data);
        MeshData mesh_data(*scene, joint_data, string_data);

        // Release imported data now.
        importer.reset();

        // Write processed data.
        return write_file(path_out, mesh_data, joint_data, string_data);
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to process mesh '" << path_in.u8string() << "': " << e.what() << "\n";
        return false;
    }
}

} // namespace Mg
