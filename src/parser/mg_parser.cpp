//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/parser/mg_parser.h"

#include "mg/gfx/mg_shader_related_types.h"
#include "mg_lexer.h"

#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/utils/mg_math_utils.h"
#include "mg/utils/mg_string_utils.h"

#include <fmt/core.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <hjson/hjson.h>

namespace Mg::parser {

namespace {

[[noreturn]] void parse_error(std::string_view reason, const Token& t)
{
    throw RuntimeError{ "Parse error at line {}: {} [parsing '{}']", t.line, reason, t.lexeme };
}

class TokenStream {
public:
    explicit TokenStream(std::vector<Token> tokens)
        : m_tokens(std::move(tokens)), m_remaining_tokens(m_tokens)
    {}

    explicit TokenStream(std::string_view definition)
        : m_tokens(lex_definition(definition)), m_remaining_tokens(m_tokens)
    {}

    const Token& next_token()
    {
        auto& ret_val = peek_token();
        m_remaining_tokens = m_remaining_tokens.subspan(1);
        return ret_val;
    }

    const Token& peek_token() const
    {
        if (m_remaining_tokens.empty()) {
            throw RuntimeError{ "Parse error: unexpected end of file." };
        }
        return m_remaining_tokens.front();
    }

    const Token& expect_next(TokenType expected_type,
                             Opt<std::string_view> additional_message = nullopt)
    {
        auto& t = next_token();

        if (t.type != expected_type) {
            auto expected_type_str = token_type_to_str(expected_type);

            auto make_detail_error_msg = [&](std::string_view m) {
                return fmt::format("Expected {} ({})", expected_type_str, m);
            };
            auto make_error_msg = [&] { return fmt::format("Expected {}.", expected_type_str); };

            auto msg_string =
                additional_message.map(make_detail_error_msg).or_else(make_error_msg).value();

            parse_error(msg_string, t);
        }

        return t;
    }

    [[nodiscard]] bool has_more() const { return !m_remaining_tokens.empty(); }

private:
    std::vector<Token> m_tokens;
    span<Token> m_remaining_tokens;
};

float parse_numeric(TokenStream& stream)
{
    auto& token = stream.expect_next(TokenType::NUMERIC_LITERAL);
    return numeric_value(token);
}

std::string_view parse_identifier(TokenStream& stream)
{
    auto& id_token = stream.expect_next(TokenType::IDENTIFIER);
    return string_value(id_token);
}

std::string_view parse_string_literal(TokenStream& stream)
{
    auto& str_token = stream.expect_next(TokenType::STRING_LITERAL);
    return string_value(str_token);
}

glm::vec2 parse_vec2(TokenStream& stream)
{
    glm::vec2 result = { 0.0f, 0.0f };
    stream.expect_next(TokenType::VEC2);
    stream.expect_next(TokenType::PARENTHESIS_LEFT);
    result.x = parse_numeric(stream);
    stream.expect_next(TokenType::COMMA);
    result.y = parse_numeric(stream);
    stream.expect_next(TokenType::PARENTHESIS_RIGHT);
    return result;
}

glm::vec3 parse_vec3(TokenStream& stream)
{
    glm::vec3 result = { 0.0f, 0.0f, 0.0f };
    stream.expect_next(TokenType::VEC3);
    stream.expect_next(TokenType::PARENTHESIS_LEFT);
    result.x = parse_numeric(stream);
    stream.expect_next(TokenType::COMMA);
    result.y = parse_numeric(stream);
    stream.expect_next(TokenType::COMMA);
    result.z = parse_numeric(stream);
    stream.expect_next(TokenType::PARENTHESIS_RIGHT);
    return result;
}

glm::vec4 parse_vec4(TokenStream& stream)
{
    glm::vec4 result = { 0.0f, 0.0f, 0.0f, 0.0f };
    stream.expect_next(TokenType::VEC4);
    stream.expect_next(TokenType::PARENTHESIS_LEFT);
    result.x = parse_numeric(stream);
    stream.expect_next(TokenType::COMMA);
    result.y = parse_numeric(stream);
    stream.expect_next(TokenType::COMMA);
    result.z = parse_numeric(stream);
    stream.expect_next(TokenType::COMMA);
    result.w = parse_numeric(stream);
    stream.expect_next(TokenType::PARENTHESIS_RIGHT);
    return result;
}

void parse_block(TokenStream& stream, const std::function<void(TokenStream&)>& statement_parser)
{
    stream.expect_next(TokenType::CURLY_LEFT);

    while (stream.peek_token().type != TokenType::CURLY_RIGHT) {
        statement_parser(stream);
    }

    stream.expect_next(TokenType::CURLY_RIGHT);
}

[[nodiscard]] SamplerDeclaration parse_sampler_declaration(TokenStream& stream,
                                                           const bool get_texture_resource_id)
{
    SamplerDeclaration result;

    auto& type_token = stream.next_token();
    switch (type_token.type) {
    case TokenType::SAMPLER2D:
        result.type = shader::SamplerType::Sampler2D;
        break;
    case TokenType::SAMPLERCUBE:
        result.type = shader::SamplerType::SamplerCube;
        break;
    default:
        parse_error("Unexpected token (expected sampler2D or samplerCube).", type_token);
    }

    result.name = Identifier::from_runtime_string(parse_identifier(stream));

    if (get_texture_resource_id && stream.peek_token().type == TokenType::EQUALS) {
        stream.next_token();
        result.texture_resource_id = Identifier::from_runtime_string(parse_string_literal(stream));
    }

    stream.expect_next(TokenType::SEMICOLON);
    return result;
}

[[nodiscard]] ParameterDeclaration parse_parameter_declaration(TokenStream& stream)
{
    ParameterDeclaration result;

    auto& type_token = stream.next_token();
    result.name = Identifier::from_runtime_string(parse_identifier(stream));
    stream.expect_next(TokenType::EQUALS, "Specifying default value for parameter is mandatory");

    switch (type_token.type) {
    case TokenType::INT: {
        result.type = shader::ParameterType::Int;
        result.value = round<int>(parse_numeric(stream));
        break;
    }
    case TokenType::FLOAT: {
        result.type = shader::ParameterType::Float;
        result.value = parse_numeric(stream);
        break;
    }
    case TokenType::VEC2: {
        result.type = shader::ParameterType::Vec2;
        result.value = parse_vec2(stream);
        break;
    }
    case TokenType::VEC3: {
        // According to the following source, memory layout for vec3 does not follow the
        // specification with some drivers. To prevent portability issues, the use of vec3 in
        // shader parameters is unsupported.
        // https://www.khronos.org/opengl/wiki/Interface_Block_(GLSL)#Memory_layout
        parse_error("vec3 is unsupported due to driver inconsistencies. Please use vec4 instead.",
                    type_token);
        break;
    }
    case TokenType::VEC4: {
        result.type = shader::ParameterType::Vec4;
        result.value = parse_vec4(stream);
        break;
    }
    default:
        parse_error("Unexpected token, expected parameter type (int|float|vec2|vec4).", type_token);
    }

    stream.expect_next(TokenType::SEMICOLON);
    return result;
}


[[nodiscard]] OptionDeclaration parse_option_declaration(TokenStream& stream)
{
    OptionDeclaration result;
    result.name = Identifier::from_runtime_string(parse_identifier(stream));

    if (auto& equal_token = stream.next_token(); equal_token.type != TokenType::EQUALS) {
        parse_error("Expected '= true|false' (value for option).", equal_token);
    }

    auto& value_token = stream.next_token();
    switch (value_token.type) {
    case TokenType::TRUE:
        result.value = true;
        break;
    case TokenType::FALSE:
        result.value = false;
        break;
    default:
        parse_error("Expected 'true' or 'false'.", value_token);
    }

    stream.expect_next(TokenType::SEMICOLON);
    return result;
}

class MaterialParser {
public:
    explicit MaterialParser(std::string_view resource_definition) : m_stream(resource_definition)
    {
        parse_outer_scope();
    }

    void parse_outer_scope()
    {
        auto&& t = m_stream.next_token();
        switch (t.type) {
        case TokenType::SHADER:
            if (m_result.shader_resource_id != "") {
                parse_error("Multiple shader declarations", t);
            }
            parse_shader_resource_id();
            break;
        case TokenType::PARAMETERS:
            parse_parameters_block();
            break;
        case TokenType::OPTIONS:
            parse_options_block();
            break;
        case TokenType::END_OF_FILE:
            return;
        default:
            parse_error("Unexpected token at global scope.", t);
        }

        MG_ASSERT(m_stream.has_more());
        parse_outer_scope();
    }

    void parse_shader_resource_id()
    {
        m_stream.expect_next(TokenType::EQUALS);
        m_result.shader_resource_id =
            Identifier::from_runtime_string(parse_string_literal(m_stream));
    }

    void parse_sampler_or_parameter_declaration()
    {
        auto& type_token = m_stream.peek_token();
        const bool parameter_is_sampler = type_token.type == TokenType::SAMPLER2D ||
                                          type_token.type == TokenType::SAMPLERCUBE;

        if (parameter_is_sampler) {
            m_result.samplers.push_back(parse_sampler_declaration(m_stream, true));
        }
        else {
            m_result.parameters.push_back(parse_parameter_declaration(m_stream));
        }
    }

    void parse_parameters_block()
    {
        parse_block(m_stream, [this](TokenStream&) { parse_sampler_or_parameter_declaration(); });
    }

    void parse_options_block()
    {
        parse_block(m_stream, [&](TokenStream& stream) {
            m_result.options.push_back(parse_option_declaration(stream));
        });
    }

    MaterialParseResult take_result() noexcept { return std::move(m_result); }

private:
    TokenStream m_stream;
    MaterialParseResult m_result;
};

} // namespace

bool parse(std::string_view definition, int& out)
{
    const auto [success, result] = string_to<int>(definition);
    out = result;
    return success;
}

bool parse(std::string_view definition, float& out)
{
    const auto [success, result] = string_to<float>(definition);
    out = result;
    return success;
}

bool parse(std::string_view definition, glm::vec2& out)
{
    TokenStream stream{ definition };
    try {
        out = parse_vec2(stream);
        return true;
    }
    catch (...) {
        return false;
    }
}

bool parse(std::string_view definition, glm::vec3& out)
{
    TokenStream stream{ definition };
    try {
        out = parse_vec3(stream);
        return true;
    }
    catch (...) {
        return false;
    }
}

bool parse(std::string_view definition, glm::vec4& out)
{
    TokenStream stream{ definition };
    try {
        out = parse_vec4(stream);
        return true;
    }
    catch (...) {
        return false;
    }
}

MaterialParseResult parse_material(std::string_view material_resource_definition)
{
    return MaterialParser{ material_resource_definition }.take_result();
}

namespace {

void parse_error(std::string_view key, std::string_view reason)
{
    throw RuntimeError{ "Error parsing key '{}': {}", key, reason };
}

std::string_view hjson_type_to_string(Hjson::Type type)
{
    using enum Hjson::Type;

    switch (type) {
    case Undefined:
        return "Undefined";
    case Null:
        return "Null";
    case Bool:
        return "Bool";
    case Double:
        return "Double";
    case Int64:
        return "Int64";
    case String:
        return "String";
    case Vector:
        return "Vector";
    case Map:
        return "Map";
    }

    MG_ASSERT(false && "unexpected Hjson::Type");
}

// Returns default value, or nullopt and error reason.
std::pair<Opt<glm::vec4>, std::string>
convert_default_value_property(const Hjson::Value& default_value_property, size_t num_elements)
{
    using enum Hjson::Type;

    MG_ASSERT(num_elements <= 4);
    static constexpr auto wrong_size_error_message = "Expected array of {} numeric values.";
    static constexpr auto wrong_type_error_message = "Expected numeric parameter value.";

    switch (default_value_property.type()) {
    case Undefined:
        return { glm::vec4(0.0f, 0.0f, 0.0f, 0.0f), "" };

    case Double:
    case Int64:
        if (num_elements == 1) {
            const auto value = narrow_cast<float>(default_value_property.to_double());
            return { glm::vec4(value, 0.0f, 0.0f, 0.0f), "" };
        }
        else {
            return { nullopt, fmt::format(wrong_size_error_message, num_elements) };
        }

    case Vector: {
        glm::vec4 result{ 0.0f, 0.0f, 0.0f, 0.0f };
        const size_t len = default_value_property.size();
        if (len != num_elements) {
            return { nullopt, fmt::format(wrong_size_error_message, num_elements) };
        }

        MG_ASSERT(len <= result.length());
        for (int i = 0; i < as<int>(len); ++i) {
            const auto& element = default_value_property[i];
            if (!element.is_numeric()) {
                return { nullopt, wrong_type_error_message };
            }
            result[i] = narrow_cast<float>(element.to_double());
        }
        return { result, "" };
    }

    default:
        return { nullopt, wrong_type_error_message };
    }
}

} // namespace

ShaderParseResult parse_shader(std::string_view shader_resource_definition)
{
    Hjson::Value root = Hjson::Unmarshal(shader_resource_definition.data(),
                                         shader_resource_definition.size());

    auto get_property = [](const Hjson::Value& value,
                           const char* key,
                           const Hjson::Type expected_type,
                           const bool mandatory = false) {
        const Hjson::Value& property = value[key];
        if (property.defined() && property.type() != expected_type) {
            parse_error(key,
                        fmt::format("Unexpected type. Expected: {}. Was: {}.",
                                    hjson_type_to_string(expected_type),
                                    hjson_type_to_string(property.type())));
        }
        if (mandatory && !property.defined()) {
            parse_error(key,
                        fmt::format("Missing value of (expected type {}).",
                                    hjson_type_to_string(expected_type)));
        }
        return property;
    };

    const Hjson::Value& vertex_code = get_property(root, "vertex_code", Hjson::Type::String);
    const Hjson::Value& fragment_code = get_property(root, "fragment_code", Hjson::Type::String);
    const Hjson::Value& parameters = get_property(root, "parameters", Hjson::Type::Map);
    const Hjson::Value& options = get_property(root, "options", Hjson::Type::Map);
    const Hjson::Value& tags = get_property(root, "tags", Hjson::Type::Vector);

    ShaderParseResult result;
    result.vertex_code = vertex_code.to_string();
    result.fragment_code = fragment_code.to_string();

    // Parse parameter and samplers.
    if (parameters.defined()) {
        for (const auto& [parameter_name, parameter_properties] : parameters) {
            const Hjson::Value& type =
                get_property(parameter_properties, "type", Hjson::Type::String, true);
            const auto parameter_type = shader::string_to_parameter_type(type.to_string());
            const auto sampler_type = shader::string_to_sampler_type(type.to_string());

            if (parameter_type) {
                ParameterDeclaration& parameter_declaration = result.parameters.emplace_back();
                parameter_declaration.name = Identifier::from_runtime_string(parameter_name);
                parameter_declaration.type = parameter_type.value();
                const size_t num_elements =
                    shader::parameter_type_num_elements(parameter_type.value());

                const Hjson::Value& default_value_property = parameter_properties["default_value"];
                const auto [default_value, error_reason] =
                    convert_default_value_property(default_value_property, num_elements);

                if (!default_value) {
                    throw RuntimeError("Failed to get default_value for parameter {}: {}",
                                       parameter_name,
                                       error_reason);
                }

                switch (num_elements) {
                case 1:
                    parameter_declaration.value = default_value.value().x;
                    break;
                case 2:
                    parameter_declaration.value = glm::vec2(default_value.value());
                    break;
                case 4:
                    parameter_declaration.value = default_value.value();
                    break;
                default:
                    MG_ASSERT(false && "Unexpected num_elements.");
                }
            }
            else if (sampler_type) {
                SamplerDeclaration& sampler_declaration = result.samplers.emplace_back();
                sampler_declaration.name = Identifier::from_runtime_string(parameter_name);
                sampler_declaration.type = sampler_type.value();
            }
            else {
                throw RuntimeError("Unexpected type {} for parameter {}.",
                                   type.to_string(),
                                   parameter_name);
            }
        }
    }

    // Parse options.
    if (options.defined()) {
        for (const auto& [option_name, option_default_enabled] : options) {
            if (option_default_enabled.type() != Hjson::Type::Bool) {
                parse_error("options." + option_name, "Expected boolean value.");
            }
            OptionDeclaration& option_declaration = result.options.emplace_back();
            option_declaration.name = Identifier::from_runtime_string(option_name);
            option_declaration.value = option_default_enabled.to_int64() != 0;
        }
    }

    // Parse tags.
    if (tags.defined()) {
        const auto len = tags.size();
        for (int i = 0; i < as<int>(len); ++i) {
            if (tags[i].type() != Hjson::Type::String) {
                throw RuntimeError("Unexpected type {} for element in tags.",
                                   hjson_type_to_string(tags[i].type()));
            }
            const auto& tag_name = tags[i].to_string();
            Opt<shader::Tag> tag_value = shader::string_to_tag(tag_name);
            if (!tag_value) {
                throw RuntimeError("Unexpected tag {}.", tag_name);
            }
            result.tags |= tag_value.value();
        }
    }

    return result;
}

} // namespace Mg::parser
