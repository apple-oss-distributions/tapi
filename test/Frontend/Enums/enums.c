// RUN: %tapi-frontend -target i386-apple-macos10.12 %s 2>&1 | FileCheck %s
// RUN: %tapi-frontend -target x86_64-apple-macos10.15 %s 2>&1 | FileCheck %s
// RUN: %tapi-frontend -target x86_64-apple-ios13.0-macabi %s 2>&1 | FileCheck %s

// CHECK-LABEL: enums

// Forward declaration
enum Foo;

// CHECK:      - name: Bar
// CHECK-NEXT:   USR: c:@E@Bar
// CHECK-NEXT:   loc: {{.*}}/enums.c:[[@LINE+1]]:6
enum Bar {
  // CHECK-LABLE: constants
  // CHECK:      - name: A
  // CHECK-NEXT:   loc: {{.*}}/enums.c:[[@LINE+1]]:3
  A,

  // CHECK:      - name: B
  // CHECK-NEXT:   loc: {{.*}}/enums.c:[[@LINE+1]]:3
  B,

  // CHECK:      - name: C
  // CHECK-NEXT:   loc: {{.*}}/enums.c:[[@LINE+1]]:3
  C,
};

// CHECK:      - name: Baz
// CHECK-NEXT:   USR: c:@E@Baz
// CHECK-NEXT:   loc: {{.*}}/enums.c:[[@LINE+1]]:6
enum Baz {
  // CHECK-LABLE: constants
  // CHECK:      - name: D
  // CHECK-NEXT:   loc: {{.*}}/enums.c:[[@LINE+1]]:3
  D = 10,

  // CHECK:      - name: E
  // CHECK-NEXT:   loc: {{.*}}/enums.c:[[@LINE+1]]:3
  E = 0,

  // CHECK:      - name: F
  // CHECK-NEXT:   loc: {{.*}}/enums.c:[[@LINE+1]]:3
  F = -1,
};

// CHECK:      - name: enum (unnamed at
// CHECK-NEXT:   USR: c:@Ea@G
// CHECK-NEXT:   loc: {{.*}}/enums.c:[[@LINE+1]]:1
enum {
  // CHECK-LABLE: constants
  // CHECK:      - name: G
  // CHECK-NEXT:   loc: {{.*}}/enums.c:[[@LINE+1]]:3
  G = 10,

  // CHECK:      - name: H
  // CHECK-NEXT:   loc: {{.*}}/enums.c:[[@LINE+1]]:3
  H = 0,
};
