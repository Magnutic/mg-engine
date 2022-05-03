//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

// X-macros list of parser tokens for Mg Engine.
// See: https://en.wikipedia.org/wiki/X_Macro
// Parameters are: internal_name, string_representation, is_keyword

// Symbols
X(COMMA, ",", false)
X(SEMICOLON, ";", false)
X(PARENTHESIS_LEFT, "(", false)
X(PARENTHESIS_RIGHT, ")", false)
X(CURLY_LEFT, "{", false)
X(CURLY_RIGHT, "}", false)
X(EQUALS, "=", false)

// Values
X(TRUE, "true", true)
X(FALSE, "false", true)
X(STRING_LITERAL, "STRING_LITERAL", false)
X(NUMERIC_LITERAL, "NUMERIC_LITERAL", false)

// Data types
X(SAMPLER2D, "sampler2D", true)
X(SAMPLERCUBE, "samplerCube", true)
X(INT, "int", true)
X(FLOAT, "float", true)
X(VEC2, "vec2", true)
X(VEC4, "vec4", true)

// Top-level identifier for shaders
X(TAGS, "TAGS", true)
X(PARAMETERS, "PARAMETERS", true)
X(OPTIONS, "OPTIONS", true)
X(VERTEX_CODE, "VERTEX_CODE", true)
X(FRAGMENT_CODE, "FRAGMENT_CODE", true)

// Tags for shaders
X(OPAQUE, "OPAQUE", true)
X(UNLIT, "UNLIT", true)
X(DEFINES_LIGHT_MODEL, "DEFINES_LIGHT_MODEL", true)
X(DEFINES_VERTEX_PREPROCESS, "DEFINES_VERTEX_PREPROCESS", true)

// Misc
X(IDENTIFIER, "IDENTIFIER", false)
X(END_OF_FILE, "END_OF_FILE", false)
