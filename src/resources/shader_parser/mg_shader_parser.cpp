//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
// Copyright (c) 2019 Magnus Bergsten
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgement in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
//**************************************************************************************************

#include "mg/resources/shader_parser/mg_shader_parser.h"

#include "mg_shader_lexer.h"

#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/utils/mg_math_utils.h"

#include "fmt/core.h"
#include "glm/vec4.hpp"

namespace Mg::shader {

namespace {

class Parser {
public:
    explicit Parser(std::string_view shader_resource_definition)
        : m_tokens(lex_shader_definition(shader_resource_definition))
        , m_current_token{ m_tokens.begin() }
    {
        parse_outer_scope();
    }

    enum class ShaderBlockType { Vertex, Fragment };

    void parse_outer_scope()
    {
        auto&& t = next_token();
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

        MG_ASSERT(m_current_token != m_tokens.end());
        parse_outer_scope();
    }

    void parse_error(std::string_view reason, const Token& t)
    {
        g_log.write_error(
            fmt::format("Parse error at line {}: {} [parsing '{}']", t.line, reason, t.lexeme));
        throw RuntimeError();
    }

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

    void parse_block(std::function<void(void)> statement_parser)
    {
        expect_next(TokenType::CURLY_LEFT);

        while (peek_token().type != TokenType::CURLY_RIGHT) { statement_parser(); }

        expect_next(TokenType::CURLY_RIGHT);
    }

    void parse_sampler_declaration()
    {
        auto&       type_token = next_token();
        SamplerType sampler_type{};

        switch (type_token.type) {
        case TokenType::SAMPLER2D:
            sampler_type = SamplerType::Sampler2D;
            break;
        case TokenType::SAMPLERCUBE:
            sampler_type = SamplerType::SamplerCube;
            break;
        default:
            parse_error("Unexpected token (expected sampler2D or samplerCube).", type_token);
        }

        std::string_view identifier = parse_identifier();

        expect_next(TokenType::SEMICOLON);

        Sampler s{ Identifier::from_runtime_string(identifier), sampler_type };
        m_result.samplers.push_back(s);
    }

    void parse_parameter_declaration()
    {
        // If parameter type is a sampler type
        if (auto& type_token = peek_token();
            type_token.type == TokenType::SAMPLER2D || type_token.type == TokenType::SAMPLERCUBE) {
            parse_sampler_declaration();
            return;
        }

        auto& type_token = next_token();

        std::string_view id = parse_identifier();

        expect_next(TokenType::EQUALS, "Specifying default value for parameter is mandatory");

        Parameter p{};
        p.name = Identifier::from_runtime_string(id);

        glm::vec4 value{ 0.0f };

        switch (type_token.type) {
        case TokenType::INT:
            p.type  = ParameterType::Int;
            value.x = parse_numeric();
            break;
        case TokenType::FLOAT:
            p.type  = ParameterType::Float;
            value.x = parse_numeric();
            break;
        case TokenType::VEC2:
            p.type = ParameterType::Vec2;
            expect_next(TokenType::VEC2);
            expect_next(TokenType::PARENTHESIS_LEFT);
            value.x = parse_numeric();
            expect_next(TokenType::COMMA);
            value.y = parse_numeric();
            expect_next(TokenType::PARENTHESIS_RIGHT);
            break;
        case TokenType::VEC4:
            p.type = ParameterType::Vec4;
            expect_next(TokenType::VEC4);
            expect_next(TokenType::PARENTHESIS_LEFT);
            value.x = parse_numeric();
            expect_next(TokenType::COMMA);
            value.y = parse_numeric();
            expect_next(TokenType::COMMA);
            value.z = parse_numeric();
            expect_next(TokenType::COMMA);
            value.w = parse_numeric();
            expect_next(TokenType::PARENTHESIS_RIGHT);
            break;
        default:
            parse_error("Unexpected token, expected parameter type (int|float|vec2|vec4).",
                        type_token);
        }

        // If the required type is an integer, round the given value to nearest int.
        // TODO: a better solution would be to parse as int to begin with.
        if (p.type == ParameterType::Int) {
            std::array<int32_t, 4> int_value = { round<int32_t>(value.x), 0, 0, 0 };
            std::memcpy(&p.value, &int_value, sizeof(p.value));
        }
        else {
            std::memcpy(&p.value, &value, sizeof(p.value));
        }

        expect_next(TokenType::SEMICOLON);
        m_result.parameters.push_back(p);
    }

    void parse_option_declaration()
    {
        std::string_view option_name = parse_identifier();

        if (auto& equal_token = next_token(); equal_token.type != TokenType::EQUALS) {
            parse_error("Expected = (i.e. default value for option, true|false).", equal_token);
        }

        bool  default_value{};
        auto& value_token = next_token();
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

        expect_next(TokenType::SEMICOLON);

        m_result.options.push_back({ Identifier::from_runtime_string(option_name), default_value });
    }

    void parse_tags_block()
    {
        auto parse_tag = [this] {
            auto& tag_token = next_token();
            switch (tag_token.type) {
            case TokenType::UNLIT:
                m_result.tags |= Tag::UNLIT;
                break;
            case TokenType::OPAQUE:
                m_result.tags |= Tag::OPAQUE;
                break;
            case TokenType::DEFINES_LIGHT_MODEL:
                m_result.tags |= Tag::DEFINES_LIGHT_MODEL;
                break;
            case TokenType::DEFINES_VERTEX_PREPROCESS:
                m_result.tags |= Tag::DEFINES_VERTEX_PREPROCESS;
                break;
            default:
                parse_error("Unexpected tag.", tag_token);
                break;
            }

            expect_next(TokenType::SEMICOLON);
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

    const Token& next_token()
    {
        auto& ret_val = peek_token();
        ++m_current_token;
        return ret_val;
    }

    const Token& peek_token() const
    {
        if (m_current_token == m_tokens.end()) {
            g_log.write_error("Parse error: unexpected end of file.");
            throw RuntimeError();
        }
        return *m_current_token;
    }

    const Token& expect_next(TokenType             expected_type,
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

    ParseResult take_result() { return std::move(m_result); }

private:
    std::vector<Token>           m_tokens;
    decltype(m_tokens)::iterator m_current_token{};
    ParseResult                  m_result;
};

} // namespace

ParseResult parse_shader(std::string_view shader_resource_definition)
{
    return Parser{ shader_resource_definition }.take_result();
}

} // namespace Mg::shader
