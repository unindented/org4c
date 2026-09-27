/*
 * ORG4C: Org-mode parser for C
 * (https://github.com/unindented/org4c)
 *
 * Copyright (c) 2026 Daniel Perez Alvarez
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "org4c-html.h"
#include "cmdline.h"



/* Global options. */
static unsigned parser_flags = 0;
#ifndef ORG4C_USE_ASCII
    static unsigned renderer_flags = ORG_HTML_FLAG_DEBUG | ORG_HTML_FLAG_SKIP_UTF8_BOM | ORG_HTML_FLAG_IN_BUFFER_OPTIONS;
#else
    static unsigned renderer_flags = ORG_HTML_FLAG_DEBUG | ORG_HTML_FLAG_IN_BUFFER_OPTIONS;
#endif
static int want_fullhtml = 0;
static int want_xhtml = 0;
static int want_stat = 0;
static int want_replay_fuzz = 0;

static const char* html_title = NULL;
static const char* css_path = NULL;


/*********************************
 ***  Simple grow-able buffer  ***
 *********************************/

/* We render to a memory buffer instead of directly outputting the rendered
 * documents, as this allows using this utility for evaluating performance
 * of ORG4C (--stat option). This allows us to measure just time of the parser,
 * without the I/O.
 */

struct membuffer {
    char* data;
    size_t asize;
    size_t size;
};

static void
membuf_init(struct membuffer* buf, ORG_SIZE new_asize)
{
    buf->size = 0;
    buf->asize = new_asize;
    buf->data = malloc(buf->asize);
    if(buf->data == NULL) {
        fprintf(stderr, "membuf_init: malloc() failed.\n");
        exit(EXIT_FAILURE);
    }
}

static void
membuf_fini(struct membuffer* buf)
{
    if(buf->data)
        free(buf->data);
}

static void
membuf_grow(struct membuffer* buf, size_t new_asize)
{
    buf->data = realloc(buf->data, new_asize);
    if(buf->data == NULL) {
        fprintf(stderr, "membuf_grow: realloc() failed.\n");
        exit(EXIT_FAILURE);
    }
    buf->asize = new_asize;
}

static void
membuf_append(struct membuffer* buf, const char* data, ORG_SIZE size)
{
    if(size > (size_t)-1 - buf->size) {
        fprintf(stderr, "membuf_append: size overflow.\n");
        exit(EXIT_FAILURE);
    }
    if(buf->asize < buf->size + size) {
        size_t new_asize = buf->size + buf->size / 2 + size;
        if(new_asize < buf->size + size)
            new_asize = buf->size + size;
        membuf_grow(buf, new_asize);
    }
    memcpy(buf->data + buf->size, data, size);
    buf->size += size;
}


/**********************
 ***  Main program  ***
 **********************/

static void
process_output(const ORG_CHAR* text, ORG_SIZE size, void* userdata)
{
    membuf_append((struct membuffer*) userdata, text, size);
}

/* If the line [off, line_end) is "#+KEY: VALUE" (with the case-insensitive
 * 'key' given including the colon, e.g. "TITLE:"), return the offset of the
 * value (after any whitespace); otherwise zero. */
static size_t
keyword_value(const char* input, size_t off, size_t line_end, const char* key)
{
    size_t key_size = strlen(key);
    size_t i;

    if(line_end - off < 2 + key_size  ||  input[off] != '#'  ||  input[off+1] != '+')
        return 0;
    for(i = 0; i < key_size; i++) {
        char ch = input[off + 2 + i];
        if(ch >= 'a'  &&  ch <= 'z')
            ch += 'A' - 'a';
        if(ch != key[i])
            return 0;
    }

    off += 2 + key_size;
    while(off < line_end  &&  (input[off] == ' '  ||  input[off] == '\t'))
        off++;
    return off;
}

/* Collect the values of the "#+TITLE:" lines of the document, joined with a
 * space (as Emacs does); and the "#+OPTIONS:" lines, so that the title can be
 * rendered with them. The lines are looked for the same way org_html() looks
 * for the "#+OPTIONS:" lines. */
static void
find_title(const char* input, size_t size, struct membuffer* title, struct membuffer* options)
{
    size_t off = 0;

    /* Skip UTF-8 byte order mark (BOM), if any. */
    if(size >= 3  &&  memcmp(input, "\xef\xbb\xbf", 3) == 0)
        off = 3;

    while(off < size) {
        size_t line_end = off;
        size_t value;

        while(line_end < size  &&  input[line_end] != '\n'  &&  input[line_end] != '\r')
            line_end++;
        while(off < line_end  &&  (input[off] == ' '  ||  input[off] == '\t'))
            off++;

        if((value = keyword_value(input, off, line_end, "TITLE:")) != 0) {
            size_t value_end = line_end;

            while(value_end > value  &&  (input[value_end-1] == ' '  ||  input[value_end-1] == '\t'))
                value_end--;
            if(value_end > value) {
                if(title->size > 0)
                    membuf_append(title, " ", 1);
                membuf_append(title, input + value, (ORG_SIZE)(value_end - value));
            }
        } else if(keyword_value(input, off, line_end, "OPTIONS:") != 0) {
            membuf_append(options, input + off, (ORG_SIZE)(line_end - off));
            membuf_append(options, "\n", 1);
        }

        off = line_end + 1;
    }
}

/* Output the text escaped for HTML. */
static void
write_html_escaped(FILE* out, const char* text, size_t size)
{
    size_t i;

    for(i = 0; i < size; i++) {
        switch(text[i]) {
            case '&':   fputs("&amp;", out); break;
            case '<':   fputs("&lt;", out); break;
            case '>':   fputs("&gt;", out); break;
            case '"':   fputs("&quot;", out); break;
            default:    fputc(text[i], out); break;
        }
    }
}

/* Output the title rendered with the Org markup (and with the document's
 * "#+OPTIONS:", which produce no output), as the contents of the single
 * paragraph org_html() produces for it. If it produces anything else, output
 * the title as plain text. */
static void
write_rendered_title(FILE* out, const struct membuffer* title, const struct membuffer* options,
                     unsigned p_flags, unsigned r_flags)
{
    struct membuffer doc = {0};
    struct membuffer buf = {0};
    size_t off = 3;
    int ret;

    membuf_init(&doc, (ORG_SIZE)(options->size + title->size + 1));
    membuf_append(&doc, options->data, (ORG_SIZE)options->size);
    membuf_append(&doc, title->data, (ORG_SIZE)title->size);

    membuf_init(&buf, (ORG_SIZE)(title->size + title->size/8 + 64));
    ret = org_html(doc.data, (ORG_SIZE)doc.size, process_output,
                (void*) &buf, p_flags, r_flags);

    /* Find the first "</p>" (which must be the one at the end). */
    while(off + 4 <= buf.size  &&  memcmp(buf.data + off, "</p>", 4) != 0)
        off++;

    if(ret == 0  &&  buf.size >= 8  &&  memcmp(buf.data, "<p>", 3) == 0  &&
       off == buf.size - 5  &&  buf.data[buf.size - 1] == '\n')
        fwrite(buf.data + 3, 1, buf.size - 8, out);
    else
        write_html_escaped(out, title->data, title->size);

    membuf_fini(&doc);
    membuf_fini(&buf);
}

static int
process_file(const char* in_path, FILE* in, FILE* out)
{
    size_t n;
    struct membuffer buf_in = {0};
    struct membuffer buf_out = {0};
    struct membuffer title = {0};
    struct membuffer options = {0};
    int ret = -1;
    clock_t t0, t1;
    unsigned p_flags = parser_flags;
    unsigned r_flags = renderer_flags;

    membuf_init(&buf_in, 32 * 1024);

    /* Read the input file into a buffer. */
    while(1) {
        if(buf_in.size >= buf_in.asize)
            membuf_grow(&buf_in, buf_in.asize + buf_in.asize / 2);

        n = fread(buf_in.data + buf_in.size, 1, buf_in.asize - buf_in.size, in);
        if(n == 0)
            break;
        buf_in.size += n;
    }

    /* Input size is good estimation of output size. Add some more reserve to
     * deal with the HTML header/footer and tags. */
    membuf_init(&buf_out, (ORG_SIZE)(buf_in.size + buf_in.size/8 + 64));

    /* Special mode for reproducing a test case found with a fuzzing tool.
     * We assume file the same file format as produced by the fuzzer implemented
     * in test/fuzzers/fuzz-orghtml.c. */
    if(want_replay_fuzz) {
        if(buf_in.size < 2 * sizeof(unsigned)) {
            fprintf(stderr, "File %s isn't valid fuzz test case.\n", in_path);
            ret = -1;
            goto out;
        }

        /* Override parser and renderer flags with those from the test case. */
        p_flags = ((unsigned*)buf_in.data)[0];
        r_flags = ((unsigned*)buf_in.data)[1] | ORG_HTML_FLAG_DEBUG;

        /* And get rid of them from the text input to the parser. */
        memmove(buf_in.data, buf_in.data + 2 * sizeof(unsigned),
                    buf_in.size - 2 * sizeof(unsigned));
        buf_in.size -= 2 * sizeof(unsigned);
    }

    /* Parse the document. This shall call our callbacks provided via the
     * ORG_PARSER structure. */
    t0 = clock();

    ret = org_html(buf_in.data, (ORG_SIZE)buf_in.size, process_output,
                (void*) &buf_out, p_flags, r_flags);

    t1 = clock();
    if(ret != 0) {
        fprintf(stderr, "Parsing failed.\n");
        goto out;
    }

    /* Write down the document in the HTML format. */
    if(want_fullhtml) {
        membuf_init(&title, 64);
        membuf_init(&options, 64);
        find_title(buf_in.data, buf_in.size, &title, &options);

        if(want_xhtml) {
            fprintf(out, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
            fprintf(out, "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" "
                            "\"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">\n");
            fprintf(out, "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n");
        } else {
            fprintf(out, "<!DOCTYPE html>\n");
            fprintf(out, "<html>\n");
        }
        fprintf(out, "<head>\n");
        if(html_title != NULL) {
            fprintf(out, "<title>%s</title>\n", html_title);
        } else {
            fprintf(out, "<title>");
            write_html_escaped(out, title.data, title.size);
            fprintf(out, "</title>\n");
        }
        fprintf(out, "<meta name=\"generator\" content=\"org2html\"%s>\n", want_xhtml ? " /" : "");
#if !defined ORG4C_USE_ASCII && !defined ORG4C_USE_UTF16
        fprintf(out, "<meta charset=\"UTF-8\"%s>\n", want_xhtml ? " /" : "");
#endif
        if(css_path != NULL) {
            fprintf(out, "<link rel=\"stylesheet\" href=\"%s\"%s>\n", css_path, want_xhtml ? " /" : "");
        }
        fprintf(out, "</head>\n");
        fprintf(out, "<body>\n");
        if((r_flags & ORG_HTML_FLAG_TOPLEVEL_H2)  &&  title.size > 0) {
            fprintf(out, "<h1 class=\"title\">");
            write_rendered_title(out, &title, &options, p_flags, r_flags);
            fprintf(out, "</h1>\n");
        }
    }

    fwrite(buf_out.data, 1, buf_out.size, out);

    if(want_fullhtml) {
        fprintf(out, "</body>\n");
        fprintf(out, "</html>\n");
    }

    if(want_stat) {
        if(t0 != (clock_t)-1  &&  t1 != (clock_t)-1) {
            double elapsed = (double)(t1 - t0) / CLOCKS_PER_SEC;
            if (elapsed < 1)
                fprintf(stderr, "Time spent on parsing: %7.2f ms.\n", elapsed*1e3);
            else
                fprintf(stderr, "Time spent on parsing: %6.3f s.\n", elapsed);
        }
    }

    /* Success if we have reached here. */
    ret = 0;

out:
    membuf_fini(&buf_in);
    membuf_fini(&buf_out);
    membuf_fini(&title);
    membuf_fini(&options);

    return ret;
}


static const CMDLINE_OPTION cmdline_options[] = {
    { 'o', "output",                        'o', CMDLINE_OPTFLAG_REQUIREDARG },
    { 'f', "full-html",                     'f', 0 },
    { 'x', "xhtml",                         'x', 0 },
    { 's', "stat",                          's', 0 },
    { 'h', "help",                          'h', 0 },
    { 'v', "version",                       'v', 0 },

    {  0,  "html-title",                    '1', CMDLINE_OPTFLAG_REQUIREDARG },
    {  0,  "html-css",                      '2', CMDLINE_OPTFLAG_REQUIREDARG },

    {  0,  "fhard-soft-breaks",             'B', 0 },
    {  0,  "fignore-options",               'O', 0 },
    {  0,  "finlinetasks",                  'i', 0 },
    {  0,  "fsubsuperscripts",              '^', 0 },
    {  0,  "fsubsuperscripts-braced",       '{', 0 },
    {  0,  "ftoplevel-h2",                  'H', 0 },
    {  0,  "fverbatim-entities",            'e', 0 },
    {  0,  "fverbatim-special-strings",     'S', 0 },

    {  0,  "fno-export-blocks",             'E', 0 },
    {  0,  "fno-latex",                     'X', 0 },
    {  0,  "fno-plain-links",               'L', 0 },

    /* Undocumented option for replaying test cases from fuzzers. */
    {  0,  "replay-fuzz",                   'r', 0 },

    {  0,  NULL,                             0,  0 }
};

static void
usage(void)
{
    printf(
        "Usage: org2html [OPTION]... [FILE]\n"
        "Convert input FILE (or standard input) in Org format to HTML.\n"
        "Commented subtrees and the ones tagged :noexport: are skipped.\n"
        "\n"
        "General options:\n"
        "  -o, --output=FILE    Output file (default is standard output)\n"
        "  -f, --full-html      Generate full HTML document, including header\n"
        "  -x, --xhtml          Generate XHTML instead of HTML\n"
        "  -s, --stat           Measure time of input parsing\n"
        "  -h, --help           Display this help and exit\n"
        "  -v, --version        Display version and exit\n"
        "\n"
        "Org extension options:\n"
        "      --finlinetasks   Enable inline tasks (headlines of level 15 or deeper)\n"
        "      --fsubsuperscripts\n"
        "                       Enable sub/superscripts (a_b, a^{b})\n"
        "      --fsubsuperscripts-braced\n"
        "                       Enable only braced sub/superscripts (a_{b}), as Org's ^:{}\n"
        "\n"
        "Org suppression options:\n"
        "      --fno-export-blocks\n"
        "                       Do not output export blocks (#+BEGIN_EXPORT)\n"
        "      --fno-latex      Do not recognize LaTeX fragments and environments\n"
        "      --fno-plain-links\n"
        "                       Do not recognize plain links (e.g. https://example.com)\n"
        "\n"
        "HTML generator options:\n"
        "      --fhard-soft-breaks\n"
        "                       Render all soft line breaks as hard ones\n"
        "      --fignore-options\n"
        "                       Ignore the in-buffer #+OPTIONS of the document\n"
        "                       (otherwise they override the respective options)\n"
        "      --ftoplevel-h2   Render level-N headlines as <hN+1>, as ox-html does\n"
        "                       (and, in full HTML or XHTML mode, #+TITLE as <h1>)\n"
        "      --fverbatim-entities\n"
        "                       Do not translate entities (e.g. \\alpha)\n"
        "      --fverbatim-special-strings\n"
        "                       Do not translate \"--\", \"---\", \"...\" and \"\\-\"\n"
        "      --html-title=TITLE Sets the title of the document\n"
        "      --html-css=URL   In full HTML or XHTML mode add a css link\n"
        "\n"
    );
}

static void
version(void)
{
    printf("%d.%d.%d\n", ORG_VERSION_MAJOR, ORG_VERSION_MINOR, ORG_VERSION_RELEASE);
}

static const char* input_path = NULL;
static const char* output_path = NULL;

static int
cmdline_callback(int opt, char const* value, void* data)
{
    (void) data;   /* unused parameter */

    switch(opt) {
        case 0:
            if(input_path) {
                fprintf(stderr, "Too many arguments. Only one input file can be specified.\n");
                fprintf(stderr, "Use --help for more info.\n");
                exit(EXIT_FAILURE);
            }
            input_path = value;
            break;

        case 'o':   output_path = value; break;
        case 'f':   want_fullhtml = 1; break;
        case 'x':   want_xhtml = 1; renderer_flags |= ORG_HTML_FLAG_XHTML; break;
        case 's':   want_stat = 1; break;
        case 'r':   want_replay_fuzz = 1; break;
        case 'h':   usage(); exit(EXIT_SUCCESS); break;
        case 'v':   version(); exit(EXIT_SUCCESS); break;

        case '1':   html_title = value; break;
        case '2':   css_path = value; break;

        case 'B':   renderer_flags |= ORG_HTML_FLAG_HARD_SOFT_BREAKS; break;
        case 'O':   renderer_flags &= ~ORG_HTML_FLAG_IN_BUFFER_OPTIONS; break;
        case 'i':   parser_flags |= ORG_FLAG_INLINETASKS; break;
        case '^':   parser_flags |= ORG_FLAG_SUBSUPERSCRIPTS; break;
        case '{':   parser_flags |= ORG_FLAG_SUBSUPERSCRIPTS_BRACED; break;
        case 'H':   renderer_flags |= ORG_HTML_FLAG_TOPLEVEL_H2; break;
        case 'e':   renderer_flags |= ORG_HTML_FLAG_VERBATIM_ENTITIES; break;
        case 'S':   renderer_flags |= ORG_HTML_FLAG_VERBATIM_SPECIAL_STRINGS; break;

        case 'E':   parser_flags |= ORG_FLAG_NOEXPORTBLOCKS; break;
        case 'X':   parser_flags |= ORG_FLAG_NOLATEX; break;
        case 'L':   parser_flags |= ORG_FLAG_NOPLAINLINKS; break;

        default:
            fprintf(stderr, "Illegal option: %s\n", value);
            fprintf(stderr, "Use --help for more info.\n");
            exit(EXIT_FAILURE);
            break;
    }

    return 0;
}

int
main(int argc, char** argv)
{
    FILE* in = stdin;
    FILE* out = stdout;
    int ret = 0;

    if(cmdline_read(cmdline_options, argc, argv, cmdline_callback, NULL) != 0) {
        usage();
        exit(EXIT_FAILURE);
    }

    if(input_path != NULL && strcmp(input_path, "-") != 0) {
        in = fopen(input_path, "rb");
        if(in == NULL) {
            fprintf(stderr, "Cannot open %s.\n", input_path);
            exit(EXIT_FAILURE);
        }
    }
    if(output_path != NULL && strcmp(output_path, "-") != 0) {
        out = fopen(output_path, "wt");
        if(out == NULL) {
            fprintf(stderr, "Cannot open %s.\n", output_path);
            exit(EXIT_FAILURE);
        }
    }

    ret = process_file((input_path != NULL) ? input_path : "<stdin>", in, out);
    if(in != stdin)
        fclose(in);
    if(out != stdout)
        fclose(out);

    return ret;
}
