#import <Foundation/Foundation.h>

#if defined(__arm64__)
int foo() { return 0; }
#endif

@interface BazClass : NSObject {
  int result;
}
@end

@implementation BazClass
@end
