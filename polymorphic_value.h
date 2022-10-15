/* Copyright (c) 2016 The Polymorphic Value Authors. All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
==============================================================================*/

#ifndef ISOCPP_P0201_POLYMORPHIC_VALUE_H_INCLUDED
#define ISOCPP_P0201_POLYMORPHIC_VALUE_H_INCLUDED

#include <cassert>
#include <exception>
#include <memory>
#include <type_traits>
#include <typeinfo>
#include <utility>

#if (__cpp_constexpr >= 202002)
#define ISOCPP_P0201_CONSTEXPR_CXX20 constexpr
#else
#define ISOCPP_P0201_CONSTEXPR_CXX20
#endif

namespace isocpp_p0201 {

namespace detail {

////////////////////////////////////////////////////////////////////////////
// Implementation detail classes
////////////////////////////////////////////////////////////////////////////

template <class T, class = void>
struct copier_traits_deleter_base {};

template <class T>
struct copier_traits_deleter_base<T, std::void_t<typename T::deleter_type>> {
  using deleter_type = typename T::deleter_type;
};

template <class U, class V>
struct copier_traits_deleter_base<U* (*)(V)> {
  using deleter_type = void (*)(U*);
};

class control_block_deleter {
 public:
  template <class T>
  constexpr void operator()(T* t) const noexcept {
    if (t != nullptr) {
      t->destroy();
    }
  }
};

template <class T>
struct control_block {
  ISOCPP_P0201_CONSTEXPR_CXX20 virtual ~control_block() = default;

  ISOCPP_P0201_CONSTEXPR_CXX20 virtual std::unique_ptr<control_block,
                                                       control_block_deleter>
  clone() const = 0;

  ISOCPP_P0201_CONSTEXPR_CXX20 virtual T* ptr() = 0;

  ISOCPP_P0201_CONSTEXPR_CXX20 virtual void destroy() noexcept { delete this; }
};

template <class T, class U = T>
class direct_control_block : public control_block<T> {
  static_assert(!std::is_reference<U>::value, "");
  U u_;

 public:
  template <class... Ts>
  constexpr explicit direct_control_block(Ts&&... ts)
      : u_(U(std::forward<Ts>(ts)...)) {}

  constexpr std::unique_ptr<control_block<T>, control_block_deleter> clone()
      const override {
    return std::unique_ptr<direct_control_block, control_block_deleter>(
        new direct_control_block(*this));
  }

  constexpr T* ptr() override { return std::addressof(u_); }
};

template <class T, class U, class C, class D>
class pointer_control_block : public control_block<T>, public C {
  std::unique_ptr<U, D> p_;

 public:
  constexpr explicit pointer_control_block(U* u, C c, D d)
      : C(std::move(c)), p_(u, std::move(d)) {}

  constexpr explicit pointer_control_block(std::unique_ptr<U, D> p, C c)
      : C(std::move(c)), p_(std::move(p)) {}

  constexpr std::unique_ptr<control_block<T>, control_block_deleter> clone()
      const override {
    assert(p_);
    return std::unique_ptr<pointer_control_block, control_block_deleter>(
        new pointer_control_block(C::operator()(*p_),
                                  static_cast<const C&>(*this),
                                  p_.get_deleter()));
  }

  constexpr T* ptr() override { return p_.get(); }
};

template <class T, class U>
class delegating_control_block : public control_block<T> {
  std::unique_ptr<control_block<U>, control_block_deleter> delegate_;

 public:
  constexpr explicit delegating_control_block(
      std::unique_ptr<control_block<U>, control_block_deleter> b)
      : delegate_(std::move(b)) {}

  constexpr std::unique_ptr<control_block<T>, control_block_deleter> clone()
      const override {
    return std::unique_ptr<delegating_control_block, control_block_deleter>(
        new delegating_control_block(delegate_->clone()));
  }

  constexpr T* ptr() override { return delegate_->ptr(); }
};

template <typename A>
struct allocator_wrapper : A {
  constexpr allocator_wrapper(A& a) : A(a) {}

  constexpr const A& get_allocator() const {
    return static_cast<const A&>(*this);
  }
};

template <typename T, typename A, typename... Args>
ISOCPP_P0201_CONSTEXPR_CXX20 T* allocate_object(A& a, Args&&... args) {
  using t_allocator =
      typename std::allocator_traits<A>::template rebind_alloc<T>;
  using t_traits = std::allocator_traits<t_allocator>;
  t_allocator t_alloc(a);
  T* mem = t_traits::allocate(t_alloc, 1);
  try {
    t_traits::construct(t_alloc, mem, std::forward<Args>(args)...);
    return mem;
  } catch (...) {
    t_traits::deallocate(t_alloc, mem, 1);
    throw;
  }
}

template <typename T, typename A>
constexpr void deallocate_object(A& a, T* p) {
  using t_allocator =
      typename std::allocator_traits<A>::template rebind_alloc<T>;
  using t_traits = std::allocator_traits<t_allocator>;
  t_allocator t_alloc(a);
  t_traits::destroy(t_alloc, p);
  t_traits::deallocate(t_alloc, p, 1);
}

template <class T, class U, class A>
class allocated_pointer_control_block : public control_block<T>,
                                        allocator_wrapper<A> {
  U* p_;

 public:
  constexpr explicit allocated_pointer_control_block(U* u, A a)
      : allocator_wrapper<A>(a), p_(u) {}

  ISOCPP_P0201_CONSTEXPR_CXX20 ~allocated_pointer_control_block() {
    detail::deallocate_object(this->get_allocator(), p_);
  }

  ISOCPP_P0201_CONSTEXPR_CXX20
      std::unique_ptr<control_block<T>, control_block_deleter>
      clone()
      const override {
    assert(p_);

    auto* cloned_ptr = detail::allocate_object<U>(this->get_allocator(), *p_);
    try {
      auto* new_cb = detail::allocate_object<allocated_pointer_control_block>(
          this->get_allocator(), cloned_ptr, this->get_allocator());
      return std::unique_ptr<control_block<T>, control_block_deleter>(new_cb);
    } catch (...) {
      detail::deallocate_object(this->get_allocator(), cloned_ptr);
      throw;
    }
  }

  ISOCPP_P0201_CONSTEXPR_CXX20 T* ptr() override { return p_; }

  ISOCPP_P0201_CONSTEXPR_CXX20 void destroy() noexcept override {
    detail::deallocate_object(this->get_allocator(), this);
  }
};

}  // end namespace detail

template <class T>
struct default_copy {
  using deleter_type = std::default_delete<T>;
  constexpr T* operator()(const T& t) const { return new T(t); }
};

template <class T>
struct copier_traits : detail::copier_traits_deleter_base<T, void> {};

class bad_polymorphic_value_construction : public std::exception {
 public:
  bad_polymorphic_value_construction() noexcept = default;

  const char* what() const noexcept override {
    return "Dynamic and static type mismatch in polymorphic_value "
           "construction";
  }
};

template <class T>
class polymorphic_value;

template <class T>
struct is_polymorphic_value : std::false_type {};

template <class T>
struct is_polymorphic_value<polymorphic_value<T>> : std::true_type {};

////////////////////////////////////////////////////////////////////////////////
// `polymorphic_value` class definition
////////////////////////////////////////////////////////////////////////////////

template <class T>
class polymorphic_value {
  static_assert(!std::is_union<T>::value, "");
  static_assert(std::is_class<T>::value, "");

  template <class U>
  friend class polymorphic_value;

  template <class T_, class U, class... Ts>
  friend constexpr polymorphic_value<T_> make_polymorphic_value(Ts&&... ts);

  template <class T_, class U, class A, class... Ts>
  friend constexpr polymorphic_value<T_> allocate_polymorphic_value(
      std::allocator_arg_t, A& a, Ts&&... ts);

  T* ptr_ = nullptr;
  std::unique_ptr<detail::control_block<T>, detail::control_block_deleter> cb_;

 public:
  //
  // Destructor
  //

  ISOCPP_P0201_CONSTEXPR_CXX20 ~polymorphic_value() = default;

  //
  // Constructors
  //

  constexpr polymorphic_value() {}

  template <class U, class C, class D,
            class = std::enable_if_t<std::is_convertible_v<U*, T*>>>
  explicit ISOCPP_P0201_CONSTEXPR_CXX20 polymorphic_value(U* u, C copier,
                                                          D deleter) {
    if (!u) {
      return;
    }

#ifndef ISOCPP_P0201_POLYMORPHIC_VALUE_NO_RTTI
    if (std::is_same<D, std::default_delete<U>>::value &&
        std::is_same<C, default_copy<U>>::value && typeid(*u) != typeid(U))
      throw bad_polymorphic_value_construction();
#endif
    std::unique_ptr<U, D> p(u, std::move(deleter));

    cb_ = std::unique_ptr<detail::pointer_control_block<T, U, C, D>,
                          detail::control_block_deleter>(
        new detail::pointer_control_block<T, U, C, D>(std::move(p),
                                                      std::move(copier)));
    ptr_ = u;
  }

  template <class U, class C, class D = typename copier_traits<C>::deleter_type,
            class V = std::enable_if_t<std::is_convertible_v<U*, T*> &&
                                       std::is_default_constructible_v<D> &&
                                       !std::is_pointer_v<D>>>
  explicit constexpr polymorphic_value(U* u, C copier)
      : polymorphic_value(u, std::move(copier), D{}) {}

  template <
      class U, class C = default_copy<U>,
      class D = typename copier_traits<C>::deleter_type,
      class = std::enable_if_t<
          std::is_convertible_v<U*, T*> && std::is_default_constructible_v<C> &&
          std::is_default_constructible_v<D> && !std::is_pointer_v<D>>>
  explicit constexpr polymorphic_value(U* u) : polymorphic_value(u, C{}, D{}) {}

  template <class U, class A,
            class = std::enable_if_t<std::is_convertible_v<U*, T*>>>
  ISOCPP_P0201_CONSTEXPR_CXX20 constexpr polymorphic_value(U* u,
                                                           std::allocator_arg_t,
                                       const A& alloc) {
    if (!u) {
      return;
    }

#ifndef ISOCPP_P0201_POLYMORPHIC_VALUE_NO_RTTI
    if (typeid(*u) != typeid(U)) throw bad_polymorphic_value_construction();
#endif

    cb_ = std::unique_ptr<detail::allocated_pointer_control_block<T, U, A>,
                          detail::control_block_deleter>(
        detail::allocate_object<
            detail::allocated_pointer_control_block<T, U, A>>(alloc, u, alloc));
    ptr_ = u;
  }

  //
  // Copy-constructors
  //

  constexpr polymorphic_value(const polymorphic_value& p) {
    if (!p) {
      return;
    }
    auto tmp_cb = p.cb_->clone();
    ptr_ = tmp_cb->ptr();
    cb_ = std::move(tmp_cb);
  }

  //
  // Move-constructors
  //

  constexpr polymorphic_value(polymorphic_value&& p) noexcept {
    ptr_ = p.ptr_;
    cb_ = std::move(p.cb_);
    p.ptr_ = nullptr;
  }

  //
  // Converting constructors
  //

  template <class U,
            class V = std::enable_if_t<!std::is_same<T, U>::value &&
                                       std::is_convertible<U*, T*>::value>>
  ISOCPP_P0201_CONSTEXPR_CXX20 explicit polymorphic_value(
      const polymorphic_value<U>& p) {
    polymorphic_value<U> tmp(p);
    ptr_ = tmp.ptr_;
    cb_ = std::unique_ptr<detail::delegating_control_block<T, U>,
                          detail::control_block_deleter>(
        new detail::delegating_control_block<T, U>(std::move(tmp.cb_)));
  }

  template <class U,
            class V = std::enable_if_t<!std::is_same<T, U>::value &&
                                       std::is_convertible<U*, T*>::value>>
  ISOCPP_P0201_CONSTEXPR_CXX20 explicit polymorphic_value(
      polymorphic_value<U>&& p) {
    ptr_ = p.ptr_;
    cb_ = std::unique_ptr<detail::delegating_control_block<T, U>,
                          detail::control_block_deleter>(
        new detail::delegating_control_block<T, U>(std::move(p.cb_)));
    p.ptr_ = nullptr;
  }

  //
  // In-place constructor
  //

  template <class U,
            class V = std::enable_if_t<
                std::is_convertible<std::decay_t<U>*, T*>::value &&
                !is_polymorphic_value<std::decay_t<U>>::value>,
            class... Ts>
  ISOCPP_P0201_CONSTEXPR_CXX20 explicit polymorphic_value(
      std::in_place_type_t<U>, Ts&&... ts)
      : cb_(std::unique_ptr<detail::direct_control_block<T, U>,
                            detail::control_block_deleter>(
            new detail::direct_control_block<T, U>(std::forward<Ts>(ts)...))) {
    ptr_ = cb_->ptr();
  }

  //
  // Assignment
  //

  constexpr polymorphic_value& operator=(const polymorphic_value& p) {
    if (std::addressof(p) == this) {
      return *this;
    }

    if (!p) {
      cb_.reset();
      ptr_ = nullptr;
      return *this;
    }

    auto tmp_cb = p.cb_->clone();
    ptr_ = tmp_cb->ptr();
    cb_ = std::move(tmp_cb);
    return *this;
  }

  //
  // Move-assignment
  //

  constexpr polymorphic_value& operator=(polymorphic_value&& p) noexcept {
    if (std::addressof(p) == this) {
      return *this;
    }

    cb_ = std::move(p.cb_);
    ptr_ = p.ptr_;
    p.ptr_ = nullptr;
    return *this;
  }

  //
  // Modifiers
  //

  constexpr void swap(polymorphic_value& p) noexcept {
    using std::swap;
    swap(ptr_, p.ptr_);
    swap(cb_, p.cb_);
  }

  //
  // Observers
  //

  constexpr explicit operator bool() const { return bool(cb_); }

  constexpr const T* operator->() const {
    assert(ptr_);
    return ptr_;
  }

  constexpr const T& operator*() const {
    assert(*this);
    return *ptr_;
  }

  constexpr T* operator->() {
    assert(*this);
    return ptr_;
  }

  constexpr T& operator*() {
    assert(*this);
    return *ptr_;
  }
};

//
// polymorphic_value creation
//
template <class T, class U = T, class... Ts>
ISOCPP_P0201_CONSTEXPR_CXX20 polymorphic_value<T> make_polymorphic_value(
    Ts&&... ts) {
  polymorphic_value<T> p;
  p.cb_ = std::unique_ptr<detail::direct_control_block<T, U>,
                          detail::control_block_deleter>(
      new detail::direct_control_block<T, U>(std::forward<Ts>(ts)...));
  p.ptr_ = p.cb_->ptr();
  return p;
}

template <class T, class U = T, class A = std::allocator<U>, class... Ts>
ISOCPP_P0201_CONSTEXPR_CXX20 polymorphic_value<T> allocate_polymorphic_value(
    std::allocator_arg_t,
                                                          A& a, Ts&&... ts) {
  polymorphic_value<T> p;
  auto* u = detail::allocate_object<U>(a, std::forward<Ts>(ts)...);
  try {
    p.cb_ = std::unique_ptr<detail::allocated_pointer_control_block<T, U, A>,
                            detail::control_block_deleter>(
        detail::allocate_object<
            detail::allocated_pointer_control_block<T, U, A>>(a, u, a));
  } catch (...) {
    detail::deallocate_object(a, u);
    throw;
  }
  p.ptr_ = p.cb_->ptr();
  return p;
}

//
// non-member swap
//
template <class T>
constexpr void swap(polymorphic_value<T>& t, polymorphic_value<T>& u) noexcept {
  t.swap(u);
}

}  // namespace isocpp_p0201

#endif  // ISOCPP_P0201_POLYMORPHIC_VALUE_H_INCLUDED
