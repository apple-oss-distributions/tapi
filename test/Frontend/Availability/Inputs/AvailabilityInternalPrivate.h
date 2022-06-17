// Strip down version of AvailabilityInternalPrivate.h
// tapi check that availability macro expension going through a header named AvailabilityInternalPrivate.h

#define SPI_AVAILABLE(...) __attribute__((availability(macosx, introduced = 10.9)))
