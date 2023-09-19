// RUN: %tapi-frontend -target i386-apple-macos10.12 -isysroot %sysroot %s 2>&1 | FileCheck %s
// RUN: %tapi-frontend -target x86_64-apple-macos10.15 -isysroot %sysroot %s 2>&1 | FileCheck %s
// RUN: %tapi-frontend -target x86_64-apple-ios13.0-macabi -isysroot %sysroot %s 2>&1 | FileCheck %s

typedef long NSInteger;

typedef enum __attribute__((enum_extensibility(open))) Foo : NSInteger Foo;
// CHECK:      enums:
// CHECK-NEXT: - name: Foo
// CHECK-NEXT:   USR: c:@E@Foo
// CHECK-NEXT:   loc: {{.*}}/enums.m:[[@LINE+1]]:6
enum Foo : NSInteger {
  // CHECK-LABLE: constants
  // CHECK:      - name: A
  // CHECK-NEXT:   loc: {{.*}}/enums.m:[[@LINE+1]]:3
  A,

  // CHECK:      - name: B
  // CHECK-NEXT:   loc: {{.*}}/enums.m:[[@LINE+1]]:3
  B,

  // CHECK:      - name: C
  // CHECK-NEXT:   loc: {{.*}}/enums.m:[[@LINE+1]]:3
  C,
};

// CHECK:      - name: enum (unnamed at 
// CHECK-NEXT:   USR: c:@Ea@D
// CHECK-NEXT:   loc: {{.*}}/enums.m:[[@LINE+1]]:1
enum __attribute__((enum_extensibility(open))) : NSInteger {
  // CHECK-LABLE: constants
  // CHECK:      - name: D
  // CHECK-NEXT:   loc: {{.*}}/enums.m:[[@LINE+1]]:3
  D,

  // CHECK:      - name: E
  // CHECK-NEXT:   loc: {{.*}}/enums.m:[[@LINE+1]]:3
  E,

  // CHECK:      - name: F
  // CHECK-NEXT:   loc: {{.*}}/enums.m:[[@LINE+1]]:3
  F,
};
