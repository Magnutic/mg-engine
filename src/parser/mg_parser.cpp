//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/parser/mg_parser.h"

#include "mg_lexer.h"

#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/utils/mg_math_utils.h"
#include "mg/utils/mg_string_utils.h"

#include "fmt/core.h"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"

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

class ShaderParser {
public:
    explicit ShaderParser(std::string_view resource_definition) : m_stream(resource_definition)
    {
        parse_outer_scope();
    }

    void parse_outer_scope()
    {
        auto&& t = m_stream.next_token();
        switch (t.type) {
        case TokenType::TAGS:
            parse_tags_block();
            break;
        case TokenType::PARAMETERS:
            parse_parameters_block();
            break;
        case TokenType::OPTIONS:
            parse_options_block();
            break;
        case TokenType::VERTEX_CODE:
            m_result.vertex_code = string_value(t);
            break;
        case TokenType::FRAGMENT_CODE:
            m_result.fragment_code = string_value(t);
            break;
        case TokenType::END_OF_FILE:
            return;
        default:
            parse_error("Unexpected token at global scope.", t);
        }

        MG_ASSERT(m_stream.has_more());
        parse_outer_scope();
    }

    void parse_tags_block()
    {
        const auto parse_tag = [this](TokenStream& stream) {
            auto& tag_token = stream.next_token();
            switch (tag_token.type) {
            case TokenType::UNLIT:
                m_result.tags |= shader::Tag::UNLIT;
                break;
            case TokenType::OPAQUE:
                m_result.tags |= shader::Tag::OPAQUE;
                break;
            case TokenType::DEFINES_LIGHT_MODEL:
                m_result.tags |= shader::Tag::DEFINES_LIGHT_MODEL;
                break;
            case TokenType::DEFINES_VERTEX_PREPROCESS:
                m_result.tags |= shader::Tag::DEFINES_VERTEX_PREPROCESS;
                break;
            default:
                parse_error("Unexpected tag.", tag_token);
                break;
            }

            stream.expect_next(TokenType::SEMICOLON);
        };

        parse_block(m_stream, parse_tag);
    }

    void parse_sampler_or_parameter_declaration()
    {
        auto& type_token = m_stream.peek_token();
        const bool parameter_is_sampler = type_token.type == TokenType::SAMPLER2D ||
                                          type_token.type == TokenType::SAMPLERCUBE;

        if (parameter_is_sampler) {
            m_result.samplers.push_back(parse_sampler_declaration(m_stream, false));
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

    ShaderParseResult take_result() noexcept { return std::move(m_result); }

private:
    TokenStream m_stream;
    ShaderParseResult m_result;
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

ShaderParseResult parse_shader(std::string_view shader_resource_definition)
{
    return ShaderParser{ shader_resource_definition }.take_result();
}

} // namespace Mg::parser
