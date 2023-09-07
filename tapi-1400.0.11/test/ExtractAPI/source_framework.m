// RUN: rm -rf %t
// RUN: split-file %s %t
// RUN: sed -e "s:DST_DIR:%t:g" %t/input.json.in >> %t/input.json
// RUN: %tapi extractapi -target arm64-apple-macos12 -F%t/Frameworks \
// RUN: %t/input.json -o %t/output.json | FileCheck -allow-empty %s
// RUN: diff -a %t/reference.output.json %t/output.json

// CHECK-NOT: error:
// CHECK-NOT: warning:

//--- Frameworks/Foundation.framework/Headers/Foundation.h
@interface NSString
@end

//--- Frameworks/MyString.framework/Headers/MyString.h
#import <Foundation/Foundation.h>

@interface NSString (MyString)
@end

//--- input.json.in
{
  "version" : "2",
  "headers" : [
    {
      "type" : "public",
      "path" : "DST_DIR/Frameworks/MyString.framework/Headers/MyString.h"
    }
  ]
}

//--- reference.output.json
{
  "Private": [],
  "Project": [],
  "Public": [
    {
      "categories": [
        {
          "USR": "c:objc(cy)NSString@MyString",
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
              "kind": "typeIdentifier",
              "preciseIdentifier": "c:objc(cs)NSString",
              "spelling": "NSString"
            },
            {
              "kind": "text",
              "spelling": " ("
            },
            {
              "kind": "identifier",
              "spelling": "MyString"
            },
            {
              "kind": "text",
              "spelling": ")"
            }
          ],
          "file": "MyString.h",
          "interface": "NSString",
          "interfaceSourceModule": "Foundation",
          "interfaceUSR": "c:objc(cs)NSString",
          "line": 3,
          "name": "MyString",
          "subHeading": [
            {
              "kind": "identifier",
              "spelling": "MyString"
            }
          ]
        }
      ],
      "target": "arm64-apple-macos12"
    }
  ]
}
