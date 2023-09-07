// RUN: rm -rf %t
// RUN: split-file %s %t
// RUN: sed -e "s:DST_DIR:%t:g" %t/input.json.in >> %t/input.json
// RUN: %tapi extractapi -target arm64-apple-ios15-macabi -F%t/Frameworks \
// RUN: %t/input.json -o %t/output.json | FileCheck -allow-empty %s
// RUN: diff -a %t/reference.output.json %t/output.json

// CHECK-NOT: error:
// CHECK-NOT: warning:

//--- Frameworks/DeclarationFragments.framework/Headers/DeclarationFragments.h
typedef enum {
  Red,
  Orange,
  Yellow,
} Color;

extern Color color;

extern Color getColor(unsigned r, unsigned g, unsigned b);

@interface Hat
@property(readonly, getter=getSize) unsigned size;
@property(readwrite, setter=setColor:) Color color;
+ (id)getWithSize:(unsigned)size andColor:(Color)color;
@end

@interface Beret : Hat {
  unsigned serialNum;
}
@property(weak, nonatomic, assign, nullable) Hat *weak_nonatomic_assign_nullable
  __attribute__((availability(ios, unavailable)));
@property(strong, atomic, retain, nonnull) Hat *strong_atomic_retain_nonnull;
@property(class, direct, copy, null_resettable) Hat *class_direct_copy_null_resettable;
- (unsigned)getSerialNum;
@end

@interface Hat (BrimlessHat)
@end

@protocol Wearable
@end

//--- input.json.in
{
  "version" : "2",
  "headers" : [
    {
      "type" : "public",
      "path" : "DST_DIR/Frameworks/DeclarationFragments.framework/Headers/DeclarationFragments.h"
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
          "USR": "c:objc(cy)Hat@BrimlessHat",
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
              "preciseIdentifier": "c:objc(cs)Hat",
              "spelling": "Hat"
            },
            {
              "kind": "text",
              "spelling": " ("
            },
            {
              "kind": "identifier",
              "spelling": "BrimlessHat"
            },
            {
              "kind": "text",
              "spelling": ")"
            }
          ],
          "file": "DeclarationFragments.h",
          "interface": "Hat",
          "interfaceSourceModule": "DeclarationFragments",
          "interfaceUSR": "c:objc(cs)Hat",
          "line": 27,
          "name": "BrimlessHat",
          "subHeading": [
            {
              "kind": "identifier",
              "spelling": "BrimlessHat"
            }
          ]
        }
      ],
      "enums": [
        {
          "USR": "c:@EA@Color",
          "access": "public",
          "col": 9,
          "constants": [
            {
              "USR": "c:@EA@Color@Red",
              "access": "public",
              "col": 3,
              "declarationFragments": [
                {
                  "kind": "identifier",
                  "spelling": "Red"
                }
              ],
              "file": "DeclarationFragments.h",
              "line": 2,
              "name": "Red",
              "subHeading": [
                {
                  "kind": "identifier",
                  "spelling": "Red"
                }
              ]
            },
            {
              "USR": "c:@EA@Color@Orange",
              "access": "public",
              "col": 3,
              "declarationFragments": [
                {
                  "kind": "identifier",
                  "spelling": "Orange"
                }
              ],
              "file": "DeclarationFragments.h",
              "line": 3,
              "name": "Orange",
              "subHeading": [
                {
                  "kind": "identifier",
                  "spelling": "Orange"
                }
              ]
            },
            {
              "USR": "c:@EA@Color@Yellow",
              "access": "public",
              "col": 3,
              "declarationFragments": [
                {
                  "kind": "identifier",
                  "spelling": "Yellow"
                }
              ],
              "file": "DeclarationFragments.h",
              "line": 4,
              "name": "Yellow",
              "subHeading": [
                {
                  "kind": "identifier",
                  "spelling": "Yellow"
                }
              ]
            }
          ],
          "declName": "Color",
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
              "spelling": "enum"
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
          "file": "DeclarationFragments.h",
          "line": 1,
          "name": "(anonymous)"
        }
      ],
      "globals": [
        {
          "USR": "c:@color",
          "access": "public",
          "col": 14,
          "declName": "color",
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
              "preciseIdentifier": "c:@EA@Color",
              "spelling": "Color"
            },
            {
              "kind": "text",
              "spelling": " "
            },
            {
              "kind": "identifier",
              "spelling": "color"
            }
          ],
          "file": "DeclarationFragments.h",
          "kind": "variable",
          "line": 7,
          "linkage": "exported",
          "name": "_color",
          "subHeading": [
            {
              "kind": "identifier",
              "spelling": "color"
            }
          ]
        },
        {
          "USR": "c:@F@getColor",
          "access": "public",
          "col": 14,
          "declName": "getColor",
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
              "preciseIdentifier": "c:@EA@Color",
              "spelling": "Color"
            },
            {
              "kind": "text",
              "spelling": " "
            },
            {
              "kind": "identifier",
              "spelling": "getColor"
            },
            {
              "kind": "text",
              "spelling": "("
            },
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
              "kind": "internalParam",
              "spelling": "r"
            },
            {
              "kind": "text",
              "spelling": ", "
            },
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
              "kind": "internalParam",
              "spelling": "g"
            },
            {
              "kind": "text",
              "spelling": ", "
            },
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
              "kind": "internalParam",
              "spelling": "b"
            },
            {
              "kind": "text",
              "spelling": ")"
            }
          ],
          "file": "DeclarationFragments.h",
          "functionSignature": {
            "parameters": [
              {
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
                    "kind": "internalParam",
                    "spelling": "r"
                  }
                ],
                "name": "r"
              },
              {
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
                    "kind": "internalParam",
                    "spelling": "g"
                  }
                ],
                "name": "g"
              },
              {
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
                    "kind": "internalParam",
                    "spelling": "b"
                  }
                ],
                "name": "b"
              }
            ],
            "returns": [
              {
                "kind": "typeIdentifier",
                "preciseIdentifier": "c:@EA@Color",
                "spelling": "Color"
              }
            ]
          },
          "kind": "function",
          "line": 9,
          "linkage": "exported",
          "name": "_getColor",
          "subHeading": [
            {
              "kind": "identifier",
              "spelling": "getColor"
            }
          ]
        }
      ],
      "interfaces": [
        {
          "USR": "c:objc(cs)Hat",
          "access": "public",
          "categories": [
            "BrimlessHat"
          ],
          "classMethods": [
            {
              "USR": "c:objc(cs)Hat(cm)getWithSize:andColor:",
              "access": "public",
              "col": 1,
              "declarationFragments": [
                {
                  "kind": "text",
                  "spelling": "+ ("
                },
                {
                  "kind": "keyword",
                  "spelling": "id"
                },
                {
                  "kind": "text",
                  "spelling": ")"
                },
                {
                  "kind": "identifier",
                  "spelling": "getWithSize"
                },
                {
                  "kind": "text",
                  "spelling": ":"
                },
                {
                  "kind": "text",
                  "spelling": "("
                },
                {
                  "kind": "typeIdentifier",
                  "preciseIdentifier": "c:i",
                  "spelling": "unsigned int"
                },
                {
                  "kind": "text",
                  "spelling": ")"
                },
                {
                  "kind": "internalParam",
                  "spelling": "size"
                },
                {
                  "kind": "text",
                  "spelling": " "
                },
                {
                  "kind": "externalParam",
                  "spelling": "andColor"
                },
                {
                  "kind": "text",
                  "spelling": ":"
                },
                {
                  "kind": "text",
                  "spelling": "("
                },
                {
                  "kind": "typeIdentifier",
                  "preciseIdentifier": "c:@EA@Color",
                  "spelling": "Color"
                },
                {
                  "kind": "text",
                  "spelling": ")"
                },
                {
                  "kind": "internalParam",
                  "spelling": "color"
                }
              ],
              "file": "DeclarationFragments.h",
              "functionSignature": {
                "parameters": [
                  {
                    "declarationFragments": [
                      {
                        "kind": "text",
                        "spelling": "("
                      },
                      {
                        "kind": "typeIdentifier",
                        "preciseIdentifier": "c:i",
                        "spelling": "unsigned int"
                      },
                      {
                        "kind": "text",
                        "spelling": ")"
                      },
                      {
                        "kind": "internalParam",
                        "spelling": "size"
                      }
                    ],
                    "name": "size"
                  },
                  {
                    "declarationFragments": [
                      {
                        "kind": "text",
                        "spelling": "("
                      },
                      {
                        "kind": "typeIdentifier",
                        "preciseIdentifier": "c:@EA@Color",
                        "spelling": "Color"
                      },
                      {
                        "kind": "text",
                        "spelling": ")"
                      },
                      {
                        "kind": "internalParam",
                        "spelling": "color"
                      }
                    ],
                    "name": "color"
                  }
                ],
                "returns": [
                  {
                    "kind": "keyword",
                    "spelling": "id"
                  }
                ]
              },
              "line": 14,
              "name": "getWithSize:andColor:",
              "subHeading": [
                {
                  "kind": "text",
                  "spelling": "+ "
                },
                {
                  "kind": "identifier",
                  "spelling": "getWithSize:andColor:"
                }
              ]
            }
          ],
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
              "spelling": "Hat"
            }
          ],
          "file": "DeclarationFragments.h",
          "line": 11,
          "linkage": "exported",
          "name": "Hat",
          "properties": [
            {
              "USR": "c:objc(cs)Hat(py)size",
              "access": "public",
              "attr": [
                "readonly"
              ],
              "col": 46,
              "declarationFragments": [
                {
                  "kind": "keyword",
                  "spelling": "@property"
                },
                {
                  "kind": "text",
                  "spelling": " ("
                },
                {
                  "kind": "keyword",
                  "spelling": "atomic"
                },
                {
                  "kind": "text",
                  "spelling": ", "
                },
                {
                  "kind": "keyword",
                  "spelling": "readonly"
                },
                {
                  "kind": "text",
                  "spelling": ", "
                },
                {
                  "kind": "keyword",
                  "spelling": "getter"
                },
                {
                  "kind": "text",
                  "spelling": "="
                },
                {
                  "kind": "identifier",
                  "spelling": "getSize"
                },
                {
                  "kind": "text",
                  "spelling": ")"
                },
                {
                  "kind": "typeIdentifier",
                  "preciseIdentifier": "c:i",
                  "spelling": "unsigned int"
                },
                {
                  "kind": "identifier",
                  "spelling": "size"
                }
              ],
              "file": "DeclarationFragments.h",
              "getter": "getSize",
              "line": 12,
              "name": "size",
              "subHeading": [
                {
                  "kind": "identifier",
                  "spelling": "size"
                }
              ]
            },
            {
              "USR": "c:objc(cs)Hat(py)color",
              "access": "public",
              "col": 46,
              "declarationFragments": [
                {
                  "kind": "keyword",
                  "spelling": "@property"
                },
                {
                  "kind": "text",
                  "spelling": " ("
                },
                {
                  "kind": "keyword",
                  "spelling": "atomic"
                },
                {
                  "kind": "text",
                  "spelling": ", "
                },
                {
                  "kind": "keyword",
                  "spelling": "assign"
                },
                {
                  "kind": "text",
                  "spelling": ", "
                },
                {
                  "kind": "keyword",
                  "spelling": "unsafe_unretained"
                },
                {
                  "kind": "text",
                  "spelling": ", "
                },
                {
                  "kind": "keyword",
                  "spelling": "readwrite"
                },
                {
                  "kind": "text",
                  "spelling": ", "
                },
                {
                  "kind": "keyword",
                  "spelling": "setter"
                },
                {
                  "kind": "text",
                  "spelling": "="
                },
                {
                  "kind": "identifier",
                  "spelling": "setColor:"
                },
                {
                  "kind": "text",
                  "spelling": ")"
                },
                {
                  "kind": "typeIdentifier",
                  "preciseIdentifier": "c:@EA@Color",
                  "spelling": "Color"
                },
                {
                  "kind": "identifier",
                  "spelling": "color"
                }
              ],
              "file": "DeclarationFragments.h",
              "getter": "color",
              "line": 13,
              "name": "color",
              "setter": "setColor:",
              "subHeading": [
                {
                  "kind": "identifier",
                  "spelling": "color"
                }
              ]
            }
          ],
          "subHeading": [
            {
              "kind": "identifier",
              "spelling": "Hat"
            }
          ],
          "super": ""
        },
        {
          "USR": "c:objc(cs)Beret",
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
              "spelling": "Beret"
            },
            {
              "kind": "text",
              "spelling": " : "
            },
            {
              "kind": "typeIdentifier",
              "preciseIdentifier": "c:objc(cs)Hat",
              "spelling": "Hat"
            }
          ],
          "file": "DeclarationFragments.h",
          "instanceMethods": [
            {
              "USR": "c:objc(cs)Beret(im)getSerialNum",
              "access": "public",
              "col": 1,
              "declarationFragments": [
                {
                  "kind": "text",
                  "spelling": "- ("
                },
                {
                  "kind": "typeIdentifier",
                  "preciseIdentifier": "c:i",
                  "spelling": "unsigned int"
                },
                {
                  "kind": "text",
                  "spelling": ")"
                },
                {
                  "kind": "identifier",
                  "spelling": "getSerialNum"
                }
              ],
              "file": "DeclarationFragments.h",
              "functionSignature": {
                "returns": [
                  {
                    "kind": "typeIdentifier",
                    "preciseIdentifier": "c:i",
                    "spelling": "unsigned int"
                  }
                ]
              },
              "line": 24,
              "name": "getSerialNum",
              "subHeading": [
                {
                  "kind": "text",
                  "spelling": "- "
                },
                {
                  "kind": "identifier",
                  "spelling": "getSerialNum"
                }
              ]
            }
          ],
          "ivars": [
            {
              "USR": "c:objc(cs)Beret@serialNum",
              "access": "public",
              "accessControl": "protected",
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
                  "spelling": "serialNum"
                }
              ],
              "file": "DeclarationFragments.h",
              "line": 18,
              "linkage": "exported",
              "name": "serialNum",
              "subHeading": [
                {
                  "kind": "identifier",
                  "spelling": "serialNum"
                }
              ]
            }
          ],
          "line": 17,
          "linkage": "exported",
          "name": "Beret",
          "properties": [
            {
              "USR": "c:objc(cs)Beret(py)weak_nonatomic_assign_nullable",
              "access": "public",
              "col": 51,
              "declarationFragments": [
                {
                  "kind": "keyword",
                  "spelling": "@property"
                },
                {
                  "kind": "text",
                  "spelling": " ("
                },
                {
                  "kind": "keyword",
                  "spelling": "nonatomic"
                },
                {
                  "kind": "text",
                  "spelling": ", "
                },
                {
                  "kind": "keyword",
                  "spelling": "assign"
                },
                {
                  "kind": "text",
                  "spelling": ", "
                },
                {
                  "kind": "keyword",
                  "spelling": "weak"
                },
                {
                  "kind": "text",
                  "spelling": ", "
                },
                {
                  "kind": "keyword",
                  "spelling": "unsafe_unretained"
                },
                {
                  "kind": "text",
                  "spelling": ", "
                },
                {
                  "kind": "keyword",
                  "spelling": "readwrite"
                },
                {
                  "kind": "text",
                  "spelling": ", "
                },
                {
                  "kind": "keyword",
                  "spelling": "nullable"
                },
                {
                  "kind": "text",
                  "spelling": ")"
                },
                {
                  "kind": "typeIdentifier",
                  "preciseIdentifier": "c:objc(cs)Hat",
                  "spelling": "Hat"
                },
                {
                  "kind": "text",
                  "spelling": " *"
                },
                {
                  "kind": "identifier",
                  "spelling": "weak_nonatomic_assign_nullable"
                }
              ],
              "file": "DeclarationFragments.h",
              "getter": "weak_nonatomic_assign_nullable",
              "line": 20,
              "name": "weak_nonatomic_assign_nullable",
              "setter": "setWeak_nonatomic_assign_nullable:",
              "subHeading": [
                {
                  "kind": "identifier",
                  "spelling": "weak_nonatomic_assign_nullable"
                }
              ],
              "unavailable": true
            },
            {
              "USR": "c:objc(cs)Beret(py)strong_atomic_retain_nonnull",
              "access": "public",
              "col": 49,
              "declarationFragments": [
                {
                  "kind": "keyword",
                  "spelling": "@property"
                },
                {
                  "kind": "text",
                  "spelling": " ("
                },
                {
                  "kind": "keyword",
                  "spelling": "atomic"
                },
                {
                  "kind": "text",
                  "spelling": ", "
                },
                {
                  "kind": "keyword",
                  "spelling": "retain"
                },
                {
                  "kind": "text",
                  "spelling": ", "
                },
                {
                  "kind": "keyword",
                  "spelling": "strong"
                },
                {
                  "kind": "text",
                  "spelling": ", "
                },
                {
                  "kind": "keyword",
                  "spelling": "readwrite"
                },
                {
                  "kind": "text",
                  "spelling": ", "
                },
                {
                  "kind": "keyword",
                  "spelling": "nonnull"
                },
                {
                  "kind": "text",
                  "spelling": ")"
                },
                {
                  "kind": "typeIdentifier",
                  "preciseIdentifier": "c:objc(cs)Hat",
                  "spelling": "Hat"
                },
                {
                  "kind": "text",
                  "spelling": " *"
                },
                {
                  "kind": "identifier",
                  "spelling": "strong_atomic_retain_nonnull"
                }
              ],
              "file": "DeclarationFragments.h",
              "getter": "strong_atomic_retain_nonnull",
              "line": 22,
              "name": "strong_atomic_retain_nonnull",
              "setter": "setStrong_atomic_retain_nonnull:",
              "subHeading": [
                {
                  "kind": "identifier",
                  "spelling": "strong_atomic_retain_nonnull"
                }
              ]
            },
            {
              "USR": "c:objc(cs)Beret(cpy)class_direct_copy_null_resettable",
              "access": "public",
              "attr": [
                "class"
              ],
              "col": 54,
              "declarationFragments": [
                {
                  "kind": "keyword",
                  "spelling": "@property"
                },
                {
                  "kind": "text",
                  "spelling": " ("
                },
                {
                  "kind": "keyword",
                  "spelling": "class"
                },
                {
                  "kind": "text",
                  "spelling": ", "
                },
                {
                  "kind": "keyword",
                  "spelling": "direct"
                },
                {
                  "kind": "text",
                  "spelling": ", "
                },
                {
                  "kind": "keyword",
                  "spelling": "atomic"
                },
                {
                  "kind": "text",
                  "spelling": ", "
                },
                {
                  "kind": "keyword",
                  "spelling": "copy"
                },
                {
                  "kind": "text",
                  "spelling": ", "
                },
                {
                  "kind": "keyword",
                  "spelling": "readwrite"
                },
                {
                  "kind": "text",
                  "spelling": ", "
                },
                {
                  "kind": "keyword",
                  "spelling": "null_resettable"
                },
                {
                  "kind": "text",
                  "spelling": ")"
                },
                {
                  "kind": "typeIdentifier",
                  "preciseIdentifier": "c:objc(cs)Hat",
                  "spelling": "Hat"
                },
                {
                  "kind": "text",
                  "spelling": " *"
                },
                {
                  "kind": "identifier",
                  "spelling": "class_direct_copy_null_resettable"
                }
              ],
              "file": "DeclarationFragments.h",
              "getter": "class_direct_copy_null_resettable",
              "line": 23,
              "name": "class_direct_copy_null_resettable",
              "setter": "setClass_direct_copy_null_resettable:",
              "subHeading": [
                {
                  "kind": "identifier",
                  "spelling": "class_direct_copy_null_resettable"
                }
              ]
            }
          ],
          "subHeading": [
            {
              "kind": "identifier",
              "spelling": "Beret"
            }
          ],
          "super": "Hat",
          "superSourceModule": "DeclarationFragments",
          "superUSR": "c:objc(cs)Hat"
        }
      ],
      "protocols": [
        {
          "USR": "c:objc(pl)Wearable",
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
              "spelling": "Wearable"
            }
          ],
          "file": "DeclarationFragments.h",
          "line": 30,
          "name": "Wearable",
          "subHeading": [
            {
              "kind": "identifier",
              "spelling": "Wearable"
            }
          ]
        }
      ],
      "target": "arm64-apple-ios15-macabi"
    }
  ]
}
