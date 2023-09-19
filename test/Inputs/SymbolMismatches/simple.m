__attribute__((visibility("default"))) int foo() {
  int x, y = 2;
  int z;
  return x + y;
}
__attribute__((visibility("default"))) int bar = 0;
