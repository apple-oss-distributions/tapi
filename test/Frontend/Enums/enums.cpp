// RUN: %tapi-frontend -target i386-apple-macos10.12 -std=c++11 %s 2>&1 | FileCheck %s
// RUN: %tapi-frontend -target x86_64-apple-macos10.15 -std=c++11 %s 2>&1 | FileCheck %s
// RUN: %tapi-frontend -target x86_64-apple-ios13.0-macabi -std=c++11 %s 2>&1 | FileCheck %s

// CHECK-LABEL: enums

// Forward declaration
enum Foo : int;

// CHECK:      - name: Bar
// CHECK-NEXT:   USR: c:@E@Bar
// CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:6
enum Bar {
  // CHECK-LABLE: constants
  // CHECK:      - name: A
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  A,

  // CHECK:      - name: B
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  B,

  // CHECK:      - name: C
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  C,
};

// CHECK:      - name: Baz
// CHECK-NEXT:   USR: c:@E@Baz
// CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:12
enum class Baz {
  // CHECK-LABLE: constants
  // CHECK:      - name: Baz::A
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  A,

  // CHECK:      - name: Baz::B
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  B,

  // CHECK:      - name: Baz::C
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  C,
};

// CHECK:      - name: Bazinga
// CHECK-NEXT:   USR: c:@E@Bazinga
// CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:13
enum struct Bazinga {
  // CHECK-LABLE: constants
  // CHECK:      - name: Bazinga::A
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  A,

  // CHECK:      - name: Bazinga::B
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  B,

  // CHECK:      - name: Bazinga::C
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  C,
};

// CHECK:      - name: (unnamed enum at
// CHECK-NEXT:   USR: c:@Ea@U
// CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:1
enum {
  // CHECK-LABLE: constants
  // CHECK:      - name: U
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  U,

  // CHECK:      - name: V
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  V,

  // CHECK:      - name: W
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  W,
};

// CHECK:      - name: (unnamed enum at
// CHECK-NEXT:   USR: c:@Ea@X
// CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:1
enum {
  // CHECK-LABLE: constants
  // CHECK:      - name: X
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  X,

  // CHECK:      - name: Y
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  Y,

  // CHECK:      - name: Z
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  Z,
};
