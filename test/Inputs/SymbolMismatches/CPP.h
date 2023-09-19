class Foo {
  virtual int init() = 0;
};

class Bar : Foo {
  int init();
};
