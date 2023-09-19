// RUN: %tapi-frontend -target i386-apple-macos10.12 -std=c++11 %s 2>&1 | FileCheck %s
// RUN: %tapi-frontend -target x86_64-apple-macos10.15 -std=c++11 %s 2>&1 | FileCheck %s
// RUN: %tapi-frontend -target x86_64-apple-ios13.0-macabi -std=c++11 %s 2>&1 | FileCheck %s

// constexpr
// CHECK: check_const
// CHECK: linkage: internal
constexpr int check_const();

// extern.
// CHECK:      - name: __Z12check_externv
extern void check_extern();

// visibility.
// CHECK:      - name: __Z12check_hiddenv
// CHECK:        linkage: internal
void check_hidden() __attribute__((visibility("hidden")));

// CHECK: check_inline
// CHECK: linkage: internal
inline void check_inline() {}

// CHECK:      - name: __Z13check_defaultv
void check_default() __attribute__((visibility("default")));

// CHECK: check_inline1
// CHECK: linkage: internal
inline void check_inline1();
void check_inline1() {}

// templates.
template <typename T>
void check_template(T) {}

// CHECK:      - name: __Z14check_templateIfEvT_
template <>
void check_template<float>(float);

// CHECK:      - name: __Z14check_templateIiEvT_
template <>
void check_template<int>(int) {}

// CHECK:      - name: __Z3foov
// CHECK:      - name: __Z3fooi
// CHECK:      - name: __Z3fooif
void foo();
void foo(int);
void foo(int, float);

// redeclare.
void foo(int, float);

// CHECK: check_static
// CHECK: linkage: internal
static void check_static(void);

// anonymous namespace.
// CHECK: check_anonymous
// CHECK: linkage: internal
namespace {
void check_anonymous();
}

// CHECK:      - name: __ZN4test3fooEv
namespace test {
void foo();
}

// CHECK:      - name: _foo1
extern "C" {
void foo1(void);
}
