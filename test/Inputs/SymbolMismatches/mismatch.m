int inlinedFunc() { return 1; }

int foo() { return 1; }

__attribute__((visibility("hidden"))) int bar() { return 1; }

int baz() { return 1; }

int fooBar = 1;
int unavailableSymbol = 1;
