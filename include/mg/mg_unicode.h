//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2021, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_unicode.h
 * Unicode utilities.
 */

#pragma once

#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_optional.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace Mg {

/** Result of get_unicode_codepoint_at. */
struct CodepointResult {
    char32_t codepoint;
    uint32_t num_bytes;
    bool result_valid;
};

/** Gets the Unicode code point starting at the given index of a utf-8-encoded string.
 * If the character at the given index is not the start of a utf8-encoded code point, the result
 * will have codepoint==0, num_bytes==1, and result_valid==false.
 */
CodepointResult get_unicode_codepoint_at(std::string_view utf8_string, size_t char_index);

/** Convert utf-8 string to utf-32. If the utf8-string contains invalid data, those bytes are
 * ignored and will not be present in resulting string. Optionally, reports whether there was an
 * error in the `error` parameter.
 */
std::u32string utf8_to_utf32(std::string_view utf8_string, bool* error = nullptr);

enum class UnicodeBlock;

/** Range of unicode codepoints. */
struct UnicodeRange {
    char32_t start;
    uint32_t length;
};

constexpr bool operator<(const UnicodeRange l, const UnicodeRange r)
{
    return l.start < r.start;
}
constexpr bool operator<=(const UnicodeRange l, const UnicodeRange r)
{
    return l.start <= r.start;
}
constexpr bool operator>(const UnicodeRange l, const UnicodeRange r)
{
    return l.start > r.start;
}
constexpr bool operator>=(const UnicodeRange l, const UnicodeRange r)
{
    return l.start >= r.start;
}
constexpr bool operator==(const UnicodeRange l, const UnicodeRange r)
{
    return l.start == r.start && l.length == r.length;
}
constexpr bool operator!=(const UnicodeRange l, const UnicodeRange r)
{
    return l.start != r.start || l.length != r.length;
}

/** Get corresponding range of codepoints. */
UnicodeRange get_unicode_range(UnicodeBlock block);

/** Get whether UnicodeRange contains the given codepoint. */
inline bool contains_codepoint(const UnicodeRange& ur, char32_t codepoint)
{
    return ur.start <= codepoint && ur.start + ur.length > codepoint;
}

/** Get whether UnicodeBlock contains the given codepoint. */
inline bool contains_codepoint(const UnicodeBlock& block, char32_t codepoint)
{
    return contains_codepoint(get_unicode_range(block), codepoint);
}

/** Find the block that contains the given codepoint. */
Opt<UnicodeBlock> unicode_block_containing(char32_t codepoint);

// Deleted overload to prevent mistakes with implicit conversions.
void unicode_block_containing(char codepoint) = delete;

/** Get all unicode ranges needed to represent the codepoints present in text. */
std::vector<UnicodeRange> unicode_ranges_for(std::u32string_view text);

/** Get all unicode ranges needed to represent the codepoints present in text_utf8. */
std::vector<UnicodeRange> unicode_ranges_for(std::string_view text_utf8);

/** Merge UncodeRanges that overlap (or can be merged to one longer range). */
std::vector<UnicodeRange> merge_overlapping_ranges(std::span<const UnicodeRange> unicode_ranges);

/** Unicode block: each block corresponds to a range of codepoints. */
enum class UnicodeBlock {
    Basic_Latin,
    Latin_1_Supplement,
    Latin_Extended_A,
    Latin_Extended_B,
    IPA_Extensions,
    Spacing_Modifier_Letters,
    Combining_Diacritical_Marks,
    Greek_and_Coptic,
    Cyrillic,
    Cyrillic_Supplement,
    Armenian,
    Hebrew,
    Arabic,
    Syriac,
    Arabic_Supplement,
    Thaana,
    NKo,
    Samaritan,
    Mandaic,
    Devanagari,
    Bengali,
    Gurmukhi,
    Gujarati,
    Oriya,
    Tamil,
    Telugu,
    Kannada,
    Malayalam,
    Sinhala,
    Thai,
    Lao,
    Tibetan,
    Myanmar,
    Georgian,
    Hangul_Jamo,
    Ethiopic,
    Ethiopic_Supplement,
    Cherokee,
    Unified_Canadian_Aboriginal_Syllabics,
    Ogham,
    Runic,
    Tagalog,
    Hanunoo,
    Buhid,
    Tagbanwa,
    Khmer,
    Mongolian,
    Unified_Canadian_Aboriginal_Syllabics_Extended,
    Limbu,
    Tai_Le,
    New_Tai_Lue,
    Khmer_Symbols,
    Buginese,
    Tai_Tham,
    Balinese,
    Sundanese,
    Batak,
    Lepcha,
    Ol_Chiki,
    Vedic_Extensions,
    Phonetic_Extensions,
    Phonetic_Extensions_Supplement,
    Combining_Diacritical_Marks_Supplement,
    Latin_Extended_Additional,
    Greek_Extended,
    General_Punctuation,
    Superscripts_and_Subscripts,
    Currency_Symbols,
    Combining_Diacritical_Marks_for_Symbols,
    Letterlike_Symbols,
    Number_Forms,
    Arrows,
    Mathematical_Operators,
    Miscellaneous_Technical,
    Control_Pictures,
    Optical_Character_Recognition,
    Enclosed_Alphanumerics,
    Box_Drawing,
    Block_Elements,
    Geometric_Shapes,
    Miscellaneous_Symbols,
    Dingbats,
    Miscellaneous_Mathematical_Symbols_A,
    Supplemental_Arrows_A,
    Braille_Patterns,
    Supplemental_Arrows_B,
    Miscellaneous_Mathematical_Symbols_B,
    Supplemental_Mathematical_Operators,
    Miscellaneous_Symbols_and_Arrows,
    Glagolitic,
    Latin_Extended_C,
    Coptic,
    Georgian_Supplement,
    Tifinagh,
    Ethiopic_Extended,
    Cyrillic_Extended_A,
    Supplemental_Punctuation,
    CJK_Radicals_Supplement,
    Kangxi_Radicals,
    Ideographic_Description_Characters,
    CJK_Symbols_and_Punctuation,
    Hiragana,
    Katakana,
    Bopomofo,
    Hangul_Compatibility_Jamo,
    Kanbun,
    Bopomofo_Extended,
    CJK_Strokes,
    Katakana_Phonetic_Extensions,
    Enclosed_CJK_Letters_and_Months,
    CJK_Compatibility,
    CJK_Unified_Ideographs_Extension_A,
    Yijing_Hexagram_Symbols,
    CJK_Unified_Ideographs,
    Yi_Syllables,
    Yi_Radicals,
    Lisu,
    Vai,
    Cyrillic_Extended_B,
    Bamum,
    Modifier_Tone_Letters,
    Latin_Extended_D,
    Syloti_Nagri,
    Common_Indic_Number_Forms,
    Phags_pa,
    Saurashtra,
    Devanagari_Extended,
    Kayah_Li,
    Rejang,
    Hangul_Jamo_Extended_A,
    Javanese,
    Cham,
    Myanmar_Extended_A,
    Tai_Viet,
    Ethiopic_Extended_A,
    Meetei_Mayek,
    Hangul_Syllables,
    Hangul_Jamo_Extended_B,
    High_Surrogates,
    High_Private_Use_Surrogates,
    Low_Surrogates,
    Private_Use_Area,
    CJK_Compatibility_Ideographs,
    Alphabetic_Presentation_Forms,
    Arabic_Presentation_Forms_A,
    Variation_Selectors,
    Vertical_Forms,
    Combining_Half_Marks,
    CJK_Compatibility_Forms,
    Small_Form_Variants,
    Arabic_Presentation_Forms_B,
    Halfwidth_and_Fullwidth_Forms,
    Specials,
    Linear_B_Syllabary,
    Linear_B_Ideograms,
    Aegean_Numbers,
    Ancient_Greek_Numbers,
    Ancient_Symbols,
    Phaistos_Disc,
    Lycian,
    Carian,
    Old_Italic,
    Gothic,
    Ugaritic,
    Old_Persian,
    Deseret,
    Shavian,
    Osmanya,
    Cypriot_Syllabary,
    Imperial_Aramaic,
    Phoenician,
    Lydian,
    Kharoshthi,
    Old_South_Arabian,
    Avestan,
    Inscriptional_Parthian,
    Inscriptional_Pahlavi,
    Old_Turkic,
    Rumi_Numeral_Symbols,
    Brahmi,
    Kaithi,
    Cuneiform,
    Cuneiform_Numbers_and_Punctuation,
    Egyptian_Hieroglyphs,
    Bamum_Supplement,
    Kana_Supplement,
    Byzantine_Musical_Symbols,
    Musical_Symbols,
    Ancient_Greek_Musical_Notation,
    Tai_Xuan_Jing_Symbols,
    Counting_Rod_Numerals,
    Mathematical_Alphanumeric_Symbols,
    Mahjong_Tiles,
    Domino_Tiles,
    Playing_Cards,
    Enclosed_Alphanumeric_Supplement,
    Enclosed_Ideographic_Supplement,
    Miscellaneous_Symbols_And_Pictographs,
    Emoticons,
    Transport_And_Map_Symbols,
    Alchemical_Symbols,
    CJK_Unified_Ideographs_Extension_B,
    CJK_Unified_Ideographs_Extension_C,
    CJK_Unified_Ideographs_Extension_D,
    CJK_Compatibility_Ideographs_Supplement,
    Tags,
    Variation_Selectors_Supplement
};

} // namespace Mg
