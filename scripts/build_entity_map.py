#!/usr/bin/env python3

# Generate the table of Org entities (for src/entity.c) from the variable
# `org-entities' of the Emacs Org mode. (Emacs has to be installed.)

import json
import subprocess
import sys


elisp = r'''
(progn
  (require 'org-entities)
  (require 'json)
  (princ (json-encode
    (delq nil (mapcar (lambda (e)
                        (when (consp e)
                          (list (cons 'name (nth 0 e)) (cons 'html (nth 3 e)))))
                      org-entities)))))
'''

out = subprocess.run(["emacs", "--batch", "--eval", elisp],
                     capture_output=True, check=True).stdout
entities = json.loads(out.decode("utf-8"))


def c_string(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


records = {}
for e in entities:
    if e["name"] not in records:
        records[e["name"]] = e["html"]

sys.stdout.write("static const ORG_ENTITY ENTITY_MAP[] = {\n")
sys.stdout.write(",\n".join("    { " + c_string(name) + ", " + c_string(records[name]) + " }"
                            for name in sorted(records, key=lambda n: n.encode("utf-8"))))
sys.stdout.write("\n};\n")
