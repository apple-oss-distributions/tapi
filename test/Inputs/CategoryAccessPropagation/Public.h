#ifndef PUBLIC_H
#define PUBLIC_H

// Scenario: ChildClass overrides a public method defined by a category on the
// parent class. The method should also be marked as public in the child class.
// Same with overridden protocol methods of the parent class.
// Covers rdar://108015419

@interface ParentClass
@end


@protocol SampleProtocol
- (void) methodFromProtocol;
@end


@interface ParentClass (MyCategory) <SampleProtocol>
@property (readonly) int propertyFromCategory;
- (void) methodFromCategory;
@end


@interface ChildClass : ParentClass
@end

#endif // PUBLIC_H
