// RUN: %tapi-frontend -isysroot %sysroot -target x86_64-apple-macos10.15 %s 2>&1 | FileCheck %s

#include <Availability.h>

// If internal SDK, it already has SPI_AVAILABLE defined, use the version
// in the SDK. Otherwise, use the stripped down version of local header
// for testing.
#ifndef SPI_AVAILABLE
// SPI Available has to come from AvailabilityInternalPrivate.h.
#include "Inputs/AvailabilityInternalPrivate.h"
#endif

#define MY_SPI_AVAILABLE SPI_AVAILABLE
#define MY_API_AVAILABLE __API_AVAILABLE

extern int api_sym __API_AVAILABLE(macos(10.9));
extern int api_sym2 MY_API_AVAILABLE(macos(10.9));
extern int spi_sym SPI_AVAILABLE(macos(10.9));
extern int spi_sym2 MY_SPI_AVAILABLE(macos(10.9));

// CHECK: - name: _api_sym
// CHECK:   availability: i:10.9 d:0 o:0 u:0
// CHECK: - name: _api_sym2
// CHECK:   availability: i:10.9 d:0 o:0 u:0
// CHECK: - name: _spi_sym
// CHECK:   availability: i:10.9 d:0 o:0 u:0 (spi)
// CHECK: - name: _spi_sym2
// CHECK:   availability: i:10.9 d:0 o:0 u:0 (spi)
