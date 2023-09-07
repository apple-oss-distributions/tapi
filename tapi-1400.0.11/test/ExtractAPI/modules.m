// RUN: rm -rf %t
// RUN: split-file %s %t
// RUN: sed -e "s:DST_DIR:%t:g" %t/input.json.in >> %t/input.json
// RUN: %tapi extractapi -target arm64-apple-macos12 -F%t/Frameworks \
// RUN: %t/input.json -o %t/output.json -fmodules -fmodules-cache-path=%t/ModuleCache | FileCheck -allow-empty %s
// RUN: diff -a %t/reference.output.json %t/output.json

// CHECK-NOT: error:
// CHECK-NOT: warning:

//--- Frameworks/Ant.framework/Headers/Ant.h
@interface Ant
@end

//--- Frameworks/Hill.framework/Headers/Hill.h
@interface Hill
@end

//--- Frameworks/Hill.framework/Modules/module.modulemap
framework module Hill {
  umbrella header "Hill.h"

  export *
  module * { export * }
}

//--- src/Queen.h
@import Hill;

@interface Queen
@end

@interface Hill (HillExt)
@end

//--- input.json.in
{
  "version" : "2",
  "headers" : [
    {
      "type" : "public",
      "path" : "DST_DIR/Frameworks/Ant.framework/Headers/Ant.h"
    },
    {
      "type" : "project",
      "path" : "DST_DIR/src/Queen.h"
    }
  ]
}

//--- reference.output.json
{
  "Private": [],
  "Project": [
    {
      "categories": [
        {
          "USR": "c:objc(cy)Hill@HillExt",
          "access": "project",
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
              "kind": "typeIdentifier",
              "preciseIdentifier": "c:objc(cs)Hill",
              "spelling": "Hill"
            },
            {
              "kind": "text",
              "spelling": " ("
            },
            {
              "kind": "identifier",
              "spelling": "HillExt"
            },
            {
              "kind": "text",
              "spelling": ")"
            }
          ],
          "file": "Queen.h",
          "interface": "Hill",
          "interfaceSourceModule": "Hill",
          "interfaceUSR": "c:objc(cs)Hill",
          "line": 6,
          "name": "HillExt",
          "subHeading": [
            {
              "kind": "identifier",
              "spelling": "HillExt"
            }
          ]
        }
      ],
      "interfaces": [
        {
          "USR": "c:objc(cs)Queen",
          "access": "project",
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
              "spelling": "Queen"
            }
          ],
          "file": "Queen.h",
          "line": 3,
          "linkage": "exported",
          "name": "Queen",
          "subHeading": [
            {
              "kind": "identifier",
              "spelling": "Queen"
            }
          ],
          "super": ""
        }
      ],
      "target": "arm64-apple-macos12"
    }
  ],
  "Public": [
    {
      "interfaces": [
        {
          "USR": "c:objc(cs)Ant",
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
              "spelling": "Ant"
            }
          ],
          "file": "Ant.h",
          "line": 1,
          "linkage": "exported",
          "name": "Ant",
          "subHeading": [
            {
              "kind": "identifier",
              "spelling": "Ant"
            }
          ],
          "super": ""
        }
      ],
      "target": "arm64-apple-macos12"
    }
  ]
}
