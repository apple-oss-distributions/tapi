#ifndef ASM_H
#define ASM_H

#import <Foundation/Foundation.h>

extern int ivar __asm("_OBJC_IVAR_$_SomeClass._ivar1");
extern int objcClass1 __asm("_OBJC_CLASS_$_SomeClass");
extern int objcClass2 __asm("_OBJC_METACLASS_$_SomeClass");
extern int objcClass3 __asm("_OBJC_EHTYPE_$_SomeClass");
extern int objcClass4 __asm(".objc_class_name_SomeClass");

__attribute__((visibility("hidden")))
@interface NSString() {
}
@end

extern int ivarExtra __asm("_OBJC_IVAR_$_NSString._ivar1");
#endif // ASM_H
