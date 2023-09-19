#import <CategoryAccessPropagation/Public.h>

@implementation ParentClass
@end


@implementation ParentClass (MyCategory)
- (void) methodFromCategory {}
- (int) propertyFromCategory { return 1; }
- (void) methodFromProtocol {}
@end


@implementation ChildClass
- (void) methodFromCategory {}
- (int) propertyFromCategory { return 2; }
- (void) methodFromProtocol {}
@end

