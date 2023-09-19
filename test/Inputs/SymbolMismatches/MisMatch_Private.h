#include <Foundation/Foundation.h>
extern int bar();

@interface NSFoo : NSObject {
  NSString *string;
}
@end

__attribute__((objc_exception))
@interface NSFooExcept : NSObject
@end

extern int unavailableSymbol __attribute__((
    availability(macosx, unavailable), availability(macCatalyst, unavailable)));
