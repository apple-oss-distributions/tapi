// RUN: rm -rf %t
// RUN: split-file %s %t
// RUN: sed -e "s:DST_DIR:%t:g" %t/input.json.in >> %t/input.json
// RUN: %tapi extractapi -x c++ -target arm64-apple-macos12 -F%t/Frameworks \
// RUN: %t/input.json -o %t/output.json | FileCheck -allow-empty %s
// RUN: diff -a %t/reference.output.json %t/output.json

// CHECK-NOT: error:
// CHECK-NOT: warning:

//--- Frameworks/CPP.framework/Headers/CPP.h

// Skip C++ records completely for now.

// C++ struct/class/union
struct S { int i; };
class C { public: int i; };
union U { int i; };

// Template
template<class T, class U>
struct Template {
  T* t;
  U* u;
};

// Explicit instantiation
template struct Template<char, int>;

//--- input.json.in
{
  "version" : "2",
  "headers" : [
    {
      "type" : "public",
      "path" : "DST_DIR/Frameworks/CPP.framework/Headers/CPP.h"
    }
  ]
}

//--- reference.output.json
{
  "Private": [],
  "Project": [],
  "Public": [
    {
      "target": "arm64-apple-macos12"
    }
  ]
}
