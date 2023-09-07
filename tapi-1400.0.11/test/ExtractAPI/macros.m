// RUN: rm -rf %t
// RUN: split-file %s %t
// RUN: sed -e "s:DST_DIR:%t:g" %t/input.json.in >> %t/input.json
// RUN: %tapi extractapi -target arm64-apple-macos12 -F%t/Frameworks \
// RUN: %t/input.json -o %t/output.json | FileCheck -allow-empty %s
// RUN: diff -a %t/reference.output.json %t/output.json

// CHECK-NOT: error:
// CHECK-NOT: warning:

//--- Frameworks/Macros.framework/Headers/Macros.h
#define HELLO 1
#define WORLD 2
#define MACRO_FUN(x) x x
#define FUN(x, y, z) x + y + z
#define FUNC99(x, ...)
#define FUNGNU(x...)


//--- input.json.in
{
  "version" : "2",
  "headers" : [
    {
      "type" : "public",
      "path" : "DST_DIR/Frameworks/Macros.framework/Headers/Macros.h"
    }
  ]
}

//--- reference.output.json
{
  "Private": [],
  "Project": [],
  "Public": [
    {
      "macros": [
        {
          "USR": "c:Macros.h@8@macro@HELLO",
          "access": "public",
          "col": 9,
          "declarationFragments": [
            {
              "kind": "keyword",
              "spelling": "#define"
            },
            {
              "kind": "text",
              "spelling": " "
            },
            {
              "kind": "identifier",
              "spelling": "HELLO"
            }
          ],
          "file": "Macros.h",
          "line": 1,
          "name": "HELLO"
        },
        {
          "USR": "c:Macros.h@24@macro@WORLD",
          "access": "public",
          "col": 9,
          "declarationFragments": [
            {
              "kind": "keyword",
              "spelling": "#define"
            },
            {
              "kind": "text",
              "spelling": " "
            },
            {
              "kind": "identifier",
              "spelling": "WORLD"
            }
          ],
          "file": "Macros.h",
          "line": 2,
          "name": "WORLD"
        },
        {
          "USR": "c:Macros.h@40@macro@MACRO_FUN",
          "access": "public",
          "col": 9,
          "declarationFragments": [
            {
              "kind": "keyword",
              "spelling": "#define"
            },
            {
              "kind": "text",
              "spelling": " "
            },
            {
              "kind": "identifier",
              "spelling": "MACRO_FUN"
            },
            {
              "kind": "text",
              "spelling": "("
            },
            {
              "kind": "internalParam",
              "spelling": "x"
            },
            {
              "kind": "text",
              "spelling": ")"
            }
          ],
          "file": "Macros.h",
          "line": 3,
          "name": "MACRO_FUN"
        },
        {
          "USR": "c:Macros.h@65@macro@FUN",
          "access": "public",
          "col": 9,
          "declarationFragments": [
            {
              "kind": "keyword",
              "spelling": "#define"
            },
            {
              "kind": "text",
              "spelling": " "
            },
            {
              "kind": "identifier",
              "spelling": "FUN"
            },
            {
              "kind": "text",
              "spelling": "("
            },
            {
              "kind": "internalParam",
              "spelling": "x"
            },
            {
              "kind": "text",
              "spelling": ", "
            },
            {
              "kind": "internalParam",
              "spelling": "y"
            },
            {
              "kind": "text",
              "spelling": ", "
            },
            {
              "kind": "internalParam",
              "spelling": "z"
            },
            {
              "kind": "text",
              "spelling": ")"
            }
          ],
          "file": "Macros.h",
          "line": 4,
          "name": "FUN"
        },
        {
          "USR": "c:Macros.h@96@macro@FUNC99",
          "access": "public",
          "col": 9,
          "declarationFragments": [
            {
              "kind": "keyword",
              "spelling": "#define"
            },
            {
              "kind": "text",
              "spelling": " "
            },
            {
              "kind": "identifier",
              "spelling": "FUNC99"
            },
            {
              "kind": "text",
              "spelling": "("
            },
            {
              "kind": "internalParam",
              "spelling": "x"
            },
            {
              "kind": "text",
              "spelling": ", ...)"
            }
          ],
          "file": "Macros.h",
          "line": 5,
          "name": "FUNC99"
        },
        {
          "USR": "c:Macros.h@119@macro@FUNGNU",
          "access": "public",
          "col": 9,
          "declarationFragments": [
            {
              "kind": "keyword",
              "spelling": "#define"
            },
            {
              "kind": "text",
              "spelling": " "
            },
            {
              "kind": "identifier",
              "spelling": "FUNGNU"
            },
            {
              "kind": "text",
              "spelling": "("
            },
            {
              "kind": "internalParam",
              "spelling": "x"
            },
            {
              "kind": "text",
              "spelling": "...)"
            }
          ],
          "file": "Macros.h",
          "line": 6,
          "name": "FUNGNU"
        }
      ],
      "target": "arm64-apple-macos12"
    }
  ]
}
