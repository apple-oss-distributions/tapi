// RUN: %tapi-frontend -std=c++14 -target x86_64-apple-macos10.15 %s 2>&1 | FileCheck %s

template <typename T> struct A {
  static constexpr int size = sizeof(T);
};

// Just a template, not a definition, no exported symbol.
template <typename T> constexpr int t = A<T>::size;

// Specialization
template <> constexpr int t<int> = 4;

// Instantiation.
extern template constexpr int t<float>;

// CHECK: globals:
// CHECK-NEXT: - name: __Z1tIiE
// CHECK-NEXT:   loc:
// CHECK-SAME:   template-var.cpp:11:27
// CHECK-NEXT:   availability: i:0 d:0 o:0 u:0
// CHECK-NEXT:   access: public
// CHECK-NEXT:   isWeakDefined: false
// CHECK-NEXT:   isThreadLocalValue: false
// CHECK-NEXT:   kind: variable
// CHECK-NEXT:   linkage: exported
// CHECK-NEXT: - name: __Z1tIfE
// CHECK-NEXT:   loc:
// CHECK-SAME:   template-var.cpp:8:37
// CHECK-NEXT:   availability: i:0 d:0 o:0 u:0
// CHECK-NEXT:   access: public
// CHECK-NEXT:   isWeakDefined: false
// CHECK-NEXT:   isThreadLocalValue: false
// CHECK-NEXT:   kind: variable
// CHECK-NEXT:   linkage: exported

// CHECK-NOT: - name: _t
