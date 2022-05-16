//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg_lexer.h"

#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/utils/mg_string_utils.h"

#include "fmt/core.h"

namespace Mg::parser {

namespace {

class Lexer {
public:
    std::vector<Token> lex(std::string_view definition)
    {
        m_stream = SimpleInputStream{ definition };
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
        log.error("Error parsing at line {} col {}: {}",
                  m_stream.line,
                  m_stream.pos_in_line,
                  reason);
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

    void string_literal()
    {
        while (m_stream.peek() != '"' && !m_stream.is_at_end()) {
            if (m_stream.peek() == '\n') {
                lex_error("Unexpected line break in string-literal.");
            }
            m_stream.advance();
        }

        m_stream.advance(); // Skip ending quote

        add_token(TokenType::STRING_LITERAL,
                  m_stream.data.substr(token_start + 1, lexeme_length() - 2));
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
        case '"':
            string_literal();
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

std::vector<Token> lex_definition(std::string_view definition)
{
    return Lexer{}.lex(definition);
}

} // namespace Mg::parser
