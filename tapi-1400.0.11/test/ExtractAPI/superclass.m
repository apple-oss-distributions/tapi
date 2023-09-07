// RUN: rm -rf %t
// RUN: split-file %s %t
// RUN: sed -e "s:DST_DIR:%t:g" %t/input.json.in >> %t/input.json
// RUN: %tapi extractapi -target arm64-apple-macos12 -F%t/Frameworks \
// RUN: %t/input.json -o %t/output.json | FileCheck -allow-empty %s
// RUN: diff -a %t/reference.output.json %t/output.json

// CHECK-NOT: error:
// CHECK-NOT: warning:

//--- Frameworks/Super.framework/Headers/Super.h
@interface NSObject
@end

@interface Super : NSObject
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
          "USR": "c:objc(cs)NSObject",
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
              "spelling": "NSObject"
            }
          ],
          "file": "Super.h",
          "line": 1,
          "linkage": "exported",
          "name": "NSObject",
          "subHeading": [
            {
              "kind": "identifier",
              "spelling": "NSObject"
            }
          ],
          "super": ""
        },
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
            },
            {
              "kind": "text",
              "spelling": " : "
            },
            {
              "kind": "typeIdentifier",
              "preciseIdentifier": "c:objc(cs)NSObject",
              "spelling": "NSObject"
            }
          ],
          "file": "Super.h",
          "line": 4,
          "linkage": "exported",
          "name": "Super",
          "subHeading": [
            {
              "kind": "identifier",
              "spelling": "Super"
            }
          ],
          "super": "NSObject",
          "superSourceModule": "Super",
          "superUSR": "c:objc(cs)NSObject"
        }
      ],
      "target": "arm64-apple-macos12"
    }
  ]
}
