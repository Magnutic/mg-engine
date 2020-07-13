//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_shader_parser_token.h
 * Token types for shader pasrer.
 */

#pragma once

#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_optional.h"

#include <array>
#include <string_view>
#include <variant>

namespace Mg::shader {

enum class TokenType {
    // Symbols
    COMMA,
    SEMICOLON,
    PARENTHESIS_LEFT,
    PARENTHESIS_RIGHT,
    CURLY_LEFT,
    CURLY_RIGHT,
    EQUALS,

    // Values
    TRUE,
    FALSE,
    STRING_LITERAL,
    NUMERIC_LITERAL,

    // Data types
    SAMPLER2D,
    SAMPLERCUBE,
    INT,
    FLOAT,
    VEC2,
    VEC4,

    // Top-level identifier
    TAGS,
    PARAMETERS,
    OPTIONS,
    VERTEX_CODE,
    FRAGMENT_CODE,

    // Tags
    OPAQUE,
    UNLIT,
    DEFINES_LIGHT_MODEL,
    DEFINES_VERTEX_PREPROCESS,

    // Misc
    IDENTIFIER,
    END_OF_FILE,
};

inline std::string_view token_type_to_str(TokenType type)
{
    // clang-format off
    switch (type) {
    // Symbols
    case TokenType::COMMA: return ",";
    case TokenType::SEMICOLON: return ";";
    case TokenType::PARENTHESIS_LEFT: return "(";
    case TokenType::PARENTHESIS_RIGHT: return ")";
    case TokenType::CURLY_LEFT: return "{";
    case TokenType::CURLY_RIGHT: return "}";
    case TokenType::EQUALS: return "=";

    // Data types
    case TokenType::SAMPLER2D: return "sampler2D";
    case TokenType::SAMPLERCUBE: return "samplerCube";
    case TokenType::INT: return "int";
    case TokenType::FLOAT: return "float";
    case TokenType::VEC2: return "vec2";
    case TokenType::VEC4: return "vec4";

    // Values
    case TokenType::TRUE: return "true";
    case TokenType::FALSE: return "false";
    case TokenType::STRING_LITERAL: return "STRING_LITERAL";
    case TokenType::NUMERIC_LITERAL: return "NUMERIC_LITERAL";

    // Top-level identifiers
    case TokenType::TAGS: return "TAGS";
    case TokenType::PARAMETERS: return "PARAMETERS";
    case TokenType::OPTIONS: return "OPTIONS";
    case TokenType::VERTEX_CODE: return "VERTEX_CODE";
    case TokenType::FRAGMENT_CODE: return "FRAGMENT_CODE";

    // Tags
    case TokenType::OPAQUE: return "OPAQUE";
    case TokenType::UNLIT: return "UNLIT";
    case TokenType::DEFINES_LIGHT_MODEL: return "DEFINES_LIGHT_MODEL";
    case TokenType::DEFINES_VERTEX_PREPROCESS: return "DEFINES_VERTEX_PREPROCESS";

    // Misc
    case TokenType::IDENTIFIER: return "IDENTIFIER";
    case TokenType::END_OF_FILE: return "END_OF_FILE";
    }
    // clang-format on

    MG_ASSERT(false && "Unexpected TokenType");
}

inline Opt<TokenType> get_keyword_type(std::string_view lexeme) noexcept
{
    static constexpr std::array<std::pair<std::string_view, TokenType>, 18> keywords{ {
        { "int", TokenType::INT },
        { "float", TokenType::FLOAT },
        { "vec2", TokenType::VEC2 },
        { "vec4", TokenType::VEC4 },
        { "true", TokenType::TRUE },
        { "false", TokenType::FALSE },
        { "sampler2D", TokenType::SAMPLER2D },
        { "samplerCube", TokenType::SAMPLERCUBE },
        { "PARAMETERS", TokenType::PARAMETERS },
        { "OPTIONS", TokenType::OPTIONS },
        { "VERTEX_CODE", TokenType::VERTEX_CODE },
        { "FRAGMENT_CODE", TokenType::FRAGMENT_CODE },
        { "TAGS", TokenType::TAGS },
        { "UNLIT", TokenType::UNLIT },
        { "OPAQUE", TokenType::OPAQUE },
        { "DEFINES_LIGHT_MODEL", TokenType::DEFINES_LIGHT_MODEL },
        { "DEFINES_VERTEX_PREPROCESS", TokenType::DEFINES_VERTEX_PREPROCESS },
    } };

    for (const auto& [str, token_type] : keywords) {
        if (str == lexeme) {
            return token_type;
        }
    }

    return nullopt;
}

struct Token {
    TokenType type{};
    std::string_view lexeme{};
    std::variant<float, std::string_view> literal_value;
    size_t line{};
};

inline float numeric_value(const Token& token) noexcept
{
    return std::get<float>(token.literal_value);
}

inline std::string_view string_value(const Token& token) noexcept
{
    return std::get<std::string_view>(token.literal_value);
}

} // namespace Mg::shader
