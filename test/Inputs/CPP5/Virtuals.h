namespace test1 {
class Foo {
public:
  struct Base {
    virtual ~Base() = 0;
    virtual void overrideFunc() const = 0;
  };
};

struct Derived : public Foo::Base {
  virtual void overrideFunc() const override {}
};

} // namespace test1

namespace test2 {
struct Base {
  virtual ~Base();
  virtual void overrideFunc() const = 0;
};

struct Derived : public Base {
  virtual void overrideFunc() const override {}
};

} // namespace test2
