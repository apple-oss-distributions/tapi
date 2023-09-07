// RUN: %tapi-frontend -target x86_64-apple-macos10.15 %s 2>&1 | FileCheck %s

// CHECK:       macros:
// CHECK-NEXT:  - name: API_MACRO
// CHECK:       - name: SPI_MACRO
// CHECK:       - name: MACRO_FUN

// Comment before macro
#define API_MACRO something

#define SPI_MACRO else // Comment after macro

#define MACRO_FUN(X) X X