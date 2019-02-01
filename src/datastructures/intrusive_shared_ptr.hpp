//
// Created by lukas on 18.05.18.
//

#ifndef TCPSPSUITE_INTRUSIVE_SHARED_PTR_HPP
#define TCPSPSUITE_INTRUSIVE_SHARED_PTR_HPP

#include <list>

template <class T, class Accessor, bool safe_mode = false>
class IntrusiveSharedPtr {
public:
  using own_type = IntrusiveSharedPtr<T, Accessor, safe_mode>;

  IntrusiveSharedPtr(const own_type & other) noexcept : content(other.content)
  {
    if (this->content != nullptr) {
      Accessor::increment(this->content);
    }
  };

  IntrusiveSharedPtr(own_type && other) noexcept : content(other.content)
  {
    other.content = nullptr;
  };

  IntrusiveSharedPtr(T * content_new) noexcept : content(content_new)
  {
    // TODO safety-assert
    if (content_new != nullptr) {
      Accessor::set_count(this->content, 1);
    }
  };

  IntrusiveSharedPtr() noexcept { this->content = nullptr; };

  ~IntrusiveSharedPtr()
  {
    if (this->content == nullptr)
      return;

    // TODO safety-assert
    Accessor::decrement(this->content);
    if (Accessor::get_count(this->content) == 0) {
      Accessor::deallocate(this->content);
    }
  };

  void
  reset(T * content_new) noexcept
  {
    if (this->content != nullptr) {
      // TODO safety-assert
      Accessor::decrement(this->content);
      if (Accessor::get_count(this->content) == 0) {
	Accessor::deallocate(this->content);
      }
    }

    this->content = content_new;
    // TODO safety-assert
    if (this->content != nullptr) {
      Accessor::set_count(content_new, 1);
    }
  };

  T & operator*() const noexcept { return *(this->content); };
  T * operator->() const noexcept { return this->content; };

  T *
  get() const noexcept
  {
    return this->content;
  };

  own_type &
  operator=(const own_type & other)
  {
    this->content = other.content;
    if (this->content != nullptr) {
      Accessor::increment(this->content);
    }

    return *this;
  }

  own_type &
  operator=(own_type && other)
  {
    this->content = other.content;
    other.content = nullptr;

    return *this;
  }

private:
  T * content;
};

template <class T>
class SharedPtrPool {
private:
  class ContainerAccessor;

public:
  using PointerT = IntrusiveSharedPtr<T, ContainerAccessor>;

  class Container : public T {
  public:
    Container(SharedPtrPool<T> * pool_new) : T(), pool(pool_new), count(0) {}
    Container() = delete;

  private:
    SharedPtrPool<T> * pool;
    size_t count;

    friend class ContainerAccessor;
    friend class SharedPtrPool<T>;
  };

  IntrusiveSharedPtr<T, ContainerAccessor>
  get() noexcept
  {
    if (this->free.empty()) {
      this->store.push_back(
          std::vector<Container>(this->chunk_size, Container(this)));
      for (Container & c : this->store.back()) {
	this->free.push_back(&c);
      }
    }

    Container * c = this->free.back();
    this->free.pop_back();
    return IntrusiveSharedPtr<T, ContainerAccessor>((T *)c);
  }

  SharedPtrPool() : chunk_size(10000) {}
  SharedPtrPool(size_t chunk_size_new) : chunk_size(chunk_size_new) {}

private:
  class ContainerAccessor {
  public:
    static size_t
    get_count(const T * c) noexcept
    {
      return ((const Container *)c)->Container::count;
    }
    static void
    set_count(T * c, size_t count) noexcept
    {
      ((Container *)c)->Container::count = count;
    }
    static void
    increment(T * c) noexcept
    {
      ((Container *)c)->Container::count++;
    }
    static void
    decrement(T * c) noexcept
    {
      ((Container *)c)->Container::count--;
    }
    static void
    deallocate(T * c) noexcept
    {
      ((Container *)c)->Container::pool->free.push_back((Container *)c);
      // TODO safety-assert?
    }
  };

  std::list<std::vector<Container>> store;
  std::vector<Container *> free;

  size_t chunk_size;
};

#endif // TCPSPSUITE_INTRUSIVE_SHARED_PTR_HPP
