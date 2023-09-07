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
  // CHECK-NEXT:   USR: c:@E@Bar@A
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  A,

  // CHECK:      - name: B
  // CHECK-NEXT:   USR: c:@E@Bar@B
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  B,

  // CHECK:      - name: C
  // CHECK-NEXT:   USR: c:@E@Bar@C
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  C,
};

// CHECK:      - name: Baz
// CHECK-NEXT:   USR: c:@E@Baz
// CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:12
enum class Baz {
  // CHECK-LABLE: constants
  // CHECK:      - name: Baz::A
  // CHECK-NEXT:   USR: c:@E@Baz@A
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  A,

  // CHECK:      - name: Baz::B
  // CHECK-NEXT:   USR: c:@E@Baz@B
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  B,

  // CHECK:      - name: Baz::C
  // CHECK-NEXT:   USR: c:@E@Baz@C
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  C,
};

// CHECK:      - name: Bazinga
// CHECK-NEXT:   USR: c:@E@Bazinga
// CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:13
enum struct Bazinga {
  // CHECK-LABLE: constants
  // CHECK:      - name: Bazinga::A
  // CHECK-NEXT:   USR: c:@E@Bazinga@A
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  A,

  // CHECK:      - name: Bazinga::B
  // CHECK-NEXT:   USR: c:@E@Bazinga@B
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  B,

  // CHECK:      - name: Bazinga::C
  // CHECK-NEXT:   USR: c:@E@Bazinga@C
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  C,
};

// CHECK:      - name: (anonymous)
// CHECK-NEXT:   USR: c:@Ea@X
// CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:1
enum {
  // CHECK-LABLE: constants
  // CHECK:      - name: X
  // CHECK-NEXT:   USR: c:@Ea@X@X
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  X,

  // CHECK:      - name: Y
  // CHECK-NEXT:   USR: c:@Ea@X@Y
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  Y,

  // CHECK:      - name: Z
  // CHECK-NEXT:   USR: c:@Ea@X@Z
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  Z,
};

// CHECK:      - name: (anonymous)
// CHECK-NEXT:   USR: c:@Ea@U
// CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:1
enum {
  // CHECK-LABLE: constants
  // CHECK:      - name: U
  // CHECK-NEXT:   USR: c:@Ea@U@U
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  U,

  // CHECK:      - name: V
  // CHECK-NEXT:   USR: c:@Ea@U@V
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  V,

  // CHECK:      - name: W
  // CHECK-NEXT:   USR: c:@Ea@U@W
  // CHECK-NEXT:   loc: {{.*}}/enums.cpp:[[@LINE+1]]:3
  W,
};
