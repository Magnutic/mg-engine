//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_parser_token.h
 * Token types for resource parser.
 */

#pragma once

#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_optional.h"

#include <array>
#include <string_view>
#include <variant>

namespace Mg::parser {

/** Token type generated when lexing Mg resource files. */
enum class TokenType {
#define X(token_enum, token_string, is_keyword) token_enum,
#include "mg_tokens.h"
#undef X
};


inline std::string_view token_type_to_str(TokenType type)
{
    switch (type) {
#define X(token_enum, token_string, is_keyword) \
    case TokenType::token_enum:                 \
        return token_string;
#include "mg_tokens.h"
#undef X
    }

    MG_ASSERT(false && "Unexpected TokenType");
}

// Get what type of keyword the lexeme represents, if any.
inline Opt<TokenType> get_keyword_type(std::string_view lexeme) noexcept
{
    using KeywordElement = std::tuple<std::string_view, TokenType, bool>;

    static constexpr std::array keywords{
#define X(token_enum, token_string, is_keyword) \
    KeywordElement{ token_string, TokenType::token_enum, is_keyword },
#include "mg_tokens.h"
#undef X
    };

    for (const auto& [str, token_type, is_keyword] : keywords) {
        if (is_keyword && str == lexeme) {
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

inline float numeric_value(const Token& token)
{
    return std::get<float>(token.literal_value);
}

inline std::string_view string_value(const Token& token)
{
    return std::get<std::string_view>(token.literal_value);
}

} // namespace Mg::parser
