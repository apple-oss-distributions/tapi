#ifndef PRIVATE_H
#define PRIVATE_H

#import <CategoryPropertyOverrideInSPI/Public.h>

// Private category of the class
@interface MyClass (SPI)
// MyProperty is readwrite in internal SDK
@property (readwrite) int MyProperty;
@end

#endif // PRIVATE_H
