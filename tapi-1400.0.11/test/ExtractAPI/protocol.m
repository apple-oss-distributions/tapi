// RUN: rm -rf %t
// RUN: split-file %s %t
// RUN: sed -e "s:DST_DIR:%t:g" %t/input.json.in >> %t/input.json
// RUN: %tapi extractapi -target arm64-apple-macos12 -F%t/Frameworks \
// RUN: %t/input.json -o %t/output.json | FileCheck -allow-empty %s
// RUN: diff -a %t/reference.output.json %t/output.json

// CHECK-NOT: error:
// CHECK-NOT: warning:

//--- Frameworks/Super.framework/Headers/Super.h
@protocol SuperNewAPIs
@end

@protocol AwesomeNewAPIs <SuperNewAPIs>
@end

@interface Super <AwesomeNewAPIs>
@end

//--- input.json.in
{
  "version" : "2",
  "headers" : [
    {
      "type" : "public",
      "path" : "DST_DIR/Frameworks/Super.framework/Headers/Super.h"
    }
  ]
}

//--- reference.output.json
{
  "Private": [],
  "Project": [],
  "Public": [
    {
      "interfaces": [
        {
          "USR": "c:objc(cs)Super",
          "access": "public",
          "col": 12,
          "declarationFragments": [
            {
              "kind": "keyword",
              "spelling": "@interface"
            },
            {
              "kind": "text",
              "spelling": " "
            },
            {
              "kind": "identifier",
              "spelling": "Super"
            }
          ],
          "file": "Super.h",
          "line": 7,
          "linkage": "exported",
          "name": "Super",
          "protocols": [
            {
              "USR": "c:objc(pl)AwesomeNewAPIs",
              "name": "AwesomeNewAPIs",
              "sourceModule": "Super"
            }
          ],
          "subHeading": [
            {
              "kind": "identifier",
              "spelling": "Super"
            }
          ],
          "super": ""
        }
      ],
      "protocols": [
        {
          "USR": "c:objc(pl)SuperNewAPIs",
          "access": "public",
          "col": 11,
          "declarationFragments": [
            {
              "kind": "keyword",
              "spelling": "@protocol"
            },
            {
              "kind": "text",
              "spelling": " "
            },
            {
              "kind": "identifier",
              "spelling": "SuperNewAPIs"
            }
          ],
          "file": "Super.h",
          "line": 1,
          "name": "SuperNewAPIs",
          "subHeading": [
            {
              "kind": "identifier",
              "spelling": "SuperNewAPIs"
            }
          ]
        },
        {
          "USR": "c:objc(pl)AwesomeNewAPIs",
          "access": "public",
          "col": 11,
          "declarationFragments": [
            {
              "kind": "keyword",
              "spelling": "@protocol"
            },
            {
              "kind": "text",
              "spelling": " "
            },
            {
              "kind": "identifier",
              "spelling": "AwesomeNewAPIs"
            },
            {
              "kind": "text",
              "spelling": "<"
            },
            {
              "kind": "keyword",
              "spelling": "SuperNewAPIs"
            },
            {
              "kind": "text",
              "spelling": ">"
            }
          ],
          "file": "Super.h",
          "line": 4,
          "name": "AwesomeNewAPIs",
          "protocols": [
            {
              "USR": "c:objc(pl)SuperNewAPIs",
              "name": "SuperNewAPIs",
              "sourceModule": "Super"
            }
          ],
          "subHeading": [
            {
              "kind": "identifier",
              "spelling": "AwesomeNewAPIs"
            }
          ]
        }
      ],
      "target": "arm64-apple-macos12"
    }
  ]
}
