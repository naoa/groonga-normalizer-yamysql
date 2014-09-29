/* -*- c-basic-offset: 2 -*- */
/*
  Copyright(C) 2014 Naoya Murakami <naoya@createfield.com>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; version 2
  of the License.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the Free
  Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
  MA 02110-1301, USA

  This file includes the Groonga MySQL normalizer code.
  https://github.com/groonga/groonga-normalizer-mysql/blob/master/normalizers/mysql.c

  The following is the header of the file:

    Copyright(C) 2013-2014  Kouhei Sutou <kou@clear-code.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; version 2
    of the License.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
    MA 02110-1301, USA
*/

#include <groonga/normalizer.h>
#include <groonga/nfkc.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "mysql_custom_kana_ci_table.h"
#include "mysql_custom_table.h"

#include <mecab.h>

#ifdef __GNUC__
#  define GNUC_UNUSED __attribute__((__unused__))
#else
#  define GNUC_UNUSED
#endif

#ifdef _MSC_VER
#  define inline _inline
#  define snprintf _snprintf
#endif

#define SNIPPET_BUFFER_SIZE 256

static mecab_t *sole_mecab = NULL;
static grn_plugin_mutex *sole_mecab_mutex = NULL;
static grn_encoding sole_mecab_encoding = GRN_ENC_NONE;

#define DEFAULT_MECAB_PARSE_LIMIT 1200000
#define DEFAULT_RFIND_PUNCT_OFFSET 300

#define STOPWORDS_TABLE_NAME "@yamysql_stopwords"
#define STOPWORDS_TABLE_NAME_MRN "@0040yamysql_stopwords"
#define PARTOFSPEECH_TABLE_NAME "@yamysql_partofspeech"
#define PARTOFSPEECH_TABLE_NAME_MRN "@0040yamysql_partofspeech"

static grn_hash *default_pos_table = NULL;

static grn_encoding
translate_mecab_charset_to_grn_encoding(const char *charset)
{
  if (strcasecmp(charset, "euc-jp") == 0) {
    return GRN_ENC_EUC_JP;
  } else if (strcasecmp(charset, "utf-8") == 0 ||
             strcasecmp(charset, "utf8") == 0) {
    return GRN_ENC_UTF8;
  } else if (strcasecmp(charset, "shift_jis") == 0 ||
             strcasecmp(charset, "shift-jis") == 0 ||
             strcasecmp(charset, "sjis") == 0) {
    return GRN_ENC_SJIS;
  }
  return GRN_ENC_NONE;
}

static grn_encoding
get_mecab_encoding(mecab_t *mecab)
{
  grn_encoding encoding = GRN_ENC_NONE;
  const mecab_dictionary_info_t *dictionary_info;
  dictionary_info = mecab_dictionary_info(mecab);
  if (dictionary_info) {
    const char *charset = dictionary_info->charset;
    encoding = translate_mecab_charset_to_grn_encoding(charset);
  }
  return encoding;
}

static int
rfind_punct(grn_ctx *ctx, grn_encoding encoding,
            const char *string, int start, int offset_limit, int end)
{
  int punct_position;
  int char_length;
  const char *string_top;
  const char *string_tail;

  if (offset_limit < start) {
    offset_limit = start;
  }
  string_top = string + offset_limit;

  for (string_tail = string + end;
       string_tail > string_top; string_tail -= char_length) {
    char_length = grn_plugin_charlen(ctx, (char *)string_tail,
                                     end - start, encoding);
    if (string_tail + char_length &&
        (ispunct(*string_tail) ||
         !memcmp(string_tail, "。", char_length) ||
         !memcmp(string_tail, "、", char_length) )) {
      break;
    }
  }
  punct_position = string_tail - string;
  if (punct_position <= start) {
    punct_position = end;
  }
  return punct_position;
}

static int
check_euc(const unsigned char *x, int y)
{
  const unsigned char *p;
  for (p = x + y - 1; p >= x && *p >= 0x80U; p--);
  return (int) ((x + y - p) & 1);
}

static int
check_sjis(const unsigned char *x, int y)
{
  const unsigned char *p;
  for (p = x + y - 1; p >= x; p--)
  if ((*p < 0x81U) || (*p > 0x9fU && *p < 0xe0U) || (*p > 0xfcU))
    break;
  return (int) ((x + y - p) & 1);
}

static int
rfind_lastbyte(GNUC_UNUSED grn_ctx *ctx, grn_encoding encoding,
               const char *string, int offset)
{
  switch (encoding) {
  case GRN_ENC_EUC_JP:
    while (!(check_euc((unsigned char *) string, offset))) {
      offset -= 1;
    }
    break;
  case GRN_ENC_SJIS:
    while (!(check_sjis((unsigned char *) string, offset))) {
      offset -= 1;
    }
    break;
  case GRN_ENC_UTF8:
    while (offset && string[offset] <= (char)0xc0) {
      offset -= 1;
    }
    break;
  default:
    break;
  }
  return offset;
}

static const mecab_node_t*
split_mecab_sparse_node(grn_ctx *ctx, mecab_t *mecab, grn_encoding encoding,
                        unsigned int parse_limit, unsigned int rfind_punct_offset,
                        const char *string, unsigned int string_length,
                        unsigned int *parsed_string_length)
{
  const mecab_node_t *node;
  if (string_length < parse_limit) {
    node = mecab_sparse_tonode2(mecab, string, string_length);
    *parsed_string_length = string_length;
  } else {
    int splitted_string_end = parse_limit;
    unsigned int splitted_string_length;
    int punct_position = 0;
    splitted_string_end = rfind_lastbyte(ctx, encoding,
                                         string,
                                         splitted_string_end);
    if (splitted_string_end == 0) {
      splitted_string_end = parse_limit;
    }
    punct_position = rfind_punct(ctx, encoding,
                                 string,
                                 0,
                                 splitted_string_end - rfind_punct_offset,
                                 splitted_string_end);
    splitted_string_length = punct_position;
    node = mecab_sparse_tonode2(mecab, string, splitted_string_length);
    *parsed_string_length = splitted_string_length;
  }
  return node;
}


typedef grn_bool (*normalizer_func)(grn_ctx *ctx,
                                    const char *utf8,
                                    int *character_length,
                                    int rest_length,
                                    uint32_t **normalize_table,
                                    char *normalized,
                                    unsigned int *normalized_characer_length,
                                    unsigned int *normalized_length_in_bytes,
                                    unsigned int *normalized_n_characters);

static inline unsigned int
unichar_to_utf8(uint32_t unichar, char *output)
{
  unsigned int n_bytes;

  if (unichar < 0x80) {
    output[0] = unichar;
    n_bytes = 1;
  } else if (unichar < 0x0800) {
    output[0] = ((unichar >> 6) & 0x1f) | 0xc0;
    output[1] = (unichar & 0x3f) | 0x80;
    n_bytes = 2;
  } else if (unichar < 0x10000) {
    output[0] = (unichar >> 12) | 0xe0;
    output[1] = ((unichar >> 6) & 0x3f) | 0x80;
    output[2] = (unichar & 0x3f) | 0x80;
    n_bytes = 3;
  } else if (unichar < 0x200000) {
    output[0] = (unichar >> 18) | 0xf0;
    output[1] = ((unichar >> 12) & 0x3f) | 0x80;
    output[2] = ((unichar >> 6) & 0x3f) | 0x80;
    output[3] = (unichar & 0x3f) | 0x80;
    n_bytes = 4;
  } else if (unichar < 0x4000000) {
    output[0] = (unichar >> 24) | 0xf8;
    output[1] = ((unichar >> 18) & 0x3f) | 0x80;
    output[2] = ((unichar >> 12) & 0x3f) | 0x80;
    output[3] = ((unichar >> 6) & 0x3f) | 0x80;
    output[4] = (unichar & 0x3f) | 0x80;
    n_bytes = 5;
  } else {
    output[0] = (unichar >> 30) | 0xfc;
    output[1] = ((unichar >> 24) & 0x3f) | 0x80;
    output[2] = ((unichar >> 18) & 0x3f) | 0x80;
    output[3] = ((unichar >> 12) & 0x3f) | 0x80;
    output[4] = ((unichar >> 6) & 0x3f) | 0x80;
    output[5] = (unichar & 0x3f) | 0x80;
    n_bytes = 6;
  }

  return n_bytes;
}

static inline uint32_t
utf8_to_unichar(const char *utf8, int byte_size)
{
  uint32_t unichar;
  const unsigned char *bytes = (const unsigned char *)utf8;

  switch (byte_size) {
  case 1 :
    unichar = bytes[0] & 0x7f;
    break;
  case 2 :
    unichar = ((bytes[0] & 0x1f) << 6) + (bytes[1] & 0x3f);
    break;
  case 3 :
    unichar =
      ((bytes[0] & 0x0f) << 12) +
      ((bytes[1] & 0x3f) << 6) +
      ((bytes[2] & 0x3f));
    break;
  case 4 :
    unichar =
      ((bytes[0] & 0x07) << 18) +
      ((bytes[1] & 0x3f) << 12) +
      ((bytes[2] & 0x3f) << 6) +
      ((bytes[3] & 0x3f));
    break;
  case 5 :
    unichar =
      ((bytes[0] & 0x03) << 24) +
      ((bytes[1] & 0x3f) << 18) +
      ((bytes[2] & 0x3f) << 12) +
      ((bytes[3] & 0x3f) << 6) +
      ((bytes[4] & 0x3f));
    break;
  case 6 :
    unichar =
      ((bytes[0] & 0x01) << 30) +
      ((bytes[1] & 0x3f) << 24) +
      ((bytes[2] & 0x3f) << 18) +
      ((bytes[3] & 0x3f) << 12) +
      ((bytes[4] & 0x3f) << 6) +
      ((bytes[5] & 0x3f));
    break;
  default :
    unichar = 0;
    break;
  }

  return unichar;
}

static inline void
decompose_character(const char *rest, int character_length,
                    int *page, uint32_t *low_code)
{
  switch (character_length) {
  case 1 :
    *page = 0x00;
    *low_code = rest[0] & 0x7f;
    break;
  case 2 :
    *page = (rest[0] & 0x1c) >> 2;
    *low_code = ((rest[0] & 0x03) << 6) + (rest[1] & 0x3f);
    break;
  case 3 :
    *page = ((rest[0] & 0x0f) << 4) + ((rest[1] & 0x3c) >> 2);
    *low_code = ((rest[1] & 0x03) << 6) + (rest[2] & 0x3f);
    break;
  case 4 :
    *page =
      ((rest[0] & 0x07) << 10) +
      ((rest[1] & 0x3f) << 4) +
      ((rest[2] & 0x3c) >> 2);
    *low_code = ((rest[1] & 0x03) << 6) + (rest[2] & 0x3f);
    break;
  default :
    *page = -1;
    *low_code = 0x00;
    break;
  }
}

static inline void
normalize_character(const char *utf8, int character_length,
                    uint32_t **normalize_table,
                    char *normalized,
                    unsigned int *normalized_character_length,
                    unsigned int *normalized_length_in_bytes,
                    unsigned int *normalized_n_characters)
{
  int page;
  uint32_t low_code;
  decompose_character(utf8, character_length, &page, &low_code);
  if ((0x00 <= page && page <= 0xff) && normalize_table[page]) {
    uint32_t normalized_code;
    unsigned int n_bytes;
    normalized_code = normalize_table[page][low_code];
    if (normalized_code == 0x00000) {
      *normalized_character_length = 0;
    } else {
      n_bytes = unichar_to_utf8(normalized_code,
                                normalized + *normalized_length_in_bytes);
      *normalized_character_length = n_bytes;
      *normalized_length_in_bytes += n_bytes;
      (*normalized_n_characters)++;
    }
  } else {
    int i;
    for (i = 0; i < character_length; i++) {
      normalized[*normalized_length_in_bytes + i] = utf8[i];
    }
    *normalized_character_length = character_length;
    *normalized_length_in_bytes += character_length;
    (*normalized_n_characters)++;
  }
}

static void
sized_buffer_append(char *buffer,
                    unsigned int buffer_length,
                    unsigned int *buffer_rest_length,
                    const char *string)
{
  size_t string_length;

  string_length = strlen(string);
  if (string_length >= *buffer_rest_length) {
    return;
  }

  strncat(buffer, string, buffer_length);
  *buffer_rest_length -= string_length;
}

static void
sized_buffer_dump_string(char *buffer,
                         unsigned int buffer_length,
                         unsigned int *buffer_rest_length,
                         const char *string, unsigned int string_length)
{
  const unsigned char *bytes;
  unsigned int i;

  bytes = (const unsigned char *)string;
  for (i = 0; i < string_length; i++) {
    unsigned char byte = bytes[i];
#define FORMATTED_BYTE_BUFFER_SIZE 5 /* "0xFF\0" */
    char formatted_byte[FORMATTED_BYTE_BUFFER_SIZE];
    if (i > 0) {
      sized_buffer_append(buffer, buffer_length, buffer_rest_length,
                          " ");
    }
    if (byte == 0) {
      strncpy(formatted_byte, "0x00", FORMATTED_BYTE_BUFFER_SIZE);
    } else {
      snprintf(formatted_byte, FORMATTED_BYTE_BUFFER_SIZE, "%#04x", byte);
    }
    sized_buffer_append(buffer, buffer_length, buffer_rest_length,
                        formatted_byte);
#undef FORMATTED_BYTE_BUFFER_SIZE
  }
}

static const char *
snippet(const char *string, unsigned int length, unsigned int target_byte,
        char *buffer, unsigned int buffer_length)
{
  const char *elision_mark = "...";
  unsigned int max_window_length = 12;
  unsigned int window_length;
  unsigned int buffer_rest_length = buffer_length - 1;

  buffer[0] = '\0';

  if (target_byte > 0) {
    sized_buffer_append(buffer, buffer_length, &buffer_rest_length,
                        elision_mark);
  }

  sized_buffer_append(buffer, buffer_length, &buffer_rest_length, "<");
  if (target_byte + max_window_length > length) {
    window_length = length - target_byte;
  } else {
    window_length = max_window_length;
  }
  sized_buffer_dump_string(buffer, buffer_length, &buffer_rest_length,
                           string + target_byte, window_length);
  sized_buffer_append(buffer, buffer_length, &buffer_rest_length,
                      ">");

  if (target_byte + window_length < length) {
    sized_buffer_append(buffer, buffer_length, &buffer_rest_length,
                        elision_mark);
  }

  return buffer;
}

static void
normalize(grn_ctx *ctx, grn_obj *string,
          const char *normalizer_type_label,
          uint32_t **normalize_table,
          normalizer_func custom_normalizer,
          grn_bool *remove_checks)
{
  const char *original, *rest;
  unsigned int original_length_in_bytes, rest_length;
  char *normalized;
  unsigned int normalized_length_in_bytes = 0;
  unsigned int normalized_n_characters = 0;
  unsigned char *types = NULL;
  unsigned char *current_type = NULL;
  short *checks = NULL;
  short *current_check = NULL;
  grn_encoding encoding;
  int flags;
  grn_bool remove_blank_p;
  unsigned int current_remove_checks = 0;
  grn_bool is_removed = GRN_FALSE;

  encoding = grn_string_get_encoding(ctx, string);
  flags = grn_string_get_flags(ctx, string);
  remove_blank_p = flags & GRN_STRING_REMOVE_BLANK;

  grn_string_get_original(ctx, string, &original, &original_length_in_bytes);
  {
    unsigned int max_normalized_length_in_bytes =
      original_length_in_bytes + 1;
    normalized = GRN_PLUGIN_MALLOC(ctx, max_normalized_length_in_bytes);
  }
  if (flags & GRN_STRING_WITH_TYPES) {
    unsigned int max_normalized_n_characters = original_length_in_bytes + 1;
    types = GRN_PLUGIN_MALLOC(ctx, max_normalized_n_characters);
    current_type = types;
  }
  if (flags & GRN_STRING_WITH_CHECKS) {
    unsigned int max_checks_size = sizeof(short) * original_length_in_bytes + 1;
    checks = GRN_PLUGIN_MALLOC(ctx, max_checks_size);
    current_check = checks;
    current_check[0] = 0;
  }
  rest = original;
  rest_length = original_length_in_bytes;
  while (rest_length > 0) {
    int character_length;

    character_length = grn_plugin_charlen(ctx, rest, rest_length, encoding);
    if (character_length == 0) {
      break;
    }

    if (remove_blank_p && character_length == 1 && rest[0] == ' ') {
      if (current_type > types) {
        current_type[-1] |= GRN_CHAR_BLANK;
      }
      if (current_check) {
        current_check[0]++;
      }
    } else if (character_length == 1 && (rest[0] == 0x000a || rest[0] == 0x000d)) {
      if (current_check) {
        current_check[0]++;
      }
    } else if (remove_checks && remove_checks[current_remove_checks]) {
      if (!is_removed) {
        normalized[normalized_length_in_bytes] = ' ';
        normalized_length_in_bytes++;
        normalized_n_characters++;
        if (current_type) {
          current_type[0] = GRN_CHAR_BLANK;
          current_type++;
        }
        if (current_check) {
          current_check[0] += character_length;
          current_check++;
          current_check[0] = 0;
        }
      } else {
        if (current_check) {
          current_check[-1] += character_length;
        }
      }
      is_removed = GRN_TRUE;
    } else {
      grn_bool custom_normalized = GRN_FALSE;
      unsigned int normalized_character_length;

      if (custom_normalizer) {
        custom_normalized = custom_normalizer(ctx,
                                              rest,
                                              &character_length,
                                              rest_length - character_length,
                                              normalize_table,
                                              normalized,
                                              &normalized_character_length,
                                              &normalized_length_in_bytes,
                                              &normalized_n_characters);
      }
      if (!custom_normalized) {
        normalize_character(rest, character_length, normalize_table,
                            normalized,
                            &normalized_character_length,
                            &normalized_length_in_bytes,
                            &normalized_n_characters);
      }

      if (current_type && normalized_character_length > 0) {
        char *current_normalized;
        current_normalized =
          normalized + normalized_length_in_bytes - normalized_character_length;
        current_type[0] =
          grn_nfkc_char_type((unsigned char *)current_normalized);
        current_type++;
      }
      if (current_check) {
        current_check[0] += character_length;
        if (normalized_character_length > 0) {
          unsigned int i;
          current_check++;
          for (i = 1; i < normalized_character_length; i++) {
            current_check[0] = 0;
            current_check++;
          }
          current_check[0] = 0;
        }
      }
      is_removed = GRN_FALSE;
    }
    if (!(character_length == 1 &&
          (rest[0] == ' ' || rest[0] == 0x000a || rest[0] == 0x000d)
        )) {
      if (character_length == 6) {
        current_remove_checks += 2;
      } else {
        current_remove_checks++;
      }
    }
    rest += character_length;
    rest_length -= character_length;
  }
  if (current_type) {
    current_type[0] = GRN_CHAR_NULL;
  }
  normalized[normalized_length_in_bytes] = '\0';

  if (rest_length > 0) {
    char buffer[SNIPPET_BUFFER_SIZE];
    GRN_PLUGIN_LOG(ctx, GRN_LOG_DEBUG,
                   "[normalizer][%s] failed to normalize at %u byte: %s",
                   normalizer_type_label,
                   original_length_in_bytes - rest_length,
                   snippet(original,
                           original_length_in_bytes,
                           original_length_in_bytes - rest_length,
                           buffer,
                           SNIPPET_BUFFER_SIZE));
  }
  grn_string_set_normalized(ctx,
                            string,
                            normalized,
                            normalized_length_in_bytes,
                            normalized_n_characters);
  grn_string_set_types(ctx, string, types);
  grn_string_set_checks(ctx, string, checks);
}

#define HALFWIDTH_KATAKANA_LETTER_KA 0xff76
#define HALFWIDTH_KATAKANA_LETTER_KI 0xff77
#define HALFWIDTH_KATAKANA_LETTER_KU 0xff78
#define HALFWIDTH_KATAKANA_LETTER_KE 0xff79
#define HALFWIDTH_KATAKANA_LETTER_KO 0xff7a

#define HALFWIDTH_KATAKANA_LETTER_SA 0xff7b
#define HALFWIDTH_KATAKANA_LETTER_SI 0xff7c
#define HALFWIDTH_KATAKANA_LETTER_SU 0xff7d
#define HALFWIDTH_KATAKANA_LETTER_SE 0xff7e
#define HALFWIDTH_KATAKANA_LETTER_SO 0xff7f

#define HALFWIDTH_KATAKANA_LETTER_TA 0xff80
#define HALFWIDTH_KATAKANA_LETTER_TI 0xff81
#define HALFWIDTH_KATAKANA_LETTER_TU 0xff82
#define HALFWIDTH_KATAKANA_LETTER_TE 0xff83
#define HALFWIDTH_KATAKANA_LETTER_TO 0xff84

#define HALFWIDTH_KATAKANA_LETTER_HA 0xff8a
#define HALFWIDTH_KATAKANA_LETTER_HI 0xff8b
#define HALFWIDTH_KATAKANA_LETTER_HU 0xff8c
#define HALFWIDTH_KATAKANA_LETTER_HE 0xff8d
#define HALFWIDTH_KATAKANA_LETTER_HO 0xff8e

#define HALFWIDTH_KATAKANA_VOICED_SOUND_MARK      0xff9e
#define HALFWIDTH_KATAKANA_SEMI_VOICED_SOUND_MARK 0xff9f

#define KATAKANA_LETTER_KA                0x30ab
#define KATAKANA_VOICED_SOUND_MARK_OFFSET 1
#define KATAKANA_VOICED_SOUND_MARK_GAP    2

#define KATAKANA_LETTER_HA         0x30cf
#define KATAKANA_HA_LINE_BA_OFFSET 1
#define KATAKANA_HA_LINE_PA_OFFSET 2
#define KATAKANA_HA_LINE_GAP       3

#define HALFWIDTH_KATAKANA_DI_DU_OFFSET 5

#define FULLWIDTH_KATAKANA_LETTER_VU 0x30f4
#define FULLWIDTH_KATAKANA_LETTER_BA 0x30d0
#define FULLWIDTH_KATAKANA_LETTER_BI 0x30d3
#define FULLWIDTH_KATAKANA_LETTER_BU 0x30d6
#define FULLWIDTH_KATAKANA_LETTER_BE 0x30d9
#define FULLWIDTH_KATAKANA_LETTER_BO 0x30dc
#define FULLWIDTH_KATAKANA_LETTER_SMALL_A 0x30a1
#define FULLWIDTH_KATAKANA_LETTER_SMALL_I 0x30a3
#define FULLWIDTH_KATAKANA_LETTER_SMALL_U 0x30a5
#define FULLWIDTH_KATAKANA_LETTER_SMALL_E 0x30a7
#define FULLWIDTH_KATAKANA_LETTER_SMALL_O 0x30a9

static grn_bool
custom_normalizer(
  grn_ctx *ctx,
  const char *utf8,
  int *character_length,
  int rest_length,
  GNUC_UNUSED uint32_t **normalize_table,
  char *normalized,
  unsigned int *normalized_character_length,
  unsigned int *normalized_length_in_bytes,
  unsigned int *normalized_n_characters)
{
  grn_bool custom_normalized = GRN_FALSE;
  grn_bool is_voiced_sound_markable_halfwidth_katakana = GRN_FALSE;
  grn_bool is_semi_voiced_sound_markable_halfwidth_katakana = GRN_FALSE;
  grn_bool is_ha_line = GRN_FALSE;
  grn_bool is_vu = GRN_FALSE;
  uint32_t unichar;

  if (*character_length != 3) {
    return GRN_FALSE;
  }
  if (rest_length < 3) {
    return GRN_FALSE;
  }

  unichar = utf8_to_unichar(utf8, *character_length);
  if (HALFWIDTH_KATAKANA_LETTER_KA <= unichar &&
      unichar <= HALFWIDTH_KATAKANA_LETTER_TO) {
    is_voiced_sound_markable_halfwidth_katakana = GRN_TRUE;
  } else if (HALFWIDTH_KATAKANA_LETTER_HA <= unichar &&
             unichar <= HALFWIDTH_KATAKANA_LETTER_HO) {
    is_voiced_sound_markable_halfwidth_katakana = GRN_TRUE;
    is_semi_voiced_sound_markable_halfwidth_katakana = GRN_TRUE;
    is_ha_line = GRN_TRUE;
  } else if (unichar == FULLWIDTH_KATAKANA_LETTER_VU) {
    is_vu = GRN_TRUE;
  }

  if (!is_voiced_sound_markable_halfwidth_katakana &&
      !is_semi_voiced_sound_markable_halfwidth_katakana &&
      !is_vu) {
    return GRN_FALSE;
  }

  {
    int next_character_length;
    uint32_t next_unichar;
    next_character_length = grn_plugin_charlen(ctx,
                                               utf8 + *character_length,
                                               rest_length,
                                               GRN_ENC_UTF8);
    if (next_character_length != 3) {
      return GRN_FALSE;
    }
    next_unichar = utf8_to_unichar(utf8 + *character_length,
                                   next_character_length);
    if (next_unichar == HALFWIDTH_KATAKANA_VOICED_SOUND_MARK) {
      if (is_voiced_sound_markable_halfwidth_katakana) {
        unsigned int n_bytes;
        if (is_ha_line) {
          n_bytes = unichar_to_utf8(KATAKANA_LETTER_HA +
                                    KATAKANA_HA_LINE_BA_OFFSET +
                                    ((unichar - HALFWIDTH_KATAKANA_LETTER_HA) *
                                     KATAKANA_HA_LINE_GAP),
                                    normalized + *normalized_length_in_bytes);
        } else {
          int small_tu_offset = 0;
          if (HALFWIDTH_KATAKANA_LETTER_TU <= unichar &&
              unichar <= HALFWIDTH_KATAKANA_LETTER_TO) {
            if (unichar == HALFWIDTH_KATAKANA_LETTER_TU) {
              unichar -= HALFWIDTH_KATAKANA_DI_DU_OFFSET;
            } else {
              small_tu_offset = 1;
            }
          } else if (unichar == HALFWIDTH_KATAKANA_LETTER_TI) {
            unichar -= HALFWIDTH_KATAKANA_DI_DU_OFFSET;
          }
          n_bytes = unichar_to_utf8(KATAKANA_LETTER_KA +
                                    KATAKANA_VOICED_SOUND_MARK_OFFSET +
                                    small_tu_offset +
                                    ((unichar - HALFWIDTH_KATAKANA_LETTER_KA) *
                                     KATAKANA_VOICED_SOUND_MARK_GAP),
                                    normalized + *normalized_length_in_bytes);
        }
        *character_length += next_character_length;
        *normalized_character_length = n_bytes;
        *normalized_length_in_bytes += n_bytes;
        (*normalized_n_characters)++;
        custom_normalized = GRN_TRUE;
      }
    } else if (next_unichar == HALFWIDTH_KATAKANA_SEMI_VOICED_SOUND_MARK) {
      if (is_semi_voiced_sound_markable_halfwidth_katakana) {
        unsigned int n_bytes;
        n_bytes = unichar_to_utf8(KATAKANA_LETTER_HA +
                                  KATAKANA_HA_LINE_PA_OFFSET +
                                  ((unichar - HALFWIDTH_KATAKANA_LETTER_HA) *
                                   KATAKANA_HA_LINE_GAP),
                                  normalized + *normalized_length_in_bytes);
        *character_length += next_character_length;
        *normalized_character_length = n_bytes;
        *normalized_length_in_bytes += n_bytes;
        (*normalized_n_characters)++;
        custom_normalized = GRN_TRUE;
      }
    } else if (next_unichar == FULLWIDTH_KATAKANA_LETTER_SMALL_A) {
      if (is_vu) {
        unsigned int n_bytes;
        n_bytes = unichar_to_utf8(FULLWIDTH_KATAKANA_LETTER_BA,
                                 normalized + *normalized_length_in_bytes);
        *character_length += next_character_length;
        *normalized_character_length = n_bytes;
        *normalized_length_in_bytes += n_bytes;
        (*normalized_n_characters)++;
        custom_normalized = GRN_TRUE;
      }
    } else if (next_unichar == FULLWIDTH_KATAKANA_LETTER_SMALL_I) {
      if (is_vu) {
        unsigned int n_bytes;
        n_bytes = unichar_to_utf8(FULLWIDTH_KATAKANA_LETTER_BI,
                                 normalized + *normalized_length_in_bytes);
        *character_length += next_character_length;
        *normalized_character_length = n_bytes;
        *normalized_length_in_bytes += n_bytes;
        (*normalized_n_characters)++;
        custom_normalized = GRN_TRUE;
      }
    } else if (next_unichar == FULLWIDTH_KATAKANA_LETTER_SMALL_E) {
      if (is_vu) {
        unsigned int n_bytes;
        n_bytes = unichar_to_utf8(FULLWIDTH_KATAKANA_LETTER_BE,
                                 normalized + *normalized_length_in_bytes);
        *character_length += next_character_length;
        *normalized_character_length = n_bytes;
        *normalized_length_in_bytes += n_bytes;
        (*normalized_n_characters)++;
        custom_normalized = GRN_TRUE;
      }
    } else if (next_unichar == FULLWIDTH_KATAKANA_LETTER_SMALL_O) {
      if (is_vu) {
        unsigned int n_bytes;
        n_bytes = unichar_to_utf8(FULLWIDTH_KATAKANA_LETTER_BO,
                                 normalized + *normalized_length_in_bytes);
        *character_length += next_character_length;
        *normalized_character_length = n_bytes;
        *normalized_length_in_bytes += n_bytes;
        (*normalized_n_characters)++;
        custom_normalized = GRN_TRUE;
      }
    }
  }

  return custom_normalized;
}

static grn_bool
execute_token_filter(grn_ctx *ctx, const char *token, unsigned int token_length,
                     const char *pos, unsigned int pos_length,
                     grn_obj *stopwords_table, grn_obj *pos_table)
{
  if (stopwords_table) {
    grn_id id;
    id = grn_table_get(ctx, stopwords_table, token, token_length);
    if (id) { return GRN_TRUE; }
  }
  if (pos_table) {
    grn_id id;
    id = grn_table_get(ctx, pos_table, pos, pos_length);
    if (id) { return GRN_TRUE; }
  }
  return GRN_FALSE;
}

static unsigned int
char_filter(grn_ctx *ctx, const char *string, unsigned int string_length,
            grn_encoding encoding, grn_bool *remove_checks,
            grn_bool filter_symbol, grn_bool filter_html)
{
  unsigned int char_length;
  unsigned int current_char = 0;
  unsigned int rest_length = string_length;
  grn_bool in_tag = GRN_FALSE;

  while ((char_length = grn_plugin_charlen(ctx, string, rest_length, encoding))) {
    grn_bool is_removed = GRN_FALSE;
    if (filter_html) {
      switch (string[0]) {
        case '<' :
          in_tag = GRN_TRUE;
          is_removed = GRN_TRUE;
          break;
        case '>' :
          in_tag = GRN_FALSE;
          is_removed = GRN_TRUE;
          break;
        default :
          break;
      }
    }
    if (filter_symbol) {
      if (grn_nfkc_char_type((unsigned char *)string) == GRN_CHAR_SYMBOL) {
        is_removed = GRN_TRUE;
      }
    }
    if (filter_html && in_tag) {
      remove_checks[current_char] = in_tag;
    } else {
      remove_checks[current_char] = is_removed;
    }
    current_char++;
    string += char_length;
    rest_length -= char_length;
  }

  return current_char;
}

static unsigned int
mecab_filter(grn_ctx *ctx, const char *string, unsigned int string_length,
             grn_encoding encoding, grn_bool *remove_checks,
             grn_obj *stopwords_table, grn_obj *pos_table,
             grn_bool filter_symbol, grn_bool filter_html)
{
  mecab_t *mecab;
  const mecab_node_t *node;
  unsigned int parse_limit = DEFAULT_MECAB_PARSE_LIMIT;
  unsigned int rfind_punct_offset = DEFAULT_RFIND_PUNCT_OFFSET;
  const char *rest_string;
  unsigned int rest_string_length;
  unsigned int current_char = 0;
  grn_bool in_tag = GRN_FALSE;

  if (!sole_mecab) {
    grn_plugin_mutex_lock(ctx, sole_mecab_mutex);
    if (!sole_mecab) {
      sole_mecab = mecab_new2("-Owakati");
      if (!sole_mecab) {
        GRN_PLUGIN_ERROR(ctx, GRN_NORMALIZER_ERROR,
                         "[normalizer][yamysql] "
                         "mecab_new2() failed on yamysql_init(): %s",
                         mecab_strerror(NULL));
      } else {
        sole_mecab_encoding = get_mecab_encoding(sole_mecab);
      }
    }
    grn_plugin_mutex_unlock(ctx, sole_mecab_mutex);
  }
  if (!sole_mecab) {
    return GRN_FALSE;
  }
  if (encoding != sole_mecab_encoding) {
    GRN_PLUGIN_ERROR(ctx, GRN_NORMALIZER_ERROR,
                     "[normalizer][yamysql] "
                     "MeCab dictionary charset (%s) does not match "
                     "the table encoding: <%s>",
                     grn_encoding_to_string(sole_mecab_encoding),
                     grn_encoding_to_string(encoding));
    return GRN_FALSE;
  }
  mecab = sole_mecab;

  rest_string_length = string_length;
  rest_string = string;

  grn_plugin_mutex_lock(ctx, sole_mecab_mutex);

  while (rest_string_length) {
#define MECAB_PARSE_MIN 4096
    unsigned int parsed_string_length;
    grn_bool is_success = GRN_FALSE;
    while (!is_success) {
      node = split_mecab_sparse_node(ctx, mecab,
                                     encoding,
                                     parse_limit,
                                     rfind_punct_offset,
                                     rest_string,
                                     rest_string_length,
                                     &parsed_string_length);
      if (!node) {
        parse_limit /= 2;
        GRN_PLUGIN_LOG(ctx, GRN_LOG_NOTICE,
                       "[normalizer][yamysql] "
                       "mecab_sparse_tonode() failed len=%d err=%s do retry",
                       parsed_string_length,
                       mecab_strerror(mecab));
      } else {
        node = node->next;
        rest_string_length -= parsed_string_length;
        rest_string += parsed_string_length;
        is_success = GRN_TRUE;
      }
      if (parse_limit < MECAB_PARSE_MIN) {
        break;
      }
    }
    if (!node) {
      GRN_PLUGIN_ERROR(ctx, GRN_NORMALIZER_ERROR,
                       "[normalizer][yamysql] "
                       "mecab_sparse_tonode() failed len=%d err=%s",
                       parsed_string_length,
                       mecab_strerror(mecab));
      return GRN_FALSE;
    }

    for (; node; node = node->next) {
      if (node->stat == MECAB_NOR_NODE || node->stat == MECAB_UNK_NODE) {
        unsigned int char_length;
        char *token = (char *)node->surface;
        unsigned int rest_length = node->length;
        char *feature = (char *)node->feature;
        unsigned int feature_rest_length = strlen(feature);
        const char *delimiter = ",";
        unsigned int pos_length = 0;
        grn_bool is_removed = GRN_FALSE;
        grn_bool is_token_removed = GRN_FALSE;

        while ((char_length = grn_plugin_charlen(ctx, feature, feature_rest_length, encoding))) {
          if (feature + char_length && !memcmp(feature, delimiter, char_length)) {
            break;
          }
          pos_length += char_length;
          feature += char_length;
          feature_rest_length -= char_length;
        }
        if (execute_token_filter(ctx, node->surface, node->length,
                                 node->feature, pos_length,
                                 stopwords_table, pos_table)) {
          is_token_removed = GRN_TRUE;
        } 

        while ((char_length = grn_plugin_charlen(ctx, token, rest_length, encoding))) {
          is_removed = GRN_FALSE;

          if (token[0] == 0x000a || token[0] == 0x000d) {
            token += char_length;
            rest_length -= char_length;
            continue;
          }
          if (filter_html) {
            switch (token[0]) {
              case '<' :
                in_tag = GRN_TRUE;
                is_removed = GRN_TRUE;
                break;
              case '>' :
                in_tag = GRN_FALSE;
                is_removed = GRN_TRUE;
                break;
              default :
                break;
            }
          }
          if (is_token_removed && is_removed == GRN_FALSE) {
            is_removed = GRN_TRUE;
          }
          if (filter_symbol && is_removed == GRN_FALSE) {
            if (grn_nfkc_char_type((unsigned char *)token) == GRN_CHAR_SYMBOL) {
              is_removed = GRN_TRUE;
            }
          }
          
          if (filter_html && in_tag == GRN_TRUE) {
            remove_checks[current_char] = in_tag;
          } else {
            remove_checks[current_char] = is_removed;
          }
          current_char++;
          token += char_length;
          rest_length -= char_length;
        }
      }
    }
#undef MECAB_PARSE_MIN
  }
  grn_plugin_mutex_unlock(ctx, sole_mecab_mutex);

  return current_char;
}

static grn_obj *
mysql_unicode_ci_custom_next(
  GNUC_UNUSED grn_ctx *ctx,
  GNUC_UNUSED int nargs,
  grn_obj **args,
  GNUC_UNUSED grn_user_data *user_data)
{
  grn_obj *string = args[0];
  grn_encoding encoding;
  const char *normalizer_type_label = "yamysql";
  int flags;
  grn_bool filter_symbol = GRN_FALSE;
  grn_bool filter_pos = GRN_FALSE;
  grn_bool filter_html = GRN_FALSE;
  grn_bool kana_ci = GRN_FALSE;
  grn_obj *var; 
  grn_bool *remove_checks = NULL;

  encoding = grn_string_get_encoding(ctx, string);
  if (encoding != GRN_ENC_UTF8) {
    GRN_PLUGIN_ERROR(ctx,
                     GRN_FUNCTION_NOT_IMPLEMENTED,
                     "[normalizer][%s] "
                     "UTF-8 encoding is only supported: %s",
                     normalizer_type_label,
                     grn_encoding_to_string(encoding));
    return NULL;
  }
  var = grn_plugin_proc_get_var(ctx, user_data, "filter_symbol", -1); 
  if (GRN_TEXT_LEN(var) != 0) { 
    filter_symbol = GRN_BOOL_VALUE(var);
  }
  var = grn_plugin_proc_get_var(ctx, user_data, "filter_html", -1); 
  if (GRN_TEXT_LEN(var) != 0) { 
    filter_html = GRN_BOOL_VALUE(var);
  }
  var = grn_plugin_proc_get_var(ctx, user_data, "filter_pos", -1); 
  if (GRN_TEXT_LEN(var) != 0) { 
    filter_pos = GRN_BOOL_VALUE(var);
  }
  var = grn_plugin_proc_get_var(ctx, user_data, "kana_ci", -1); 
  if (GRN_TEXT_LEN(var) != 0) { 
    kana_ci = GRN_BOOL_VALUE(var);
  }

  flags = grn_string_get_flags(ctx, string);

  //キー操作のWITH_NORMALIZEでノーマライザーが呼ばれた場合は、フィルター処理は動作しないようにする。
  //TokenMecab、TokenDelimitではflagがなく区別できないため、フィルター処理は常に動作しない。
  if (flags) {
    grn_obj *stopwords_table = NULL;
    grn_obj *pos_table = NULL;
    const char *original_string = NULL;
    unsigned int original_length_in_bytes = 0;
    unsigned int max_remove_checks_size = 0;

    stopwords_table = grn_ctx_get(ctx,
                                  STOPWORDS_TABLE_NAME,
                                  strlen(STOPWORDS_TABLE_NAME));
    if (!stopwords_table) {
      stopwords_table = grn_ctx_get(ctx,
                                    STOPWORDS_TABLE_NAME_MRN,
                                    strlen(STOPWORDS_TABLE_NAME_MRN));
    }
    if (filter_pos) {
      pos_table = grn_ctx_get(ctx,
                              PARTOFSPEECH_TABLE_NAME,
                              strlen(PARTOFSPEECH_TABLE_NAME));
      if (!pos_table) {
        pos_table = grn_ctx_get(ctx,
                                PARTOFSPEECH_TABLE_NAME_MRN,
                                strlen(PARTOFSPEECH_TABLE_NAME_MRN));
      }
    }

    if (filter_pos || stopwords_table || filter_symbol || filter_html) {
      grn_string_get_original(ctx, string, &original_string, &original_length_in_bytes);
      max_remove_checks_size = sizeof(grn_bool) * original_length_in_bytes + 1;
      remove_checks = GRN_PLUGIN_MALLOC(ctx, max_remove_checks_size);
    }

    if (filter_pos && pos_table) {
      mecab_filter(ctx, original_string, original_length_in_bytes, encoding, remove_checks,
                   stopwords_table, pos_table, filter_symbol, filter_html);
    } else if (filter_pos && default_pos_table) {
      mecab_filter(ctx, original_string, original_length_in_bytes, encoding, remove_checks,
                   stopwords_table, (grn_obj *)default_pos_table, filter_symbol, filter_html);
    } else if (stopwords_table) {
      mecab_filter(ctx, original_string, original_length_in_bytes, encoding, remove_checks,
                   stopwords_table, NULL, filter_symbol, filter_html);
    } else if (filter_symbol || filter_html) {
      char_filter(ctx, original_string, original_length_in_bytes, encoding, remove_checks,
                  filter_symbol, filter_html);
    }

    if (pos_table) {
      grn_obj_unlink(ctx, pos_table);
      pos_table = NULL;
    }
    if (stopwords_table) {
      grn_obj_unlink(ctx, stopwords_table);
      stopwords_table = NULL;
    }
  }

  if (kana_ci) {
    normalize(ctx, string,
              normalizer_type_label,
              custom_kana_ci_table,
              custom_normalizer,
              remove_checks);
  } else {
    normalize(ctx, string,
              normalizer_type_label,
              custom_table,
              custom_normalizer,
              remove_checks);
  }

  if (remove_checks) {
    GRN_PLUGIN_FREE(ctx, remove_checks);
    remove_checks = NULL;
  }

  return NULL;
}

static void
check_mecab_dictionary_encoding(GNUC_UNUSED grn_ctx *ctx)
{
#ifdef HAVE_MECAB_DICTIONARY_INFO_T
  mecab_t *mecab;

  mecab = mecab_new2("-Owakati");
  if (mecab) {
    grn_encoding encoding;
    int have_same_encoding_dictionary = 0;

    encoding = GRN_CTX_GET_ENCODING(ctx);
    have_same_encoding_dictionary = encoding == get_mecab_encoding(mecab);
    mecab_destroy(mecab);

    if (!have_same_encoding_dictionary) {
      GRN_PLUGIN_ERROR(ctx, GRN_NORMALIZER_ERROR,
                       "[normalizer][yamysql] "
                       "MeCab has no dictionary that uses the context encoding"
                       ": <%s>",
                       grn_encoding_to_string(encoding));
    }
  } else {
    GRN_PLUGIN_ERROR(ctx, GRN_NORMALIZER_ERROR,
                     "[normalizer][yamysql] "
                     "mecab_new2 failed in check_mecab_dictionary_encoding: %s",
                     mecab_strerror(NULL));
  }
#endif
}

grn_rc
GRN_PLUGIN_INIT(grn_ctx *ctx)
{
  sole_mecab = NULL;
  sole_mecab_mutex = grn_plugin_mutex_open(ctx);
  if (!sole_mecab_mutex) {
    GRN_PLUGIN_ERROR(ctx, GRN_NO_MEMORY_AVAILABLE,
                     "[normalizer][yamysql] grn_plugin_mutex_open() failed");
    return ctx->rc;
  }

  check_mecab_dictionary_encoding(ctx);

  default_pos_table = grn_hash_create(ctx, NULL,
                                      GRN_TABLE_MAX_KEY_SIZE,
                                      0,
                                      GRN_OBJ_TABLE_HASH_KEY|GRN_OBJ_KEY_VAR_SIZE);
  if (default_pos_table) {
    grn_hash_add(ctx, default_pos_table, "助詞", strlen("助詞"), NULL, NULL);
    grn_hash_add(ctx, default_pos_table, "助動詞", strlen("助動詞"), NULL, NULL);
    grn_hash_add(ctx, default_pos_table, "連体詞", strlen("連体詞"), NULL, NULL);
    grn_hash_add(ctx, default_pos_table, "接続詞", strlen("接続詞"), NULL, NULL);
  }

  return ctx->rc;
}

grn_rc
GRN_PLUGIN_REGISTER(grn_ctx *ctx)
{
  grn_expr_var vars[5];

  grn_plugin_expr_var_init(ctx, &vars[0], NULL, -1);
  grn_plugin_expr_var_init(ctx, &vars[1], "filter_symbol", -1);
  grn_plugin_expr_var_init(ctx, &vars[2], "filter_html", -1);
  grn_plugin_expr_var_init(ctx, &vars[3], "filter_pos", -1);
  grn_plugin_expr_var_init(ctx, &vars[4], "kana_ci", -1);

  GRN_BOOL_SET(ctx, &vars[1].value, GRN_FALSE);
  GRN_BOOL_SET(ctx, &vars[2].value, GRN_FALSE);
  GRN_BOOL_SET(ctx, &vars[3].value, GRN_FALSE);
  GRN_BOOL_SET(ctx, &vars[4].value, GRN_FALSE);

  grn_proc_create(ctx, "NormalizerYaMySQL", -1,
                  GRN_PROC_NORMALIZER,
                  NULL, mysql_unicode_ci_custom_next, NULL, 5, vars);

  GRN_BOOL_SET(ctx, &vars[4].value, GRN_TRUE);
  grn_proc_create(ctx, "NormalizerYaMySQLKanaCI", -1,
                  GRN_PROC_NORMALIZER,
                  NULL, mysql_unicode_ci_custom_next, NULL, 5, vars);

  GRN_BOOL_SET(ctx, &vars[2].value, GRN_TRUE);
  GRN_BOOL_SET(ctx, &vars[4].value, GRN_FALSE);
  grn_proc_create(ctx, "NormalizerYaMySQLHtml", -1,
                  GRN_PROC_NORMALIZER,
                  NULL, mysql_unicode_ci_custom_next, NULL, 5, vars);

  GRN_BOOL_SET(ctx, &vars[4].value, GRN_TRUE);
  grn_proc_create(ctx, "NormalizerYaMySQLKanaCIHtml", -1,
                  GRN_PROC_NORMALIZER,
                  NULL, mysql_unicode_ci_custom_next, NULL, 5, vars);

  GRN_BOOL_SET(ctx, &vars[1].value, GRN_TRUE);
  GRN_BOOL_SET(ctx, &vars[2].value, GRN_FALSE);
  GRN_BOOL_SET(ctx, &vars[4].value, GRN_FALSE);
  grn_proc_create(ctx, "NormalizerYaMySQLSymbol", -1,
                  GRN_PROC_NORMALIZER,
                  NULL, mysql_unicode_ci_custom_next, NULL, 5, vars);

  GRN_BOOL_SET(ctx, &vars[4].value, GRN_TRUE);
  grn_proc_create(ctx, "NormalizerYaMySQLKanaCISymbol", -1,
                  GRN_PROC_NORMALIZER,
                  NULL, mysql_unicode_ci_custom_next, NULL, 5, vars);

  GRN_BOOL_SET(ctx, &vars[1].value, GRN_TRUE);
  GRN_BOOL_SET(ctx, &vars[2].value, GRN_TRUE);
  GRN_BOOL_SET(ctx, &vars[4].value, GRN_FALSE);
  grn_proc_create(ctx, "NormalizerYaMySQLSymbolHtml", -1,
                  GRN_PROC_NORMALIZER,
                  NULL, mysql_unicode_ci_custom_next, NULL, 5, vars);

  GRN_BOOL_SET(ctx, &vars[4].value, GRN_TRUE);
  grn_proc_create(ctx, "NormalizerYaMySQLKanaCISymbolHtml", -1,
                  GRN_PROC_NORMALIZER,
                  NULL, mysql_unicode_ci_custom_next, NULL, 5, vars);

  GRN_BOOL_SET(ctx, &vars[1].value, GRN_FALSE);
  GRN_BOOL_SET(ctx, &vars[2].value, GRN_FALSE);
  GRN_BOOL_SET(ctx, &vars[3].value, GRN_TRUE);
  GRN_BOOL_SET(ctx, &vars[4].value, GRN_FALSE);
  grn_proc_create(ctx, "NormalizerYaMySQLPartofspeech", -1,
                  GRN_PROC_NORMALIZER,
                  NULL, mysql_unicode_ci_custom_next, NULL, 5, vars);

  GRN_BOOL_SET(ctx, &vars[1].value, GRN_TRUE);
  grn_proc_create(ctx, "NormalizerYaMySQLSymbolPartofspeech", -1,
                  GRN_PROC_NORMALIZER,
                  NULL, mysql_unicode_ci_custom_next, NULL, 5, vars);

  GRN_BOOL_SET(ctx, &vars[1].value, GRN_FALSE);
  GRN_BOOL_SET(ctx, &vars[4].value, GRN_TRUE);
  grn_proc_create(ctx, "NormalizerYaMySQLKanaCIPartofspeech", -1,
                  GRN_PROC_NORMALIZER,
                  NULL, mysql_unicode_ci_custom_next, NULL, 5, vars);

  GRN_BOOL_SET(ctx, &vars[1].value, GRN_TRUE);
  grn_proc_create(ctx, "NormalizerYaMySQLKanaCISymbolPartofspeech", -1,
                  GRN_PROC_NORMALIZER,
                  NULL, mysql_unicode_ci_custom_next, NULL, 5, vars);

  GRN_BOOL_SET(ctx, &vars[1].value, GRN_FALSE);
  GRN_BOOL_SET(ctx, &vars[2].value, GRN_TRUE);
  GRN_BOOL_SET(ctx, &vars[3].value, GRN_TRUE);
  GRN_BOOL_SET(ctx, &vars[4].value, GRN_FALSE);
  grn_proc_create(ctx, "NormalizerYaMySQLPartofspeechHtml", -1,
                  GRN_PROC_NORMALIZER,
                  NULL, mysql_unicode_ci_custom_next, NULL, 5, vars);

  GRN_BOOL_SET(ctx, &vars[1].value, GRN_TRUE);
  grn_proc_create(ctx, "NormalizerYaMySQLSymbolPartofspeechHtml", -1,
                  GRN_PROC_NORMALIZER,
                  NULL, mysql_unicode_ci_custom_next, NULL, 5, vars);

  GRN_BOOL_SET(ctx, &vars[1].value, GRN_FALSE);
  GRN_BOOL_SET(ctx, &vars[4].value, GRN_TRUE);
  grn_proc_create(ctx, "NormalizerYaMySQLKanaCIPartofspeechHtml", -1,
                  GRN_PROC_NORMALIZER,
                  NULL, mysql_unicode_ci_custom_next, NULL, 5, vars);

  GRN_BOOL_SET(ctx, &vars[1].value, GRN_TRUE);
  grn_proc_create(ctx, "NormalizerYaMySQLKanaCISymbolPartofspeechHtml", -1,
                  GRN_PROC_NORMALIZER,
                  NULL, mysql_unicode_ci_custom_next, NULL, 5, vars);

  return GRN_SUCCESS;
}

grn_rc
GRN_PLUGIN_FIN(GNUC_UNUSED grn_ctx *ctx)
{
  if (sole_mecab) {
    mecab_destroy(sole_mecab);
    sole_mecab = NULL;
  }
  if (sole_mecab_mutex) {
    grn_plugin_mutex_close(ctx, sole_mecab_mutex);
    sole_mecab_mutex = NULL;
  }
  if (default_pos_table) {
    grn_hash_close(ctx, default_pos_table);
    default_pos_table = NULL;
  }

  return GRN_SUCCESS;
}
