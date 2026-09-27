<div align="center">
  <img src="media/logo.png" height="300" alt="">
</div>

<h1 align="center"><code>org4c</code></h1>

This is an [Org](https://orgmode.org) parser implementation in C. It is modeled after the [MD4C](https://github.com/mity/md4c) Markdown parser, and shares its design and some code with it.

## LLM disclosure

> [!warning]
> I used LLMs extensively to create this project, mostly Claude Opus 5.5.

## What is Org

Org is the markup language of the Emacs [Org mode](https://orgmode.org). Its syntax is described in the document [Org syntax](https://orgmode.org/worg/org-syntax.html).

## What is ORG4C

ORG4C is an Org parser implementation in C, with the following features:

- **Compatibility:** ORG4C aims to parse the documents the same way as the Emacs parser (`org-element`) does. The HTML renderer produces output similar to the Emacs `ox-html` exporter (but simpler).

- **Compactness:** ORG4C parser is implemented in one source file and one header file (plus the table of Org entities in `entity.[hc]`). There are no dependencies other than standard C library.

- **Embedding:** ORG4C parser is easy to reuse in other projects, its API is very straightforward: There is actually just one function, `org_parse()`.

- **Push model:** ORG4C parses the complete document and calls few callback functions provided by the application to inform it about a start/end of every block, a start/end of every span, and with any textual contents.

- **Robustness:** ORG4C is designed to parse in (nearly) linear time, whatever the input is. See `test/pathological-tests.py`.

- **Encoding:** ORG4C by default expects UTF-8 encoding of the input document. But it can be compiled to recognize ASCII-only control characters (`ORG4C_USE_ASCII`), or (on Windows, and only in the parser) to expect UTF-16 (`ORG4C_USE_UTF16`). See [Input/Output Encoding](#inputoutput-encoding).

- **Permissive license:** ORG4C is available under the [MIT license](LICENSE.txt).

## Using ORG4C

### Parsing Org

If you need just to parse an Org document, you need to include `org4c.h` and link against the ORG4C library (`-lorg4c`); or alternatively add `org4c.[hc]` and `entity.[hc]` directly to your code base as the parser is only implemented in the single C source file (and the table of the entities it recognizes).

The main provided function is `org_parse()`. It takes a text in the Org syntax and a pointer to a structure which provides pointers to several callback functions.

As `org_parse()` processes the input, it calls the callbacks (when entering or leaving any Org block or span; and when outputting any textual content of the document), allowing application to convert it into another format or render it onto the screen. Any additional information about a block or span (e.g. the level of a headline or the target of a link) is provided in a type-specific "detail" structure.

### Converting to HTML

If you need to convert Org to HTML, include `org4c-html.h` and link against the ORG4C-HTML library (`-lorg4c-html`); or alternatively add the sources `org4c.[hc]`, `org4c-html.[hc]` and `entity.[hc]` into your code base.

To convert an Org input, call `org_html()` function. It takes the Org input and calls the provided callback function. The callback is fed with chunks of the HTML output. Typical callback implementation just appends the chunks into a buffer or writes them to a file.

The utility `org2html` (in the directory `org2html/`) is a small command line wrapper of it:

```sh
$ org2html README.org > README.html
```

## Org Syntax Options

The default behavior is to parse the document the same way as Emacs Org mode does with its default settings.

However, with appropriate flags, the behavior can be tuned:

- With the flag `ORG_FLAG_INLINETASKS`, inline tasks (headlines of level 15 or deeper, as with Emacs `org-inlinetask`) are recognized.

- With the flag `ORG_FLAG_SUBSUPERSCRIPTS`, subscripts and superscripts (e.g. `a_b` or `a^{b}`) are recognized.

- With the flag `ORG_FLAG_SUBSUPERSCRIPTS_BRACED`, only the braced subscripts and superscripts (e.g. `a_{b}`) are recognized, as with Org's `^:{}`.

- With the flag `ORG_FLAG_SKIPCOMMENTED` or `ORG_FLAG_SKIPNOEXPORT`, subtrees of headlines with the `COMMENT` keyword or tagged `noexport` respectively are not reported at all.

Some features may be disabled with the following flags:

- With the flag `ORG_FLAG_NOEXPORTBLOCKS`, export blocks (`#+BEGIN_EXPORT`) are not reported.

- With the flag `ORG_FLAG_NOLATEX`, LaTeX fragments and environments are not recognized. (Entities still are.)

- With the flag `ORG_FLAG_NOPLAINLINKS`, plain links (e.g. `https://example.com`) are not recognized.

## Design Notes

Similarly to MD4C, the parser is hand-written and it works in two phases:

1. **Block analysis.** The document is processed line by line. Each line is classified (headline, list item, table row, `#+BEGIN_name` line, ...) and the lines are grouped into blocks. A stack of containers (sections, lists and list items, greater blocks and drawers) is maintained, driven by the headline levels and the indentation. The result is stored in a compact flat buffer of block records, which refer to the input only via offsets.

2. **Inline analysis.** For each leaf block, all the potential inline marks are collected, resolved and the result is reported via the callbacks.

Some notes about the Org specifics:

- Many Org constructs are only recognized if their terminating counterpart exists (e.g. a `#+BEGIN_SRC` without a matching `#+END_SRC` is just a paragraph line). To avoid scanning the document again and again (which could be quadratic), all the headlines, `#+END_name` and `:END:` lines are indexed in a pre-pass, so the terminating line is found by a binary search.

- Org resolves the emphasis in a simpler way than CommonMark does: A span is formed by a potential opener and the _nearest_ potential closer of the same kind (so `*a *b* c*` is `<b>a *b</b> c*`). The contents of a span (and of a link description, and of a table cell) is then parsed as a stand-alone text whose beginning and end act like a beginning and end of a line (so `**a**` is `<b><b>a</b></b>`).

## Status

Supported:

- Headlines (TODO keywords, including custom ones by `#+TODO:`, priority cookies, tags, `COMMENT`), planning lines and property drawers.
- Paragraphs and hard line breaks.
- Plain lists: unordered, ordered and description lists; checkboxes, counters.
- Blocks: `SRC`, `EXAMPLE`, `EXPORT`, `VERSE`, `QUOTE`, `CENTER`, `COMMENT` and special blocks.
- Drawers, keywords, comments, fixed-width areas, horizontal rules, LaTeX environments.
- Footnotes (definitions, references, inline and anonymous definitions).
- Tables, including the alignment cookies.
- Emphasis (`*bold*`, `/italic/`, `_underline_`, `+strike-through+`), `=verbatim=` and `~code~`.
- Bracket links (`[[target][description]]`), angle links and plain links.
- Entities (`\alpha`), LaTeX fragments (`$x$`, `\(x\)`, `\cmd{arg}`, ...), timestamps, targets (`<<target>>`), statistics cookies, export snippets, inline source blocks.
- Subscripts and superscripts (only with `ORG_FLAG_SUBSUPERSCRIPTS`, or `ORG_FLAG_SUBSUPERSCRIPTS_BRACED` for the braced ones only; or with `#+OPTIONS: ^:t` and `ORG_HTML_FLAG_IN_BUFFER_OPTIONS`).
- Macros: user-defined (`#+MACRO:`) and the built-in `title`, `author`, `email`, `date`, `keyword` and `n`. The expansions are parsed as Org text. (The nesting and the total size of the expansions are limited, so a malicious document cannot make the parser explode.)
- Radio targets (`<<<target>>>`) and the radio links.
- Citations (`[cite/style:prefix @key suffix; ...]`); the HTML renderer outputs just the keys (there is no bibliography processing).
- Inline Babel calls (reported, but nothing is rendered: ORG4C never evaluates any code).
- Line numbers (`-n`, `+n`) and code references (`(ref:name)`, `-l`, `-r`) in source and example blocks.
- Dynamic blocks (`#+BEGIN: name` ... `#+END:`).
- Inline tasks (only with `ORG_FLAG_INLINETASKS`, as they need `org-inlinetask` in Emacs).
- Captions (`#+CAPTION:`) of tables, source blocks and figures (paragraphs consisting only of an image).
- Names (`#+NAME:`, rendered as the `id` of the element) and HTML attributes (`#+ATTR_HTML:`).
- Some of `#+OPTIONS:` (see `ORG_HTML_FLAG_IN_BUFFER_OPTIONS`).

Not supported (yet):

- Macros with `(eval ...)` templates (they are kept verbatim), and the built-in macros `time`, `property`, `input-file`, `modification-time` (they expand to nothing).
- Affiliated keywords other than `#+CAPTION:`, `#+NAME:` and `#+ATTR_HTML:` are only reported (e.g. `#+ATTR_LATEX:`). The `#+ATTR_HTML:` option `:textarea` of example blocks is output as an ordinary attribute.
- Other in-buffer settings. Supported are only `#+TODO:`, `#+SEQ_TODO:`, `#+TYP_TODO:`, `#+MACRO:`, the keywords used by the macros (e.g. `#+TITLE:`) and some of `#+OPTIONS:`. Note that, unlike Emacs, ORG4C honors `#+OPTIONS:` even inside a block (e.g. `#+BEGIN_SRC`); and so does `org2html` with `#+TITLE:`.

## Differences from Emacs `ox-html`

The HTML renderer does not try to reproduce `ox-html` output byte-by-byte:

- Headline of level N is rendered as `<hN>` (at most `<h6>`), without any `<div>` wrappers, `id` attributes, section numbers or table of contents. `ox-html` uses `<hN+1>`, as it reserves `<h1>` for the document title; `ORG_HTML_FLAG_TOPLEVEL_H2` does the same. With `org2html --full-html --ftoplevel-h2`, the `#+TITLE:` is then output as `<h1 class="title">`.
- Unlike `ox-html`, headlines deeper than `H:` (`org-export-headline-levels`, 3 by default) are still rendered as headings, not as list items.
- Source blocks are rendered as `<pre><code class="language-xxx">` and no syntax highlighting is done.
- No heuristic right-alignment of numeric table columns.
- Special strings (`--`, `---`, `...`, `\-`) are translated as `ox-html` does (`ORG_HTML_FLAG_VERBATIM_SPECIAL_STRINGS` disables that).
- Internal links (to headlines, custom IDs, ...) are not resolved; they just point to `#target` (and targets `<<target>>` are rendered as `<a id="target"></a>`, so the links to targets work).
- The name (`#+NAME:`) of an element is its `id` (as with `org-html-prefer-user-labels`); unlike `ox-html`, also for paragraphs, lists, verse and center blocks and fixed-width areas, so that the links to them work. Elements without a name get no generated `id`.
- Of several `#+ATTR_HTML:` attributes with the same key, the last one is used (`ox-html` outputs all of them). At most 16 attributes per element are used.
- Footnote numbering and the footnote section follow `ox-html`. (`org_html()` always uses `ORG_FLAG_SKIPCOMMENTED` and `ORG_FLAG_SKIPNOEXPORT`, so that footnotes referenced only from non-exported subtrees are not listed.)

## Input/Output Encoding

Most of ORG4C code only assumes that the encoding of your choice is compatible with ASCII, i.e. that the codepoints below 128 have the same numeric values as ASCII. Any input ORG4C does not understand is simply seen as a part of the document text and sent to the callbacks unchanged.

Unicode plays a role only in the recognition of whitespace (e.g. at the boundaries of emphasis, sub/superscripts and plain links). It is handled as specified by the following preprocessor macros (at the time ORG4C is being built):

- If `ORG4C_USE_UTF8` is defined, ORG4C assumes UTF-8. When none of these macros is explicitly used, this is the default behavior.

- On Windows, if `ORG4C_USE_UTF16` is defined, ORG4C uses `WCHAR` instead of `char` and assumes UTF-16. Because this macro affects also the types in `org4c.h`, you have to define it both when building ORG4C as well as when including `org4c.h`. Also note this is only supported in the parser (`org4c.[hc]`). The HTML renderer does not support this and you will have to write your own custom renderer to use this feature.

- If `ORG4C_USE_ASCII` is defined, ORG4C assumes nothing but an ASCII input, i.e. non-ASCII whitespace characters are not recognized as such.

## Documentation

The API of the parser is documented in the comments in the `org4c.h`. Similarly, the Org-to-HTML API is described in its header `org4c-html.h`.

## Build and Test

ORG4C uses CMake:

```sh
$ cmake -B build -DCMAKE_BUILD_TYPE=Release
$ cmake --build build
```

The tests (which require Python 3) are run from the build directory:

```sh
$ cd build
$ python3 ../scripts/run-tests.py
```

The test suite consists of the specification files `test/spec*.txt`, which also serve as the documentation of the supported syntax, and of `test/regressions.txt` and `test/coverage.txt`. The pathological inputs in `test/pathological-tests.py` verify the parser stays fast on nasty inputs.

The UTF-16 build of the parser (see [Input/Output Encoding](#inputoutput-encoding)) is tested by `test/utf16/test-utf16.c`. The CI runs it on Windows (UTF-16) and on Linux (UTF-8): It works in any build, so its expectations can be checked also with the default UTF-8 build:

```sh
$ cc -Isrc test/utf16/test-utf16.c src/org4c.c src/entity.c -o build/test-utf16
$ build/test-utf16
```

There is also a fuzzer (`test/fuzzers/fuzz-orghtml.c`), which can be built with a libFuzzer-capable clang:

```sh
$ clang -g -O1 -fsanitize=fuzzer,address,undefined -Isrc \
        src/org4c.c src/entity.c src/org4c-html.c test/fuzzers/fuzz-orghtml.c -o build/fuzz-orghtml
$ mkdir build/corpus && cp test/fuzzers/seed-corpus/* build/corpus/
$ build/fuzz-orghtml build/corpus
```

(The fuzzer adds the new interesting inputs into the corpus directory, so do not run it on `test/fuzzers/seed-corpus` directly.)

## FAQ

**Q: Does ORG4C perform any input validation?**

**A:** No. As MD4C, ORG4C sees any sequence of bytes as a valid input (garbage in, garbage out): Any ill-formed UTF-8 byte sequence propagates to the respective callback as a part of the text. If you need to validate the input, do so before passing it to `org_parse()`.

**Q: ORG4C's API does not expect/produce zero-terminated strings. Why?**

**A:** For the same reasons as MD4C: correctness (the document may contain `U+0000`, reported as `ORG_TEXT_NULLCHAR`) and performance (most of the strings passed to the callbacks point directly into the input document, without any copying).

## License

ORG4C is covered with MIT license, see the file `LICENSE.txt`.
