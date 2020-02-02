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

#include "mg_shader_lexer.h"

#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/utils/mg_string_utils.h"

#include "fmt/core.h"

namespace Mg::shader {

namespace {

class ShaderLexer {
public:
    std::vector<Token> lex(std::string_view shader_resource_definition)
    {
        m_stream = SimpleInputStream{ shader_resource_definition };
        m_tokens.clear();

        while (!m_stream.is_at_end()) {
            token_start = m_stream.pos;
            next_token();
        }

        add_token(TokenType::END_OF_FILE);
        return std::move(m_tokens);
    }

private:
    SimpleInputStream m_stream{ "" };
    std::vector<Token> m_tokens;

    size_t token_start = 0;
    void lex_error(std::string_view reason)
    {
        g_log.write_error(fmt::format("Error parsing at line {} col {}: {}",
                                      m_stream.line,
                                      m_stream.pos_in_line,
                                      reason));
        throw RuntimeError();
    }

    void skip_whitespace()
    {
        while (is_whitespace(m_stream.peek())) {
            m_stream.advance();
        }
    };

    size_t lexeme_length() const noexcept { return m_stream.pos - token_start; }

    template<typename T = float> void add_token(TokenType type, T literal_value = {})
    {
        const std::string_view lexeme = m_stream.data.substr(token_start, lexeme_length());
        m_tokens.push_back({ type, lexeme, literal_value, m_stream.line });
    }

    void numeric_literal()
    {
        while (is_ascii_digit(m_stream.peek()) || m_stream.peek() == '.') {
            m_stream.advance();
        }

        const auto number_str = m_stream.data.substr(token_start, lexeme_length());
        const auto [success, value] = string_to<float>(number_str);
        MG_ASSERT(success);
        add_token(TokenType::NUMERIC_LITERAL, value);
    }

    void code_block_literal(TokenType type)
    {
        skip_whitespace();

        if (!m_stream.match('{')) {
            lex_error(fmt::format("Expected {{ after {}", token_type_to_str(type)));
        }

        // Scan until we reach a closing '}' (while counting the brace-level, since GLSL code may
        // also include braces).
        const auto code_start_pos = m_stream.pos;

        for (size_t brace_level = 1; brace_level > 0; m_stream.advance()) {
            const char c = m_stream.peek();
            if (c == '{') {
                ++brace_level;
            }
            if (c == '}') {
                --brace_level;
            }
            if (c == '\0') {
                lex_error(fmt::format("Unexpected end-of-file in code block."));
            }
        }

        const auto code_length = m_stream.pos - code_start_pos - 1;

        std::string_view code_block_content = m_stream.data.substr(code_start_pos, code_length);
        add_token(type, code_block_content);
    }

    void identifier()
    {
        while (is_ascii_alphanumeric(m_stream.peek())) {
            m_stream.advance();
        }

        auto lexeme = m_stream.data.substr(token_start, lexeme_length());

        // According to the following source, memory layout for vec3 does not follow the
        // specification with some drivers. To prevent portability issues, the use of vec3 is
        // unsupported.
        // https://www.khronos.org/opengl/wiki/Interface_Block_(GLSL)#Memory_layout
        if (lexeme == "vec3") {
            lex_error(
                "vec3 is unsupported due to driver inconsistencies. Please use vec4 instead.");
        }

        auto opt_token_type = get_keyword_type(lexeme);

        if (opt_token_type) {
            const TokenType token_type = opt_token_type.value();

            if (token_type == TokenType::VERTEX_CODE || token_type == TokenType::FRAGMENT_CODE) {
                code_block_literal(token_type);
                return;
            }
        }

        opt_token_type.map_or_else([&](TokenType type) { add_token(type); },
                                   [&] { add_token(TokenType::IDENTIFIER, lexeme); });
    }

    void next_token()
    {
        const char c = m_stream.advance();
        switch (c) {
        case ' ':
            break;
        case '\t':
            break;
        case '\r':
            break;
        case '\n':
            break;
        case ',':
            add_token(TokenType::COMMA);
            break;
        case ';':
            add_token(TokenType::SEMICOLON);
            break;
        case '(':
            add_token(TokenType::PARENTHESIS_LEFT);
            break;
        case ')':
            add_token(TokenType::PARENTHESIS_RIGHT);
            break;
        case '{':
            add_token(TokenType::CURLY_LEFT);
            break;
        case '}':
            add_token(TokenType::CURLY_RIGHT);
            break;
        case '=':
            add_token(TokenType::EQUALS);
            break;
        case '/':
            if (m_stream.peek() == '/') {
                while (m_stream.advance() != '\n' && !m_stream.is_at_end()) {
                }
                break;
            }
            [[fallthrough]];
        default:
            if (is_ascii_digit(c)) {
                numeric_literal();
                break;
            }
            if (is_ascii_alphanumeric(c)) {
                identifier();
                break;
            }
            lex_error(fmt::format("Unexpected character: {}", c));
        }
    }
};

} // namespace

std::vector<Token> lex_shader_definition(std::string_view shader_resource_definition)
{
    return ShaderLexer{}.lex(shader_resource_definition);
}

} // namespace Mg::shader
