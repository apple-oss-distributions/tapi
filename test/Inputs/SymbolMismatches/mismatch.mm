class Foo {
  virtual int init() = 0;
};

class Bar : Foo {
  int init() { return 1; }
};
Bar bar;

template <typename T> int foo(T val) { return 1; }
template <> int foo(unsigned val) { return 1; }
