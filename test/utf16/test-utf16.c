/*
 * Test of the ORG4C parser in the UTF-16 build.
 *
 * The UTF-16 build (ORG4C_USE_UTF16, Windows only) cannot be tested with the
 * other tests as org2html (and the HTML renderer) do not support it. So this
 * program parses a few inputs where the encoding matters and compares the
 * reported events with the expected ones.
 *
 * The inputs are written in UTF-8 and converted into UTF-16 when needed, and
 * the text in the reported events is converted back to Unicode codepoints.
 * Hence the program works (and the expected events are the same) in any
 * build, so it can be used also to check the expectations in the default
 * UTF-8 build:
 *
 *   $ cc -Isrc test/utf16/test-utf16.c src/org4c.c src/entity.c -o build/test-utf16
 *   > cl /DORG4C_USE_UTF16 /D_CRT_SECURE_NO_WARNINGS /Isrc test\utf16\test-utf16.c src\org4c.c src\entity.c
 */

#include <stdio.h>
#include <string.h>

#include "org4c.h"


typedef struct TEST_tag TEST;
struct TEST_tag {
    const char* name;
    const char* input;          /* UTF-8. */
    unsigned input_size;        /* Zero means strlen(input). */
    const char* expected;
};

/* The events are recorded as follows:
 *  -- "[P" ... "]" for a paragraph (other blocks are not recorded);
 *  -- "<b" ... ">" for spans ("b" bold, "i" italic, "u" underline, "c" code,
 *     "a" link, "t" target, "l" LaTeX, "?" any other);
 *  -- "T'...'" for normal text, "E'...'" for an entity, "C'...'" for code,
 *     "L'...'" for LaTeX, "0" for ORG_TEXT_NULLCHAR, "/" for a soft break,
 *     "X'...'" for any other text.
 * Non-ASCII codepoints (and U+0000) are shown as "{U+XXXX}". */
static const TEST tests[] = {
    { "entities",
      "\\alpha \\alpha{} \\frac12 \\Leftrightarrow \\_  x \\notanentity",
      0,
      "[PE'\\alpha'T' 'E'\\alpha{}'T' 'E'\\frac12'T' 'E'\\Leftrightarrow'T' 'E'\\_  'T'x '<lL'\\notanentity'>]" },

    { "non-ASCII in emphasis",
      "*\xc3\xa9t\xc3\xa9* /\xe4\xb8\xad/ _\xe4\xb8\xad_",
      0,
      "[P<bT'{U+00E9}t{U+00E9}'>T' '<iT'{U+4E2D}'>T' '<uT'{U+4E2D}'>]" },

    /* Surrogate pairs in UTF-16. */
    { "non-BMP characters",
      "*\xf0\x9f\x98\x80* ~\xf0\x9f\x98\x80~",
      0,
      "[P<bT'{U+1F600}'>T' '<cC'{U+1F600}'>]" },

    /* U+3000 (ideographic space) is a whitespace. */
    { "Unicode whitespace",
      "a\xe3\x80\x80*b*\xe3\x80\x80" "c https://x.org\xe3\x80\x80y",
      0,
      "[PT'a{U+3000}'<bT'b'>T'{U+3000}c '<aT'https://x.org'>T'{U+3000}y']" },

    { "radio targets",
      "<<<\xc3\xa9t\xc3\xa9>>> \xc3\xa9t\xc3\xa9 <<<\xe4\xb8\xad>>> \xe4\xb8\xad",
      0,
      "[P<tT'{U+00E9}t{U+00E9}'>T' '<aT'{U+00E9}t{U+00E9}'>T' '<tT'{U+4E2D}'>T' '<aT'{U+4E2D}'>]" },

    { "NUL character",
      "a\0b",
      3,
      "[PT'a'0T'b']" }
};


#define TRACE_MAX   1024

typedef struct TRACE_tag TRACE;
struct TRACE_tag {
    char buffer[TRACE_MAX];
    size_t size;
};

static void
trace(TRACE* t, const char* str)
{
    size_t n = strlen(str);

    if(t->size + n < TRACE_MAX) {
        memcpy(t->buffer + t->size, str, n);
        t->size += n;
        t->buffer[t->size] = '\0';
    }
}

static void
trace_codepoint(TRACE* t, unsigned codepoint)
{
    char buf[16];

    if(0 < codepoint  &&  codepoint < 0x80) {
        buf[0] = (char) codepoint;
        buf[1] = '\0';
    } else {
        sprintf(buf, "{U+%04X}", codepoint);
    }
    trace(t, buf);
}

/* Output the text as the sequence of the codepoints. */
static void
trace_text(TRACE* t, const ORG_CHAR* text, ORG_SIZE size)
{
    ORG_SIZE i = 0;

    while(i < size) {
        unsigned codepoint;

#if defined ORG4C_USE_UTF16
        codepoint = (unsigned) text[i++];
        if(0xd800 <= codepoint  &&  codepoint <= 0xdbff  &&  i < size  &&
           0xdc00 <= (unsigned) text[i]  &&  (unsigned) text[i] <= 0xdfff)
        {
            codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + ((unsigned) text[i++] - 0xdc00);
        }
#else
        unsigned char ch = (unsigned char) text[i++];

        if(ch >= 0xf0  &&  i + 2 < size) {
            codepoint = ((ch & 0x07) << 18) | ((text[i] & 0x3f) << 12) | ((text[i+1] & 0x3f) << 6) | (text[i+2] & 0x3f);
            i += 3;
        } else if(ch >= 0xe0  &&  i + 1 < size) {
            codepoint = ((ch & 0x0f) << 12) | ((text[i] & 0x3f) << 6) | (text[i+1] & 0x3f);
            i += 2;
        } else if(ch >= 0xc0  &&  i < size) {
            codepoint = ((ch & 0x1f) << 6) | (text[i] & 0x3f);
            i += 1;
        } else {
            codepoint = ch;
        }
#endif
        trace_codepoint(t, codepoint);
    }
}

static int
enter_block_callback(ORG_BLOCKTYPE type, void* detail, void* userdata)
{
    (void) detail;
    if(type == ORG_BLOCK_P)
        trace((TRACE*) userdata, "[P");
    return 0;
}

static int
leave_block_callback(ORG_BLOCKTYPE type, void* detail, void* userdata)
{
    (void) detail;
    if(type == ORG_BLOCK_P)
        trace((TRACE*) userdata, "]");
    return 0;
}

static int
enter_span_callback(ORG_SPANTYPE type, void* detail, void* userdata)
{
    TRACE* t = (TRACE*) userdata;

    (void) detail;
    switch(type) {
        case ORG_SPAN_BOLD:         trace(t, "<b"); break;
        case ORG_SPAN_ITALIC:       trace(t, "<i"); break;
        case ORG_SPAN_UNDERLINE:    trace(t, "<u"); break;
        case ORG_SPAN_CODE:         trace(t, "<c"); break;
        case ORG_SPAN_LINK:         trace(t, "<a"); break;
        case ORG_SPAN_TARGET:       trace(t, "<t"); break;
        case ORG_SPAN_LATEX:        trace(t, "<l"); break;
        default:                    trace(t, "<?"); break;
    }
    return 0;
}

static int
leave_span_callback(ORG_SPANTYPE type, void* detail, void* userdata)
{
    (void) type;
    (void) detail;
    trace((TRACE*) userdata, ">");
    return 0;
}

static int
text_callback(ORG_TEXTTYPE type, const ORG_CHAR* text, ORG_SIZE size, void* userdata)
{
    TRACE* t = (TRACE*) userdata;

    switch(type) {
        case ORG_TEXT_NULLCHAR:     trace(t, "0"); return 0;
        case ORG_TEXT_SOFTBR:       trace(t, "/"); return 0;
        case ORG_TEXT_NORMAL:       trace(t, "T'"); break;
        case ORG_TEXT_ENTITY:       trace(t, "E'"); break;
        case ORG_TEXT_CODE:         trace(t, "C'"); break;
        case ORG_TEXT_LATEX:        trace(t, "L'"); break;
        default:                    trace(t, "X'"); break;
    }
    trace_text(t, text, size);
    trace(t, "'");
    return 0;
}

/* Convert the UTF-8 input into ORG_CHAR. Returns the size of the result. */
static ORG_SIZE
convert_input(const char* input, unsigned size, ORG_CHAR* buffer, ORG_SIZE buffer_size)
{
#if defined ORG4C_USE_UTF16
    ORG_SIZE n = 0;
    unsigned i = 0;

    while(i < size  &&  n + 2 <= buffer_size) {
        unsigned char ch = (unsigned char) input[i++];
        unsigned codepoint;

        if(ch >= 0xf0) {
            codepoint = ((ch & 0x07) << 18) | ((input[i] & 0x3f) << 12) | ((input[i+1] & 0x3f) << 6) | (input[i+2] & 0x3f);
            i += 3;
        } else if(ch >= 0xe0) {
            codepoint = ((ch & 0x0f) << 12) | ((input[i] & 0x3f) << 6) | (input[i+1] & 0x3f);
            i += 2;
        } else if(ch >= 0xc0) {
            codepoint = ((ch & 0x1f) << 6) | (input[i] & 0x3f);
            i += 1;
        } else {
            codepoint = ch;
        }

        if(codepoint >= 0x10000) {
            codepoint -= 0x10000;
            buffer[n++] = (ORG_CHAR) (0xd800 + (codepoint >> 10));
            buffer[n++] = (ORG_CHAR) (0xdc00 + (codepoint & 0x3ff));
        } else {
            buffer[n++] = (ORG_CHAR) codepoint;
        }
    }
    return n;
#else
    if(size > buffer_size)
        size = buffer_size;
    memcpy(buffer, input, size);
    return size;
#endif
}

int
main(void)
{
    ORG_PARSER parser;
    int n_failed = 0;
    int i;

    memset(&parser, 0, sizeof(parser));
    parser.enter_block = enter_block_callback;
    parser.leave_block = leave_block_callback;
    parser.enter_span = enter_span_callback;
    parser.leave_span = leave_span_callback;
    parser.text = text_callback;

    for(i = 0; i < (int) (sizeof(tests) / sizeof(tests[0])); i++) {
        const TEST* test = &tests[i];
        ORG_CHAR input[256];
        ORG_SIZE input_size;
        TRACE t;
        int ret;

        input_size = convert_input(test->input,
                    (test->input_size > 0 ? test->input_size : (unsigned) strlen(test->input)),
                    input, sizeof(input) / sizeof(input[0]));
        t.size = 0;
        t.buffer[0] = '\0';
        ret = org_parse(input, input_size, &parser, &t);

        if(ret != 0  ||  strcmp(t.buffer, test->expected) != 0) {
            printf("FAILED: %s\n  expected: %s\n  actual:   %s\n", test->name, test->expected, t.buffer);
            n_failed++;
        } else {
            printf("passed: %s\n", test->name);
        }
    }

    printf("%d passed, %d failed\n", i - n_failed, n_failed);
    return (n_failed > 0 ? 1 : 0);
}
