#define CATCH_CONFIG_MAIN

#include <catch.hpp>
#include "cloning_ptr.h"

struct BaseType
{
  virtual int value() = 0; // intentionally non-const
  virtual void set_value(int) = 0;
  virtual ~BaseType() = default;
};

struct DerivedType : BaseType
{
  int value_ = 0;

  DerivedType()
  {
    ++object_count;
  }

  DerivedType(const DerivedType& d)
  {
    value_ = d.value_;
    ++object_count;
  }

  DerivedType(int v) : value_(v)
  {
    ++object_count;
  }

  ~DerivedType()
  {
    --object_count;
  }

  int value() override { return value_; }

  void set_value(int i) override { value_ = i; }

  static size_t object_count;
};

size_t DerivedType::object_count = 0;

TEST_CASE("Default constructor","[cloning_ptr.constructors]")
{
  GIVEN("A default constructed cloning_ptr to BaseType")
  {
    cloning_ptr<BaseType> dptr;

    THEN("get returns nullptr")
    {
      REQUIRE(dptr.get() == nullptr);
    }

    THEN("operator-> returns nullptr")
    {
      REQUIRE(dptr.operator->() == nullptr);
    }

    THEN("operator bool returns false")
    {
      REQUIRE((bool)dptr == false);
    }
  }

  GIVEN("A default constructed const cloning_ptr to BaseType")
  {
    const cloning_ptr<BaseType> cdptr;

    THEN("get returns nullptr")
    {
      REQUIRE(cdptr.get() == nullptr);
    }

    THEN("operator-> returns nullptr")
    {
      REQUIRE(cdptr.operator->() == nullptr);
    }

    THEN("operator bool returns false")
    {
      REQUIRE((bool)cdptr == false);
    }
  }
}

TEST_CASE("Pointer constructor","[cloning_ptr.constructors]")
{
  GIVEN("A pointer-constructed cloning_ptr")
  {
    int v = 7;
    cloning_ptr<BaseType> dptr(new DerivedType(v));

    THEN("get returns a non-null pointer")
    {
      REQUIRE(dptr.get() != nullptr);
    }

    THEN("Operator-> calls the pointee method")
    {
      REQUIRE(dptr->value() == v);
    }

    THEN("operator bool returns true")
    {
      REQUIRE((bool)dptr == true);
    }
  }
  GIVEN("A pointer-constructed const cloning_ptr")
  {
    int v = 7;
    const cloning_ptr<BaseType> cdptr(new DerivedType(v));

    THEN("get returns a non-null pointer")
    {
      REQUIRE(cdptr.get() != nullptr);
    }

    THEN("Operator-> calls the pointee method")
    {
      REQUIRE(cdptr->value() == v);
    }

    THEN("operator bool returns true")
    {
      REQUIRE((bool)cdptr == true);
    }
  }
}

TEST_CASE("cloning_ptr destructor","[cloning_ptr.destructor]")
{
  GIVEN("No derived objects")
  {
    REQUIRE(DerivedType::object_count == 0);

    THEN("Object count is increased on construction and decreased on destruction")
    {
      // begin and end scope to force destruction
      {
        cloning_ptr<BaseType> tmp(new DerivedType());
        REQUIRE(DerivedType::object_count == 1);
      }
      REQUIRE(DerivedType::object_count == 0);
    }
  }
}

TEST_CASE("cloning_ptr copy constructor","[cloning_ptr.constructors]")
{
  GIVEN("A cloning_ptr copied from a default-constructed cloning_ptr")
  {
    cloning_ptr<BaseType> original_dptr;
    cloning_ptr<BaseType> dptr(original_dptr);

    THEN("get returns nullptr")
    {
      REQUIRE(dptr.get() == nullptr);
    }

    THEN("operator-> returns nullptr")
    {
      REQUIRE(dptr.operator->() == nullptr);
    }

    THEN("operator bool returns false")
    {
      REQUIRE((bool)dptr == false);
    }
  }

  GIVEN("A cloning_ptr copied from a pointer-constructed cloning_ptr")
  {
    REQUIRE(DerivedType::object_count == 0);

    int v = 7;
    cloning_ptr<BaseType> original_dptr(new DerivedType(v));
    cloning_ptr<BaseType> dptr(original_dptr);

    THEN("get returns a distinct non-null pointer")
    {
      REQUIRE(dptr.get() != nullptr);
      REQUIRE(dptr.get() != original_dptr.get());
    }

    THEN("Operator-> calls the pointee method")
    {
      REQUIRE(dptr->value() == v);
    }

    THEN("operator bool returns true")
    {
      REQUIRE((bool)dptr == true);
    }

    THEN("object count is two")
    {
      REQUIRE(DerivedType::object_count == 2);
    }

    WHEN("Changes are made to the original cloning pointer after copying")
    {
      int new_value = 99;
      original_dptr->set_value(new_value);
      REQUIRE(original_dptr->value() == new_value);
      THEN("They are not reflected in the copy (copy is distinct)")
      {
        REQUIRE(dptr->value() != new_value);
        REQUIRE(dptr->value() == v);
      }
    }
  }
}

TEST_CASE("cloning_ptr move constructor","[cloning_ptr.constructors]")
{
  GIVEN("A cloning_ptr move-constructed from a default-constructed cloning_ptr")
  {
    cloning_ptr<BaseType> original_dptr;
    cloning_ptr<BaseType> dptr(std::move(original_dptr));

    THEN("The original pointer is null")
    {
      REQUIRE(original_dptr.get()==nullptr);
      REQUIRE(original_dptr.operator->()==nullptr);
      REQUIRE(!(bool)original_dptr);
    }

    THEN("The move-constructed pointer is null")
    {
      REQUIRE(dptr.get()==nullptr);
      REQUIRE(dptr.operator->()==nullptr);
      REQUIRE(!(bool)dptr);
    }
  }

  GIVEN("A cloning_ptr move-constructed from a default-constructed cloning_ptr")
  {
    int v = 7;
    cloning_ptr<BaseType> original_dptr(new DerivedType(v));
    auto original_pointer = original_dptr.get();
    CHECK(DerivedType::object_count == 1);

    cloning_ptr<BaseType> dptr(std::move(original_dptr));
    CHECK(DerivedType::object_count == 1);

    THEN("The original pointer is null")
    {
      REQUIRE(original_dptr.get()==nullptr);
      REQUIRE(original_dptr.operator->()==nullptr);
      REQUIRE(!(bool)original_dptr);
    }

    THEN("The move-constructed pointer is the original pointer")
    {
      REQUIRE(dptr.get()==original_pointer);
      REQUIRE(dptr.operator->()==original_pointer);
      REQUIRE((bool)dptr);
    }

    THEN("The move-constructed pointer value is the constructed value")
    {
      REQUIRE(dptr->value() == v);
    }
  }
}

TEST_CASE("cloning_ptr assignment","[cloning_ptr.assignment]")
{
  GIVEN("A default-constructed cloning_ptr assigned-to a default-constructed cloning_ptr")
  {
    cloning_ptr<BaseType> dptr1;
    const cloning_ptr<BaseType> dptr2;
    const auto p = dptr2.get();

    REQUIRE(DerivedType::object_count == 0);

    dptr1 = dptr2;

    REQUIRE(DerivedType::object_count == 0);

    THEN("The assigned-from object is unchanged")
    {
      REQUIRE(dptr2.get() == p);
    }

    THEN("The assigned-to object is null")
    {
      REQUIRE(dptr1.get() == nullptr);
    }
  }

  GIVEN("A default-constructed cloning_ptr assigned to a pointer-constructed cloning_ptr")
  {
    int v1 = 7;

    cloning_ptr<BaseType> dptr1(new DerivedType(v1));
    const cloning_ptr<BaseType> dptr2;
    const auto p = dptr2.get();

    REQUIRE(DerivedType::object_count == 1);

    dptr1 = dptr2;

    REQUIRE(DerivedType::object_count == 0);

    THEN("The assigned-from object is unchanged")
    {
      REQUIRE(dptr2.get() == p);
    }

    THEN("The assigned-to object is null")
    {
      REQUIRE(dptr1.get() == nullptr);
    }
  }

  GIVEN("A pointer-constructed cloning_ptr assigned to a default-constructed cloning_ptr")
  {
    int v1 = 7;

    cloning_ptr<BaseType> dptr1;
    const cloning_ptr<BaseType> dptr2(new DerivedType(v1));
    const auto p = dptr2.get();

    REQUIRE(DerivedType::object_count == 1);

    dptr1 = dptr2;

    REQUIRE(DerivedType::object_count == 2);

    THEN("The assigned-from object is unchanged")
    {
      REQUIRE(dptr2.get() == p);
    }

    THEN("The assigned-to object is non-null")
    {
      REQUIRE(dptr1.get() != nullptr);
    }

    THEN("The assigned-from object 'value' is the assigned-to object value")
    {
      REQUIRE(dptr1->value() == dptr2->value());
    }

    THEN("The assigned-from object pointer and the assigned-to object pointer are distinct")
    {
      REQUIRE(dptr1.get() != dptr2.get());
    }

  }

  GIVEN("A pointer-constructed cloning_ptr assigned to a pointer-constructed cloning_ptr")
  {
    int v1 = 7;
    int v2 = 87;

    cloning_ptr<BaseType> dptr1(new DerivedType(v1));
    const cloning_ptr<BaseType> dptr2(new DerivedType(v2));
    const auto p = dptr2.get();

    REQUIRE(DerivedType::object_count == 2);

    dptr1 = dptr2;

    REQUIRE(DerivedType::object_count == 2);

    THEN("The assigned-from object is unchanged")
    {
      REQUIRE(dptr2.get() == p);
    }

    THEN("The assigned-to object is non-null")
    {
      REQUIRE(dptr1.get() != nullptr);
    }

    THEN("The assigned-from object 'value' is the assigned-to object value")
    {
      REQUIRE(dptr1->value() == dptr2->value());
    }

    THEN("The assigned-from object pointer and the assigned-to object pointer are distinct")
    {
      REQUIRE(dptr1.get() != dptr2.get());
    }
  }

  GIVEN("A pointer-constructed cloning_ptr assigned to itself")
  {
    int v1 = 7;

    cloning_ptr<BaseType> dptr1(new DerivedType(v1));
    const auto p = dptr1.get();

    REQUIRE(DerivedType::object_count == 1);

    dptr1 = dptr1;

    REQUIRE(DerivedType::object_count == 1);

    THEN("The assigned-from object is unchanged")
    {
      REQUIRE(dptr1.get() == p);
    }
  }
}

TEST_CASE("cloning_ptr move-assignment","[cloning_ptr.assignment]")
{
  GIVEN("A default-constructed cloning_ptr move-assigned-to a default-constructed cloning_ptr")
  {
    cloning_ptr<BaseType> dptr1;
    cloning_ptr<BaseType> dptr2;
    const auto p = dptr2.get();

    REQUIRE(DerivedType::object_count == 0);

    dptr1 = std::move(dptr2);

    REQUIRE(DerivedType::object_count == 0);

    THEN("The move-assigned-from object is null")
    {
      REQUIRE(dptr1.get() == nullptr);
    }

    THEN("The move-assigned-to object is null")
    {
      REQUIRE(dptr1.get() == nullptr);
    }
  }

  GIVEN("A default-constructed cloning_ptr move-assigned to a pointer-constructed cloning_ptr")
  {
    int v1 = 7;

    cloning_ptr<BaseType> dptr1(new DerivedType(v1));
    cloning_ptr<BaseType> dptr2;
    const auto p = dptr2.get();

    REQUIRE(DerivedType::object_count == 1);

    dptr1 = std::move(dptr2);

    REQUIRE(DerivedType::object_count == 0);

    THEN("The move-assigned-from object is null")
    {
      REQUIRE(dptr1.get() == nullptr);
    }

    THEN("The move-assigned-to object is null")
    {
      REQUIRE(dptr1.get() == nullptr);
    }
  }

  GIVEN("A pointer-constructed cloning_ptr move-assigned to a default-constructed cloning_ptr")
  {
    int v1 = 7;

    cloning_ptr<BaseType> dptr1;
    cloning_ptr<BaseType> dptr2(new DerivedType(v1));
    const auto p = dptr2.get();

    REQUIRE(DerivedType::object_count == 1);

    dptr1 = std::move(dptr2);

    REQUIRE(DerivedType::object_count == 1);

    THEN("The move-assigned-from object is null")
    {
      REQUIRE(dptr2.get() == nullptr);
    }

    THEN("The move-assigned-to object pointer is the move-assigned-from pointer")
    {
      REQUIRE(dptr1.get() == p);
    }
  }

  GIVEN("A pointer-constructed cloning_ptr move-assigned to a pointer-constructed cloning_ptr")
  {
    int v1 = 7;
    int v2 = 87;

    cloning_ptr<BaseType> dptr1(new DerivedType(v1));
    cloning_ptr<BaseType> dptr2(new DerivedType(v2));
    const auto p = dptr2.get();

    REQUIRE(DerivedType::object_count == 2);

    dptr1 = std::move(dptr2);

    REQUIRE(DerivedType::object_count == 1);

    THEN("The move-assigned-from object is null")
    {
      REQUIRE(dptr2.get() == nullptr);
    }

    THEN("The move-assigned-to object pointer is the move-assigned-from pointer")
    {
      REQUIRE(dptr1.get() == p);
    }
  }

  GIVEN("A pointer-constructed cloning_ptr move-assigned to itself")
  {
    int v = 7;

    cloning_ptr<BaseType> dptr(new DerivedType(v));
    const auto p = dptr.get();

    REQUIRE(DerivedType::object_count == 1);

    dptr = std::move(dptr);

    THEN("The cloning_ptr is unaffected")
    {
      REQUIRE(DerivedType::object_count == 1);
      REQUIRE(dptr.get() == p);
    }
  }
}

TEST_CASE("Derived types", "[cloning_ptr.derived_types]")
{
  GIVEN("A cloning_ptr<BaseType> constructed from make_cloning_ptr<DerivedType>")
  {
    int v = 7;
    auto dptr = make_cloning_ptr<DerivedType>(v);

    WHEN("A cloning_ptr<BaseType> is copy-constructed")
    {
      cloning_ptr<BaseType> bptr(dptr);

      THEN("get returns a non-null pointer")
      {
        REQUIRE(bptr.get() != nullptr);
      }

      THEN("Operator-> calls the pointee method")
      {
        REQUIRE(bptr->value() == v);
      }

      THEN("operator bool returns true")
      {
        REQUIRE((bool)bptr == true);
      }
    }

    WHEN("A cloning_ptr<BaseType> is assigned")
    {
      cloning_ptr<BaseType> bptr;
      bptr = dptr;

      THEN("get returns a non-null pointer")
      {
        REQUIRE(bptr.get() != nullptr);
      }

      THEN("Operator-> calls the pointee method")
      {
        REQUIRE(bptr->value() == v);
      }

      THEN("operator bool returns true")
      {
        REQUIRE((bool)bptr == true);
      }
    }

    WHEN("A cloning_ptr<BaseType> is move-constructed")
    {
      cloning_ptr<BaseType> bptr(std::move(dptr));

      THEN("get returns a non-null pointer")
      {
        REQUIRE(bptr.get() != nullptr);
      }

      THEN("Operator-> calls the pointee method")
      {
        REQUIRE(bptr->value() == v);
      }

      THEN("operator bool returns true")
      {
        REQUIRE((bool)bptr == true);
      }
    }

    WHEN("A cloning_ptr<BaseType> is move-assigned")
    {
      cloning_ptr<BaseType> bptr;
      bptr = std::move(dptr);

      THEN("get returns a non-null pointer")
      {
        REQUIRE(bptr.get() != nullptr);
      }

      THEN("Operator-> calls the pointee method")
      {
        REQUIRE(bptr->value() == v);
      }

      THEN("operator bool returns true")
      {
        REQUIRE((bool)bptr == true);
      }
    }
  }
}

TEST_CASE("make_cloning_ptr return type can be converted to base-type", "[cloning_ptr.make_cloning_ptr]")
{
  GIVEN("A cloning_ptr<BaseType> constructed from make_cloning_ptr<DerivedType>")
  {
    int v = 7;
    cloning_ptr<BaseType> dptr = make_cloning_ptr<DerivedType>(v);

    THEN("get returns a non-null pointer")
    {
      REQUIRE(dptr.get() != nullptr);
    }

    THEN("Operator-> calls the pointee method")
    {
      REQUIRE(dptr->value() == v);
    }

    THEN("operator bool returns true")
    {
      REQUIRE((bool)dptr == true);
    }
  }
}

TEST_CASE("release","[cloning_ptr.release]")
{
  GIVEN("An empty cloning_ptr")
  {
    cloning_ptr<DerivedType> dptr;

    WHEN("release is called")
    {
      auto p = dptr.release();

      THEN("The cloning_ptr remains empty and the returned pointer is null")
      {
        REQUIRE(!dptr);
        REQUIRE(dptr.get()==nullptr);
        REQUIRE(p==nullptr);
      }
    }
  }

  GIVEN("A non-empty cloning_ptr")
  {
    int v = 7;
    cloning_ptr<DerivedType> dptr(new DerivedType(v));
    CHECK(DerivedType::object_count == 1);

    const auto op =dptr.get();


    WHEN("release is called")
    {
      auto p = dptr.release();
      std::unique_ptr<DerivedType> cleanup(p);
      CHECK(DerivedType::object_count == 1);

      THEN("The cloning_ptr is empty and the returned pointer is the previous pointer value")
      {
        REQUIRE(!dptr);
        REQUIRE(dptr.get()==nullptr);
        REQUIRE(p==op);
      }
    }
    CHECK(DerivedType::object_count == 0);
  }
}

TEST_CASE("reset","[cloning_ptr.reset]")
{
  GIVEN("An empty cloning_ptr")
  {
    cloning_ptr<DerivedType> dptr;

    WHEN("reset to null")
    {
      dptr.reset();

      THEN("The cloning_ptr remains empty")
      {
        REQUIRE(!dptr);
        REQUIRE(dptr.get()==nullptr);
      }
    }

    WHEN("reset to non-null")
    {
      int v = 7;
      dptr.reset(new DerivedType(v));

      CHECK(DerivedType::object_count == 1);

      THEN("The cloning_ptr is non-empty and owns the pointer")
      {
        REQUIRE(dptr);
        REQUIRE(dptr.get()!=nullptr);
        REQUIRE(dptr->value() == v);
      }
    }
  }
  CHECK(DerivedType::object_count == 0);

  GIVEN("A non-empty cloning_ptr")
  {
    int v1 = 7;
    cloning_ptr<DerivedType> dptr(new DerivedType(v1));
    CHECK(DerivedType::object_count == 1);

    WHEN("reset to null")
    {
      dptr.reset();
      CHECK(DerivedType::object_count == 0);

      THEN("The cloning_ptr is empty")
      {
        REQUIRE(!dptr);
        REQUIRE(dptr.get()==nullptr);
      }
    }

    WHEN("reset to non-null")
    {
      int v2 = 7;
      dptr.reset(new DerivedType(v2));
      CHECK(DerivedType::object_count == 1);

      THEN("The cloning_ptr is non-empty and owns the pointer")
      {
        REQUIRE(dptr);
        REQUIRE(dptr.get()!=nullptr);
        REQUIRE(dptr->value() == v2);
      }
    }
  }
}

TEST_CASE("Comparisons", "[cloning_ptr.comparisons]")
{
  GIVEN("Two pointer-constructed cloning_ptr")
  {
    int v1 = 0;
    int v2 = 1;

    cloning_ptr<BaseType> dptr1(new DerivedType(v1));
    cloning_ptr<BaseType> dptr2(new DerivedType(v2));

    THEN("Comparisons give same results as raw-pointer comparisons")
    {
      REQUIRE( (dptr1 == dptr2) == (dptr1.get() == dptr2.get()) );
      REQUIRE( (dptr1 != dptr2) == (dptr1.get() != dptr2.get()) );
      REQUIRE( (dptr1 < dptr2) == (dptr1.get() < dptr2.get()) );
      REQUIRE( (dptr1 > dptr2) == (dptr1.get() > dptr2.get()) );
      REQUIRE( (dptr1 <= dptr2) == (dptr1.get() <= dptr2.get()) );
      REQUIRE( (dptr1 >= dptr2) == (dptr1.get() >= dptr2.get()) );
    }
  }

  GIVEN("A nullptr_t and a pointer constructed cloning_ptr")
  {
    int v = 7;
    cloning_ptr<BaseType> dptr(new DerivedType(v));

    THEN("Comparisons give same results as raw-pointer comparisons")
    {
      REQUIRE( (dptr == nullptr) == (dptr.get() == nullptr) );
      REQUIRE( (dptr != nullptr) == (dptr.get() != nullptr) );
      REQUIRE( (dptr < nullptr) == (dptr.get() < nullptr) );
      REQUIRE( (dptr > nullptr) == (dptr.get() > nullptr) );
      REQUIRE( (dptr <= nullptr) == (dptr.get() <= nullptr) );
      REQUIRE( (dptr >= nullptr) == (dptr.get() >= nullptr) );

      REQUIRE((nullptr == dptr) == (nullptr == dptr.get()));
      REQUIRE( (nullptr != dptr) == (nullptr != dptr.get()) );
      REQUIRE( (nullptr < dptr) == (nullptr < dptr.get()) );
      REQUIRE( (nullptr > dptr) == (nullptr > dptr.get()) );
      REQUIRE( (nullptr <= dptr) == (nullptr <= dptr.get()) );
      REQUIRE( (nullptr >= dptr) == (nullptr >= dptr.get()) );
    }
  }
}

struct AlternativeBaseType {
  virtual ~AlternativeBaseType() = default;
  virtual int alternative_value() = 0;
};

class AlternativeDerivedType : public BaseType, public AlternativeBaseType {
  int value_;
public:
  AlternativeDerivedType(int value) : value_(value) {}

  int value() override { return value_; }
  void set_value(int v) override { value_ = v; }
  int alternative_value() override { return value_; }
};

TEST_CASE("cast operations", "[cloning_ptr.casts]")
{
  GIVEN("A pointer-constructed cloning_ptr<BaseType>")
  {
    int v = 7;
    cloning_ptr<BaseType> dptr(new DerivedType(v));
    REQUIRE(DerivedType::object_count == 1);

    WHEN("static_pointer_cast to the derived type is called")
    {
      auto st_dptr = std::static_pointer_cast<DerivedType>(dptr);

      THEN("The static-cast pointer is non-null")
      {
        REQUIRE(st_dptr);
      }
      THEN("The static-cast pointer has the required value")
      {
        REQUIRE(st_dptr->value() == v);
      }
      THEN("The static-cast pointer is distinct from the original pointer")
      {
        REQUIRE(st_dptr.get() != dptr.get());
      }
      THEN("Object count is increased")
      {
        REQUIRE(DerivedType::object_count == 2);
      }
    }
    WHEN("dynamic_pointer_cast to the derived type is called")
    {
      auto dyn_dptr = std::dynamic_pointer_cast<DerivedType>(dptr);

      THEN("The dynamic-cast pointer is non-null")
      {
        REQUIRE(dyn_dptr);
      }
      THEN("The dynamic-cast pointer has the required value")
      {
        REQUIRE(dyn_dptr->value() == v);
      }
      THEN("The dynamic-cast pointer is distinct from the original pointer")
      {
        REQUIRE(dyn_dptr.get() != dptr.get());
      }
      THEN("Object count is increased")
      {
        REQUIRE(DerivedType::object_count == 2);
      }
    }
    WHEN("dynamic_pointer_cast to the wrong derived type is called")
    {
      auto dyn_dptr = std::dynamic_pointer_cast<AlternativeDerivedType>(dptr);

      THEN("The dynamic-cast pointer is null")
      {
        REQUIRE(!dyn_dptr);
      }
      THEN("Object count is unchanged")
      {
        REQUIRE(DerivedType::object_count == 1);
      }
    }
  }
  GIVEN("A pointer-constructed cloning_ptr<const DerivedType>")
  {
    int v = 7;
    cloning_ptr<const DerivedType> cdptr(new DerivedType(v));
    REQUIRE(DerivedType::object_count == 1);

    WHEN("static_pointer_cast to the derived type is called")
    {
      auto dptr = std::const_pointer_cast<DerivedType>(cdptr);

      THEN("The static-cast pointer is non-null")
      {
        REQUIRE(dptr);
      }
      THEN("The static-cast pointer has the required value")
      {
        REQUIRE(dptr->value() == v);
      }
      THEN("The static-cast pointer is distinct from the original pointer")
      {
        REQUIRE(dptr.get() != cdptr.get());
      }
      THEN("Object count is increased")
      {
        REQUIRE(DerivedType::object_count == 2);
      }
    }
  }
  GIVEN("An AlternativeDerivedType-pointer-constructed cloning_ptr<BaseType>")
  {
    int v = 7;
    cloning_ptr<BaseType> dptr(new AlternativeDerivedType(v));

    WHEN("dynamic_pointer_cast to AlternativeBaseType is called")
    {
      auto dyn_dptr = std::dynamic_pointer_cast<AlternativeBaseType>(dptr);

      THEN("The dynamic-cast pointer is non-null")
      {
        REQUIRE(dyn_dptr);
      }
      THEN("The dynamic-cast pointer has the required value")
      {
        REQUIRE(dyn_dptr->alternative_value() == v);
      }
      THEN("The dynamic-cast pointer is distinct from the original pointer")
      {
        REQUIRE(dyn_dptr.get() != dynamic_cast<AlternativeBaseType*>(dptr.get()));
      }
    }
  }
}

struct Base { int v_ = 42; virtual ~Base() = default; };
struct IntermediateBaseA : virtual Base { int a_ = 3; };
struct IntermediateBaseB : virtual Base { int b_ = 101; };
struct MultiplyDerived : IntermediateBaseA, IntermediateBaseB { int value_ = 0; MultiplyDerived(int value) : value_(value) {}; };

TEST_CASE("Gustafsson's dilemma: multiple (virtual) base classes", "[cloning_ptr.constructors]")
{
  GIVEN("A value-constructed multiply-derived-class cloning_ptr")
  {
    int v = 7;
    cloning_ptr<MultiplyDerived> dptr(new MultiplyDerived(v));

    THEN("When copied to a cloning_ptr to an intermediate base type, data is accessible as expected")
    {
      cloning_ptr<IntermediateBaseA> dptr_IA = dptr;
      REQUIRE(dptr_IA->a_ == 3);
      REQUIRE(dptr_IA->v_ == 42);
    }

    THEN("When copied to a cloning_ptr to an intermediate base type, data is accessible as expected")
    {
      cloning_ptr<IntermediateBaseB> dptr_IB = dptr;
      REQUIRE(dptr_IB->b_ == 101);
      REQUIRE(dptr_IB->v_ == 42);
    }
  }
}
