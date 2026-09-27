#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import itertools
import multiprocessing
import re
import sys
import queue
from prog import Prog
from timeit import default_timer as timer

# list of pairs consisting of input and a regex that must match the output.
pathological = {
    # note - some pythons have limit of 65535 for {num-matches} in re.
    "U+0000":
            ("abc\u0000de\u0000",
            re.compile("abc\ufffd?de\ufffd?")),
    "Windows line endings":
            ("* foo\r\nbar\r\nbaz\r\n",
            re.compile("^<h1>foo</h1>\n<p>bar\nbaz</p>")),
    "U+FEFF (Unicode BOM)":
            ("\ufefffoo",
            re.compile("<p>foo</p>")),
    "nested emphasis":
            (("*a /a " * 65000) + "b" + (" a/ a*" * 65000),
            re.compile(r"^<p><b>a <i>a (\*a /a ){64999}b a</i> a</b>( a/ a\*){64999}</p>")),
    "nested same-kind emphasis":
            (("*" * 65000) + "a" + ("*" * 65000),
            re.compile(r"^<p>(<b>){32}\*+a\*+(</b>){32}</p>")),
    "many emph closers with no openers":
            (("a* " * 65000),
            re.compile(r"(a\* ){64999}a\*")),
    "many emph openers with no closers":
            (("*a " * 65000),
            re.compile(r"(\*a ){64999}\*a")),
    "many verbatim openers with no closers":
            (("=a " * 65000),
            re.compile(r"(=a ){64999}=a")),
    "many link openers with no closers":
            (("[[a" * 65000),
            re.compile(r"(\[\[a){65000}")),
    "many link descriptions with no closers":
            (("[[a][b " * 65000),
            re.compile(r"(\[\[a\]\[b ){64999}")),
    "many links":
            (("[[a][b]] " * 65000),
            re.compile("(<a href=\"#a\">b</a> ){64999}")),
    "many plain links":
            (("https://x.org " * 65000),
            re.compile("(<a href=\"https://x.org\">https://x.org</a> ){64999}")),
    "many hard breaks":
            (("a\\\\\n" * 65000),
            re.compile("(a<br>\n){64999}a</p>")),
    "deeply nested lists":
            ("".join(map(lambda x: ("  " * x + "- a\n"), range(0,1000))),
            re.compile("^<ul>\n(<li>a<ul>\n){999}<li>a</li>\n</ul>\n(</li>\n</ul>\n){999}$")),
    "many headlines":
            (("* a\n" * 65000),
            re.compile("(<h1>a</h1>\n){65000}")),
    "deep headline":
            (("*" * 65000 + " a\n"),
            re.compile("^<h6>a</h6>")),
    "many unterminated blocks":
            (("#+begin_src\nx\n" * 30000),
            re.compile(r"^<p>(#\+begin_src\nx\n){29999}#\+begin_src\nx</p>")),
    "many unterminated blocks of distinct names":
            ("".join(map(lambda x: ("#+begin_x%d\n" % x), range(0,30000))),
            re.compile(r"^<p>(#\+begin_x[0-9]+\n){29999}#\+begin_x[0-9]+</p>")),
    "many block ends of other names":
            (("#+begin_a\n" * 10000 + "#+end_b\n" * 10000),
            re.compile(r"^<p>(#\+begin_a\n){10000}(#\+end_b\n){9999}#\+end_b</p>")),
    "deeply nested blocks":
            (("#+begin_quote\n" * 5000 + "x\n" + "#+end_quote\n" * 5000),
            re.compile("^<blockquote>\n<p>(#\\+begin_quote\n){4999}x</p>\n</blockquote>\n<p>(#\\+end_quote\n){4998}#\\+end_quote</p>")),
    "many drawers without end":
            ((":FOO:\n" * 65000),
            re.compile("^<p>(:FOO:\n){64999}:FOO:</p>")),
    "many drawer ends":
            ((":END:\n" * 65000),
            re.compile("^<p>(:END:\n){64999}:END:</p>")),
    "huge table":
            (("| a " * 1000 + "|\n") * 1000,
            re.compile("^<table>\n<tbody>\n(<tr>\n(<td>a</td>\n){1000}</tr>\n){1000}</tbody>\n</table>")),
    "many \\( openers":
            (("\\(a " * 65000),
            re.compile(r"^<p>(\\\(a ){64999}\\\(a</p>")),
    "many \\( openers and one closer":
            (("\\(a " * 65000 + "\\)"),
            re.compile(r"^<p>\\\(a (\\\(a ){64999}\\\)</p>")),
    "many $$ pairs":
            (("$$a " * 65000),
            re.compile(r"^<p>(\\\[a \\\]a ){32499}\\\[a \\\]a</p>")),
    "many export snippets":
            (("@@a:b " * 65000),
            re.compile(r"^<p>(a:b ){32499}a:b</p>")),
    "many inline footnote openers":
            (("[fn::a " * 65000),
            re.compile(r"^<p>(\[fn::a ){64999}\[fn::a</p>")),
    "deeply nested inline footnotes":
            (("x" + "[fn::" * 30000 + "x" + "]" * 30000),
            re.compile(r"^<p>x<sup><a id=\"fnr.1\" class=\"footref\" href=\"#fn.1\">1</a></sup></p>")),
    "many footnote references":
            (("x" + "".join(map(lambda x: ("[fn:%d]" % x), range(0,65000)))),
            re.compile(r"(<div class=\"footdef\"><sup><a id=\"fn.[0-9]+\" class=\"footnum\" href=\"#fnr.[0-9]+\">[0-9]+</a></sup> <div class=\"footpara\"></div></div>\n){65000}")),
    "many footnote definitions":
            ("".join(map(lambda x: ("[fn:%d] a\n\n\n" % (x % 100)), range(0,65000))) + "x[fn:1]",
            re.compile(r"^<p>x<sup>.*<p>a</p>\n</div></div>\n</div>\n</div>\n$", re.S)),
    "many timestamp openers":
            (("<2024-01-01 " * 65000),
            re.compile(r"^<p>(&lt;2024-01-01 ){64999}&lt;2024-01-01</p>")),
    "many unterminated latex environments":
            (("\\begin{x}\n" * 30000),
            re.compile(r"^<p>(\\begin\{x\}\n){29999}\\begin\{x\}</p>")),
    "many inline source block openers":
            (("src_a{" * 65000),
            re.compile(r"^<p>(src_a\{){65000}</p>")),
    "many subscript openers":
            (("a_{b" * 65000),
            re.compile(r"^<p>(a_\{b){65000}</p>"),
            ["--fsubsuperscripts"]),
    "exponential macro expansion":
            (("#+MACRO: a0 x\n" + "".join(map(lambda x: ("#+MACRO: a%d {{{a%d}}}{{{a%d}}}\n" % (x, x-1, x-1)), range(1,40))) + "{{{a39}}}\n"),
            re.compile(r"^<p>x*</p>")),
    "recursive macro":
            (("#+MACRO: r {{{r}}}{{{r}}}\n{{{r}}}\n"),
            re.compile(r"^<p></p>")),
    "many macro openers":
            (("{{{a(" * 65000),
            re.compile(r"^<p>(\{\{\{a\(){65000}</p>")),
    "many macro calls":
            (("#+MACRO: m <$1>\n" + "{{{m(x)}}} " * 65000),
            re.compile(r"^<p>(&lt;x&gt; ){64999}&lt;x&gt;</p>")),
    "many radio targets":
            (("".join(map(lambda x: ("<<<t%d>>> " % x), range(0,5000))) + "\n\n" + "".join(map(lambda x: ("t%d " % (x % 1000)), range(0,65000)))),
            re.compile(r"(<a href=\"#t[0-9]+\">t[0-9]+</a> ){64999}")),
    "many citation openers":
            (("[cite:@a " * 65000),
            re.compile(r"^<p>(\[cite:@a ){64999}\[cite:@a</p>")),
    "many inline babel call openers":
            (("call_a(" * 65000),
            re.compile(r"^<p>(call_a\(){65000}</p>")),
    "many code reference links":
            (("#+begin_src sh -n -r\nx (ref:a)\n#+end_src\n" + "[[(a)]] " * 65000),
            re.compile(r"(<a href=\"#coderef-a\" class=\"coderef\">1</a> ){64999}")),
    "many code reference labels":
            (("#+begin_src sh\n" + "".join(map(lambda x: ("x (ref:r%d)\n" % x), range(0,30000))) + "#+end_src\n" + "[[(r5)]]"),
            re.compile(r"<a href=\"#coderef-r5\" class=\"coderef\">r5</a>")),
    "many inline tasks":
            (("*************** a\n" * 30000 + "*************** END\n"),
            re.compile(r"^(<div class=\"inlinetask\">\n<b>a</b><br>\n</div>\n){29999}"),
            ["--finlinetasks"]),
    "many figures":
            (("[[file:a.png]]\n\n" * 30000),
            re.compile(r"^(<div class=\"figure\">\n<p><img src=\"a.png\" alt=\"a.png\">\n</p>\n</div>\n){30000}")),
    "many description list separators":
            (("- a" + " :: a" * 30000 + "\n"),
            re.compile("^<dl>\n<dt>a( :: a){29999}</dt>\n<dd>a</dd>")),
    "many attr_html keywords":
            (("#+ATTR_HTML: :a 1 :b 2 :class c\n" * 30000 + "x\n"),
            re.compile("^<p class=\"c\" a=\"1\" b=\"2\">x</p>")),
    "many attr_html attributes":
            (("#+ATTR_HTML:" + " :a b" * 30000 + "\nx\n"),
            re.compile("^<p a=\"b\">x</p>")),
}

whitespace_re = re.compile('/s+/')

def run_tests(args):
    allowed_failures = {}
    TIMEOUT = 5

    q = multiprocessing.Queue()
    passed = []
    errored = []
    failed = []
    ignored = []

    #print("Testing pathological cases:")
    for description in pathological:
        if len(pathological[description]) == 2:
            (inp, regex) = pathological[description]
            prog = Prog(cmdline=args.program)
        else:
            (inp, regex, default_options) = pathological[description]
            prog = Prog(cmdline=args.program, default_options=default_options)

        start = timer()
        p = multiprocessing.Process(
            target=q.put(prog.to_html(inp)),
            args=(q, inp, args.program))
        end = timer()
        p.start()
        try:
            # wait TIMEOUT seconds or until it finishes
            rc, actual, err = q.get(True, TIMEOUT)
            p.join()
            if rc != 0:
                print(description, '[ERRORED (return code %d)]' %rc)
                print(err)
                if description in allowed_failures:
                    ignored.append(description)
                else:
                    errored.append(description)
            elif regex.search(actual):
                print('{:35} [PASSED] {:.3f} secs'.format(description, end-start))
                passed.append(description)
            else:
                print(description, '[FAILED]')
                print(repr(actual[:60]))
                if description in allowed_failures:
                    ignored.append(description)
                else:
                    failed.append(description)
        except queue.Empty:
            p.terminate()
            p.join()
            print(description, '[TIMEOUT]')
            if description in allowed_failures:
                ignored.append(description)
            else:
                errored.append(description)

    print("%d passed, %d failed, %d errored" %
          (len(passed), len(failed), len(errored)))
    if ignored:
        print("Ignoring these allowed failures:")
        for x in ignored:
            print(x)
    if failed or errored:
        exit(1)
    else:
        exit(0)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Run Org tests.')
    parser.add_argument('-p', '--program', dest='program', nargs='?', default=None,
                    help='program to test')
    args = parser.parse_args(sys.argv[1:])
    run_tests(args)
