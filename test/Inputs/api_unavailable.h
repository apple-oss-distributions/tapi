#import <Foundation/Foundation.h>
#import <os/availability.h>

void forMacOS() API_UNAVAILABLE(ios);
void foriOS() API_UNAVAILABLE(macos);

API_UNAVAILABLE(ios) @interface macOSClass : NSObject {
@public
  BOOL foo;
}
@end

API_UNAVAILABLE(macos) @interface iOSClass : NSObject {}
@end
