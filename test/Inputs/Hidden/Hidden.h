__private_extern__ int
foo(); // Clang doesn't warn on this: rdar://105043576 (Wprivate-extern should
       // also check for function decls)
__private_extern__ int baz;
__attribute__((visibility("hidden"))) int bar();
