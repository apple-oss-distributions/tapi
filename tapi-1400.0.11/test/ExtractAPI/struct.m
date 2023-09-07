// RUN: rm -rf %t
// RUN: split-file %s %t
// RUN: sed -e "s:DST_DIR:%t:g" %t/input.json.in >> %t/input.json
// RUN: %tapi extractapi -target arm64-apple-macos12 -F%t/Frameworks \
// RUN: %t/input.json -o %t/output.json | FileCheck -allow-empty %s
// RUN: diff -a %t/reference.output.json %t/output.json

// CHECK-NOT: error:
// CHECK-NOT: warning:

//--- Frameworks/Struct.framework/Headers/Struct.h
struct Color {
  unsigned red;
  unsigned green;
  unsigned blue;
  unsigned alpha;
};

typedef struct _Location {
  unsigned line;
  unsigned col;
} Location;

//--- input.json.in
{
  "version" : "2",
  "headers" : [
    {
      "type" : "public",
      "path" : "DST_DIR/Frameworks/Struct.framework/Headers/Struct.h"
    }
  ]
}

//--- reference.output.json
{
  "Private": [],
  "Project": [],
  "Public": [
    {
      "structs": [
        {
          "USR": "c:@S@Color",
          "access": "public",
          "col": 8,
          "declarationFragments": [
            {
              "kind": "keyword",
              "spelling": "struct"
            },
            {
              "kind": "text",
              "spelling": " "
            },
            {
              "kind": "identifier",
              "spelling": "Color"
            }
          ],
          "fields": [
            {
              "USR": "c:@S@Color@FI@red",
              "access": "public",
              "col": 12,
              "declarationFragments": [
                {
                  "kind": "typeIdentifier",
                  "preciseIdentifier": "c:i",
                  "spelling": "unsigned int"
                },
                {
                  "kind": "text",
                  "spelling": " "
                },
                {
                  "kind": "identifier",
                  "spelling": "red"
                }
              ],
              "file": "Struct.h",
              "line": 2,
              "name": "red",
              "subHeading": [
                {
                  "kind": "identifier",
                  "spelling": "red"
                }
              ]
            },
            {
              "USR": "c:@S@Color@FI@green",
              "access": "public",
              "col": 12,
              "declarationFragments": [
                {
                  "kind": "typeIdentifier",
                  "preciseIdentifier": "c:i",
                  "spelling": "unsigned int"
                },
                {
                  "kind": "text",
                  "spelling": " "
                },
                {
                  "kind": "identifier",
                  "spelling": "green"
                }
              ],
              "file": "Struct.h",
              "line": 3,
              "name": "green",
              "subHeading": [
                {
                  "kind": "identifier",
                  "spelling": "green"
                }
              ]
            },
            {
              "USR": "c:@S@Color@FI@blue",
              "access": "public",
              "col": 12,
              "declarationFragments": [
                {
                  "kind": "typeIdentifier",
                  "preciseIdentifier": "c:i",
                  "spelling": "unsigned int"
                },
                {
                  "kind": "text",
                  "spelling": " "
                },
                {
                  "kind": "identifier",
                  "spelling": "blue"
                }
              ],
              "file": "Struct.h",
              "line": 4,
              "name": "blue",
              "subHeading": [
                {
                  "kind": "identifier",
                  "spelling": "blue"
                }
              ]
            },
            {
              "USR": "c:@S@Color@FI@alpha",
              "access": "public",
              "col": 12,
              "declarationFragments": [
                {
                  "kind": "typeIdentifier",
                  "preciseIdentifier": "c:i",
                  "spelling": "unsigned int"
                },
                {
                  "kind": "text",
                  "spelling": " "
                },
                {
                  "kind": "identifier",
                  "spelling": "alpha"
                }
              ],
              "file": "Struct.h",
              "line": 5,
              "name": "alpha",
              "subHeading": [
                {
                  "kind": "identifier",
                  "spelling": "alpha"
                }
              ]
            }
          ],
          "file": "Struct.h",
          "line": 1,
          "name": "Color",
          "subHeading": [
            {
              "kind": "identifier",
              "spelling": "Color"
            }
          ]
        },
        {
          "USR": "c:@S@_Location",
          "access": "public",
          "col": 16,
          "declarationFragments": [
            {
              "kind": "keyword",
              "spelling": "struct"
            },
            {
              "kind": "text",
              "spelling": " "
            },
            {
              "kind": "identifier",
              "spelling": "_Location"
            }
          ],
          "fields": [
            {
              "USR": "c:@S@_Location@FI@line",
              "access": "public",
              "col": 12,
              "declarationFragments": [
                {
                  "kind": "typeIdentifier",
                  "preciseIdentifier": "c:i",
                  "spelling": "unsigned int"
                },
                {
                  "kind": "text",
                  "spelling": " "
                },
                {
                  "kind": "identifier",
                  "spelling": "line"
                }
              ],
              "file": "Struct.h",
              "line": 9,
              "name": "line",
              "subHeading": [
                {
                  "kind": "identifier",
                  "spelling": "line"
                }
              ]
            },
            {
              "USR": "c:@S@_Location@FI@col",
              "access": "public",
              "col": 12,
              "declarationFragments": [
                {
                  "kind": "typeIdentifier",
                  "preciseIdentifier": "c:i",
                  "spelling": "unsigned int"
                },
                {
                  "kind": "text",
                  "spelling": " "
                },
                {
                  "kind": "identifier",
                  "spelling": "col"
                }
              ],
              "file": "Struct.h",
              "line": 10,
              "name": "col",
              "subHeading": [
                {
                  "kind": "identifier",
                  "spelling": "col"
                }
              ]
            }
          ],
          "file": "Struct.h",
          "line": 8,
          "name": "_Location",
          "subHeading": [
            {
              "kind": "identifier",
              "spelling": "_Location"
            }
          ]
        }
      ],
      "target": "arm64-apple-macos12",
      "typedefs": [
        {
          "USR": "c:Struct.h@T@Location",
          "access": "public",
          "col": 3,
          "declarationFragments": [
            {
              "kind": "keyword",
              "spelling": "typedef"
            },
            {
              "kind": "text",
              "spelling": " "
            },
            {
              "kind": "keyword",
              "spelling": "struct"
            },
            {
              "kind": "text",
              "spelling": " "
            },
            {
              "kind": "typeIdentifier",
              "preciseIdentifier": "c:@S@_Location",
              "spelling": "_Location"
            },
            {
              "kind": "text",
              "spelling": " "
            },
            {
              "kind": "identifier",
              "spelling": "Location"
            }
          ],
          "file": "Struct.h",
          "line": 11,
          "name": "Location",
          "subHeading": [
            {
              "kind": "identifier",
              "spelling": "Location"
            }
          ],
          "type": "_Location",
          "typeSourceModule": "Struct",
          "typeUSR": "c:@S@_Location"
        }
      ]
    }
  ]
}
