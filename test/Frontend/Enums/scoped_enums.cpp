// RUN: %tapi-frontend -target i386-apple-macos10.12 -std=c++11 %s 2>&1 | FileCheck %s
// RUN: %tapi-frontend -target x86_64-apple-macos10.15 -std=c++11 %s 2>&1 | FileCheck %s
// RUN: %tapi-frontend -target x86_64-apple-ios13.0-macabi -std=c++11 %s 2>&1 | FileCheck %s

// CHECK-LABEL: enums

// CHECK:      - name: Bar
// CHECK-NEXT:   USR: c:@E@Bar
// CHECK-NEXT:   loc: {{.*}}/scoped_enums.cpp:[[@LINE+1]]:6
enum Bar {
  // CHECK-LABLE: constants
  // CHECK:      - name: A
  // CHECK-NEXT:   loc: {{.*}}/scoped_enums.cpp:[[@LINE+1]]:3
  A,

  // CHECK:      - name: B
  // CHECK-NEXT:   loc: {{.*}}/scoped_enums.cpp:[[@LINE+1]]:3
  B,

  // CHECK:      - name: C
  // CHECK-NEXT:   loc: {{.*}}/scoped_enums.cpp:[[@LINE+1]]:3
  C,
};

namespace Baz {
// CHECK:      - name: Baz::Bar
// CHECK-NEXT:   USR: c:@N@Baz@E@Bar
// CHECK-NEXT:   loc: {{.*}}/scoped_enums.cpp:[[@LINE+1]]:6
enum Bar {
  // CHECK-LABLE: constants
  // CHECK:      - name: Baz::A
  // CHECK-NEXT:   loc: {{.*}}/scoped_enums.cpp:[[@LINE+1]]:3
  A,

  // CHECK:      - name: Baz::B
  // CHECK-NEXT:   loc: {{.*}}/scoped_enums.cpp:[[@LINE+1]]:3
  B,

  // CHECK:      - name: Baz::C
  // CHECK-NEXT:   loc: {{.*}}/scoped_enums.cpp:[[@LINE+1]]:3
  C,
};
}

namespace Foo {
// CHECK:      - name: Foo::Bar
// CHECK-NEXT:   USR: c:@N@Foo@E@Bar
// CHECK-NEXT:   loc: {{.*}}/scoped_enums.cpp:[[@LINE+1]]:6
enum Bar {
  // CHECK-LABLE: constants
  // CHECK:      - name: Foo::A
  // CHECK-NEXT:   loc: {{.*}}/scoped_enums.cpp:[[@LINE+1]]:3
  A,

  // CHECK:      - name: Foo::B
  // CHECK-NEXT:   loc: {{.*}}/scoped_enums.cpp:[[@LINE+1]]:3
  B,

  // CHECK:      - name: Foo::C
  // CHECK-NEXT:   loc: {{.*}}/scoped_enums.cpp:[[@LINE+1]]:3
  C,
};
} // namespace Foo
