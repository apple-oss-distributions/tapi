#import <Foundation/Foundation.h>
#import <ObjCLib.h>

@interface MyTest : Test
- (int)getVar;
@end

@implementation MyTest
- (int)getVar {
  @try {
    @throw [NSException exceptionWithName:@"TestException"
                                   reason:@"Test"
                                 userInfo:nil];
  } @catch (NSException *e) {
  }
  return test_var;
}
@end
