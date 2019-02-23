//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
// Copyright (c) 2018 Magnus Bergsten
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

#include "mg/resources/mg_shader_resource.h"

#include "mg/core/mg_file_loader.h"
#include "mg/core/mg_resource_exceptions.h"
#include "mg/core/mg_resource_loading_input.h"
#include "mg/resources/mg_text_resource.h"
#include "mg/utils/mg_stl_helpers.h"
#include "mg/utils/mg_string_utils.h"

#include <fmt/core.h>

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace fs = std::filesystem;

// The shader-file parser is quite messy and I am not a big fan of how it turned out (in particular,
// I do not like the use of exceptions). However, it has turned out suprisingly tricky to refactor
// it into something nicer. I should look at some examples on how to write parsers elegantly and
// imitate that, at some point.

namespace Mg {

enum class TokenType {
    // Symbols
    COMMA,
    SEMICOLON,
    PARENTHESIS_LEFT,
    PARENTHESIS_RIGHT,
    CURLY_LEFT,
    CURLY_RIGHT,
    EQUALS,
    HASH,

    // Values
    TRUE,
    FALSE,
    STRING_LITERAL,
    NUMERIC_LITERAL,

    // Data types
    SAMPLER2D,
    SAMPLERCUBE,
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
    INCLUDE,
    END_OF_FILE,
};

static std::string_view token_type_to_str(TokenType type)
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
    case TokenType::HASH: return "#";

    // Data types
    case TokenType::SAMPLER2D: return "sampler2D";
    case TokenType::SAMPLERCUBE: return "samplerCube";
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
    case TokenType::INCLUDE: return "include";
    case TokenType::END_OF_FILE: return "END_OF_FILE";
    }
    // clang-format on

    MG_ASSERT(false && "Unexpected TokenType");
}

constexpr std::array<std::pair<std::string_view, TokenType>, 18> keywords{ {
    { "float", TokenType::FLOAT },
    { "vec2", TokenType::VEC2 },
    { "vec4", TokenType::VEC4 },
    { "true", TokenType::TRUE },
    { "false", TokenType::FALSE },
    { "sampler2D", TokenType::SAMPLER2D },
    { "samplerCube", TokenType::SAMPLERCUBE },
    { "include", TokenType::INCLUDE },
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

inline std::optional<TokenType> get_keyword_type(std::string_view lexeme)
{
    for (auto&& [str, token_type] : keywords) {
        if (str == lexeme) { return token_type; }
    }
    return std::nullopt;
}

struct Token {
    TokenType                             type{};
    std::string_view                      lexeme{};
    std::variant<float, std::string_view> literal_value;
    size_t                                line{};
};

float numeric_value(const Token& token)
{
    return std::get<float>(token.literal_value);
}

inline std::string_view string_value(const Token& token)
{
    return std::get<std::string_view>(token.literal_value);
}

std::vector<Token> lex(std::string_view code);

struct LexerState {
    SimpleInputStream  stream;
    std::vector<Token> tokens;

    size_t line        = 1;
    size_t token_start = 0;
};

inline void lex_error(LexerState& lex, std::string_view reason)
{
    throw std::runtime_error(fmt::format("Error parsing at line {}: {}", lex.line, reason));
}

inline size_t lexeme_length(LexerState& lex)
{
    return lex.stream.pos - lex.token_start;
}

template<typename T = float>
inline void add_token(LexerState& lex, TokenType type, T literal_value = {})
{
    std::string_view lexeme = lex.stream.data.substr(lex.token_start, lexeme_length(lex));
    lex.tokens.push_back({ type, lexeme, literal_value, lex.line });
}

inline void string_literal(LexerState& lex)
{
    while (lex.stream.peek() != '"' && !lex.stream.is_at_end()) {
        if (lex.stream.peek() == '\n') {
            lex_error(lex, "Unexpected line break in string-literal.");
        }
        lex.stream.advance();
    }

    lex.stream.advance(); // Skip ending quote

    add_token(lex,
              TokenType::STRING_LITERAL,
              lex.stream.data.substr(lex.token_start + 1, lexeme_length(lex) - 2));
}

inline bool is_digit(char c)
{
    return (c >= '0' && c <= '9');
}

inline bool is_alphanumeric(char c)
{
    return ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || is_digit(c));
}

inline void numeric_literal(LexerState& lex)
{
    while (is_digit(lex.stream.peek()) || lex.stream.peek() == '.') { lex.stream.advance(); }

    auto number_str       = lex.stream.data.substr(lex.token_start, lexeme_length(lex));
    auto [success, value] = string_to<float>(number_str);
    MG_ASSERT(success);
    add_token(lex, TokenType::NUMERIC_LITERAL, value);
}

inline void identifier(LexerState& lex)
{
    while (is_alphanumeric(lex.stream.peek())) { lex.stream.advance(); }

    auto lexeme         = lex.stream.data.substr(lex.token_start, lexeme_length(lex));
    auto opt_token_type = get_keyword_type(lexeme);

    if (opt_token_type.has_value()) { add_token(lex, *opt_token_type); }
    else {
        add_token(lex, TokenType::IDENTIFIER, lexeme);
    }
}

inline void next_token(LexerState& lex)
{
    char c = lex.stream.advance();
    switch (c) {
    case ' ':
        break;
    case '\t':
        break;
    case '\r':
        break;
    case '\n':
        ++lex.line;
        break;
    case ',':
        add_token(lex, TokenType::COMMA);
        break;
    case ';':
        add_token(lex, TokenType::SEMICOLON);
        break;
    case '(':
        add_token(lex, TokenType::PARENTHESIS_LEFT);
        break;
    case ')':
        add_token(lex, TokenType::PARENTHESIS_RIGHT);
        break;
    case '{':
        add_token(lex, TokenType::CURLY_LEFT);
        break;
    case '}':
        add_token(lex, TokenType::CURLY_RIGHT);
        break;
    case '=':
        add_token(lex, TokenType::EQUALS);
        break;
    case '#':
        add_token(lex, TokenType::HASH);
        break;
    case '"':
        string_literal(lex);
        break;
    case '/':
        if (lex.stream.peek() == '/') {
            while (lex.stream.advance() != '\n' && !lex.stream.is_at_end()) {}
            break;
        }
        [[fallthrough]];
    default:
        if (is_digit(c)) {
            numeric_literal(lex);
            break;
        }
        if (is_alphanumeric(c)) {
            identifier(lex);
            break;
        }
        lex_error(lex, fmt::format("Unexpected character: {}", c));
    }
}

inline std::vector<Token> lex(std::string_view code)
{
    LexerState lex{ SimpleInputStream{ code }, {} };

    while (!lex.stream.is_at_end()) {
        lex.token_start = lex.stream.pos;
        next_token(lex);
    }

    add_token(lex, TokenType::END_OF_FILE);
    return std::move(lex.tokens);
}

class Parser {
public:
    explicit Parser(std::string_view input)
        : m_tokens(lex(input)), m_current_token{ m_tokens.begin() }
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
            parse_code_block(ShaderBlockType::Vertex);
            break;
        case TokenType::FRAGMENT_CODE:
            parse_code_block(ShaderBlockType::Fragment);
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
        throw std::runtime_error(
            fmt::format("Parse error at line {}: {} [parsing '{}']", t.line, reason, t.lexeme));
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
        auto&             type_token = next_token();
        ShaderSamplerType sampler_type{};

        switch (type_token.type) {
        case TokenType::SAMPLER2D:
            sampler_type = ShaderSamplerType::Sampler2D;
            break;
        case TokenType::SAMPLERCUBE:
            sampler_type = ShaderSamplerType::SamplerCube;
            break;
        default:
            parse_error("Unexpected token (expected sampler2D or samplerCube).", type_token);
        }

        std::string_view identifier = parse_identifier();

        expect_next(TokenType::SEMICOLON);

        ShaderResource::Sampler s{ Identifier::from_runtime_string(identifier), sampler_type };
        samplers.push_back(s);
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

        ShaderResource::Parameter p{};
        p.name = Identifier::from_runtime_string(id);

        switch (type_token.type) {
        case TokenType::FLOAT:
            p.type    = ShaderParameterType::Float;
            p.value.x = parse_numeric();
            break;
        case TokenType::VEC2:
            p.type = ShaderParameterType::Vec2;
            expect_next(TokenType::VEC2);
            expect_next(TokenType::PARENTHESIS_LEFT);
            p.value.x = parse_numeric();
            expect_next(TokenType::COMMA);
            p.value.y = parse_numeric();
            expect_next(TokenType::PARENTHESIS_RIGHT);
            break;
        case TokenType::VEC4:
            p.type = ShaderParameterType::Vec4;
            expect_next(TokenType::VEC4);
            expect_next(TokenType::PARENTHESIS_LEFT);
            p.value.x = parse_numeric();
            expect_next(TokenType::COMMA);
            p.value.y = parse_numeric();
            expect_next(TokenType::COMMA);
            p.value.z = parse_numeric();
            expect_next(TokenType::COMMA);
            p.value.w = parse_numeric();
            expect_next(TokenType::PARENTHESIS_RIGHT);
            break;
        default:
            parse_error("Unexpected token, expected parameter type (float|vec2|vec4).", type_token);
        }

        expect_next(TokenType::SEMICOLON);
        parameters.push_back(p);
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

        options.push_back({ Identifier::from_runtime_string(option_name), default_value });
    }

    void parse_tags_block()
    {
        auto parse_tag = [this] {
            auto& tag_token = next_token();
            switch (tag_token.type) {
            case TokenType::UNLIT:
                tags |= ShaderTag::UNLIT;
                break;
            case TokenType::OPAQUE:
                tags |= ShaderTag::OPAQUE;
                break;
            case TokenType::DEFINES_LIGHT_MODEL:
                tags |= ShaderTag::DEFINES_LIGHT_MODEL;
                break;
            case TokenType::DEFINES_VERTEX_PREPROCESS:
                tags |= ShaderTag::DEFINES_VERTEX_PREPROCESS;
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

    void parse_code_block_statement(ShaderBlockType shader_type)
    {
        std::string_view include_filename;

        auto& t = next_token();

        switch (t.type) {
        case TokenType::HASH:
            expect_next(TokenType::INCLUDE);
            include_filename = parse_string_literal();
            break;
        default:
            parse_error("Unexpected token.", t);
        }

        switch (shader_type) {
        case ShaderBlockType::Vertex:
            vertex_includes.emplace_back(include_filename);
            break;
        case ShaderBlockType::Fragment:
            fragment_includes.emplace_back(include_filename);
            break;
        }
    }

    void parse_code_block(ShaderBlockType shader_type)
    {
        parse_block([this, shader_type] { parse_code_block_statement(shader_type); });
    }

    const Token& next_token()
    {
        auto& ret_val = peek_token();
        ++m_current_token;
        return ret_val;
    }

    const Token& peek_token()
    {
        if (m_current_token == m_tokens.end()) {
            throw std::runtime_error("Parse error: unexpected end of file.");
        }
        return *m_current_token;
    }

    const Token& expect_next(TokenType                       expected_type,
                             std::optional<std::string_view> additional_message = std::nullopt)
    {
        auto& t = next_token();
        if (t.type != expected_type) {
            if (additional_message.has_value()) {
                parse_error(fmt::format("Expected {} ({})",
                                        token_type_to_str(expected_type),
                                        *additional_message),
                            t);
            }
            parse_error(fmt::format("Expected {}.", token_type_to_str(expected_type)), t);
        }

        return t;
    }

    ShaderTag::Value tags = {};

    std::vector<std::string> vertex_includes;
    std::vector<std::string> fragment_includes;

    std::vector<ShaderResource::Sampler>   samplers;
    std::vector<ShaderResource::Parameter> parameters;
    std::vector<ShaderResource::Option>    options;

private:
    std::vector<Token>           m_tokens;
    decltype(m_tokens)::iterator m_current_token{};
};

// Used delimit sections in assembled shader code.
static constexpr auto k_delimiter_comment = R"(
//==============================================================================
// Origin: {}
//==============================================================================

)";

// Helper for ShaderResource::load_resource. Assemble shader code by loading included code files.
inline std::string assemble_shader_code(const fs::path&             include_directory,
                                        span<std::string>           include_files,
                                        const ResourceLoadingInput& input)
{
    std::string code;
    code.reserve(1024);

    for (auto&& include_file : include_files) {
        // Get include file path relative to resource root directory
        auto include_path = (include_directory / include_file).u8string();

        // Add origin tracking comment (helps debugging shader)
        code += fmt::format(k_delimiter_comment, include_path);

        // Load include file as a dependency of this resource.
        Identifier     path_identifier     = Identifier::from_runtime_string(include_path);
        ResourceHandle include_file_handle = input.load_dependency<TextResource>(path_identifier);
        ResourceAccessGuard include_access(include_file_handle);
        code += include_access->text();
    }

    return code;
}

LoadResourceResult ShaderResource::load_resource_impl(const ResourceLoadingInput& input)
{
    std::string_view shader_description = input.resource_data_as_text();

    // I would prefer not to use exceptions to catch parse errors here but changing the parsing code
    // to not use exceptions seems suprisingly tricky.
    try {
        Parser parser{ shader_description };
        m_parameters = Array<Parameter>::make_copy(parser.parameters);
        m_samplers   = Array<Sampler>::make_copy(parser.samplers);
        m_options    = Array<Option>::make_copy(parser.options);

        // Get directory of shader file so that #include directives search relative to that path.
        fs::path include_path = fs::path{ resource_id().str_view() }.parent_path();

        m_vertex_code   = assemble_shader_code(include_path, parser.vertex_includes, input);
        m_fragment_code = assemble_shader_code(include_path, parser.fragment_includes, input);

        // Sort parameters so that larger types come first (for the sake of alignment)
        sort(m_parameters, [](const Parameter& l, const Parameter& r) {
            return static_cast<int>(l.type) > static_cast<int>(r.type);
        });

        m_tags = parser.tags;
    }
    catch (const std::exception& e) {
        return LoadResourceResult::data_error(e.what());
    }

    return LoadResourceResult::success();
}

std::string ShaderResource::debug_print() const
{
    std::ostringstream oss;

    oss << "ShaderResource '" << resource_id() << "'\n";

    oss << "Options:\n";
    for (auto&& o : options()) { oss << "\t" << o.name << "\n"; }

    oss << "Parameters:\n";
    for (auto&& p : parameters()) {
        oss << "\t" << shader_parameter_type_to_string(p.type) << " " << p.name << "\n";
    }

    oss << "Samplers:\n";
    for (auto&& s : samplers()) {
        oss << "\t" << shader_sampler_type_to_string(s.type) << " " << s.name << "\n";
    }

    return oss.str();
}

} // namespace Mg
