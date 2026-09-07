#ifndef SHAREDPTR_H
#define SHAREDPTR_H
 
#include <memory>
 
template <typename T>
class WeakPtr;
 
template <typename T>
class EnableSharedFromThis;
 
struct IBaseControlBlock {
  size_t shared_count;
  size_t weak_count;
 
  IBaseControlBlock(size_t shared_count, size_t weak_count) : shared_count(shared_count), weak_count(weak_count) {
  }
 
  virtual void SharedDestroy() = 0;
  virtual void WeakDestroy() = 0;
  virtual ~IBaseControlBlock() = default;
};
 
template <typename T>
class SharedPtr {
 private:
  template <typename U = T, typename Alloc = std::allocator<U>>
  struct MadeSharedControlBlock : IBaseControlBlock {
    using MadeSharedCBAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<MadeSharedControlBlock>;
    using MadeSharedCBTraits = std::allocator_traits<MadeSharedCBAlloc>;
    MadeSharedCBAlloc alloc;
    alignas(U) char char_ptr[sizeof(U)];
 
    void SharedDestroy() override {
      Alloc default_alloc = alloc;
      std::allocator_traits<Alloc>::destroy(default_alloc, reinterpret_cast<U*>(char_ptr));
 
      if (this->weak_count == 0) {
        this->WeakDestroy();
      }
    }
 
    void WeakDestroy() override {
      MadeSharedCBTraits::deallocate(alloc, this, 1);
    }
 
    MadeSharedControlBlock(size_t shared_count, size_t weak_count, Alloc alloc)
        : IBaseControlBlock(shared_count, weak_count), alloc(alloc) {
    }
  };
 
  template <typename U = T, typename Deleter = std::default_delete<U>, typename Alloc = std::allocator<U>>
  struct DeleterControlBlock : IBaseControlBlock {
    using DeleterCBAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<DeleterControlBlock>;
    using DeleterCBTraits = std::allocator_traits<DeleterCBAlloc>;
    Deleter deleter;
    DeleterCBAlloc allocator;
    U* ptr;
 
    DeleterControlBlock(size_t shared_count, size_t weak_count, Deleter deleter, Alloc alloc, U* ptr)
        : IBaseControlBlock(shared_count, weak_count), deleter(deleter), allocator(alloc), ptr(ptr) {
    }
 
    void SharedDestroy() override {
      deleter(ptr);
 
      if (this->weak_count == 0) {
        this->WeakDestroy();
      }
    }
 
    void WeakDestroy() override {
      DeleterCBTraits::deallocate(allocator, this, 1);
    }
 
    ~DeleterControlBlock() override = default;
  };
 
 public:
  SharedPtr() noexcept : block_(nullptr), ptr_(nullptr) {
  }
 
  SharedPtr(const SharedPtr& other) noexcept : block_(other.block_), ptr_(other.ptr_) {
    if (block_ != nullptr) {
      ++block_->shared_count;
    }
  }
 
  SharedPtr(SharedPtr&& other) noexcept : block_(other.block_), ptr_(other.ptr_) {
    other.ptr_ = nullptr;
    other.block_ = nullptr;
  }
 
  SharedPtr& operator=(const SharedPtr& other) {
    return this->operator= <T>(other);
  }
 
  SharedPtr& operator=(SharedPtr&& other) noexcept {
    return this->operator= <T>(std::forward<SharedPtr<T>>(other));
  }
 
  // Base constructor
  template <typename U = T, typename Deleter, typename Alloc>
  SharedPtr(U* ptr, Deleter deleter, Alloc alloc) : ptr_(static_cast<T*>(ptr)) {
    typename DeleterControlBlock<U, Deleter, Alloc>::DeleterCBAlloc alloc1(alloc);
    auto block = DeleterControlBlock<U, Deleter, Alloc>::DeleterCBTraits::allocate(alloc1, 1);
    new (block) DeleterControlBlock<U, Deleter, Alloc>(1, 0, deleter, alloc, ptr);
 
    block_ = static_cast<IBaseControlBlock*>(block);
 
    if constexpr (std::is_base_of_v<EnableSharedFromThis<U>, U>) {
      auto enable_shared_ptr = static_cast<EnableSharedFromThis<U>*>(ptr);
      enable_shared_ptr->ptr_ = ptr;
      enable_shared_ptr->block_ = block;
    }
  }
 
  template <typename U = T>
  void reset(U* ptr) noexcept {
    this->operator= <T>(SharedPtr(ptr));
  }
 
  void reset() noexcept {
    this->operator= <T>(SharedPtr());
  }
 
  template <typename U = T, typename Deleter>
  SharedPtr(U* ptr, Deleter deleter) : SharedPtr(ptr, deleter, std::allocator<U>()) {
  }
 
  template <typename U = T>
  SharedPtr(U* ptr) : SharedPtr(ptr, std::default_delete<U>(), std::allocator<U>()) {
  }
 
  template <typename U = T>
  SharedPtr(const SharedPtr<U>& shared_ptr) noexcept
      : block_(const_cast<IBaseControlBlock*>(shared_ptr.block_)), ptr_(static_cast<T*>(shared_ptr.ptr_)) {
    ++block_->shared_count;
  }
 
  template <typename U = T>
  SharedPtr(SharedPtr<U>&& shared_ptr) noexcept
      : block_(const_cast<IBaseControlBlock*>(shared_ptr.block_)), ptr_(static_cast<T*>(shared_ptr.ptr_)) {
    shared_ptr.ptr_ = nullptr;
    shared_ptr.block_ = nullptr;
  }
 
  template <typename U = T>
  SharedPtr& operator=(const SharedPtr<U>& other) {
    auto other_ptr = static_cast<T*>(other.ptr_);
    if (other_ptr == ptr_) {
      return *this;
    }
 
    DecreaseAndDestroy();
 
    ptr_ = other_ptr;
    block_ = const_cast<IBaseControlBlock*>(other.block_);
    ++block_->shared_count;
    return *this;
  }
 
  template <typename U = T>
  SharedPtr& operator=(SharedPtr<U>&& other) {
    auto other_ptr = static_cast<T*>(other.ptr_);
    if (other_ptr == ptr_) {
      if (other_ptr != nullptr) {
        --block_->shared_count;
      }
      other.ptr_ = nullptr;
      other.block_ = nullptr;
      return *this;
    }
 
    DecreaseAndDestroy();
 
    ptr_ = other_ptr;
    block_ = const_cast<IBaseControlBlock*>(other.block_);
    other.ptr_ = nullptr;
    other.block_ = nullptr;
    return *this;
  }
 
  size_t use_count() const {
    if (block_ == nullptr) {
      throw std::runtime_error("use_count called on empty SharedPtr");
    }
    return block_->shared_count;
  }
 
  T& operator*() noexcept {
    return *ptr_;
  }
 
  const T& operator*() const noexcept {
    return *ptr_;
  }
 
  T* operator->() noexcept {
    return ptr_;
  }
 
  const T* operator->() const noexcept {
    return ptr_;
  }
 
  T* get() noexcept {
    return ptr_;
  }
 
  const T* get() const noexcept {
    return ptr_;
  }
 
  void swap(SharedPtr& other) noexcept {
    std::swap(ptr_, other.ptr_);
    std::swap(block_, other.block_);
  }
 
 private:
  IBaseControlBlock* block_;
  T* ptr_;
 
  // Make Shared constructor
  SharedPtr(IBaseControlBlock* block, T* ptr) noexcept : block_(block), ptr_(ptr) {
  }
 
  void DecreaseAndDestroy() {
    if (ptr_ == nullptr) {
      return;
    }
 
    if (--block_->shared_count > 0) {
      return;
    }
 
    block_->SharedDestroy();
  }
 
 public:
  ~SharedPtr() {
    DecreaseAndDestroy();
  }
 
  template <typename Type, typename Alloc, typename... Args>
  friend SharedPtr<Type> allocateShared(Alloc alloc, Args&&... args);
 
  template <class>
  friend class SharedPtr;
 
  template <class>
  friend class WeakPtr;
 
  template <class>
  friend class EnableSharedFromThis;
};
 
template <typename T, typename Alloc, typename... Args>
SharedPtr<T> allocateShared(Alloc alloc, Args&&... args) {
  using MadeSharedCB = typename SharedPtr<T>::template MadeSharedControlBlock<T, Alloc>;
  using MadeSharedCBAlloc = typename MadeSharedCB::MadeSharedCBAlloc;
  using MadeSharedCBTraits = std::allocator_traits<MadeSharedCBAlloc>;
 
  MadeSharedCBAlloc alloc2 = alloc;
  MadeSharedCB* made_shared_cb_ptr = MadeSharedCBTraits::allocate(alloc2, 1);
  new (made_shared_cb_ptr) MadeSharedCB(1, 0, alloc);
  T* t_ptr = reinterpret_cast<T*>(made_shared_cb_ptr->char_ptr);
  std::allocator_traits<Alloc>::construct(alloc, t_ptr, std::forward<Args>(args)...);
 
  auto base_cb_ptr = static_cast<IBaseControlBlock*>(made_shared_cb_ptr);
 
  if constexpr (std::is_base_of_v<EnableSharedFromThis<T>, T>) {
    auto enable_shared_ptr = static_cast<EnableSharedFromThis<T>*>(t_ptr);
    enable_shared_ptr->ptr_ = t_ptr;
    enable_shared_ptr->block_ = made_shared_cb_ptr;
  }
 
  return SharedPtr(base_cb_ptr, t_ptr);
}
 
template <typename T, typename... Args>
SharedPtr<T> makeShared(Args&&... args) {
  return allocateShared<T, std::allocator<T>, Args...>(std::allocator<T>(), std::forward<Args>(args)...);
}
 
template <typename T>
class WeakPtr {
 public:
  WeakPtr() : ptr_(nullptr), block_(nullptr) {
  }
 
  template <typename U = T>
  WeakPtr(const SharedPtr<U>& shared_ptr)
      : ptr_(static_cast<T*>(shared_ptr.ptr_)), block_(reinterpret_cast<decltype(block_)>(shared_ptr.block_)) {
    ++block_->weak_count;
  }
 
  WeakPtr(const WeakPtr& other) : ptr_(other.ptr_), block_(other.block_) {
    if (block_ != nullptr) {
      ++block_->weak_count;
    }
  }
 
  WeakPtr(WeakPtr&& other) noexcept : ptr_(other.ptr_), block_(other.block_) {
    other.ptr_ = nullptr;
    other.block_ = nullptr;
  }
 
  WeakPtr& operator=(const WeakPtr& other) {
    return this->operator= <T>(other);
  }
 
  WeakPtr& operator=(WeakPtr&& other) noexcept {
    return this->operator= <T>(std::forward<WeakPtr<T>>(other));
  }
 
  template <typename U = T>
  WeakPtr(const WeakPtr<U>& weak_ptr)
      : ptr_(static_cast<T*>(weak_ptr.ptr_)), block_(reinterpret_cast<decltype(block_)>(weak_ptr.block_)) {
    ++block_->weak_count;
  }
 
  template <typename U = T>
  WeakPtr(WeakPtr<U>&& weak_ptr)
      : ptr_(static_cast<T*>(weak_ptr.ptr_)), block_(reinterpret_cast<decltype(block_)>(weak_ptr.block_)) {
    weak_ptr.ptr_ = nullptr;
    weak_ptr.block_ = nullptr;
  }
 
  template <typename U = T>
  WeakPtr& operator=(const WeakPtr<U>& other) {
    auto other_ptr = static_cast<T*>(other.ptr_);
    if (other_ptr == ptr_) {
      return *this;
    }
 
    DecreaseAndDestroy();
 
    ptr_ = other_ptr;
    block_ = reinterpret_cast<decltype(block_)>(other.block_);
    ++block_->weak_count;
    return *this;
  }
 
  template <typename U = T>
  WeakPtr& operator=(WeakPtr<U>&& other) {
    auto other_ptr = static_cast<T*>(other.ptr_);
    if (other_ptr == ptr_) {
      if (other.ptr_ != nullptr) {
        --block_->weak_count;
      }
      other.ptr_ = nullptr;
      other.block_ = nullptr;
      return *this;
    }
 
    DecreaseAndDestroy();
 
    ptr_ = other_ptr;
    block_ = reinterpret_cast<decltype(block_)>(other.block_);
    other.ptr_ = nullptr;
    other.block_ = nullptr;
    return *this;
  }
 
  bool expired() const {
    return block_ == nullptr || block_->shared_count == 0;
  }
 
  SharedPtr<T> lock() const {
    if (expired()) {
      throw std::bad_weak_ptr();
    }
    ++block_->shared_count;
    return SharedPtr<T>(block_, ptr_);
  }
 
  size_t use_count() const {
    if (block_ == nullptr) {
      throw std::runtime_error("use_count called on empty SharedPtr");
    }
    return block_->shared_count;
  }
 
  ~WeakPtr() {
    DecreaseAndDestroy();
  }
 
 private:
  T* ptr_;
  IBaseControlBlock* block_;
 
  void DecreaseAndDestroy() {
    if (ptr_ == nullptr) {
      return;
    }
 
    if (--block_->weak_count == 0 && block_->shared_count == 0) {
      block_->WeakDestroy();
    }
  }
 
  template <class>
  friend class WeakPtr;
};
 
template <typename T>
class EnableSharedFromThis {
 public:
  EnableSharedFromThis() : ptr_(nullptr), block_(nullptr) {
  }
 
  SharedPtr<T> shared_from_this() {
    if (block_ == nullptr) {
      throw std::runtime_error("No SharedPtr asigned to this object");
    }
 
    ++block_->shared_count;
    return SharedPtr<T>(block_, ptr_);
  }
 
 private:
  T* ptr_;
  typename SharedPtr<T>::IBaseControlBlock* block_;
 
  template <class>
  friend class SharedPtr;
};
 
#endif  // SHAREDPTR_H
