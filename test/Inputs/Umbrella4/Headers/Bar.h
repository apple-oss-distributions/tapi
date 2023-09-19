#import <Foundation/Foundation.h>

#if defined(__arm64__)
extern int foo();
#endif

@interface BazClass : NSObject {
  int result;
}
@end
