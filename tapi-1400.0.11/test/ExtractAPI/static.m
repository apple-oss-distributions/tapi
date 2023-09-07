// RUN: rm -rf %t
// RUN: split-file %s %t
// RUN: sed -e "s:DST_DIR:%t:g" %t/input.json.in >> %t/input.json
// RUN: %tapi extractapi -target arm64-apple-macos12 -F%t/Frameworks \
// RUN: %t/input.json -o %t/output.json | FileCheck -allow-empty %s
// RUN: diff -a %t/reference.output.json %t/output.json

// CHECK-NOT: error:
// CHECK-NOT: warning:

//--- Frameworks/Static.framework/Headers/Static.h
static const int constantGlobalVariable = 1;
__attribute__((visibility("hidden"))) const int hiddenGlobalVariable = 1;

static int fun1() {
  return 0;
}

extern int fun2() __attribute__((visibility("hidden")));

//--- input.json.in
{
  "version" : "2",
  "headers" : [
    {
      "type" : "public",
      "path" : "DST_DIR/Frameworks/Static.framework/Headers/Static.h"
    }
  ]
}

//--- reference.output.json
{
  "Private": [],
  "Project": [],
  "Public": [
    {
      "globals": [
        {
          "USR": "c:Static.h@constantGlobalVariable",
          "access": "public",
          "col": 18,
          "declName": "constantGlobalVariable",
          "declarationFragments": [
            {
              "kind": "keyword",
              "spelling": "static"
            },
            {
              "kind": "text",
              "spelling": " "
            },
            {
              "kind": "keyword",
              "spelling": "const"
            },
            {
              "kind": "text",
              "spelling": " "
            },
            {
              "kind": "typeIdentifier",
              "preciseIdentifier": "c:I",
              "spelling": "int"
            },
            {
              "kind": "text",
              "spelling": " "
            },
            {
              "kind": "identifier",
              "spelling": "constantGlobalVariable"
            }
          ],
          "file": "Static.h",
          "kind": "variable",
          "line": 1,
          "linkage": "internal",
          "name": "_constantGlobalVariable",
          "subHeading": [
            {
              "kind": "identifier",
              "spelling": "constantGlobalVariable"
            }
          ]
        },
        {
          "USR": "c:@hiddenGlobalVariable",
          "access": "public",
          "col": 49,
          "declName": "hiddenGlobalVariable",
          "declarationFragments": [
            {
              "kind": "keyword",
              "spelling": "const"
            },
            {
              "kind": "text",
              "spelling": " "
            },
            {
              "kind": "typeIdentifier",
              "preciseIdentifier": "c:I",
              "spelling": "int"
            },
            {
              "kind": "text",
              "spelling": " "
            },
            {
              "kind": "identifier",
              "spelling": "hiddenGlobalVariable"
            }
          ],
          "file": "Static.h",
          "kind": "variable",
          "line": 2,
          "linkage": "internal",
          "name": "_hiddenGlobalVariable",
          "subHeading": [
            {
              "kind": "identifier",
              "spelling": "hiddenGlobalVariable"
            }
          ]
        },
        {
          "USR": "c:Static.h@F@fun1",
          "access": "public",
          "col": 12,
          "declName": "fun1",
          "declarationFragments": [
            {
              "kind": "keyword",
              "spelling": "static"
            },
            {
              "kind": "text",
              "spelling": " "
            },
            {
              "kind": "typeIdentifier",
              "preciseIdentifier": "c:I",
              "spelling": "int"
            },
            {
              "kind": "text",
              "spelling": " "
            },
            {
              "kind": "identifier",
              "spelling": "fun1"
            },
            {
              "kind": "text",
              "spelling": "()"
            }
          ],
          "file": "Static.h",
          "functionSignature": {
            "returns": [
              {
                "kind": "typeIdentifier",
                "preciseIdentifier": "c:I",
                "spelling": "int"
              }
            ]
          },
          "kind": "function",
          "line": 4,
          "linkage": "internal",
          "name": "_fun1",
          "subHeading": [
            {
              "kind": "identifier",
              "spelling": "fun1"
            }
          ]
        },
        {
          "USR": "c:@F@fun2",
          "access": "public",
          "col": 12,
          "declName": "fun2",
          "declarationFragments": [
            {
              "kind": "keyword",
              "spelling": "extern"
            },
            {
              "kind": "text",
              "spelling": " "
            },
            {
              "kind": "typeIdentifier",
              "preciseIdentifier": "c:I",
              "spelling": "int"
            },
            {
              "kind": "text",
              "spelling": " "
            },
            {
              "kind": "identifier",
              "spelling": "fun2"
            },
            {
              "kind": "text",
              "spelling": "()"
            }
          ],
          "file": "Static.h",
          "functionSignature": {
            "returns": [
              {
                "kind": "typeIdentifier",
                "preciseIdentifier": "c:I",
                "spelling": "int"
              }
            ]
          },
          "kind": "function",
          "line": 8,
          "linkage": "internal",
          "name": "_fun2",
          "subHeading": [
            {
              "kind": "identifier",
              "spelling": "fun2"
            }
          ]
        }
      ],
      "target": "arm64-apple-macos12"
    }
  ]
}
