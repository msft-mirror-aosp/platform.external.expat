/* Tests in the "basic" test case for the Expat test suite
                            __  __            _
                         ___\ \/ /_ __   __ _| |_
                        / _ \\  /| '_ \ / _` | __|
                       |  __//  \| |_) | (_| | |_
                        \___/_/\_\ .__/ \__,_|\__|
                                 |_| XML parser

   Copyright (c) 2001-2006 Fred L. Drake, Jr. <fdrake@users.sourceforge.net>
   Copyright (c) 2003      Greg Stein <gstein@users.sourceforge.net>
   Copyright (c) 2005-2007 Steven Solie <steven@solie.ca>
   Copyright (c) 2005-2012 Karl Waclawek <karl@waclawek.net>
   Copyright (c) 2016-2022 Sebastian Pipping <sebastian@pipping.org>
   Copyright (c) 2017-2022 Rhodri James <rhodri@wildebeest.org.uk>
   Copyright (c) 2017      Joe Orton <jorton@redhat.com>
   Copyright (c) 2017      José Gutiérrez de la Concha <jose@zeroc.com>
   Copyright (c) 2018      Marco Maggi <marco.maggi-ipsu@poste.it>
   Copyright (c) 2019      David Loffredo <loffredo@steptools.com>
   Copyright (c) 2020      Tim Gates <tim.gates@iress.com>
   Copyright (c) 2021      Dong-hee Na <donghee.na@python.org>
   Licensed under the MIT license:

   Permission is  hereby granted,  free of charge,  to any  person obtaining
   a  copy  of  this  software   and  associated  documentation  files  (the
   "Software"),  to  deal in  the  Software  without restriction,  including
   without  limitation the  rights  to use,  copy,  modify, merge,  publish,
   distribute, sublicense, and/or sell copies of the Software, and to permit
   persons  to whom  the Software  is  furnished to  do so,  subject to  the
   following conditions:

   The above copyright  notice and this permission notice  shall be included
   in all copies or substantial portions of the Software.

   THE  SOFTWARE  IS  PROVIDED  "AS  IS",  WITHOUT  WARRANTY  OF  ANY  KIND,
   EXPRESS  OR IMPLIED,  INCLUDING  BUT  NOT LIMITED  TO  THE WARRANTIES  OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
   NO EVENT SHALL THE AUTHORS OR  COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
   DAMAGES OR  OTHER LIABILITY, WHETHER  IN AN  ACTION OF CONTRACT,  TORT OR
   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
   USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#if defined(NDEBUG)
#  undef NDEBUG /* because test suite relies on assert(...) at the moment */
#endif

#include <assert.h>

#include <stdio.h>
#include <string.h>

#if ! defined(__cplusplus)
#  include <stdbool.h>
#endif

#include "expat_config.h"

#include "expat.h"
#include "internal.h"
#include "minicheck.h"
#include "structdata.h"
#include "common.h"
#include "dummy.h"
#include "handlers.h"
#include "siphash.h"
#include "basic_tests.h"

static void
basic_setup(void) {
  g_parser = XML_ParserCreate(NULL);
  if (g_parser == NULL)
    fail("Parser not created.");
}

/*
 * Character & encoding tests.
 */

START_TEST(test_nul_byte) {
  char text[] = "<doc>\0</doc>";

  /* test that a NUL byte (in US-ASCII data) is an error */
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, sizeof(text) - 1, XML_TRUE)
      == XML_STATUS_OK)
    fail("Parser did not report error on NUL-byte.");
  if (XML_GetErrorCode(g_parser) != XML_ERROR_INVALID_TOKEN)
    xml_failure(g_parser);
}
END_TEST

START_TEST(test_u0000_char) {
  /* test that a NUL byte (in US-ASCII data) is an error */
  expect_failure("<doc>&#0;</doc>", XML_ERROR_BAD_CHAR_REF,
                 "Parser did not report error on NUL-byte.");
}
END_TEST

START_TEST(test_siphash_self) {
  if (! sip24_valid())
    fail("SipHash self-test failed");
}
END_TEST

START_TEST(test_siphash_spec) {
  /* https://131002.net/siphash/siphash.pdf (page 19, "Test values") */
  const char message[] = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09"
                         "\x0a\x0b\x0c\x0d\x0e";
  const size_t len = sizeof(message) - 1;
  const uint64_t expected = SIP_ULL(0xa129ca61U, 0x49be45e5U);
  struct siphash state;
  struct sipkey key;

  sip_tokey(&key, "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09"
                  "\x0a\x0b\x0c\x0d\x0e\x0f");
  sip24_init(&state, &key);

  /* Cover spread across calls */
  sip24_update(&state, message, 4);
  sip24_update(&state, message + 4, len - 4);

  /* Cover null length */
  sip24_update(&state, message, 0);

  if (sip24_final(&state) != expected)
    fail("sip24_final failed spec test\n");

  /* Cover wrapper */
  if (siphash24(message, len, &key) != expected)
    fail("siphash24 failed spec test\n");
}
END_TEST

START_TEST(test_bom_utf8) {
  /* This test is really just making sure we don't core on a UTF-8 BOM. */
  const char *text = "\357\273\277<e/>";

  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
}
END_TEST

START_TEST(test_bom_utf16_be) {
  char text[] = "\376\377\0<\0e\0/\0>";

  if (_XML_Parse_SINGLE_BYTES(g_parser, text, sizeof(text) - 1, XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
}
END_TEST

START_TEST(test_bom_utf16_le) {
  char text[] = "\377\376<\0e\0/\0>\0";

  if (_XML_Parse_SINGLE_BYTES(g_parser, text, sizeof(text) - 1, XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
}
END_TEST

/* Parse whole buffer at once to exercise a different code path */
START_TEST(test_nobom_utf16_le) {
  char text[] = " \0<\0e\0/\0>\0";

  if (XML_Parse(g_parser, text, sizeof(text) - 1, XML_TRUE) == XML_STATUS_ERROR)
    xml_failure(g_parser);
}
END_TEST

START_TEST(test_hash_collision) {
  /* For full coverage of the lookup routine, we need to ensure a
   * hash collision even though we can only tell that we have one
   * through breakpoint debugging or coverage statistics.  The
   * following will cause a hash collision on machines with a 64-bit
   * long type; others will have to experiment.  The full coverage
   * tests invoked from qa.sh usually provide a hash collision, but
   * not always.  This is an attempt to provide insurance.
   */
#define COLLIDING_HASH_SALT (unsigned long)SIP_ULL(0xffffffffU, 0xff99fc90U)
  const char *text
      = "<doc>\n"
        "<a1/><a2/><a3/><a4/><a5/><a6/><a7/><a8/>\n"
        "<b1></b1><b2 attr='foo'>This is a foo</b2><b3></b3><b4></b4>\n"
        "<b5></b5><b6></b6><b7></b7><b8></b8>\n"
        "<c1/><c2/><c3/><c4/><c5/><c6/><c7/><c8/>\n"
        "<d1/><d2/><d3/><d4/><d5/><d6/><d7/>\n"
        "<d8>This triggers the table growth and collides with b2</d8>\n"
        "</doc>\n";

  XML_SetHashSalt(g_parser, COLLIDING_HASH_SALT);
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
}
END_TEST
#undef COLLIDING_HASH_SALT

/* Regression test for SF bug #491986. */
START_TEST(test_danish_latin1) {
  const char *text = "<?xml version='1.0' encoding='iso-8859-1'?>\n"
                     "<e>J\xF8rgen \xE6\xF8\xE5\xC6\xD8\xC5</e>";
#ifdef XML_UNICODE
  const XML_Char *expected
      = XCS("J\x00f8rgen \x00e6\x00f8\x00e5\x00c6\x00d8\x00c5");
#else
  const XML_Char *expected
      = XCS("J\xC3\xB8rgen \xC3\xA6\xC3\xB8\xC3\xA5\xC3\x86\xC3\x98\xC3\x85");
#endif
  run_character_check(text, expected);
}
END_TEST

/* Regression test for SF bug #514281. */
START_TEST(test_french_charref_hexidecimal) {
  const char *text = "<?xml version='1.0' encoding='iso-8859-1'?>\n"
                     "<doc>&#xE9;&#xE8;&#xE0;&#xE7;&#xEA;&#xC8;</doc>";
#ifdef XML_UNICODE
  const XML_Char *expected = XCS("\x00e9\x00e8\x00e0\x00e7\x00ea\x00c8");
#else
  const XML_Char *expected
      = XCS("\xC3\xA9\xC3\xA8\xC3\xA0\xC3\xA7\xC3\xAA\xC3\x88");
#endif
  run_character_check(text, expected);
}
END_TEST

START_TEST(test_french_charref_decimal) {
  const char *text = "<?xml version='1.0' encoding='iso-8859-1'?>\n"
                     "<doc>&#233;&#232;&#224;&#231;&#234;&#200;</doc>";
#ifdef XML_UNICODE
  const XML_Char *expected = XCS("\x00e9\x00e8\x00e0\x00e7\x00ea\x00c8");
#else
  const XML_Char *expected
      = XCS("\xC3\xA9\xC3\xA8\xC3\xA0\xC3\xA7\xC3\xAA\xC3\x88");
#endif
  run_character_check(text, expected);
}
END_TEST

START_TEST(test_french_latin1) {
  const char *text = "<?xml version='1.0' encoding='iso-8859-1'?>\n"
                     "<doc>\xE9\xE8\xE0\xE7\xEa\xC8</doc>";
#ifdef XML_UNICODE
  const XML_Char *expected = XCS("\x00e9\x00e8\x00e0\x00e7\x00ea\x00c8");
#else
  const XML_Char *expected
      = XCS("\xC3\xA9\xC3\xA8\xC3\xA0\xC3\xA7\xC3\xAA\xC3\x88");
#endif
  run_character_check(text, expected);
}
END_TEST

START_TEST(test_french_utf8) {
  const char *text = "<?xml version='1.0' encoding='utf-8'?>\n"
                     "<doc>\xC3\xA9</doc>";
#ifdef XML_UNICODE
  const XML_Char *expected = XCS("\x00e9");
#else
  const XML_Char *expected = XCS("\xC3\xA9");
#endif
  run_character_check(text, expected);
}
END_TEST

/* Regression test for SF bug #600479.
   XXX There should be a test that exercises all legal XML Unicode
   characters as PCDATA and attribute value content, and XML Name
   characters as part of element and attribute names.
*/
START_TEST(test_utf8_false_rejection) {
  const char *text = "<doc>\xEF\xBA\xBF</doc>";
#ifdef XML_UNICODE
  const XML_Char *expected = XCS("\xfebf");
#else
  const XML_Char *expected = XCS("\xEF\xBA\xBF");
#endif
  run_character_check(text, expected);
}
END_TEST

/* Regression test for SF bug #477667.
   This test assures that any 8-bit character followed by a 7-bit
   character will not be mistakenly interpreted as a valid UTF-8
   sequence.
*/
START_TEST(test_illegal_utf8) {
  char text[100];
  int i;

  for (i = 128; i <= 255; ++i) {
    snprintf(text, sizeof(text), "<e>%ccd</e>", i);
    if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
        == XML_STATUS_OK) {
      snprintf(text, sizeof(text),
               "expected token error for '%c' (ordinal %d) in UTF-8 text", i,
               i);
      fail(text);
    } else if (XML_GetErrorCode(g_parser) != XML_ERROR_INVALID_TOKEN)
      xml_failure(g_parser);
    /* Reset the parser since we use the same parser repeatedly. */
    XML_ParserReset(g_parser, NULL);
  }
}
END_TEST

/* Examples, not masks: */
#define UTF8_LEAD_1 "\x7f" /* 0b01111111 */
#define UTF8_LEAD_2 "\xdf" /* 0b11011111 */
#define UTF8_LEAD_3 "\xef" /* 0b11101111 */
#define UTF8_LEAD_4 "\xf7" /* 0b11110111 */
#define UTF8_FOLLOW "\xbf" /* 0b10111111 */

START_TEST(test_utf8_auto_align) {
  struct TestCase {
    ptrdiff_t expectedMovementInChars;
    const char *input;
  };

  struct TestCase cases[] = {
      {00, ""},

      {00, UTF8_LEAD_1},

      {-1, UTF8_LEAD_2},
      {00, UTF8_LEAD_2 UTF8_FOLLOW},

      {-1, UTF8_LEAD_3},
      {-2, UTF8_LEAD_3 UTF8_FOLLOW},
      {00, UTF8_LEAD_3 UTF8_FOLLOW UTF8_FOLLOW},

      {-1, UTF8_LEAD_4},
      {-2, UTF8_LEAD_4 UTF8_FOLLOW},
      {-3, UTF8_LEAD_4 UTF8_FOLLOW UTF8_FOLLOW},
      {00, UTF8_LEAD_4 UTF8_FOLLOW UTF8_FOLLOW UTF8_FOLLOW},
  };

  size_t i = 0;
  bool success = true;
  for (; i < sizeof(cases) / sizeof(*cases); i++) {
    const char *fromLim = cases[i].input + strlen(cases[i].input);
    const char *const fromLimInitially = fromLim;
    ptrdiff_t actualMovementInChars;

    _INTERNAL_trim_to_complete_utf8_characters(cases[i].input, &fromLim);

    actualMovementInChars = (fromLim - fromLimInitially);
    if (actualMovementInChars != cases[i].expectedMovementInChars) {
      size_t j = 0;
      success = false;
      printf("[-] UTF-8 case %2u: Expected movement by %2d chars"
             ", actually moved by %2d chars: \"",
             (unsigned)(i + 1), (int)cases[i].expectedMovementInChars,
             (int)actualMovementInChars);
      for (; j < strlen(cases[i].input); j++) {
        printf("\\x%02x", (unsigned char)cases[i].input[j]);
      }
      printf("\"\n");
    }
  }

  if (! success) {
    fail("UTF-8 auto-alignment is not bullet-proof\n");
  }
}
END_TEST

START_TEST(test_utf16) {
  /* <?xml version="1.0" encoding="UTF-16"?>
   *  <doc a='123'>some {A} text</doc>
   *
   * where {A} is U+FF21, FULLWIDTH LATIN CAPITAL LETTER A
   */
  char text[]
      = "\000<\000?\000x\000m\000\154\000 \000v\000e\000r\000s\000i\000o"
        "\000n\000=\000'\0001\000.\000\060\000'\000 \000e\000n\000c\000o"
        "\000d\000i\000n\000g\000=\000'\000U\000T\000F\000-\0001\000\066"
        "\000'\000?\000>\000\n"
        "\000<\000d\000o\000c\000 \000a\000=\000'\0001\0002\0003\000'\000>"
        "\000s\000o\000m\000e\000 \xff\x21\000 \000t\000e\000x\000t\000"
        "<\000/\000d\000o\000c\000>";
#ifdef XML_UNICODE
  const XML_Char *expected = XCS("some \xff21 text");
#else
  const XML_Char *expected = XCS("some \357\274\241 text");
#endif
  CharData storage;

  CharData_Init(&storage);
  XML_SetUserData(g_parser, &storage);
  XML_SetCharacterDataHandler(g_parser, accumulate_characters);
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, sizeof(text) - 1, XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  CharData_CheckXMLChars(&storage, expected);
}
END_TEST

START_TEST(test_utf16_le_epilog_newline) {
  unsigned int first_chunk_bytes = 17;
  char text[] = "\xFF\xFE"                  /* BOM */
                "<\000e\000/\000>\000"      /* document element */
                "\r\000\n\000\r\000\n\000"; /* epilog */

  if (first_chunk_bytes >= sizeof(text) - 1)
    fail("bad value of first_chunk_bytes");
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, first_chunk_bytes, XML_FALSE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  else {
    enum XML_Status rc;
    rc = _XML_Parse_SINGLE_BYTES(g_parser, text + first_chunk_bytes,
                                 sizeof(text) - first_chunk_bytes - 1,
                                 XML_TRUE);
    if (rc == XML_STATUS_ERROR)
      xml_failure(g_parser);
  }
}
END_TEST

/* Test that an outright lie in the encoding is faulted */
START_TEST(test_not_utf16) {
  const char *text = "<?xml version='1.0' encoding='utf-16'?>"
                     "<doc>Hi</doc>";

  /* Use a handler to provoke the appropriate code paths */
  XML_SetXmlDeclHandler(g_parser, dummy_xdecl_handler);
  expect_failure(text, XML_ERROR_INCORRECT_ENCODING,
                 "UTF-16 declared in UTF-8 not faulted");
}
END_TEST

/* Test that an unknown encoding is rejected */
START_TEST(test_bad_encoding) {
  const char *text = "<doc>Hi</doc>";

  if (! XML_SetEncoding(g_parser, XCS("unknown-encoding")))
    fail("XML_SetEncoding failed");
  expect_failure(text, XML_ERROR_UNKNOWN_ENCODING,
                 "Unknown encoding not faulted");
}
END_TEST

/* Regression test for SF bug #481609, #774028. */
START_TEST(test_latin1_umlauts) {
  const char *text
      = "<?xml version='1.0' encoding='iso-8859-1'?>\n"
        "<e a='\xE4 \xF6 \xFC &#228; &#246; &#252; &#x00E4; &#x0F6; &#xFC; >'\n"
        "  >\xE4 \xF6 \xFC &#228; &#246; &#252; &#x00E4; &#x0F6; &#xFC; ></e>";
#ifdef XML_UNICODE
  /* Expected results in UTF-16 */
  const XML_Char *expected = XCS("\x00e4 \x00f6 \x00fc ")
      XCS("\x00e4 \x00f6 \x00fc ") XCS("\x00e4 \x00f6 \x00fc >");
#else
  /* Expected results in UTF-8 */
  const XML_Char *expected = XCS("\xC3\xA4 \xC3\xB6 \xC3\xBC ")
      XCS("\xC3\xA4 \xC3\xB6 \xC3\xBC ") XCS("\xC3\xA4 \xC3\xB6 \xC3\xBC >");
#endif

  run_character_check(text, expected);
  XML_ParserReset(g_parser, NULL);
  run_attribute_check(text, expected);
  /* Repeat with a default handler */
  XML_ParserReset(g_parser, NULL);
  XML_SetDefaultHandler(g_parser, dummy_default_handler);
  run_character_check(text, expected);
  XML_ParserReset(g_parser, NULL);
  XML_SetDefaultHandler(g_parser, dummy_default_handler);
  run_attribute_check(text, expected);
}
END_TEST

/* Test that an element name with a 4-byte UTF-8 character is rejected */
START_TEST(test_long_utf8_character) {
  const char *text
      = "<?xml version='1.0' encoding='utf-8'?>\n"
        /* 0xf0 0x90 0x80 0x80 = U+10000, the first Linear B character */
        "<do\xf0\x90\x80\x80/>";
  expect_failure(text, XML_ERROR_INVALID_TOKEN,
                 "4-byte UTF-8 character in element name not faulted");
}
END_TEST

/* Test that a long latin-1 attribute (too long to convert in one go)
 * is correctly converted
 */
START_TEST(test_long_latin1_attribute) {
  const char *text
      = "<?xml version='1.0' encoding='iso-8859-1'?>\n"
        "<doc att='"
        /* 64 characters per line */
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNO"
        /* Last character splits across a buffer boundary */
        "\xe4'>\n</doc>";

  const XML_Char *expected =
      /* 64 characters per line */
      /* clang-format off */
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNO")
  /* clang-format on */
#ifdef XML_UNICODE
                                                  XCS("\x00e4");
#else
                                                  XCS("\xc3\xa4");
#endif

  run_attribute_check(text, expected);
}
END_TEST

/* Test that a long ASCII attribute (too long to convert in one go)
 * is correctly converted
 */
START_TEST(test_long_ascii_attribute) {
  const char *text
      = "<?xml version='1.0' encoding='us-ascii'?>\n"
        "<doc att='"
        /* 64 characters per line */
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"
        "01234'>\n</doc>";
  const XML_Char *expected =
      /* 64 characters per line */
      /* clang-format off */
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("01234");
  /* clang-format on */

  run_attribute_check(text, expected);
}
END_TEST

/* Regression test #1 for SF bug #653180. */
START_TEST(test_line_number_after_parse) {
  const char *text = "<tag>\n"
                     "\n"
                     "\n</tag>";
  XML_Size lineno;

  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_FALSE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  lineno = XML_GetCurrentLineNumber(g_parser);
  if (lineno != 4) {
    char buffer[100];
    snprintf(buffer, sizeof(buffer),
             "expected 4 lines, saw %" XML_FMT_INT_MOD "u", lineno);
    fail(buffer);
  }
}
END_TEST

/* Regression test #2 for SF bug #653180. */
START_TEST(test_column_number_after_parse) {
  const char *text = "<tag></tag>";
  XML_Size colno;

  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_FALSE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  colno = XML_GetCurrentColumnNumber(g_parser);
  if (colno != 11) {
    char buffer[100];
    snprintf(buffer, sizeof(buffer),
             "expected 11 columns, saw %" XML_FMT_INT_MOD "u", colno);
    fail(buffer);
  }
}
END_TEST

/* Regression test #3 for SF bug #653180. */
START_TEST(test_line_and_column_numbers_inside_handlers) {
  const char *text = "<a>\n"      /* Unix end-of-line */
                     "  <b>\r\n"  /* Windows end-of-line */
                     "    <c/>\r" /* Mac OS end-of-line */
                     "  </b>\n"
                     "  <d>\n"
                     "    <f/>\n"
                     "  </d>\n"
                     "</a>";
  const StructDataEntry expected[]
      = {{XCS("a"), 0, 1, STRUCT_START_TAG}, {XCS("b"), 2, 2, STRUCT_START_TAG},
         {XCS("c"), 4, 3, STRUCT_START_TAG}, {XCS("c"), 8, 3, STRUCT_END_TAG},
         {XCS("b"), 2, 4, STRUCT_END_TAG},   {XCS("d"), 2, 5, STRUCT_START_TAG},
         {XCS("f"), 4, 6, STRUCT_START_TAG}, {XCS("f"), 8, 6, STRUCT_END_TAG},
         {XCS("d"), 2, 7, STRUCT_END_TAG},   {XCS("a"), 0, 8, STRUCT_END_TAG}};
  const int expected_count = sizeof(expected) / sizeof(StructDataEntry);
  StructData storage;

  StructData_Init(&storage);
  XML_SetUserData(g_parser, &storage);
  XML_SetStartElementHandler(g_parser, start_element_event_handler2);
  XML_SetEndElementHandler(g_parser, end_element_event_handler2);
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);

  StructData_CheckItems(&storage, expected, expected_count);
  StructData_Dispose(&storage);
}
END_TEST

/* Regression test #4 for SF bug #653180. */
START_TEST(test_line_number_after_error) {
  const char *text = "<a>\n"
                     "  <b>\n"
                     "  </a>"; /* missing </b> */
  XML_Size lineno;
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_FALSE)
      != XML_STATUS_ERROR)
    fail("Expected a parse error");

  lineno = XML_GetCurrentLineNumber(g_parser);
  if (lineno != 3) {
    char buffer[100];
    snprintf(buffer, sizeof(buffer),
             "expected 3 lines, saw %" XML_FMT_INT_MOD "u", lineno);
    fail(buffer);
  }
}
END_TEST

/* Regression test #5 for SF bug #653180. */
START_TEST(test_column_number_after_error) {
  const char *text = "<a>\n"
                     "  <b>\n"
                     "  </a>"; /* missing </b> */
  XML_Size colno;
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_FALSE)
      != XML_STATUS_ERROR)
    fail("Expected a parse error");

  colno = XML_GetCurrentColumnNumber(g_parser);
  if (colno != 4) {
    char buffer[100];
    snprintf(buffer, sizeof(buffer),
             "expected 4 columns, saw %" XML_FMT_INT_MOD "u", colno);
    fail(buffer);
  }
}
END_TEST

/* Regression test for SF bug #478332. */
START_TEST(test_really_long_lines) {
  /* This parses an input line longer than INIT_DATA_BUF_SIZE
     characters long (defined to be 1024 in xmlparse.c).  We take a
     really cheesy approach to building the input buffer, because
     this avoids writing bugs in buffer-filling code.
  */
  const char *text
      = "<e>"
        /* 64 chars */
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        /* until we have at least 1024 characters on the line: */
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "</e>";
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
}
END_TEST

/* Test cdata processing across a buffer boundary */
START_TEST(test_really_long_encoded_lines) {
  /* As above, except that we want to provoke an output buffer
   * overflow with a non-trivial encoding.  For this we need to pass
   * the whole cdata in one go, not byte-by-byte.
   */
  void *buffer;
  const char *text
      = "<?xml version='1.0' encoding='iso-8859-1'?>"
        "<e>"
        /* 64 chars */
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        /* until we have at least 1024 characters on the line: */
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+"
        "</e>";
  int parse_len = (int)strlen(text);

  /* Need a cdata handler to provoke the code path we want to test */
  XML_SetCharacterDataHandler(g_parser, dummy_cdata_handler);
  buffer = XML_GetBuffer(g_parser, parse_len);
  if (buffer == NULL)
    fail("Could not allocate parse buffer");
  assert(buffer != NULL);
  memcpy(buffer, text, parse_len);
  if (XML_ParseBuffer(g_parser, parse_len, XML_TRUE) == XML_STATUS_ERROR)
    xml_failure(g_parser);
}
END_TEST

/*
 * Element event tests.
 */

START_TEST(test_end_element_events) {
  const char *text = "<a><b><c/></b><d><f/></d></a>";
  const XML_Char *expected = XCS("/c/b/f/d/a");
  CharData storage;

  CharData_Init(&storage);
  XML_SetUserData(g_parser, &storage);
  XML_SetEndElementHandler(g_parser, end_element_event_handler);
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  CharData_CheckXMLChars(&storage, expected);
}
END_TEST

/*
 * Attribute tests.
 */

/* Helper used by the following tests; this checks any "attr" and "refs"
   attributes to make sure whitespace has been normalized.

   Return true if whitespace has been normalized in a string, using
   the rules for attribute value normalization.  The 'is_cdata' flag
   is needed since CDATA attributes don't need to have multiple
   whitespace characters collapsed to a single space, while other
   attribute data types do.  (Section 3.3.3 of the recommendation.)
*/
static int
is_whitespace_normalized(const XML_Char *s, int is_cdata) {
  int blanks = 0;
  int at_start = 1;
  while (*s) {
    if (*s == XCS(' '))
      ++blanks;
    else if (*s == XCS('\t') || *s == XCS('\n') || *s == XCS('\r'))
      return 0;
    else {
      if (at_start) {
        at_start = 0;
        if (blanks && ! is_cdata)
          /* illegal leading blanks */
          return 0;
      } else if (blanks > 1 && ! is_cdata)
        return 0;
      blanks = 0;
    }
    ++s;
  }
  if (blanks && ! is_cdata)
    return 0;
  return 1;
}

/* Check the attribute whitespace checker: */
START_TEST(test_helper_is_whitespace_normalized) {
  assert(is_whitespace_normalized(XCS("abc"), 0));
  assert(is_whitespace_normalized(XCS("abc"), 1));
  assert(is_whitespace_normalized(XCS("abc def ghi"), 0));
  assert(is_whitespace_normalized(XCS("abc def ghi"), 1));
  assert(! is_whitespace_normalized(XCS(" abc def ghi"), 0));
  assert(is_whitespace_normalized(XCS(" abc def ghi"), 1));
  assert(! is_whitespace_normalized(XCS("abc  def ghi"), 0));
  assert(is_whitespace_normalized(XCS("abc  def ghi"), 1));
  assert(! is_whitespace_normalized(XCS("abc def ghi "), 0));
  assert(is_whitespace_normalized(XCS("abc def ghi "), 1));
  assert(! is_whitespace_normalized(XCS(" "), 0));
  assert(is_whitespace_normalized(XCS(" "), 1));
  assert(! is_whitespace_normalized(XCS("\t"), 0));
  assert(! is_whitespace_normalized(XCS("\t"), 1));
  assert(! is_whitespace_normalized(XCS("\n"), 0));
  assert(! is_whitespace_normalized(XCS("\n"), 1));
  assert(! is_whitespace_normalized(XCS("\r"), 0));
  assert(! is_whitespace_normalized(XCS("\r"), 1));
  assert(! is_whitespace_normalized(XCS("abc\t def"), 1));
}
END_TEST

static void XMLCALL
check_attr_contains_normalized_whitespace(void *userData, const XML_Char *name,
                                          const XML_Char **atts) {
  int i;
  UNUSED_P(userData);
  UNUSED_P(name);
  for (i = 0; atts[i] != NULL; i += 2) {
    const XML_Char *attrname = atts[i];
    const XML_Char *value = atts[i + 1];
    if (xcstrcmp(XCS("attr"), attrname) == 0
        || xcstrcmp(XCS("ents"), attrname) == 0
        || xcstrcmp(XCS("refs"), attrname) == 0) {
      if (! is_whitespace_normalized(value, 0)) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer),
                 "attribute value not normalized: %" XML_FMT_STR
                 "='%" XML_FMT_STR "'",
                 attrname, value);
        fail(buffer);
      }
    }
  }
}

START_TEST(test_attr_whitespace_normalization) {
  const char *text
      = "<!DOCTYPE doc [\n"
        "  <!ATTLIST doc\n"
        "            attr NMTOKENS #REQUIRED\n"
        "            ents ENTITIES #REQUIRED\n"
        "            refs IDREFS   #REQUIRED>\n"
        "]>\n"
        "<doc attr='    a  b c\t\td\te\t' refs=' id-1   \t  id-2\t\t'  \n"
        "     ents=' ent-1   \t\r\n"
        "            ent-2  ' >\n"
        "  <e id='id-1'/>\n"
        "  <e id='id-2'/>\n"
        "</doc>";

  XML_SetStartElementHandler(g_parser,
                             check_attr_contains_normalized_whitespace);
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
}
END_TEST

/*
 * XML declaration tests.
 */

START_TEST(test_xmldecl_misplaced) {
  expect_failure("\n"
                 "<?xml version='1.0'?>\n"
                 "<a/>",
                 XML_ERROR_MISPLACED_XML_PI,
                 "failed to report misplaced XML declaration");
}
END_TEST

START_TEST(test_xmldecl_invalid) {
  expect_failure("<?xml version='1.0' \xc3\xa7?>\n<doc/>", XML_ERROR_XML_DECL,
                 "Failed to report invalid XML declaration");
}
END_TEST

START_TEST(test_xmldecl_missing_attr) {
  expect_failure("<?xml ='1.0'?>\n<doc/>\n", XML_ERROR_XML_DECL,
                 "Failed to report missing XML declaration attribute");
}
END_TEST

START_TEST(test_xmldecl_missing_value) {
  expect_failure("<?xml version='1.0' encoding='us-ascii' standalone?>\n"
                 "<doc/>",
                 XML_ERROR_XML_DECL,
                 "Failed to report missing attribute value");
}
END_TEST

/* Regression test for SF bug #584832. */
START_TEST(test_unknown_encoding_internal_entity) {
  const char *text = "<?xml version='1.0' encoding='unsupported-encoding'?>\n"
                     "<!DOCTYPE test [<!ENTITY foo 'bar'>]>\n"
                     "<test a='&foo;'/>";

  XML_SetUnknownEncodingHandler(g_parser, UnknownEncodingHandler, NULL);
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
}
END_TEST

/* Test unrecognised encoding handler */
START_TEST(test_unrecognised_encoding_internal_entity) {
  const char *text = "<?xml version='1.0' encoding='unsupported-encoding'?>\n"
                     "<!DOCTYPE test [<!ENTITY foo 'bar'>]>\n"
                     "<test a='&foo;'/>";

  XML_SetUnknownEncodingHandler(g_parser, UnrecognisedEncodingHandler, NULL);
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      != XML_STATUS_ERROR)
    fail("Unrecognised encoding not rejected");
}
END_TEST

/* Regression test for SF bug #620106. */
START_TEST(test_ext_entity_set_encoding) {
  const char *text = "<!DOCTYPE doc [\n"
                     "  <!ENTITY en SYSTEM 'http://example.org/dummy.ent'>\n"
                     "]>\n"
                     "<doc>&en;</doc>";
  ExtTest test_data
      = {/* This text says it's an unsupported encoding, but it's really
            UTF-8, which we tell Expat using XML_SetEncoding().
         */
         "<?xml encoding='iso-8859-3'?>\xC3\xA9", XCS("utf-8"), NULL};
#ifdef XML_UNICODE
  const XML_Char *expected = XCS("\x00e9");
#else
  const XML_Char *expected = XCS("\xc3\xa9");
#endif

  XML_SetExternalEntityRefHandler(g_parser, external_entity_loader);
  run_ext_character_check(text, &test_data, expected);
}
END_TEST

/* Test external entities with no handler */
START_TEST(test_ext_entity_no_handler) {
  const char *text = "<!DOCTYPE doc [\n"
                     "  <!ENTITY en SYSTEM 'http://example.org/dummy.ent'>\n"
                     "]>\n"
                     "<doc>&en;</doc>";

  XML_SetDefaultHandler(g_parser, dummy_default_handler);
  run_character_check(text, XCS(""));
}
END_TEST

/* Test UTF-8 BOM is accepted */
START_TEST(test_ext_entity_set_bom) {
  const char *text = "<!DOCTYPE doc [\n"
                     "  <!ENTITY en SYSTEM 'http://example.org/dummy.ent'>\n"
                     "]>\n"
                     "<doc>&en;</doc>";
  ExtTest test_data = {"\xEF\xBB\xBF" /* BOM */
                       "<?xml encoding='iso-8859-3'?>"
                       "\xC3\xA9",
                       XCS("utf-8"), NULL};
#ifdef XML_UNICODE
  const XML_Char *expected = XCS("\x00e9");
#else
  const XML_Char *expected = XCS("\xc3\xa9");
#endif

  XML_SetExternalEntityRefHandler(g_parser, external_entity_loader);
  run_ext_character_check(text, &test_data, expected);
}
END_TEST

/* Test that bad encodings are faulted */
START_TEST(test_ext_entity_bad_encoding) {
  const char *text = "<!DOCTYPE doc [\n"
                     "  <!ENTITY en SYSTEM 'http://example.org/dummy.ent'>\n"
                     "]>\n"
                     "<doc>&en;</doc>";
  ExtFaults fault
      = {"<?xml encoding='iso-8859-3'?>u", "Unsupported encoding not faulted",
         XCS("unknown"), XML_ERROR_UNKNOWN_ENCODING};

  XML_SetExternalEntityRefHandler(g_parser, external_entity_faulter);
  XML_SetUserData(g_parser, &fault);
  expect_failure(text, XML_ERROR_EXTERNAL_ENTITY_HANDLING,
                 "Bad encoding should not have been accepted");
}
END_TEST

/* Try handing an invalid encoding to an external entity parser */
START_TEST(test_ext_entity_bad_encoding_2) {
  const char *text = "<?xml version='1.0' encoding='us-ascii'?>\n"
                     "<!DOCTYPE doc SYSTEM 'foo'>\n"
                     "<doc>&entity;</doc>";
  ExtFaults fault
      = {"<!ELEMENT doc (#PCDATA)*>", "Unknown encoding not faulted",
         XCS("unknown-encoding"), XML_ERROR_UNKNOWN_ENCODING};

  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetExternalEntityRefHandler(g_parser, external_entity_faulter);
  XML_SetUserData(g_parser, &fault);
  expect_failure(text, XML_ERROR_EXTERNAL_ENTITY_HANDLING,
                 "Bad encoding not faulted in external entity handler");
}
END_TEST

/* Test that no error is reported for unknown entities if we don't
   read an external subset.  This was fixed in Expat 1.95.5.
*/
START_TEST(test_wfc_undeclared_entity_unread_external_subset) {
  const char *text = "<!DOCTYPE doc SYSTEM 'foo'>\n"
                     "<doc>&entity;</doc>";

  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
}
END_TEST

/* Test that an error is reported for unknown entities if we don't
   have an external subset.
*/
START_TEST(test_wfc_undeclared_entity_no_external_subset) {
  expect_failure("<doc>&entity;</doc>", XML_ERROR_UNDEFINED_ENTITY,
                 "Parser did not report undefined entity w/out a DTD.");
}
END_TEST

/* Test that an error is reported for unknown entities if we don't
   read an external subset, but have been declared standalone.
*/
START_TEST(test_wfc_undeclared_entity_standalone) {
  const char *text
      = "<?xml version='1.0' encoding='us-ascii' standalone='yes'?>\n"
        "<!DOCTYPE doc SYSTEM 'foo'>\n"
        "<doc>&entity;</doc>";

  expect_failure(text, XML_ERROR_UNDEFINED_ENTITY,
                 "Parser did not report undefined entity (standalone).");
}
END_TEST

/* Test that an error is reported for unknown entities if we have read
   an external subset, and standalone is true.
*/
START_TEST(test_wfc_undeclared_entity_with_external_subset_standalone) {
  const char *text
      = "<?xml version='1.0' encoding='us-ascii' standalone='yes'?>\n"
        "<!DOCTYPE doc SYSTEM 'foo'>\n"
        "<doc>&entity;</doc>";
  ExtTest test_data = {"<!ELEMENT doc (#PCDATA)*>", NULL, NULL};

  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetUserData(g_parser, &test_data);
  XML_SetExternalEntityRefHandler(g_parser, external_entity_loader);
  expect_failure(text, XML_ERROR_UNDEFINED_ENTITY,
                 "Parser did not report undefined entity (external DTD).");
}
END_TEST

/* Test that external entity handling is not done if the parsing flag
 * is set to UNLESS_STANDALONE
 */
START_TEST(test_entity_with_external_subset_unless_standalone) {
  const char *text
      = "<?xml version='1.0' encoding='us-ascii' standalone='yes'?>\n"
        "<!DOCTYPE doc SYSTEM 'foo'>\n"
        "<doc>&entity;</doc>";
  ExtTest test_data = {"<!ENTITY entity 'bar'>", NULL, NULL};

  XML_SetParamEntityParsing(g_parser,
                            XML_PARAM_ENTITY_PARSING_UNLESS_STANDALONE);
  XML_SetUserData(g_parser, &test_data);
  XML_SetExternalEntityRefHandler(g_parser, external_entity_loader);
  expect_failure(text, XML_ERROR_UNDEFINED_ENTITY,
                 "Parser did not report undefined entity");
}
END_TEST

/* Test that no error is reported for unknown entities if we have read
   an external subset, and standalone is false.
*/
START_TEST(test_wfc_undeclared_entity_with_external_subset) {
  const char *text = "<?xml version='1.0' encoding='us-ascii'?>\n"
                     "<!DOCTYPE doc SYSTEM 'foo'>\n"
                     "<doc>&entity;</doc>";
  ExtTest test_data = {"<!ELEMENT doc (#PCDATA)*>", NULL, NULL};

  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetExternalEntityRefHandler(g_parser, external_entity_loader);
  run_ext_character_check(text, &test_data, XCS(""));
}
END_TEST

/* Test that an error is reported if our NotStandalone handler fails */
START_TEST(test_not_standalone_handler_reject) {
  const char *text = "<?xml version='1.0' encoding='us-ascii'?>\n"
                     "<!DOCTYPE doc SYSTEM 'foo'>\n"
                     "<doc>&entity;</doc>";
  ExtTest test_data = {"<!ELEMENT doc (#PCDATA)*>", NULL, NULL};

  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetUserData(g_parser, &test_data);
  XML_SetExternalEntityRefHandler(g_parser, external_entity_loader);
  XML_SetNotStandaloneHandler(g_parser, reject_not_standalone_handler);
  expect_failure(text, XML_ERROR_NOT_STANDALONE,
                 "NotStandalone handler failed to reject");

  /* Try again but without external entity handling */
  XML_ParserReset(g_parser, NULL);
  XML_SetNotStandaloneHandler(g_parser, reject_not_standalone_handler);
  expect_failure(text, XML_ERROR_NOT_STANDALONE,
                 "NotStandalone handler failed to reject");
}
END_TEST

/* Test that no error is reported if our NotStandalone handler succeeds */
START_TEST(test_not_standalone_handler_accept) {
  const char *text = "<?xml version='1.0' encoding='us-ascii'?>\n"
                     "<!DOCTYPE doc SYSTEM 'foo'>\n"
                     "<doc>&entity;</doc>";
  ExtTest test_data = {"<!ELEMENT doc (#PCDATA)*>", NULL, NULL};

  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetExternalEntityRefHandler(g_parser, external_entity_loader);
  XML_SetNotStandaloneHandler(g_parser, accept_not_standalone_handler);
  run_ext_character_check(text, &test_data, XCS(""));

  /* Repeat without the external entity handler */
  XML_ParserReset(g_parser, NULL);
  XML_SetNotStandaloneHandler(g_parser, accept_not_standalone_handler);
  run_character_check(text, XCS(""));
}
END_TEST

START_TEST(test_wfc_no_recursive_entity_refs) {
  const char *text = "<!DOCTYPE doc [\n"
                     "  <!ENTITY entity '&#38;entity;'>\n"
                     "]>\n"
                     "<doc>&entity;</doc>";

  expect_failure(text, XML_ERROR_RECURSIVE_ENTITY_REF,
                 "Parser did not report recursive entity reference.");
}
END_TEST

/* Test incomplete external entities are faulted */
START_TEST(test_ext_entity_invalid_parse) {
  const char *text = "<!DOCTYPE doc [\n"
                     "  <!ENTITY en SYSTEM 'http://example.org/dummy.ent'>\n"
                     "]>\n"
                     "<doc>&en;</doc>";
  const ExtFaults faults[]
      = {{"<", "Incomplete element declaration not faulted", NULL,
          XML_ERROR_UNCLOSED_TOKEN},
         {"<\xe2\x82", /* First two bytes of a three-byte char */
          "Incomplete character not faulted", NULL, XML_ERROR_PARTIAL_CHAR},
         {"<tag>\xe2\x82", "Incomplete character in CDATA not faulted", NULL,
          XML_ERROR_PARTIAL_CHAR},
         {NULL, NULL, NULL, XML_ERROR_NONE}};
  const ExtFaults *fault = faults;

  for (; fault->parse_text != NULL; fault++) {
    XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
    XML_SetExternalEntityRefHandler(g_parser, external_entity_faulter);
    XML_SetUserData(g_parser, (void *)fault);
    expect_failure(text, XML_ERROR_EXTERNAL_ENTITY_HANDLING,
                   "Parser did not report external entity error");
    XML_ParserReset(g_parser, NULL);
  }
}
END_TEST

/* Regression test for SF bug #483514. */
START_TEST(test_dtd_default_handling) {
  const char *text = "<!DOCTYPE doc [\n"
                     "<!ENTITY e SYSTEM 'http://example.org/e'>\n"
                     "<!NOTATION n SYSTEM 'http://example.org/n'>\n"
                     "<!ELEMENT doc EMPTY>\n"
                     "<!ATTLIST doc a CDATA #IMPLIED>\n"
                     "<?pi in dtd?>\n"
                     "<!--comment in dtd-->\n"
                     "]><doc/>";

  XML_SetDefaultHandler(g_parser, accumulate_characters);
  XML_SetStartDoctypeDeclHandler(g_parser, dummy_start_doctype_handler);
  XML_SetEndDoctypeDeclHandler(g_parser, dummy_end_doctype_handler);
  XML_SetEntityDeclHandler(g_parser, dummy_entity_decl_handler);
  XML_SetNotationDeclHandler(g_parser, dummy_notation_decl_handler);
  XML_SetElementDeclHandler(g_parser, dummy_element_decl_handler);
  XML_SetAttlistDeclHandler(g_parser, dummy_attlist_decl_handler);
  XML_SetProcessingInstructionHandler(g_parser, dummy_pi_handler);
  XML_SetCommentHandler(g_parser, dummy_comment_handler);
  XML_SetStartCdataSectionHandler(g_parser, dummy_start_cdata_handler);
  XML_SetEndCdataSectionHandler(g_parser, dummy_end_cdata_handler);
  run_character_check(text, XCS("\n\n\n\n\n\n\n<doc/>"));
}
END_TEST

/* Test handling of attribute declarations */
START_TEST(test_dtd_attr_handling) {
  const char *prolog = "<!DOCTYPE doc [\n"
                       "<!ELEMENT doc EMPTY>\n";
  AttTest attr_data[]
      = {{"<!ATTLIST doc a ( one | two | three ) #REQUIRED>\n"
          "]>"
          "<doc a='two'/>",
          XCS("doc"), XCS("a"),
          XCS("(one|two|three)"), /* Extraneous spaces will be removed */
          NULL, XML_TRUE},
         {"<!NOTATION foo SYSTEM 'http://example.org/foo'>\n"
          "<!ATTLIST doc a NOTATION (foo) #IMPLIED>\n"
          "]>"
          "<doc/>",
          XCS("doc"), XCS("a"), XCS("NOTATION(foo)"), NULL, XML_FALSE},
         {"<!ATTLIST doc a NOTATION (foo) 'bar'>\n"
          "]>"
          "<doc/>",
          XCS("doc"), XCS("a"), XCS("NOTATION(foo)"), XCS("bar"), XML_FALSE},
         {"<!ATTLIST doc a CDATA '\xdb\xb2'>\n"
          "]>"
          "<doc/>",
          XCS("doc"), XCS("a"), XCS("CDATA"),
#ifdef XML_UNICODE
          XCS("\x06f2"),
#else
          XCS("\xdb\xb2"),
#endif
          XML_FALSE},
         {NULL, NULL, NULL, NULL, NULL, XML_FALSE}};
  AttTest *test;

  for (test = attr_data; test->definition != NULL; test++) {
    XML_SetAttlistDeclHandler(g_parser, verify_attlist_decl_handler);
    XML_SetUserData(g_parser, test);
    if (_XML_Parse_SINGLE_BYTES(g_parser, prolog, (int)strlen(prolog),
                                XML_FALSE)
        == XML_STATUS_ERROR)
      xml_failure(g_parser);
    if (_XML_Parse_SINGLE_BYTES(g_parser, test->definition,
                                (int)strlen(test->definition), XML_TRUE)
        == XML_STATUS_ERROR)
      xml_failure(g_parser);
    XML_ParserReset(g_parser, NULL);
  }
}
END_TEST

/* See related SF bug #673791.
   When namespace processing is enabled, setting the namespace URI for
   a prefix is not allowed; this test ensures that it *is* allowed
   when namespace processing is not enabled.
   (See Namespaces in XML, section 2.)
*/
START_TEST(test_empty_ns_without_namespaces) {
  const char *text = "<doc xmlns:prefix='http://example.org/'>\n"
                     "  <e xmlns:prefix=''/>\n"
                     "</doc>";

  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
}
END_TEST

/* Regression test for SF bug #824420.
   Checks that an xmlns:prefix attribute set in an attribute's default
   value isn't misinterpreted.
*/
START_TEST(test_ns_in_attribute_default_without_namespaces) {
  const char *text = "<!DOCTYPE e:element [\n"
                     "  <!ATTLIST e:element\n"
                     "    xmlns:e CDATA 'http://example.org/'>\n"
                     "      ]>\n"
                     "<e:element/>";

  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
}
END_TEST

/* Regression test for SF bug #1515266: missing check of stopped
   parser in doContext() 'for' loop. */
START_TEST(test_stop_parser_between_char_data_calls) {
  /* The sample data must be big enough that there are two calls to
     the character data handler from within the inner "for" loop of
     the XML_TOK_DATA_CHARS case in doContent(), and the character
     handler must stop the parser and clear the character data
     handler.
  */
  const char *text = long_character_data_text;

  XML_SetCharacterDataHandler(g_parser, clearing_aborting_character_handler);
  g_resumable = XML_FALSE;
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      != XML_STATUS_ERROR)
    xml_failure(g_parser);
  if (XML_GetErrorCode(g_parser) != XML_ERROR_ABORTED)
    xml_failure(g_parser);
}
END_TEST

/* Regression test for SF bug #1515266: missing check of stopped
   parser in doContext() 'for' loop. */
START_TEST(test_suspend_parser_between_char_data_calls) {
  /* The sample data must be big enough that there are two calls to
     the character data handler from within the inner "for" loop of
     the XML_TOK_DATA_CHARS case in doContent(), and the character
     handler must stop the parser and clear the character data
     handler.
  */
  const char *text = long_character_data_text;

  XML_SetCharacterDataHandler(g_parser, clearing_aborting_character_handler);
  g_resumable = XML_TRUE;
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      != XML_STATUS_SUSPENDED)
    xml_failure(g_parser);
  if (XML_GetErrorCode(g_parser) != XML_ERROR_NONE)
    xml_failure(g_parser);
  /* Try parsing directly */
  if (XML_Parse(g_parser, text, (int)strlen(text), XML_TRUE)
      != XML_STATUS_ERROR)
    fail("Attempt to continue parse while suspended not faulted");
  if (XML_GetErrorCode(g_parser) != XML_ERROR_SUSPENDED)
    fail("Suspended parse not faulted with correct error");
}
END_TEST

/* Test repeated calls to XML_StopParser are handled correctly */
START_TEST(test_repeated_stop_parser_between_char_data_calls) {
  const char *text = long_character_data_text;

  XML_SetCharacterDataHandler(g_parser, parser_stop_character_handler);
  g_resumable = XML_FALSE;
  g_abortable = XML_FALSE;
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      != XML_STATUS_ERROR)
    fail("Failed to double-stop parser");

  XML_ParserReset(g_parser, NULL);
  XML_SetCharacterDataHandler(g_parser, parser_stop_character_handler);
  g_resumable = XML_TRUE;
  g_abortable = XML_FALSE;
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      != XML_STATUS_SUSPENDED)
    fail("Failed to double-suspend parser");

  XML_ParserReset(g_parser, NULL);
  XML_SetCharacterDataHandler(g_parser, parser_stop_character_handler);
  g_resumable = XML_TRUE;
  g_abortable = XML_TRUE;
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      != XML_STATUS_ERROR)
    fail("Failed to suspend-abort parser");
}
END_TEST

START_TEST(test_good_cdata_ascii) {
  const char *text = "<a><![CDATA[<greeting>Hello, world!</greeting>]]></a>";
  const XML_Char *expected = XCS("<greeting>Hello, world!</greeting>");

  CharData storage;
  CharData_Init(&storage);
  XML_SetUserData(g_parser, &storage);
  XML_SetCharacterDataHandler(g_parser, accumulate_characters);
  /* Add start and end handlers for coverage */
  XML_SetStartCdataSectionHandler(g_parser, dummy_start_cdata_handler);
  XML_SetEndCdataSectionHandler(g_parser, dummy_end_cdata_handler);

  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  CharData_CheckXMLChars(&storage, expected);

  /* Try again, this time with a default handler */
  XML_ParserReset(g_parser, NULL);
  CharData_Init(&storage);
  XML_SetUserData(g_parser, &storage);
  XML_SetCharacterDataHandler(g_parser, accumulate_characters);
  XML_SetDefaultHandler(g_parser, dummy_default_handler);

  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  CharData_CheckXMLChars(&storage, expected);
}
END_TEST

START_TEST(test_good_cdata_utf16) {
  /* Test data is:
   *   <?xml version='1.0' encoding='utf-16'?>
   *   <a><![CDATA[hello]]></a>
   */
  const char text[]
      = "\0<\0?\0x\0m\0l\0"
        " \0v\0e\0r\0s\0i\0o\0n\0=\0'\0\x31\0.\0\x30\0'\0"
        " \0e\0n\0c\0o\0d\0i\0n\0g\0=\0'\0u\0t\0f\0-\0"
        "1\0"
        "6\0'"
        "\0?\0>\0\n"
        "\0<\0a\0>\0<\0!\0[\0C\0D\0A\0T\0A\0[\0h\0e\0l\0l\0o\0]\0]\0>\0<\0/\0a\0>";
  const XML_Char *expected = XCS("hello");

  CharData storage;
  CharData_Init(&storage);
  XML_SetUserData(g_parser, &storage);
  XML_SetCharacterDataHandler(g_parser, accumulate_characters);

  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)sizeof(text) - 1, XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  CharData_CheckXMLChars(&storage, expected);
}
END_TEST

START_TEST(test_good_cdata_utf16_le) {
  /* Test data is:
   *   <?xml version='1.0' encoding='utf-16'?>
   *   <a><![CDATA[hello]]></a>
   */
  const char text[]
      = "<\0?\0x\0m\0l\0"
        " \0v\0e\0r\0s\0i\0o\0n\0=\0'\0\x31\0.\0\x30\0'\0"
        " \0e\0n\0c\0o\0d\0i\0n\0g\0=\0'\0u\0t\0f\0-\0"
        "1\0"
        "6\0'"
        "\0?\0>\0\n"
        "\0<\0a\0>\0<\0!\0[\0C\0D\0A\0T\0A\0[\0h\0e\0l\0l\0o\0]\0]\0>\0<\0/\0a\0>\0";
  const XML_Char *expected = XCS("hello");

  CharData storage;
  CharData_Init(&storage);
  XML_SetUserData(g_parser, &storage);
  XML_SetCharacterDataHandler(g_parser, accumulate_characters);

  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)sizeof(text) - 1, XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  CharData_CheckXMLChars(&storage, expected);
}
END_TEST

/* Test UTF16 conversion of a long cdata string */

/* 16 characters: handy macro to reduce visual clutter */
#define A_TO_P_IN_UTF16 "\0A\0B\0C\0D\0E\0F\0G\0H\0I\0J\0K\0L\0M\0N\0O\0P"

START_TEST(test_long_cdata_utf16) {
  /* Test data is:
   * <?xlm version='1.0' encoding='utf-16'?>
   * <a><![CDATA[
   * ABCDEFGHIJKLMNOP
   * ]]></a>
   */
  const char text[]
      = "\0<\0?\0x\0m\0l\0 "
        "\0v\0e\0r\0s\0i\0o\0n\0=\0'\0\x31\0.\0\x30\0'\0 "
        "\0e\0n\0c\0o\0d\0i\0n\0g\0=\0'\0u\0t\0f\0-\0\x31\0\x36\0'\0?\0>"
        "\0<\0a\0>\0<\0!\0[\0C\0D\0A\0T\0A\0["
      /* 64 characters per line */
      /* clang-format off */
        A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16
        A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16
        A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16
        A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16
        A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16
        A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16
        A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16
        A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16
        A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16
        A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16
        A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16
        A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16
        A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16
        A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16
        A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16
        A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16  A_TO_P_IN_UTF16
        A_TO_P_IN_UTF16
        /* clang-format on */
        "\0]\0]\0>\0<\0/\0a\0>";
  const XML_Char *expected =
      /* clang-format off */
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP")
        XCS("ABCDEFGHIJKLMNOP");
  /* clang-format on */
  CharData storage;
  void *buffer;

  CharData_Init(&storage);
  XML_SetUserData(g_parser, &storage);
  XML_SetCharacterDataHandler(g_parser, accumulate_characters);
  buffer = XML_GetBuffer(g_parser, sizeof(text) - 1);
  if (buffer == NULL)
    fail("Could not allocate parse buffer");
  assert(buffer != NULL);
  memcpy(buffer, text, sizeof(text) - 1);
  if (XML_ParseBuffer(g_parser, sizeof(text) - 1, XML_TRUE) == XML_STATUS_ERROR)
    xml_failure(g_parser);
  CharData_CheckXMLChars(&storage, expected);
}
END_TEST

/* Test handling of multiple unit UTF-16 characters */
START_TEST(test_multichar_cdata_utf16) {
  /* Test data is:
   *   <?xml version='1.0' encoding='utf-16'?>
   *   <a><![CDATA[{MINIM}{CROTCHET}]]></a>
   *
   * where {MINIM} is U+1d15e (a minim or half-note)
   *   UTF-16: 0xd834 0xdd5e
   *   UTF-8:  0xf0 0x9d 0x85 0x9e
   * and {CROTCHET} is U+1d15f (a crotchet or quarter-note)
   *   UTF-16: 0xd834 0xdd5f
   *   UTF-8:  0xf0 0x9d 0x85 0x9f
   */
  const char text[] = "\0<\0?\0x\0m\0l\0"
                      " \0v\0e\0r\0s\0i\0o\0n\0=\0'\0\x31\0.\0\x30\0'\0"
                      " \0e\0n\0c\0o\0d\0i\0n\0g\0=\0'\0u\0t\0f\0-\0"
                      "1\0"
                      "6\0'"
                      "\0?\0>\0\n"
                      "\0<\0a\0>\0<\0!\0[\0C\0D\0A\0T\0A\0["
                      "\xd8\x34\xdd\x5e\xd8\x34\xdd\x5f"
                      "\0]\0]\0>\0<\0/\0a\0>";
#ifdef XML_UNICODE
  const XML_Char *expected = XCS("\xd834\xdd5e\xd834\xdd5f");
#else
  const XML_Char *expected = XCS("\xf0\x9d\x85\x9e\xf0\x9d\x85\x9f");
#endif
  CharData storage;

  CharData_Init(&storage);
  XML_SetUserData(g_parser, &storage);
  XML_SetCharacterDataHandler(g_parser, accumulate_characters);

  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)sizeof(text) - 1, XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  CharData_CheckXMLChars(&storage, expected);
}
END_TEST

/* Test that an element name with a UTF-16 surrogate pair is rejected */
START_TEST(test_utf16_bad_surrogate_pair) {
  /* Test data is:
   *   <?xml version='1.0' encoding='utf-16'?>
   *   <a><![CDATA[{BADLINB}]]></a>
   *
   * where {BADLINB} is U+10000 (the first Linear B character)
   * with the UTF-16 surrogate pair in the wrong order, i.e.
   *   0xdc00 0xd800
   */
  const char text[] = "\0<\0?\0x\0m\0l\0"
                      " \0v\0e\0r\0s\0i\0o\0n\0=\0'\0\x31\0.\0\x30\0'\0"
                      " \0e\0n\0c\0o\0d\0i\0n\0g\0=\0'\0u\0t\0f\0-\0"
                      "1\0"
                      "6\0'"
                      "\0?\0>\0\n"
                      "\0<\0a\0>\0<\0!\0[\0C\0D\0A\0T\0A\0["
                      "\xdc\x00\xd8\x00"
                      "\0]\0]\0>\0<\0/\0a\0>";

  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)sizeof(text) - 1, XML_TRUE)
      != XML_STATUS_ERROR)
    fail("Reversed UTF-16 surrogate pair not faulted");
  if (XML_GetErrorCode(g_parser) != XML_ERROR_INVALID_TOKEN)
    xml_failure(g_parser);
}
END_TEST

START_TEST(test_bad_cdata) {
  struct CaseData {
    const char *text;
    enum XML_Error expectedError;
  };

  struct CaseData cases[]
      = {{"<a><", XML_ERROR_UNCLOSED_TOKEN},
         {"<a><!", XML_ERROR_UNCLOSED_TOKEN},
         {"<a><![", XML_ERROR_UNCLOSED_TOKEN},
         {"<a><![C", XML_ERROR_UNCLOSED_TOKEN},
         {"<a><![CD", XML_ERROR_UNCLOSED_TOKEN},
         {"<a><![CDA", XML_ERROR_UNCLOSED_TOKEN},
         {"<a><![CDAT", XML_ERROR_UNCLOSED_TOKEN},
         {"<a><![CDATA", XML_ERROR_UNCLOSED_TOKEN},

         {"<a><![CDATA[", XML_ERROR_UNCLOSED_CDATA_SECTION},
         {"<a><![CDATA[]", XML_ERROR_UNCLOSED_CDATA_SECTION},
         {"<a><![CDATA[]]", XML_ERROR_UNCLOSED_CDATA_SECTION},

         {"<a><!<a/>", XML_ERROR_INVALID_TOKEN},
         {"<a><![<a/>", XML_ERROR_UNCLOSED_TOKEN},  /* ?! */
         {"<a><![C<a/>", XML_ERROR_UNCLOSED_TOKEN}, /* ?! */
         {"<a><![CD<a/>", XML_ERROR_INVALID_TOKEN},
         {"<a><![CDA<a/>", XML_ERROR_INVALID_TOKEN},
         {"<a><![CDAT<a/>", XML_ERROR_INVALID_TOKEN},
         {"<a><![CDATA<a/>", XML_ERROR_INVALID_TOKEN},

         {"<a><![CDATA[<a/>", XML_ERROR_UNCLOSED_CDATA_SECTION},
         {"<a><![CDATA[]<a/>", XML_ERROR_UNCLOSED_CDATA_SECTION},
         {"<a><![CDATA[]]<a/>", XML_ERROR_UNCLOSED_CDATA_SECTION}};

  size_t i = 0;
  for (; i < sizeof(cases) / sizeof(struct CaseData); i++) {
    const enum XML_Status actualStatus = _XML_Parse_SINGLE_BYTES(
        g_parser, cases[i].text, (int)strlen(cases[i].text), XML_TRUE);
    const enum XML_Error actualError = XML_GetErrorCode(g_parser);

    assert(actualStatus == XML_STATUS_ERROR);

    if (actualError != cases[i].expectedError) {
      char message[100];
      snprintf(message, sizeof(message),
               "Expected error %d but got error %d for case %u: \"%s\"\n",
               cases[i].expectedError, actualError, (unsigned int)i + 1,
               cases[i].text);
      fail(message);
    }

    XML_ParserReset(g_parser, NULL);
  }
}
END_TEST

/* Test failures in UTF-16 CDATA */
START_TEST(test_bad_cdata_utf16) {
  struct CaseData {
    size_t text_bytes;
    const char *text;
    enum XML_Error expected_error;
  };

  const char prolog[] = "\0<\0?\0x\0m\0l\0"
                        " \0v\0e\0r\0s\0i\0o\0n\0=\0'\0\x31\0.\0\x30\0'\0"
                        " \0e\0n\0c\0o\0d\0i\0n\0g\0=\0'\0u\0t\0f\0-\0"
                        "1\0"
                        "6\0'"
                        "\0?\0>\0\n"
                        "\0<\0a\0>";
  struct CaseData cases[] = {
      {1, "\0", XML_ERROR_UNCLOSED_TOKEN},
      {2, "\0<", XML_ERROR_UNCLOSED_TOKEN},
      {3, "\0<\0", XML_ERROR_UNCLOSED_TOKEN},
      {4, "\0<\0!", XML_ERROR_UNCLOSED_TOKEN},
      {5, "\0<\0!\0", XML_ERROR_UNCLOSED_TOKEN},
      {6, "\0<\0!\0[", XML_ERROR_UNCLOSED_TOKEN},
      {7, "\0<\0!\0[\0", XML_ERROR_UNCLOSED_TOKEN},
      {8, "\0<\0!\0[\0C", XML_ERROR_UNCLOSED_TOKEN},
      {9, "\0<\0!\0[\0C\0", XML_ERROR_UNCLOSED_TOKEN},
      {10, "\0<\0!\0[\0C\0D", XML_ERROR_UNCLOSED_TOKEN},
      {11, "\0<\0!\0[\0C\0D\0", XML_ERROR_UNCLOSED_TOKEN},
      {12, "\0<\0!\0[\0C\0D\0A", XML_ERROR_UNCLOSED_TOKEN},
      {13, "\0<\0!\0[\0C\0D\0A\0", XML_ERROR_UNCLOSED_TOKEN},
      {14, "\0<\0!\0[\0C\0D\0A\0T", XML_ERROR_UNCLOSED_TOKEN},
      {15, "\0<\0!\0[\0C\0D\0A\0T\0", XML_ERROR_UNCLOSED_TOKEN},
      {16, "\0<\0!\0[\0C\0D\0A\0T\0A", XML_ERROR_UNCLOSED_TOKEN},
      {17, "\0<\0!\0[\0C\0D\0A\0T\0A\0", XML_ERROR_UNCLOSED_TOKEN},
      {18, "\0<\0!\0[\0C\0D\0A\0T\0A\0[", XML_ERROR_UNCLOSED_CDATA_SECTION},
      {19, "\0<\0!\0[\0C\0D\0A\0T\0A\0[\0", XML_ERROR_UNCLOSED_CDATA_SECTION},
      {20, "\0<\0!\0[\0C\0D\0A\0T\0A\0[\0Z", XML_ERROR_UNCLOSED_CDATA_SECTION},
      /* Now add a four-byte UTF-16 character */
      {21, "\0<\0!\0[\0C\0D\0A\0T\0A\0[\0Z\xd8",
       XML_ERROR_UNCLOSED_CDATA_SECTION},
      {22, "\0<\0!\0[\0C\0D\0A\0T\0A\0[\0Z\xd8\x34", XML_ERROR_PARTIAL_CHAR},
      {23, "\0<\0!\0[\0C\0D\0A\0T\0A\0[\0Z\xd8\x34\xdd",
       XML_ERROR_PARTIAL_CHAR},
      {24, "\0<\0!\0[\0C\0D\0A\0T\0A\0[\0Z\xd8\x34\xdd\x5e",
       XML_ERROR_UNCLOSED_CDATA_SECTION}};
  size_t i;

  for (i = 0; i < sizeof(cases) / sizeof(struct CaseData); i++) {
    enum XML_Status actual_status;
    enum XML_Error actual_error;

    if (_XML_Parse_SINGLE_BYTES(g_parser, prolog, (int)sizeof(prolog) - 1,
                                XML_FALSE)
        == XML_STATUS_ERROR)
      xml_failure(g_parser);
    actual_status = _XML_Parse_SINGLE_BYTES(g_parser, cases[i].text,
                                            (int)cases[i].text_bytes, XML_TRUE);
    assert(actual_status == XML_STATUS_ERROR);
    actual_error = XML_GetErrorCode(g_parser);
    if (actual_error != cases[i].expected_error) {
      char message[1024];

      snprintf(message, sizeof(message),
               "Expected error %d (%" XML_FMT_STR "), got %d (%" XML_FMT_STR
               ") for case %lu\n",
               cases[i].expected_error,
               XML_ErrorString(cases[i].expected_error), actual_error,
               XML_ErrorString(actual_error), (long unsigned)(i + 1));
      fail(message);
    }
    XML_ParserReset(g_parser, NULL);
  }
}
END_TEST

/* Test stopping the parser in cdata handler */
START_TEST(test_stop_parser_between_cdata_calls) {
  const char *text = long_cdata_text;

  XML_SetCharacterDataHandler(g_parser, clearing_aborting_character_handler);
  g_resumable = XML_FALSE;
  expect_failure(text, XML_ERROR_ABORTED, "Parse not aborted in CDATA handler");
}
END_TEST

/* Test suspending the parser in cdata handler */
START_TEST(test_suspend_parser_between_cdata_calls) {
  const char *text = long_cdata_text;
  enum XML_Status result;

  XML_SetCharacterDataHandler(g_parser, clearing_aborting_character_handler);
  g_resumable = XML_TRUE;
  result = _XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE);
  if (result != XML_STATUS_SUSPENDED) {
    if (result == XML_STATUS_ERROR)
      xml_failure(g_parser);
    fail("Parse not suspended in CDATA handler");
  }
  if (XML_GetErrorCode(g_parser) != XML_ERROR_NONE)
    xml_failure(g_parser);
}
END_TEST

/* Test memory allocation functions */
START_TEST(test_memory_allocation) {
  char *buffer = (char *)XML_MemMalloc(g_parser, 256);
  char *p;

  if (buffer == NULL) {
    fail("Allocation failed");
  } else {
    /* Try writing to memory; some OSes try to cheat! */
    buffer[0] = 'T';
    buffer[1] = 'E';
    buffer[2] = 'S';
    buffer[3] = 'T';
    buffer[4] = '\0';
    if (strcmp(buffer, "TEST") != 0) {
      fail("Memory not writable");
    } else {
      p = (char *)XML_MemRealloc(g_parser, buffer, 512);
      if (p == NULL) {
        fail("Reallocation failed");
      } else {
        /* Write again, just to be sure */
        buffer = p;
        buffer[0] = 'V';
        if (strcmp(buffer, "VEST") != 0) {
          fail("Reallocated memory not writable");
        }
      }
    }
    XML_MemFree(g_parser, buffer);
  }
}
END_TEST

/* Test XML_DefaultCurrent() passes handling on correctly */
START_TEST(test_default_current) {
  const char *text = "<doc>hell]</doc>";
  const char *entity_text = "<!DOCTYPE doc [\n"
                            "<!ENTITY entity '&#37;'>\n"
                            "]>\n"
                            "<doc>&entity;</doc>";
  CharData storage;

  XML_SetDefaultHandler(g_parser, record_default_handler);
  XML_SetCharacterDataHandler(g_parser, record_cdata_handler);
  CharData_Init(&storage);
  XML_SetUserData(g_parser, &storage);
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  CharData_CheckXMLChars(&storage, XCS("DCDCDCDCDCDD"));

  /* Again, without the defaulting */
  XML_ParserReset(g_parser, NULL);
  XML_SetDefaultHandler(g_parser, record_default_handler);
  XML_SetCharacterDataHandler(g_parser, record_cdata_nodefault_handler);
  CharData_Init(&storage);
  XML_SetUserData(g_parser, &storage);
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  CharData_CheckXMLChars(&storage, XCS("DcccccD"));

  /* Now with an internal entity to complicate matters */
  XML_ParserReset(g_parser, NULL);
  XML_SetDefaultHandler(g_parser, record_default_handler);
  XML_SetCharacterDataHandler(g_parser, record_cdata_handler);
  CharData_Init(&storage);
  XML_SetUserData(g_parser, &storage);
  if (_XML_Parse_SINGLE_BYTES(g_parser, entity_text, (int)strlen(entity_text),
                              XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  /* The default handler suppresses the entity */
  CharData_CheckXMLChars(&storage, XCS("DDDDDDDDDDDDDDDDDDD"));

  /* Again, with a skip handler */
  XML_ParserReset(g_parser, NULL);
  XML_SetDefaultHandler(g_parser, record_default_handler);
  XML_SetCharacterDataHandler(g_parser, record_cdata_handler);
  XML_SetSkippedEntityHandler(g_parser, record_skip_handler);
  CharData_Init(&storage);
  XML_SetUserData(g_parser, &storage);
  if (_XML_Parse_SINGLE_BYTES(g_parser, entity_text, (int)strlen(entity_text),
                              XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  /* The default handler suppresses the entity */
  CharData_CheckXMLChars(&storage, XCS("DDDDDDDDDDDDDDDDDeD"));

  /* This time, allow the entity through */
  XML_ParserReset(g_parser, NULL);
  XML_SetDefaultHandlerExpand(g_parser, record_default_handler);
  XML_SetCharacterDataHandler(g_parser, record_cdata_handler);
  CharData_Init(&storage);
  XML_SetUserData(g_parser, &storage);
  if (_XML_Parse_SINGLE_BYTES(g_parser, entity_text, (int)strlen(entity_text),
                              XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  CharData_CheckXMLChars(&storage, XCS("DDDDDDDDDDDDDDDDDCDD"));

  /* Finally, without passing the cdata to the default handler */
  XML_ParserReset(g_parser, NULL);
  XML_SetDefaultHandlerExpand(g_parser, record_default_handler);
  XML_SetCharacterDataHandler(g_parser, record_cdata_nodefault_handler);
  CharData_Init(&storage);
  XML_SetUserData(g_parser, &storage);
  if (_XML_Parse_SINGLE_BYTES(g_parser, entity_text, (int)strlen(entity_text),
                              XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  CharData_CheckXMLChars(&storage, XCS("DDDDDDDDDDDDDDDDDcD"));
}
END_TEST

/* Test DTD element parsing code paths */
START_TEST(test_dtd_elements) {
  const char *text = "<!DOCTYPE doc [\n"
                     "<!ELEMENT doc (chapter)>\n"
                     "<!ELEMENT chapter (#PCDATA)>\n"
                     "]>\n"
                     "<doc><chapter>Wombats are go</chapter></doc>";

  XML_SetElementDeclHandler(g_parser, dummy_element_decl_handler);
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
}
END_TEST

static void XMLCALL
element_decl_check_model(void *userData, const XML_Char *name,
                         XML_Content *model) {
  UNUSED_P(userData);
  uint32_t errorFlags = 0;

  /* Expected model array structure is this:
   * [0] (type 6, quant 0)
   *   [1] (type 5, quant 0)
   *     [3] (type 4, quant 0, name "bar")
   *     [4] (type 4, quant 0, name "foo")
   *     [5] (type 4, quant 3, name "xyz")
   *   [2] (type 4, quant 2, name "zebra")
   */
  errorFlags |= ((xcstrcmp(name, XCS("junk")) == 0) ? 0 : (1u << 0));
  errorFlags |= ((model != NULL) ? 0 : (1u << 1));

  errorFlags |= ((model[0].type == XML_CTYPE_SEQ) ? 0 : (1u << 2));
  errorFlags |= ((model[0].quant == XML_CQUANT_NONE) ? 0 : (1u << 3));
  errorFlags |= ((model[0].numchildren == 2) ? 0 : (1u << 4));
  errorFlags |= ((model[0].children == &model[1]) ? 0 : (1u << 5));
  errorFlags |= ((model[0].name == NULL) ? 0 : (1u << 6));

  errorFlags |= ((model[1].type == XML_CTYPE_CHOICE) ? 0 : (1u << 7));
  errorFlags |= ((model[1].quant == XML_CQUANT_NONE) ? 0 : (1u << 8));
  errorFlags |= ((model[1].numchildren == 3) ? 0 : (1u << 9));
  errorFlags |= ((model[1].children == &model[3]) ? 0 : (1u << 10));
  errorFlags |= ((model[1].name == NULL) ? 0 : (1u << 11));

  errorFlags |= ((model[2].type == XML_CTYPE_NAME) ? 0 : (1u << 12));
  errorFlags |= ((model[2].quant == XML_CQUANT_REP) ? 0 : (1u << 13));
  errorFlags |= ((model[2].numchildren == 0) ? 0 : (1u << 14));
  errorFlags |= ((model[2].children == NULL) ? 0 : (1u << 15));
  errorFlags |= ((xcstrcmp(model[2].name, XCS("zebra")) == 0) ? 0 : (1u << 16));

  errorFlags |= ((model[3].type == XML_CTYPE_NAME) ? 0 : (1u << 17));
  errorFlags |= ((model[3].quant == XML_CQUANT_NONE) ? 0 : (1u << 18));
  errorFlags |= ((model[3].numchildren == 0) ? 0 : (1u << 19));
  errorFlags |= ((model[3].children == NULL) ? 0 : (1u << 20));
  errorFlags |= ((xcstrcmp(model[3].name, XCS("bar")) == 0) ? 0 : (1u << 21));

  errorFlags |= ((model[4].type == XML_CTYPE_NAME) ? 0 : (1u << 22));
  errorFlags |= ((model[4].quant == XML_CQUANT_NONE) ? 0 : (1u << 23));
  errorFlags |= ((model[4].numchildren == 0) ? 0 : (1u << 24));
  errorFlags |= ((model[4].children == NULL) ? 0 : (1u << 25));
  errorFlags |= ((xcstrcmp(model[4].name, XCS("foo")) == 0) ? 0 : (1u << 26));

  errorFlags |= ((model[5].type == XML_CTYPE_NAME) ? 0 : (1u << 27));
  errorFlags |= ((model[5].quant == XML_CQUANT_PLUS) ? 0 : (1u << 28));
  errorFlags |= ((model[5].numchildren == 0) ? 0 : (1u << 29));
  errorFlags |= ((model[5].children == NULL) ? 0 : (1u << 30));
  errorFlags |= ((xcstrcmp(model[5].name, XCS("xyz")) == 0) ? 0 : (1u << 31));

  XML_SetUserData(g_parser, (void *)(uintptr_t)errorFlags);
  XML_FreeContentModel(g_parser, model);
}

START_TEST(test_dtd_elements_nesting) {
  // Payload inspired by a test in Perl's XML::Parser
  const char *text = "<!DOCTYPE foo [\n"
                     "<!ELEMENT junk ((bar|foo|xyz+), zebra*)>\n"
                     "]>\n"
                     "<foo/>";

  XML_SetUserData(g_parser, (void *)(uintptr_t)-1);

  XML_SetElementDeclHandler(g_parser, element_decl_check_model);
  if (XML_Parse(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);

  if ((uint32_t)(uintptr_t)XML_GetUserData(g_parser) != 0)
    fail("Element declaration model regression detected");
}
END_TEST

/* Test foreign DTD handling */
START_TEST(test_set_foreign_dtd) {
  const char *text1 = "<?xml version='1.0' encoding='us-ascii'?>\n";
  const char *text2 = "<doc>&entity;</doc>";
  ExtTest test_data = {"<!ELEMENT doc (#PCDATA)*>", NULL, NULL};

  /* Check hash salt is passed through too */
  XML_SetHashSalt(g_parser, 0x12345678);
  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetUserData(g_parser, &test_data);
  XML_SetExternalEntityRefHandler(g_parser, external_entity_loader);
  /* Add a default handler to exercise more code paths */
  XML_SetDefaultHandler(g_parser, dummy_default_handler);
  if (XML_UseForeignDTD(g_parser, XML_TRUE) != XML_ERROR_NONE)
    fail("Could not set foreign DTD");
  if (_XML_Parse_SINGLE_BYTES(g_parser, text1, (int)strlen(text1), XML_FALSE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);

  /* Ensure that trying to set the DTD after parsing has started
   * is faulted, even if it's the same setting.
   */
  if (XML_UseForeignDTD(g_parser, XML_TRUE)
      != XML_ERROR_CANT_CHANGE_FEATURE_ONCE_PARSING)
    fail("Failed to reject late foreign DTD setting");
  /* Ditto for the hash salt */
  if (XML_SetHashSalt(g_parser, 0x23456789))
    fail("Failed to reject late hash salt change");

  /* Now finish the parse */
  if (_XML_Parse_SINGLE_BYTES(g_parser, text2, (int)strlen(text2), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
}
END_TEST

/* Test foreign DTD handling with a failing NotStandalone handler */
START_TEST(test_foreign_dtd_not_standalone) {
  const char *text = "<?xml version='1.0' encoding='us-ascii'?>\n"
                     "<doc>&entity;</doc>";
  ExtTest test_data = {"<!ELEMENT doc (#PCDATA)*>", NULL, NULL};

  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetUserData(g_parser, &test_data);
  XML_SetExternalEntityRefHandler(g_parser, external_entity_loader);
  XML_SetNotStandaloneHandler(g_parser, reject_not_standalone_handler);
  if (XML_UseForeignDTD(g_parser, XML_TRUE) != XML_ERROR_NONE)
    fail("Could not set foreign DTD");
  expect_failure(text, XML_ERROR_NOT_STANDALONE,
                 "NotStandalonehandler failed to reject");
}
END_TEST

/* Test invalid character in a foreign DTD is faulted */
START_TEST(test_invalid_foreign_dtd) {
  const char *text = "<?xml version='1.0' encoding='us-ascii'?>\n"
                     "<doc>&entity;</doc>";
  ExtFaults test_data
      = {"$", "Dollar not faulted", NULL, XML_ERROR_INVALID_TOKEN};

  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetUserData(g_parser, &test_data);
  XML_SetExternalEntityRefHandler(g_parser, external_entity_faulter);
  XML_UseForeignDTD(g_parser, XML_TRUE);
  expect_failure(text, XML_ERROR_EXTERNAL_ENTITY_HANDLING,
                 "Bad DTD should not have been accepted");
}
END_TEST

/* Test foreign DTD use with a doctype */
START_TEST(test_foreign_dtd_with_doctype) {
  const char *text1 = "<?xml version='1.0' encoding='us-ascii'?>\n"
                      "<!DOCTYPE doc [<!ENTITY entity 'hello world'>]>\n";
  const char *text2 = "<doc>&entity;</doc>";
  ExtTest test_data = {"<!ELEMENT doc (#PCDATA)*>", NULL, NULL};

  /* Check hash salt is passed through too */
  XML_SetHashSalt(g_parser, 0x12345678);
  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetUserData(g_parser, &test_data);
  XML_SetExternalEntityRefHandler(g_parser, external_entity_loader);
  /* Add a default handler to exercise more code paths */
  XML_SetDefaultHandler(g_parser, dummy_default_handler);
  if (XML_UseForeignDTD(g_parser, XML_TRUE) != XML_ERROR_NONE)
    fail("Could not set foreign DTD");
  if (_XML_Parse_SINGLE_BYTES(g_parser, text1, (int)strlen(text1), XML_FALSE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);

  /* Ensure that trying to set the DTD after parsing has started
   * is faulted, even if it's the same setting.
   */
  if (XML_UseForeignDTD(g_parser, XML_TRUE)
      != XML_ERROR_CANT_CHANGE_FEATURE_ONCE_PARSING)
    fail("Failed to reject late foreign DTD setting");
  /* Ditto for the hash salt */
  if (XML_SetHashSalt(g_parser, 0x23456789))
    fail("Failed to reject late hash salt change");

  /* Now finish the parse */
  if (_XML_Parse_SINGLE_BYTES(g_parser, text2, (int)strlen(text2), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
}
END_TEST

/* Test XML_UseForeignDTD with no external subset present */
START_TEST(test_foreign_dtd_without_external_subset) {
  const char *text = "<!DOCTYPE doc [<!ENTITY foo 'bar'>]>\n"
                     "<doc>&foo;</doc>";

  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetUserData(g_parser, NULL);
  XML_SetExternalEntityRefHandler(g_parser, external_entity_null_loader);
  XML_UseForeignDTD(g_parser, XML_TRUE);
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
}
END_TEST

START_TEST(test_empty_foreign_dtd) {
  const char *text = "<?xml version='1.0' encoding='us-ascii'?>\n"
                     "<doc>&entity;</doc>";

  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetExternalEntityRefHandler(g_parser, external_entity_null_loader);
  XML_UseForeignDTD(g_parser, XML_TRUE);
  expect_failure(text, XML_ERROR_UNDEFINED_ENTITY,
                 "Undefined entity not faulted");
}
END_TEST

/* Test XML Base is set and unset appropriately */
START_TEST(test_set_base) {
  const XML_Char *old_base;
  const XML_Char *new_base = XCS("/local/file/name.xml");

  old_base = XML_GetBase(g_parser);
  if (XML_SetBase(g_parser, new_base) != XML_STATUS_OK)
    fail("Unable to set base");
  if (xcstrcmp(XML_GetBase(g_parser), new_base) != 0)
    fail("Base setting not correct");
  if (XML_SetBase(g_parser, NULL) != XML_STATUS_OK)
    fail("Unable to NULL base");
  if (XML_GetBase(g_parser) != NULL)
    fail("Base setting not nulled");
  XML_SetBase(g_parser, old_base);
}
END_TEST

/* Test attribute counts, indexing, etc */
START_TEST(test_attributes) {
  const char *text = "<!DOCTYPE doc [\n"
                     "<!ELEMENT doc (tag)>\n"
                     "<!ATTLIST doc id ID #REQUIRED>\n"
                     "]>"
                     "<doc a='1' id='one' b='2'>"
                     "<tag c='3'/>"
                     "</doc>";
  AttrInfo doc_info[] = {{XCS("a"), XCS("1")},
                         {XCS("b"), XCS("2")},
                         {XCS("id"), XCS("one")},
                         {NULL, NULL}};
  AttrInfo tag_info[] = {{XCS("c"), XCS("3")}, {NULL, NULL}};
  ElementInfo info[] = {{XCS("doc"), 3, XCS("id"), NULL},
                        {XCS("tag"), 1, NULL, NULL},
                        {NULL, 0, NULL, NULL}};
  info[0].attributes = doc_info;
  info[1].attributes = tag_info;

  XML_SetStartElementHandler(g_parser, counting_start_element_handler);
  XML_SetUserData(g_parser, info);
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
}
END_TEST

/* Test reset works correctly in the middle of processing an internal
 * entity.  Exercises some obscure code in XML_ParserReset().
 */
START_TEST(test_reset_in_entity) {
  const char *text = "<!DOCTYPE doc [\n"
                     "<!ENTITY wombat 'wom'>\n"
                     "<!ENTITY entity 'hi &wom; there'>\n"
                     "]>\n"
                     "<doc>&entity;</doc>";
  XML_ParsingStatus status;

  g_resumable = XML_TRUE;
  XML_SetCharacterDataHandler(g_parser, clearing_aborting_character_handler);
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_FALSE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  XML_GetParsingStatus(g_parser, &status);
  if (status.parsing != XML_SUSPENDED)
    fail("Parsing status not SUSPENDED");
  XML_ParserReset(g_parser, NULL);
  XML_GetParsingStatus(g_parser, &status);
  if (status.parsing != XML_INITIALIZED)
    fail("Parsing status doesn't reset to INITIALIZED");
}
END_TEST

/* Test that resume correctly passes through parse errors */
START_TEST(test_resume_invalid_parse) {
  const char *text = "<doc>Hello</doc"; /* Missing closing wedge */

  g_resumable = XML_TRUE;
  XML_SetCharacterDataHandler(g_parser, clearing_aborting_character_handler);
  if (XML_Parse(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  if (XML_ResumeParser(g_parser) == XML_STATUS_OK)
    fail("Resumed invalid parse not faulted");
  if (XML_GetErrorCode(g_parser) != XML_ERROR_UNCLOSED_TOKEN)
    fail("Invalid parse not correctly faulted");
}
END_TEST

/* Test that re-suspended parses are correctly passed through */
START_TEST(test_resume_resuspended) {
  const char *text = "<doc>Hello<meep/>world</doc>";

  g_resumable = XML_TRUE;
  XML_SetCharacterDataHandler(g_parser, clearing_aborting_character_handler);
  if (XML_Parse(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  g_resumable = XML_TRUE;
  XML_SetCharacterDataHandler(g_parser, clearing_aborting_character_handler);
  if (XML_ResumeParser(g_parser) != XML_STATUS_SUSPENDED)
    fail("Resumption not suspended");
  /* This one should succeed and finish up */
  if (XML_ResumeParser(g_parser) != XML_STATUS_OK)
    xml_failure(g_parser);
}
END_TEST

/* Test that CDATA shows up correctly through a default handler */
START_TEST(test_cdata_default) {
  const char *text = "<doc><![CDATA[Hello\nworld]]></doc>";
  const XML_Char *expected = XCS("<doc><![CDATA[Hello\nworld]]></doc>");
  CharData storage;

  CharData_Init(&storage);
  XML_SetUserData(g_parser, &storage);
  XML_SetDefaultHandler(g_parser, accumulate_characters);

  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  CharData_CheckXMLChars(&storage, expected);
}
END_TEST

/* Test resetting a subordinate parser does exactly nothing */
START_TEST(test_subordinate_reset) {
  const char *text = "<?xml version='1.0' encoding='us-ascii'?>\n"
                     "<!DOCTYPE doc SYSTEM 'foo'>\n"
                     "<doc>&entity;</doc>";

  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetExternalEntityRefHandler(g_parser, external_entity_resetter);
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
}
END_TEST

/* Test suspending a subordinate parser */
START_TEST(test_subordinate_suspend) {
  const char *text = "<?xml version='1.0' encoding='us-ascii'?>\n"
                     "<!DOCTYPE doc SYSTEM 'foo'>\n"
                     "<doc>&entity;</doc>";

  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetExternalEntityRefHandler(g_parser, external_entity_suspender);
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
}
END_TEST

/* Test suspending a subordinate parser from an XML declaration */
/* Increases code coverage of the tests */

START_TEST(test_subordinate_xdecl_suspend) {
  const char *text
      = "<!DOCTYPE doc [\n"
        "  <!ENTITY entity SYSTEM 'http://example.org/dummy.ent'>\n"
        "]>\n"
        "<doc>&entity;</doc>";

  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetExternalEntityRefHandler(g_parser, external_entity_suspend_xmldecl);
  g_resumable = XML_TRUE;
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
}
END_TEST

START_TEST(test_subordinate_xdecl_abort) {
  const char *text
      = "<!DOCTYPE doc [\n"
        "  <!ENTITY entity SYSTEM 'http://example.org/dummy.ent'>\n"
        "]>\n"
        "<doc>&entity;</doc>";

  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetExternalEntityRefHandler(g_parser, external_entity_suspend_xmldecl);
  g_resumable = XML_FALSE;
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
}
END_TEST

/* Test external entity fault handling with suspension */
START_TEST(test_ext_entity_invalid_suspended_parse) {
  const char *text = "<!DOCTYPE doc [\n"
                     "  <!ENTITY en SYSTEM 'http://example.org/dummy.ent'>\n"
                     "]>\n"
                     "<doc>&en;</doc>";
  ExtFaults faults[]
      = {{"<?xml version='1.0' encoding='us-ascii'?><",
          "Incomplete element declaration not faulted", NULL,
          XML_ERROR_UNCLOSED_TOKEN},
         {/* First two bytes of a three-byte char */
          "<?xml version='1.0' encoding='utf-8'?>\xe2\x82",
          "Incomplete character not faulted", NULL, XML_ERROR_PARTIAL_CHAR},
         {NULL, NULL, NULL, XML_ERROR_NONE}};
  ExtFaults *fault;

  for (fault = &faults[0]; fault->parse_text != NULL; fault++) {
    XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
    XML_SetExternalEntityRefHandler(g_parser,
                                    external_entity_suspending_faulter);
    XML_SetUserData(g_parser, fault);
    expect_failure(text, XML_ERROR_EXTERNAL_ENTITY_HANDLING,
                   "Parser did not report external entity error");
    XML_ParserReset(g_parser, NULL);
  }
}
END_TEST

/* Test setting an explicit encoding */
START_TEST(test_explicit_encoding) {
  const char *text1 = "<doc>Hello ";
  const char *text2 = " World</doc>";

  /* Just check that we can set the encoding to NULL before starting */
  if (XML_SetEncoding(g_parser, NULL) != XML_STATUS_OK)
    fail("Failed to initialise encoding to NULL");
  /* Say we are UTF-8 */
  if (XML_SetEncoding(g_parser, XCS("utf-8")) != XML_STATUS_OK)
    fail("Failed to set explicit encoding");
  if (_XML_Parse_SINGLE_BYTES(g_parser, text1, (int)strlen(text1), XML_FALSE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  /* Try to switch encodings mid-parse */
  if (XML_SetEncoding(g_parser, XCS("us-ascii")) != XML_STATUS_ERROR)
    fail("Allowed encoding change");
  if (_XML_Parse_SINGLE_BYTES(g_parser, text2, (int)strlen(text2), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  /* Try now the parse is over */
  if (XML_SetEncoding(g_parser, NULL) != XML_STATUS_OK)
    fail("Failed to unset encoding");
}
END_TEST

/* Test handling of trailing CR (rather than newline) */
START_TEST(test_trailing_cr) {
  const char *text = "<doc>\r";
  int found_cr;

  /* Try with a character handler, for code coverage */
  XML_SetCharacterDataHandler(g_parser, cr_cdata_handler);
  XML_SetUserData(g_parser, &found_cr);
  found_cr = 0;
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_OK)
    fail("Failed to fault unclosed doc");
  if (found_cr == 0)
    fail("Did not catch the carriage return");
  XML_ParserReset(g_parser, NULL);

  /* Now with a default handler instead */
  XML_SetDefaultHandler(g_parser, cr_cdata_handler);
  XML_SetUserData(g_parser, &found_cr);
  found_cr = 0;
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_OK)
    fail("Failed to fault unclosed doc");
  if (found_cr == 0)
    fail("Did not catch default carriage return");
}
END_TEST

/* Test trailing CR in an external entity parse */
START_TEST(test_ext_entity_trailing_cr) {
  const char *text = "<!DOCTYPE doc [\n"
                     "  <!ENTITY en SYSTEM 'http://example.org/dummy.ent'>\n"
                     "]>\n"
                     "<doc>&en;</doc>";
  int found_cr;

  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetExternalEntityRefHandler(g_parser, external_entity_cr_catcher);
  XML_SetUserData(g_parser, &found_cr);
  found_cr = 0;
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      != XML_STATUS_OK)
    xml_failure(g_parser);
  if (found_cr == 0)
    fail("No carriage return found");
  XML_ParserReset(g_parser, NULL);

  /* Try again with a different trailing CR */
  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetExternalEntityRefHandler(g_parser, external_entity_bad_cr_catcher);
  XML_SetUserData(g_parser, &found_cr);
  found_cr = 0;
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      != XML_STATUS_OK)
    xml_failure(g_parser);
  if (found_cr == 0)
    fail("No carriage return found");
}
END_TEST

/* Test handling of trailing square bracket */
START_TEST(test_trailing_rsqb) {
  const char *text8 = "<doc>]";
  const char text16[] = "\xFF\xFE<\000d\000o\000c\000>\000]\000";
  int found_rsqb;
  int text8_len = (int)strlen(text8);

  XML_SetCharacterDataHandler(g_parser, rsqb_handler);
  XML_SetUserData(g_parser, &found_rsqb);
  found_rsqb = 0;
  if (_XML_Parse_SINGLE_BYTES(g_parser, text8, text8_len, XML_TRUE)
      == XML_STATUS_OK)
    fail("Failed to fault unclosed doc");
  if (found_rsqb == 0)
    fail("Did not catch the right square bracket");

  /* Try again with a different encoding */
  XML_ParserReset(g_parser, NULL);
  XML_SetCharacterDataHandler(g_parser, rsqb_handler);
  XML_SetUserData(g_parser, &found_rsqb);
  found_rsqb = 0;
  if (_XML_Parse_SINGLE_BYTES(g_parser, text16, (int)sizeof(text16) - 1,
                              XML_TRUE)
      == XML_STATUS_OK)
    fail("Failed to fault unclosed doc");
  if (found_rsqb == 0)
    fail("Did not catch the right square bracket");

  /* And finally with a default handler */
  XML_ParserReset(g_parser, NULL);
  XML_SetDefaultHandler(g_parser, rsqb_handler);
  XML_SetUserData(g_parser, &found_rsqb);
  found_rsqb = 0;
  if (_XML_Parse_SINGLE_BYTES(g_parser, text16, (int)sizeof(text16) - 1,
                              XML_TRUE)
      == XML_STATUS_OK)
    fail("Failed to fault unclosed doc");
  if (found_rsqb == 0)
    fail("Did not catch the right square bracket");
}
END_TEST

/* Test trailing right square bracket in an external entity parse */
START_TEST(test_ext_entity_trailing_rsqb) {
  const char *text = "<!DOCTYPE doc [\n"
                     "  <!ENTITY en SYSTEM 'http://example.org/dummy.ent'>\n"
                     "]>\n"
                     "<doc>&en;</doc>";
  int found_rsqb;

  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetExternalEntityRefHandler(g_parser, external_entity_rsqb_catcher);
  XML_SetUserData(g_parser, &found_rsqb);
  found_rsqb = 0;
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      != XML_STATUS_OK)
    xml_failure(g_parser);
  if (found_rsqb == 0)
    fail("No right square bracket found");
}
END_TEST

/* Test CDATA handling in an external entity */
START_TEST(test_ext_entity_good_cdata) {
  const char *text = "<!DOCTYPE doc [\n"
                     "  <!ENTITY en SYSTEM 'http://example.org/dummy.ent'>\n"
                     "]>\n"
                     "<doc>&en;</doc>";

  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetExternalEntityRefHandler(g_parser, external_entity_good_cdata_ascii);
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      != XML_STATUS_OK)
    xml_failure(g_parser);
}
END_TEST

/* Test user parameter settings */
START_TEST(test_user_parameters) {
  const char *text = "<?xml version='1.0' encoding='us-ascii'?>\n"
                     "<!-- Primary parse -->\n"
                     "<!DOCTYPE doc SYSTEM 'foo'>\n"
                     "<doc>&entity;";
  const char *epilog = "<!-- Back to primary parser -->\n"
                       "</doc>";

  g_comment_count = 0;
  g_skip_count = 0;
  g_xdecl_count = 0;
  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetXmlDeclHandler(g_parser, xml_decl_handler);
  XML_SetExternalEntityRefHandler(g_parser, external_entity_param_checker);
  XML_SetCommentHandler(g_parser, data_check_comment_handler);
  XML_SetSkippedEntityHandler(g_parser, param_check_skip_handler);
  XML_UseParserAsHandlerArg(g_parser);
  XML_SetUserData(g_parser, (void *)1);
  g_handler_data = g_parser;
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_FALSE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  if (g_comment_count != 2)
    fail("Comment handler not invoked enough times");
  /* Ensure we can't change policy mid-parse */
  if (XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_NEVER))
    fail("Changed param entity parsing policy while parsing");
  if (_XML_Parse_SINGLE_BYTES(g_parser, epilog, (int)strlen(epilog), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  if (g_comment_count != 3)
    fail("Comment handler not invoked enough times");
  if (g_skip_count != 1)
    fail("Skip handler not invoked enough times");
  if (g_xdecl_count != 1)
    fail("XML declaration handler not invoked");
}
END_TEST

/* Test that an explicit external entity handler argument replaces
 * the parser as the first argument.
 *
 * We do not call the first parameter to the external entity handler
 * 'parser' for once, since the first time the handler is called it
 * will actually be a text string.  We need to be able to access the
 * global 'parser' variable to create our external entity parser from,
 * since there are code paths we need to ensure get executed.
 */
START_TEST(test_ext_entity_ref_parameter) {
  const char *text = "<?xml version='1.0' encoding='us-ascii'?>\n"
                     "<!DOCTYPE doc SYSTEM 'foo'>\n"
                     "<doc>&entity;</doc>";

  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetExternalEntityRefHandler(g_parser, external_entity_ref_param_checker);
  /* Set a handler arg that is not NULL and not parser (which is
   * what NULL would cause to be passed.
   */
  XML_SetExternalEntityRefHandlerArg(g_parser, (void *)text);
  g_handler_data = (void *)text;
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);

  /* Now try again with unset args */
  XML_ParserReset(g_parser, NULL);
  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetExternalEntityRefHandler(g_parser, external_entity_ref_param_checker);
  XML_SetExternalEntityRefHandlerArg(g_parser, NULL);
  g_handler_data = (void *)g_parser;
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
}
END_TEST

/* Test the parsing of an empty string */
START_TEST(test_empty_parse) {
  const char *text = "<doc></doc>";
  const char *partial = "<doc>";

  if (XML_Parse(g_parser, NULL, 0, XML_FALSE) == XML_STATUS_ERROR)
    fail("Parsing empty string faulted");
  if (XML_Parse(g_parser, NULL, 0, XML_TRUE) != XML_STATUS_ERROR)
    fail("Parsing final empty string not faulted");
  if (XML_GetErrorCode(g_parser) != XML_ERROR_NO_ELEMENTS)
    fail("Parsing final empty string faulted for wrong reason");

  /* Now try with valid text before the empty end */
  XML_ParserReset(g_parser, NULL);
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_FALSE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  if (XML_Parse(g_parser, NULL, 0, XML_TRUE) == XML_STATUS_ERROR)
    fail("Parsing final empty string faulted");

  /* Now try with invalid text before the empty end */
  XML_ParserReset(g_parser, NULL);
  if (_XML_Parse_SINGLE_BYTES(g_parser, partial, (int)strlen(partial),
                              XML_FALSE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  if (XML_Parse(g_parser, NULL, 0, XML_TRUE) != XML_STATUS_ERROR)
    fail("Parsing final incomplete empty string not faulted");
}
END_TEST

/* Test odd corners of the XML_GetBuffer interface */
static enum XML_Status
get_feature(enum XML_FeatureEnum feature_id, long *presult) {
  const XML_Feature *feature = XML_GetFeatureList();

  if (feature == NULL)
    return XML_STATUS_ERROR;
  for (; feature->feature != XML_FEATURE_END; feature++) {
    if (feature->feature == feature_id) {
      *presult = feature->value;
      return XML_STATUS_OK;
    }
  }
  return XML_STATUS_ERROR;
}

/* Test odd corners of the XML_GetBuffer interface */
START_TEST(test_get_buffer_1) {
  const char *text = get_buffer_test_text;
  void *buffer;
  long context_bytes;

  /* Attempt to allocate a negative length buffer */
  if (XML_GetBuffer(g_parser, -12) != NULL)
    fail("Negative length buffer not failed");

  /* Now get a small buffer and extend it past valid length */
  buffer = XML_GetBuffer(g_parser, 1536);
  if (buffer == NULL)
    fail("1.5K buffer failed");
  assert(buffer != NULL);
  memcpy(buffer, text, strlen(text));
  if (XML_ParseBuffer(g_parser, (int)strlen(text), XML_FALSE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  if (XML_GetBuffer(g_parser, INT_MAX) != NULL)
    fail("INT_MAX buffer not failed");

  /* Now try extending it a more reasonable but still too large
   * amount.  The allocator in XML_GetBuffer() doubles the buffer
   * size until it exceeds the requested amount or INT_MAX.  If it
   * exceeds INT_MAX, it rejects the request, so we want a request
   * between INT_MAX and INT_MAX/2.  A gap of 1K seems comfortable,
   * with an extra byte just to ensure that the request is off any
   * boundary.  The request will be inflated internally by
   * XML_CONTEXT_BYTES (if defined), so we subtract that from our
   * request.
   */
  if (get_feature(XML_FEATURE_CONTEXT_BYTES, &context_bytes) != XML_STATUS_OK)
    context_bytes = 0;
  if (XML_GetBuffer(g_parser, INT_MAX - (context_bytes + 1025)) != NULL)
    fail("INT_MAX- buffer not failed");

  /* Now try extending it a carefully crafted amount */
  if (XML_GetBuffer(g_parser, 1000) == NULL)
    fail("1000 buffer failed");
}
END_TEST

/* Test more corners of the XML_GetBuffer interface */
START_TEST(test_get_buffer_2) {
  const char *text = get_buffer_test_text;
  void *buffer;

  /* Now get a decent buffer */
  buffer = XML_GetBuffer(g_parser, 1536);
  if (buffer == NULL)
    fail("1.5K buffer failed");
  assert(buffer != NULL);
  memcpy(buffer, text, strlen(text));
  if (XML_ParseBuffer(g_parser, (int)strlen(text), XML_FALSE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);

  /* Extend it, to catch a different code path */
  if (XML_GetBuffer(g_parser, 1024) == NULL)
    fail("1024 buffer failed");
}
END_TEST

/* Test for signed integer overflow CVE-2022-23852 */
#if defined(XML_CONTEXT_BYTES)
START_TEST(test_get_buffer_3_overflow) {
  XML_Parser parser = XML_ParserCreate(NULL);
  assert(parser != NULL);

  const char *const text = "\n";
  const int expectedKeepValue = (int)strlen(text);

  // After this call, variable "keep" in XML_GetBuffer will
  // have value expectedKeepValue
  if (XML_Parse(parser, text, (int)strlen(text), XML_FALSE /* isFinal */)
      == XML_STATUS_ERROR)
    xml_failure(parser);

  assert(expectedKeepValue > 0);
  if (XML_GetBuffer(parser, INT_MAX - expectedKeepValue + 1) != NULL)
    fail("enlarging buffer not failed");

  XML_ParserFree(parser);
}
END_TEST
#endif // defined(XML_CONTEXT_BYTES)

/* Test position information macros */
START_TEST(test_byte_info_at_end) {
  const char *text = "<doc></doc>";

  if (XML_GetCurrentByteIndex(g_parser) != -1
      || XML_GetCurrentByteCount(g_parser) != 0)
    fail("Byte index/count incorrect at start of parse");
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  /* At end, the count will be zero and the index the end of string */
  if (XML_GetCurrentByteCount(g_parser) != 0)
    fail("Terminal byte count incorrect");
  if (XML_GetCurrentByteIndex(g_parser) != (XML_Index)strlen(text))
    fail("Terminal byte index incorrect");
}
END_TEST

/* Test position information from errors */
#define PRE_ERROR_STR "<doc></"
#define POST_ERROR_STR "wombat></doc>"
START_TEST(test_byte_info_at_error) {
  const char *text = PRE_ERROR_STR POST_ERROR_STR;

  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_OK)
    fail("Syntax error not faulted");
  if (XML_GetCurrentByteCount(g_parser) != 0)
    fail("Error byte count incorrect");
  if (XML_GetCurrentByteIndex(g_parser) != strlen(PRE_ERROR_STR))
    fail("Error byte index incorrect");
}
END_TEST
#undef PRE_ERROR_STR
#undef POST_ERROR_STR

/* Test position information in handler */
#define START_ELEMENT "<e>"
#define CDATA_TEXT "Hello"
#define END_ELEMENT "</e>"
START_TEST(test_byte_info_at_cdata) {
  const char *text = START_ELEMENT CDATA_TEXT END_ELEMENT;
  int offset, size;
  ByteTestData data;

  /* Check initial context is empty */
  if (XML_GetInputContext(g_parser, &offset, &size) != NULL)
    fail("Unexpected context at start of parse");

  data.start_element_len = (int)strlen(START_ELEMENT);
  data.cdata_len = (int)strlen(CDATA_TEXT);
  data.total_string_len = (int)strlen(text);
  XML_SetCharacterDataHandler(g_parser, byte_character_handler);
  XML_SetUserData(g_parser, &data);
  if (XML_Parse(g_parser, text, (int)strlen(text), XML_TRUE) != XML_STATUS_OK)
    xml_failure(g_parser);
}
END_TEST
#undef START_ELEMENT
#undef CDATA_TEXT
#undef END_ELEMENT

/* Test predefined entities are correctly recognised */
START_TEST(test_predefined_entities) {
  const char *text = "<doc>&lt;&gt;&amp;&quot;&apos;</doc>";
  const XML_Char *expected = XCS("<doc>&lt;&gt;&amp;&quot;&apos;</doc>");
  const XML_Char *result = XCS("<>&\"'");
  CharData storage;

  XML_SetDefaultHandler(g_parser, accumulate_characters);
  /* run_character_check uses XML_SetCharacterDataHandler(), which
   * unfortunately heads off a code path that we need to exercise.
   */
  CharData_Init(&storage);
  XML_SetUserData(g_parser, &storage);
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  /* The default handler doesn't translate the entities */
  CharData_CheckXMLChars(&storage, expected);

  /* Now try again and check the translation */
  XML_ParserReset(g_parser, NULL);
  run_character_check(text, result);
}
END_TEST

/* Regression test that an invalid tag in an external parameter
 * reference in an external DTD is correctly faulted.
 *
 * Only a few specific tags are legal in DTDs ignoring comments and
 * processing instructions, all of which begin with an exclamation
 * mark.  "<el/>" is not one of them, so the parser should raise an
 * error on encountering it.
 */
START_TEST(test_invalid_tag_in_dtd) {
  const char *text = "<!DOCTYPE doc SYSTEM '004-1.ent'>\n"
                     "<doc></doc>\n";

  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetExternalEntityRefHandler(g_parser, external_entity_param);
  expect_failure(text, XML_ERROR_EXTERNAL_ENTITY_HANDLING,
                 "Invalid tag IN DTD external param not rejected");
}
END_TEST

/* Test entities not quite the predefined ones are not mis-recognised */
START_TEST(test_not_predefined_entities) {
  const char *text[] = {"<doc>&pt;</doc>", "<doc>&amo;</doc>",
                        "<doc>&quid;</doc>", "<doc>&apod;</doc>", NULL};
  int i = 0;

  while (text[i] != NULL) {
    expect_failure(text[i], XML_ERROR_UNDEFINED_ENTITY,
                   "Undefined entity not rejected");
    XML_ParserReset(g_parser, NULL);
    i++;
  }
}
END_TEST

/* Test conditional inclusion (IGNORE) */
START_TEST(test_ignore_section) {
  const char *text = "<!DOCTYPE doc SYSTEM 'foo'>\n"
                     "<doc><e>&entity;</e></doc>";
  const XML_Char *expected
      = XCS("<![IGNORE[<!ELEMENT e (#PCDATA)*>]]>\n&entity;");
  CharData storage;

  CharData_Init(&storage);
  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetUserData(g_parser, &storage);
  XML_SetExternalEntityRefHandler(g_parser, external_entity_load_ignore);
  XML_SetDefaultHandler(g_parser, accumulate_characters);
  XML_SetStartDoctypeDeclHandler(g_parser, dummy_start_doctype_handler);
  XML_SetEndDoctypeDeclHandler(g_parser, dummy_end_doctype_handler);
  XML_SetElementDeclHandler(g_parser, dummy_element_decl_handler);
  XML_SetStartElementHandler(g_parser, dummy_start_element);
  XML_SetEndElementHandler(g_parser, dummy_end_element);
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  CharData_CheckXMLChars(&storage, expected);
}
END_TEST

START_TEST(test_ignore_section_utf16) {
  const char text[] =
      /* <!DOCTYPE d SYSTEM 's'> */
      "<\0!\0D\0O\0C\0T\0Y\0P\0E\0 \0d\0 "
      "\0S\0Y\0S\0T\0E\0M\0 \0'\0s\0'\0>\0\n\0"
      /* <d><e>&en;</e></d> */
      "<\0d\0>\0<\0e\0>\0&\0e\0n\0;\0<\0/\0e\0>\0<\0/\0d\0>\0";
  const XML_Char *expected = XCS("<![IGNORE[<!ELEMENT e (#PCDATA)*>]]>\n&en;");
  CharData storage;

  CharData_Init(&storage);
  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetUserData(g_parser, &storage);
  XML_SetExternalEntityRefHandler(g_parser, external_entity_load_ignore_utf16);
  XML_SetDefaultHandler(g_parser, accumulate_characters);
  XML_SetStartDoctypeDeclHandler(g_parser, dummy_start_doctype_handler);
  XML_SetEndDoctypeDeclHandler(g_parser, dummy_end_doctype_handler);
  XML_SetElementDeclHandler(g_parser, dummy_element_decl_handler);
  XML_SetStartElementHandler(g_parser, dummy_start_element);
  XML_SetEndElementHandler(g_parser, dummy_end_element);
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)sizeof(text) - 1, XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  CharData_CheckXMLChars(&storage, expected);
}
END_TEST

START_TEST(test_ignore_section_utf16_be) {
  const char text[] =
      /* <!DOCTYPE d SYSTEM 's'> */
      "\0<\0!\0D\0O\0C\0T\0Y\0P\0E\0 \0d\0 "
      "\0S\0Y\0S\0T\0E\0M\0 \0'\0s\0'\0>\0\n"
      /* <d><e>&en;</e></d> */
      "\0<\0d\0>\0<\0e\0>\0&\0e\0n\0;\0<\0/\0e\0>\0<\0/\0d\0>";
  const XML_Char *expected = XCS("<![IGNORE[<!ELEMENT e (#PCDATA)*>]]>\n&en;");
  CharData storage;

  CharData_Init(&storage);
  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetUserData(g_parser, &storage);
  XML_SetExternalEntityRefHandler(g_parser,
                                  external_entity_load_ignore_utf16_be);
  XML_SetDefaultHandler(g_parser, accumulate_characters);
  XML_SetStartDoctypeDeclHandler(g_parser, dummy_start_doctype_handler);
  XML_SetEndDoctypeDeclHandler(g_parser, dummy_end_doctype_handler);
  XML_SetElementDeclHandler(g_parser, dummy_element_decl_handler);
  XML_SetStartElementHandler(g_parser, dummy_start_element);
  XML_SetEndElementHandler(g_parser, dummy_end_element);
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)sizeof(text) - 1, XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  CharData_CheckXMLChars(&storage, expected);
}
END_TEST

/* Test mis-formatted conditional exclusion */
START_TEST(test_bad_ignore_section) {
  const char *text = "<!DOCTYPE doc SYSTEM 'foo'>\n"
                     "<doc><e>&entity;</e></doc>";
  ExtFaults faults[]
      = {{"<![IGNORE[<!ELEM", "Broken-off declaration not faulted", NULL,
          XML_ERROR_SYNTAX},
         {"<![IGNORE[\x01]]>", "Invalid XML character not faulted", NULL,
          XML_ERROR_INVALID_TOKEN},
         {/* FIrst two bytes of a three-byte char */
          "<![IGNORE[\xe2\x82", "Partial XML character not faulted", NULL,
          XML_ERROR_PARTIAL_CHAR},
         {NULL, NULL, NULL, XML_ERROR_NONE}};
  ExtFaults *fault;

  for (fault = &faults[0]; fault->parse_text != NULL; fault++) {
    XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
    XML_SetExternalEntityRefHandler(g_parser, external_entity_faulter);
    XML_SetUserData(g_parser, fault);
    expect_failure(text, XML_ERROR_EXTERNAL_ENTITY_HANDLING,
                   "Incomplete IGNORE section not failed");
    XML_ParserReset(g_parser, NULL);
  }
}
END_TEST

/* Test recursive parsing */
START_TEST(test_external_entity_values) {
  const char *text = "<!DOCTYPE doc SYSTEM '004-1.ent'>\n"
                     "<doc></doc>\n";
  ExtFaults data_004_2[] = {
      {"<!ATTLIST doc a1 CDATA 'value'>", NULL, NULL, XML_ERROR_NONE},
      {"<!ATTLIST $doc a1 CDATA 'value'>", "Invalid token not faulted", NULL,
       XML_ERROR_INVALID_TOKEN},
      {"'wombat", "Unterminated string not faulted", NULL,
       XML_ERROR_UNCLOSED_TOKEN},
      {"\xe2\x82", "Partial UTF-8 character not faulted", NULL,
       XML_ERROR_PARTIAL_CHAR},
      {"<?xml version='1.0' encoding='utf-8'?>\n", NULL, NULL, XML_ERROR_NONE},
      {"<?xml?>", "Malformed XML declaration not faulted", NULL,
       XML_ERROR_XML_DECL},
      {/* UTF-8 BOM */
       "\xEF\xBB\xBF<!ATTLIST doc a1 CDATA 'value'>", NULL, NULL,
       XML_ERROR_NONE},
      {"<?xml version='1.0' encoding='utf-8'?>\n$",
       "Invalid token after text declaration not faulted", NULL,
       XML_ERROR_INVALID_TOKEN},
      {"<?xml version='1.0' encoding='utf-8'?>\n'wombat",
       "Unterminated string after text decl not faulted", NULL,
       XML_ERROR_UNCLOSED_TOKEN},
      {"<?xml version='1.0' encoding='utf-8'?>\n\xe2\x82",
       "Partial UTF-8 character after text decl not faulted", NULL,
       XML_ERROR_PARTIAL_CHAR},
      {"%e1;", "Recursive parameter entity not faulted", NULL,
       XML_ERROR_RECURSIVE_ENTITY_REF},
      {NULL, NULL, NULL, XML_ERROR_NONE}};
  int i;

  for (i = 0; data_004_2[i].parse_text != NULL; i++) {
    XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
    XML_SetExternalEntityRefHandler(g_parser, external_entity_valuer);
    XML_SetUserData(g_parser, &data_004_2[i]);
    if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
        == XML_STATUS_ERROR)
      xml_failure(g_parser);
    XML_ParserReset(g_parser, NULL);
  }
}
END_TEST

/* Test the recursive parse interacts with a not standalone handler */
START_TEST(test_ext_entity_not_standalone) {
  const char *text = "<!DOCTYPE doc SYSTEM 'foo'>\n"
                     "<doc></doc>";

  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetExternalEntityRefHandler(g_parser, external_entity_not_standalone);
  expect_failure(text, XML_ERROR_EXTERNAL_ENTITY_HANDLING,
                 "Standalone rejection not caught");
}
END_TEST

START_TEST(test_ext_entity_value_abort) {
  const char *text = "<!DOCTYPE doc SYSTEM '004-1.ent'>\n"
                     "<doc></doc>\n";

  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetExternalEntityRefHandler(g_parser, external_entity_value_aborter);
  g_resumable = XML_FALSE;
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
}
END_TEST

START_TEST(test_bad_public_doctype) {
  const char *text = "<?xml version='1.0' encoding='utf-8'?>\n"
                     "<!DOCTYPE doc PUBLIC '{BadName}' 'test'>\n"
                     "<doc></doc>";

  /* Setting a handler provokes a particular code path */
  XML_SetDoctypeDeclHandler(g_parser, dummy_start_doctype_handler,
                            dummy_end_doctype_handler);
  expect_failure(text, XML_ERROR_PUBLICID, "Bad Public ID not failed");
}
END_TEST

/* Test based on ibm/valid/P32/ibm32v04.xml */
START_TEST(test_attribute_enum_value) {
  const char *text = "<?xml version='1.0' standalone='no'?>\n"
                     "<!DOCTYPE animal SYSTEM 'test.dtd'>\n"
                     "<animal>This is a \n    <a/>  \n\nyellow tiger</animal>";
  ExtTest dtd_data
      = {"<!ELEMENT animal (#PCDATA|a)*>\n"
         "<!ELEMENT a EMPTY>\n"
         "<!ATTLIST animal xml:space (default|preserve) 'preserve'>",
         NULL, NULL};
  const XML_Char *expected = XCS("This is a \n      \n\nyellow tiger");

  XML_SetExternalEntityRefHandler(g_parser, external_entity_loader);
  XML_SetUserData(g_parser, &dtd_data);
  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  /* An attribute list handler provokes a different code path */
  XML_SetAttlistDeclHandler(g_parser, dummy_attlist_decl_handler);
  run_ext_character_check(text, &dtd_data, expected);
}
END_TEST

/* Slightly bizarrely, the library seems to silently ignore entity
 * definitions for predefined entities, even when they are wrong.  The
 * language of the XML 1.0 spec is somewhat unhelpful as to what ought
 * to happen, so this is currently treated as acceptable.
 */
START_TEST(test_predefined_entity_redefinition) {
  const char *text = "<!DOCTYPE doc [\n"
                     "<!ENTITY apos 'foo'>\n"
                     "]>\n"
                     "<doc>&apos;</doc>";
  run_character_check(text, XCS("'"));
}
END_TEST

/* Test that the parser stops processing the DTD after an unresolved
 * parameter entity is encountered.
 */
START_TEST(test_dtd_stop_processing) {
  const char *text = "<!DOCTYPE doc [\n"
                     "%foo;\n"
                     "<!ENTITY bar 'bas'>\n"
                     "]><doc/>";

  XML_SetEntityDeclHandler(g_parser, dummy_entity_decl_handler);
  init_dummy_handlers();
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  if (get_dummy_handler_flags() != 0)
    fail("DTD processing still going after undefined PE");
}
END_TEST

/* Test public notations with no system ID */
START_TEST(test_public_notation_no_sysid) {
  const char *text = "<!DOCTYPE doc [\n"
                     "<!NOTATION note PUBLIC 'foo'>\n"
                     "<!ELEMENT doc EMPTY>\n"
                     "]>\n<doc/>";

  init_dummy_handlers();
  XML_SetNotationDeclHandler(g_parser, dummy_notation_decl_handler);
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  if (get_dummy_handler_flags() != DUMMY_NOTATION_DECL_HANDLER_FLAG)
    fail("Notation declaration handler not called");
}
END_TEST

START_TEST(test_nested_groups) {
  const char *text
      = "<!DOCTYPE doc [\n"
        "<!ELEMENT doc "
        /* Sixteen elements per line */
        "(e,(e?,(e?,(e?,(e?,(e?,(e?,(e?,(e?,(e?,(e?,(e?,(e?,(e?,(e?,(e?,"
        "(e?,(e?,(e?,(e?,(e?,(e?,(e?,(e?,(e?,(e?,(e?,(e?,(e?,(e?,(e?,(e?"
        "))))))))))))))))))))))))))))))))>\n"
        "<!ELEMENT e EMPTY>"
        "]>\n"
        "<doc><e/></doc>";
  CharData storage;

  CharData_Init(&storage);
  XML_SetElementDeclHandler(g_parser, dummy_element_decl_handler);
  XML_SetStartElementHandler(g_parser, record_element_start_handler);
  XML_SetUserData(g_parser, &storage);
  init_dummy_handlers();
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  CharData_CheckXMLChars(&storage, XCS("doce"));
  if (get_dummy_handler_flags() != DUMMY_ELEMENT_DECL_HANDLER_FLAG)
    fail("Element handler not fired");
}
END_TEST

START_TEST(test_group_choice) {
  const char *text = "<!DOCTYPE doc [\n"
                     "<!ELEMENT doc (a|b|c)+>\n"
                     "<!ELEMENT a EMPTY>\n"
                     "<!ELEMENT b (#PCDATA)>\n"
                     "<!ELEMENT c ANY>\n"
                     "]>\n"
                     "<doc>\n"
                     "<a/>\n"
                     "<b attr='foo'>This is a foo</b>\n"
                     "<c></c>\n"
                     "</doc>\n";

  XML_SetElementDeclHandler(g_parser, dummy_element_decl_handler);
  init_dummy_handlers();
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  if (get_dummy_handler_flags() != DUMMY_ELEMENT_DECL_HANDLER_FLAG)
    fail("Element handler flag not raised");
}
END_TEST

START_TEST(test_standalone_parameter_entity) {
  const char *text = "<?xml version='1.0' standalone='yes'?>\n"
                     "<!DOCTYPE doc SYSTEM 'http://example.org/' [\n"
                     "<!ENTITY % entity '<!ELEMENT doc (#PCDATA)>'>\n"
                     "%entity;\n"
                     "]>\n"
                     "<doc></doc>";
  char dtd_data[] = "<!ENTITY % e1 'foo'>\n";

  XML_SetUserData(g_parser, dtd_data);
  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetExternalEntityRefHandler(g_parser, external_entity_public);
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
}
END_TEST

/* Test skipping of parameter entity in an external DTD */
/* Derived from ibm/invalid/P69/ibm69i01.xml */
START_TEST(test_skipped_parameter_entity) {
  const char *text = "<?xml version='1.0'?>\n"
                     "<!DOCTYPE root SYSTEM 'http://example.org/dtd.ent' [\n"
                     "<!ELEMENT root (#PCDATA|a)* >\n"
                     "]>\n"
                     "<root></root>";
  ExtTest dtd_data = {"%pe2;", NULL, NULL};

  XML_SetExternalEntityRefHandler(g_parser, external_entity_loader);
  XML_SetUserData(g_parser, &dtd_data);
  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetSkippedEntityHandler(g_parser, dummy_skip_handler);
  init_dummy_handlers();
  if (_XML_Parse_SINGLE_BYTES(g_parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR)
    xml_failure(g_parser);
  if (get_dummy_handler_flags() != DUMMY_SKIP_HANDLER_FLAG)
    fail("Skip handler not executed");
}
END_TEST

/* Test recursive parameter entity definition rejected in external DTD */
START_TEST(test_recursive_external_parameter_entity) {
  const char *text = "<?xml version='1.0'?>\n"
                     "<!DOCTYPE root SYSTEM 'http://example.org/dtd.ent' [\n"
                     "<!ELEMENT root (#PCDATA|a)* >\n"
                     "]>\n"
                     "<root></root>";
  ExtFaults dtd_data = {"<!ENTITY % pe2 '&#37;pe2;'>\n%pe2;",
                        "Recursive external parameter entity not faulted", NULL,
                        XML_ERROR_RECURSIVE_ENTITY_REF};

  XML_SetExternalEntityRefHandler(g_parser, external_entity_faulter);
  XML_SetUserData(g_parser, &dtd_data);
  XML_SetParamEntityParsing(g_parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  expect_failure(text, XML_ERROR_EXTERNAL_ENTITY_HANDLING,
                 "Recursive external parameter not spotted");
}
END_TEST

TCase *
make_basic_test_case(Suite *s) {
  TCase *tc_basic = tcase_create("basic tests");

  suite_add_tcase(s, tc_basic);
  tcase_add_checked_fixture(tc_basic, basic_setup, basic_teardown);

  tcase_add_test(tc_basic, test_nul_byte);
  tcase_add_test(tc_basic, test_u0000_char);
  tcase_add_test(tc_basic, test_siphash_self);
  tcase_add_test(tc_basic, test_siphash_spec);
  tcase_add_test(tc_basic, test_bom_utf8);
  tcase_add_test(tc_basic, test_bom_utf16_be);
  tcase_add_test(tc_basic, test_bom_utf16_le);
  tcase_add_test(tc_basic, test_nobom_utf16_le);
  tcase_add_test(tc_basic, test_hash_collision);
  tcase_add_test(tc_basic, test_illegal_utf8);
  tcase_add_test(tc_basic, test_utf8_auto_align);
  tcase_add_test(tc_basic, test_utf16);
  tcase_add_test(tc_basic, test_utf16_le_epilog_newline);
  tcase_add_test(tc_basic, test_not_utf16);
  tcase_add_test(tc_basic, test_bad_encoding);
  tcase_add_test(tc_basic, test_latin1_umlauts);
  tcase_add_test(tc_basic, test_long_utf8_character);
  tcase_add_test(tc_basic, test_long_latin1_attribute);
  tcase_add_test(tc_basic, test_long_ascii_attribute);
  /* Regression test for SF bug #491986. */
  tcase_add_test(tc_basic, test_danish_latin1);
  /* Regression test for SF bug #514281. */
  tcase_add_test(tc_basic, test_french_charref_hexidecimal);
  tcase_add_test(tc_basic, test_french_charref_decimal);
  tcase_add_test(tc_basic, test_french_latin1);
  tcase_add_test(tc_basic, test_french_utf8);
  tcase_add_test(tc_basic, test_utf8_false_rejection);
  tcase_add_test(tc_basic, test_line_number_after_parse);
  tcase_add_test(tc_basic, test_column_number_after_parse);
  tcase_add_test(tc_basic, test_line_and_column_numbers_inside_handlers);
  tcase_add_test(tc_basic, test_line_number_after_error);
  tcase_add_test(tc_basic, test_column_number_after_error);
  tcase_add_test(tc_basic, test_really_long_lines);
  tcase_add_test(tc_basic, test_really_long_encoded_lines);
  tcase_add_test(tc_basic, test_end_element_events);
  tcase_add_test(tc_basic, test_helper_is_whitespace_normalized);
  tcase_add_test(tc_basic, test_attr_whitespace_normalization);
  tcase_add_test(tc_basic, test_xmldecl_misplaced);
  tcase_add_test(tc_basic, test_xmldecl_invalid);
  tcase_add_test(tc_basic, test_xmldecl_missing_attr);
  tcase_add_test(tc_basic, test_xmldecl_missing_value);
  tcase_add_test(tc_basic, test_unknown_encoding_internal_entity);
  tcase_add_test(tc_basic, test_unrecognised_encoding_internal_entity);
  tcase_add_test__ifdef_xml_dtd(tc_basic, test_ext_entity_set_encoding);
  tcase_add_test__ifdef_xml_dtd(tc_basic, test_ext_entity_no_handler);
  tcase_add_test__ifdef_xml_dtd(tc_basic, test_ext_entity_set_bom);
  tcase_add_test__ifdef_xml_dtd(tc_basic, test_ext_entity_bad_encoding);
  tcase_add_test__ifdef_xml_dtd(tc_basic, test_ext_entity_bad_encoding_2);
  tcase_add_test(tc_basic, test_wfc_undeclared_entity_unread_external_subset);
  tcase_add_test(tc_basic, test_wfc_undeclared_entity_no_external_subset);
  tcase_add_test(tc_basic, test_wfc_undeclared_entity_standalone);
  tcase_add_test(tc_basic,
                 test_wfc_undeclared_entity_with_external_subset_standalone);
  tcase_add_test(tc_basic, test_entity_with_external_subset_unless_standalone);
  tcase_add_test(tc_basic, test_wfc_undeclared_entity_with_external_subset);
  tcase_add_test(tc_basic, test_not_standalone_handler_reject);
  tcase_add_test(tc_basic, test_not_standalone_handler_accept);
  tcase_add_test(tc_basic, test_wfc_no_recursive_entity_refs);
  tcase_add_test__ifdef_xml_dtd(tc_basic, test_ext_entity_invalid_parse);
  tcase_add_test(tc_basic, test_dtd_default_handling);
  tcase_add_test(tc_basic, test_dtd_attr_handling);
  tcase_add_test(tc_basic, test_empty_ns_without_namespaces);
  tcase_add_test(tc_basic, test_ns_in_attribute_default_without_namespaces);
  tcase_add_test(tc_basic, test_stop_parser_between_char_data_calls);
  tcase_add_test(tc_basic, test_suspend_parser_between_char_data_calls);
  tcase_add_test(tc_basic, test_repeated_stop_parser_between_char_data_calls);
  tcase_add_test(tc_basic, test_good_cdata_ascii);
  tcase_add_test(tc_basic, test_good_cdata_utf16);
  tcase_add_test(tc_basic, test_good_cdata_utf16_le);
  tcase_add_test(tc_basic, test_long_cdata_utf16);
  tcase_add_test(tc_basic, test_multichar_cdata_utf16);
  tcase_add_test(tc_basic, test_utf16_bad_surrogate_pair);
  tcase_add_test(tc_basic, test_bad_cdata);
  tcase_add_test(tc_basic, test_bad_cdata_utf16);
  tcase_add_test(tc_basic, test_stop_parser_between_cdata_calls);
  tcase_add_test(tc_basic, test_suspend_parser_between_cdata_calls);
  tcase_add_test(tc_basic, test_memory_allocation);
  tcase_add_test(tc_basic, test_default_current);
  tcase_add_test(tc_basic, test_dtd_elements);
  tcase_add_test(tc_basic, test_dtd_elements_nesting);
  tcase_add_test__ifdef_xml_dtd(tc_basic, test_set_foreign_dtd);
  tcase_add_test__ifdef_xml_dtd(tc_basic, test_foreign_dtd_not_standalone);
  tcase_add_test__ifdef_xml_dtd(tc_basic, test_invalid_foreign_dtd);
  tcase_add_test__ifdef_xml_dtd(tc_basic, test_foreign_dtd_with_doctype);
  tcase_add_test__ifdef_xml_dtd(tc_basic,
                                test_foreign_dtd_without_external_subset);
  tcase_add_test__ifdef_xml_dtd(tc_basic, test_empty_foreign_dtd);
  tcase_add_test(tc_basic, test_set_base);
  tcase_add_test(tc_basic, test_attributes);
  tcase_add_test(tc_basic, test_reset_in_entity);
  tcase_add_test(tc_basic, test_resume_invalid_parse);
  tcase_add_test(tc_basic, test_resume_resuspended);
  tcase_add_test(tc_basic, test_cdata_default);
  tcase_add_test(tc_basic, test_subordinate_reset);
  tcase_add_test(tc_basic, test_subordinate_suspend);
  tcase_add_test(tc_basic, test_subordinate_xdecl_suspend);
  tcase_add_test(tc_basic, test_subordinate_xdecl_abort);
  tcase_add_test__ifdef_xml_dtd(tc_basic,
                                test_ext_entity_invalid_suspended_parse);
  tcase_add_test(tc_basic, test_explicit_encoding);
  tcase_add_test(tc_basic, test_trailing_cr);
  tcase_add_test(tc_basic, test_ext_entity_trailing_cr);
  tcase_add_test(tc_basic, test_trailing_rsqb);
  tcase_add_test(tc_basic, test_ext_entity_trailing_rsqb);
  tcase_add_test(tc_basic, test_ext_entity_good_cdata);
  tcase_add_test__ifdef_xml_dtd(tc_basic, test_user_parameters);
  tcase_add_test__ifdef_xml_dtd(tc_basic, test_ext_entity_ref_parameter);
  tcase_add_test(tc_basic, test_empty_parse);
  tcase_add_test(tc_basic, test_get_buffer_1);
  tcase_add_test(tc_basic, test_get_buffer_2);
#if defined(XML_CONTEXT_BYTES)
  tcase_add_test(tc_basic, test_get_buffer_3_overflow);
#endif
  tcase_add_test(tc_basic, test_byte_info_at_end);
  tcase_add_test(tc_basic, test_byte_info_at_error);
  tcase_add_test(tc_basic, test_byte_info_at_cdata);
  tcase_add_test(tc_basic, test_predefined_entities);
  tcase_add_test__ifdef_xml_dtd(tc_basic, test_invalid_tag_in_dtd);
  tcase_add_test(tc_basic, test_not_predefined_entities);
  tcase_add_test__ifdef_xml_dtd(tc_basic, test_ignore_section);
  tcase_add_test__ifdef_xml_dtd(tc_basic, test_ignore_section_utf16);
  tcase_add_test__ifdef_xml_dtd(tc_basic, test_ignore_section_utf16_be);
  tcase_add_test__ifdef_xml_dtd(tc_basic, test_bad_ignore_section);
  tcase_add_test__ifdef_xml_dtd(tc_basic, test_external_entity_values);
  tcase_add_test__ifdef_xml_dtd(tc_basic, test_ext_entity_not_standalone);
  tcase_add_test__ifdef_xml_dtd(tc_basic, test_ext_entity_value_abort);
  tcase_add_test(tc_basic, test_bad_public_doctype);
  tcase_add_test(tc_basic, test_attribute_enum_value);
  tcase_add_test(tc_basic, test_predefined_entity_redefinition);
  tcase_add_test__ifdef_xml_dtd(tc_basic, test_dtd_stop_processing);
  tcase_add_test(tc_basic, test_public_notation_no_sysid);
  tcase_add_test(tc_basic, test_nested_groups);
  tcase_add_test(tc_basic, test_group_choice);
  tcase_add_test(tc_basic, test_standalone_parameter_entity);
  tcase_add_test__ifdef_xml_dtd(tc_basic, test_skipped_parameter_entity);
  tcase_add_test__ifdef_xml_dtd(tc_basic,
                                test_recursive_external_parameter_entity);

  return tc_basic; /* TEMPORARY: this will become a void function */
}
