// RUN: %tapi-frontend -target i386-apple-macos10.12 -isysroot %sysroot %s 2>&1 | FileCheck %s
// RUN: %tapi-frontend -target x86_64-apple-macos10.15 -isysroot %sysroot %s 2>&1 | FileCheck %s
// RUN: %tapi-frontend -target x86_64-apple-ios13.0-macabi -isysroot %sysroot %s 2>&1 | FileCheck %s

// CHECK:      structs:
// CHECK-NEXT: - name: Color
// CHECK-NEXT:   USR: c:@S@Color
// CHECK-NEXT:   loc: {{.*}}/struct.m:[[@LINE+1]]:8
struct Color {
  // CHECK-LABLE: fields
  // CHECK:      - name: red
  // CHECK-NEXT:   USR: c:@S@Color@FI@red
  // CHECK-NEXT:   loc: {{.*}}/struct.m:[[@LINE+1]]:12
  unsigned red;

  // CHECK:      - name: green
  // CHECK-NEXT:   USR: c:@S@Color@FI@green
  // CHECK-NEXT:   loc: {{.*}}/struct.m:[[@LINE+1]]:12
  unsigned green;

  // CHECK:      - name: blue
  // CHECK-NEXT:   USR: c:@S@Color@FI@blue
  // CHECK-NEXT:   loc: {{.*}}/struct.m:[[@LINE+1]]:12
  unsigned blue;

  // CHECK:      - name: alpha
  // CHECK-NEXT:   USR: c:@S@Color@FI@alpha
  // CHECK-NEXT:   loc: {{.*}}/struct.m:[[@LINE+1]]:12
  unsigned alpha;
};
