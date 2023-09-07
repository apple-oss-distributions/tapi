// RUN: rm -rf %t
// RUN: split-file %s %t
// RUN: sed -e "s:DST_DIR:%t:g" %t/input.json.in >> %t/input.json
// RUN: %tapi extractapi -target arm64-apple-macos12 -F%t/Frameworks %t/input.json -o %t/output.json | FileCheck -allow-empty %s
// RUN: diff -a %t/reference.output.json %t/output.json

// CHECK-NOT: error:
// CHECK-NOT: warning:

//--- input.json.in
{
  "version" : "2",
  "headers" : []
}

//--- reference.output.json
{
  "Private": [],
  "Project": [],
  "Public": []
}
