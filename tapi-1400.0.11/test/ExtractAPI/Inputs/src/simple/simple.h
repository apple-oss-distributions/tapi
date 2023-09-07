extern int simple_api();

extern void deprecated_api()
    __attribute__((availability(macosx, introduced = 11, deprecated = 12)));

extern void unconditionally_deprecated_api() __attribute__((deprecated()));