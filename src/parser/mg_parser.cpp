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

#include "fmt/core.h"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"

namespace Mg::parser {

namespace {

[[noreturn]] void parse_error(std::string_view reason, const Token& t)
{
    log.error("Parse error at line {}: {} [parsing '{}']", t.line, reason, t.lexeme);
    throw RuntimeError();
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
            log.error("Parse error: unexpected end of file.");
            throw RuntimeError();
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

    float parse_numeric()
    {
        auto& token = expect_next(TokenType::NUMERIC_LITERAL);
        return numeric_value(token);
    }

    std::string_view parse_identifier()
    {
        auto& id_token = expect_next(TokenType::IDENTIFIER);
        return string_value(id_token);
    }

    std::string_view parse_string_literal()
    {
        auto& str_token = expect_next(TokenType::STRING_LITERAL);
        return string_value(str_token);
    }


private:
    std::vector<Token> m_tokens;
    span<Token> m_remaining_tokens;
};

glm::vec2 parse_vec2(TokenStream& stream)
{
    glm::vec2 result = { 0.0f, 0.0f };
    stream.expect_next(TokenType::VEC2);
    stream.expect_next(TokenType::PARENTHESIS_LEFT);
    result.x = stream.parse_numeric();
    stream.expect_next(TokenType::COMMA);
    result.y = stream.parse_numeric();
    stream.expect_next(TokenType::PARENTHESIS_RIGHT);
    return result;
}

glm::vec3 parse_vec3(TokenStream& stream)
{
    glm::vec3 result = { 0.0f, 0.0f, 0.0f };
    stream.expect_next(TokenType::VEC3);
    stream.expect_next(TokenType::PARENTHESIS_LEFT);
    result.x = stream.parse_numeric();
    stream.expect_next(TokenType::COMMA);
    result.y = stream.parse_numeric();
    stream.expect_next(TokenType::COMMA);
    result.z = stream.parse_numeric();
    stream.expect_next(TokenType::PARENTHESIS_RIGHT);
    return result;
}

glm::vec4 parse_vec4(TokenStream& stream)
{
    glm::vec4 result = { 0.0f, 0.0f, 0.0f, 0.0f };
    stream.expect_next(TokenType::VEC4);
    stream.expect_next(TokenType::PARENTHESIS_LEFT);
    result.x = stream.parse_numeric();
    stream.expect_next(TokenType::COMMA);
    result.y = stream.parse_numeric();
    stream.expect_next(TokenType::COMMA);
    result.z = stream.parse_numeric();
    stream.expect_next(TokenType::COMMA);
    result.w = stream.parse_numeric();
    stream.expect_next(TokenType::PARENTHESIS_RIGHT);
    return result;
}

class ShaderParser {
public:
    explicit ShaderParser(std::string_view shader_resource_definition)
        : m_stream(shader_resource_definition)
    {
        parse_outer_scope();
    }

    enum class ShaderBlockType { Vertex, Fragment };

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

    void parse_block(const std::function<void(void)>& statement_parser)
    {
        m_stream.expect_next(TokenType::CURLY_LEFT);

        while (m_stream.peek_token().type != TokenType::CURLY_RIGHT) {
            statement_parser();
        }

        m_stream.expect_next(TokenType::CURLY_RIGHT);
    }

    void parse_sampler_declaration()
    {
        auto& type_token = m_stream.next_token();
        shader::SamplerType sampler_type{};

        switch (type_token.type) {
        case TokenType::SAMPLER2D:
            sampler_type = shader::SamplerType::Sampler2D;
            break;
        case TokenType::SAMPLERCUBE:
            sampler_type = shader::SamplerType::SamplerCube;
            break;
        default:
            parse_error("Unexpected token (expected sampler2D or samplerCube).", type_token);
        }

        const std::string_view identifier = m_stream.parse_identifier();

        m_stream.expect_next(TokenType::SEMICOLON);

        shader::Sampler s{ Identifier::from_runtime_string(identifier), sampler_type };
        m_result.samplers.push_back(s);
    }

    void parse_parameter_declaration()
    {
        // If parameter type is a sampler type
        if (auto& type_token = m_stream.peek_token();
            type_token.type == TokenType::SAMPLER2D || type_token.type == TokenType::SAMPLERCUBE) {
            parse_sampler_declaration();
            return;
        }

        auto& type_token = m_stream.next_token();

        const std::string_view id = m_stream.parse_identifier();

        m_stream.expect_next(TokenType::EQUALS,
                             "Specifying default value for parameter is mandatory");

        shader::Parameter p{};
        p.name = Identifier::from_runtime_string(id);

        glm::vec4 value{ 0.0f };

        switch (type_token.type) {
        case TokenType::INT:
            p.type = shader::ParameterType::Int;
            value.x = m_stream.parse_numeric();
            break;
        case TokenType::FLOAT:
            p.type = shader::ParameterType::Float;
            value.x = m_stream.parse_numeric();
            break;
        case TokenType::VEC2:
            p.type = shader::ParameterType::Vec2;
            value = glm::vec4(parse_vec2(m_stream), 0.0f, 0.0f);
            break;
        case TokenType::VEC3:
            // According to the following source, memory layout for vec3 does not follow the
            // specification with some drivers. To prevent portability issues, the use of vec3 in
            // shader parameters is unsupported.
            // https://www.khronos.org/opengl/wiki/Interface_Block_(GLSL)#Memory_layout
            parse_error(
                "vec3 is unsupported due to driver inconsistencies. Please use vec4 instead.",
                type_token);
            break;
        case TokenType::VEC4:
            p.type = shader::ParameterType::Vec4;
            value = parse_vec4(m_stream);
            break;
        default:
            parse_error("Unexpected token, expected parameter type (int|float|vec2|vec4).",
                        type_token);
        }

        // If the required type is an integer, round the given value to nearest int.
        if (p.type == shader::ParameterType::Int) {
            const std::array<int32_t, 4> int_value = { round<int32_t>(value.x), 0, 0, 0 };
            std::memcpy(&p.value, &int_value, sizeof(p.value));
        }
        else {
            std::memcpy(&p.value, &value, sizeof(p.value));
        }

        m_stream.expect_next(TokenType::SEMICOLON);
        m_result.parameters.push_back(p);
    }

    void parse_option_declaration()
    {
        const std::string_view option_name = m_stream.parse_identifier();

        if (auto& equal_token = m_stream.next_token(); equal_token.type != TokenType::EQUALS) {
            parse_error("Expected = (i.e. default value for option, true|false).", equal_token);
        }

        bool default_value{};
        auto& value_token = m_stream.next_token();
        switch (value_token.type) {
        case TokenType::TRUE:
            default_value = true;
            break;
        case TokenType::FALSE:
            default_value = false;
            break;
        default:
            parse_error("Expected 'true' or 'false'.", value_token);
        }

        m_stream.expect_next(TokenType::SEMICOLON);

        m_result.options.push_back({ Identifier::from_runtime_string(option_name), default_value });
    }

    void parse_tags_block()
    {
        const auto parse_tag = [this] {
            auto& tag_token = m_stream.next_token();
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

            m_stream.expect_next(TokenType::SEMICOLON);
        };

        parse_block(parse_tag);
    }

    void parse_parameters_block()
    {
        parse_block([this] { parse_parameter_declaration(); });
    }

    void parse_options_block()
    {
        parse_block([this] { parse_option_declaration(); });
    }

    ShaderParseResult take_result() noexcept { return std::move(m_result); }

private:
    TokenStream m_stream;
    ShaderParseResult m_result;
};

} // namespace

glm::vec2 parse_vec2(std::string_view definition)
{
    TokenStream stream{ definition };
    return parse_vec2(stream);
}

glm::vec3 parse_vec3(std::string_view definition)
{
    TokenStream stream{ definition };
    return parse_vec3(stream);
}

glm::vec4 parse_vec4(std::string_view definition)
{
    TokenStream stream{ definition };
    return parse_vec4(stream);
}

ShaderParseResult parse_shader(std::string_view shader_resource_definition)
{
    return ShaderParser{ shader_resource_definition }.take_result();
}

} // namespace Mg::parser
