// RUN: %tapi-frontend -target x86_64-apple-macos10.15 -target x86_64-apple-ios13.0-macabi -verify -no-print --diag-missing-api %s 2>&1 | FileCheck %s

typedef enum {
  One = 1
} MyEnum;

#if !__is_target_environment(macabi)
typedef int MyNumber;
#else
typedef MyEnum MyNumber;
#endif

// CHECK: note: 'MyNumber' is defined to type 'MyEnum' here
typedef MyNumber Number;

@interface Foo @end
@interface Bar @end
@protocol P @end

// CHECK: note: 'Foo' conforms to protocol 'P'
// CHECK: note: 'Foo' has no corresponding protocol here
#if !__is_target_environment(macabi)
@interface Foo() <P>
@end
#else
@interface Foo() @end
#endif

// CHECK: note: 'Bar' has no corresponding method here
// CHECK: note: 'Bar' has method 'test' here 
@interface Bar() <P>
#if __is_target_environment(macabi)
- (void) test;
#endif
@end
