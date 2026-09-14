#ifndef STACKALLOCATOR_H
#define STACKALLOCATOR_H

#include <cstdint>
#include <memory>
#include <cstddef>
#include <type_traits>
#include <iterator>

template <std::size_t N>
class StackStorage {
    char mem[N];
    char* current;

public:
    StackStorage() noexcept : mem(), current{mem} {
    }

    StackStorage(const StackStorage&) = delete;

    void* allocate_raw(std::size_t alignment, std::size_t size) {
        size_t space = N - (current - mem);
        if (std::align(alignment, size, reinterpret_cast<void*&>(current), space) == nullptr) {
            return nullptr;
        }
        current += size;
        return current - size;
    }
};

template <typename T, std::size_t N>
class StackAllocator {
    StackStorage<N>* storage;

public:
    using value_type = T;

    StackAllocator() {
        storage = new StackStorage<N>();
    }

    explicit StackAllocator(StackStorage<N>& storage) : storage{&storage} {
    }

    template <typename U>
    StackAllocator(const StackAllocator<U, N>& other) : storage{other.storage} {
    }

    T* allocate(std::size_t count) {
        return reinterpret_cast<T*>(
            storage->allocate_raw(alignof(T), sizeof(T) * count)
        );
    }

    void deallocate(T*, std::size_t) noexcept {
    }

    template <typename U, std::size_t M>
    friend class StackAllocator;

    template <typename U>
    struct rebind {
        using other = StackAllocator<U, N>;
    };

    bool operator==(const StackAllocator&) const = default;
    bool operator!=(const StackAllocator&) const = default;
};

template <typename T, typename Alloc = std::allocator<T>>
struct List {
private:
    struct BaseNode {
        BaseNode* previous;
        BaseNode* next;
    };

    struct Node : BaseNode {
        T value;

        Node(const T& value_) : value(value_) {
        }

        Node() = default;
    };

    template <bool IsConst>
    struct Iterator {
    public:
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::bidirectional_iterator_tag;
        using pointer = std::conditional_t<IsConst, const T*, T*>;
        using reference = std::conditional_t<IsConst, const T&, T&>;
        using node_type = std::conditional_t<IsConst, const Node*, Node*>;
        using base_node_type = std::conditional_t<IsConst, BaseNode*, BaseNode*>;

    private:
        BaseNode* current_node;

    public:
        friend struct List;

        Iterator(base_node_type node) : current_node(node) {
        }

        Iterator(const Iterator<false>& iterator)
            requires(IsConst)
            : Iterator(iterator.current_node) {
        }

        Iterator(const Iterator<IsConst>& iterator) = default;

        Iterator& operator=(const Iterator<false>& iterator) {
            current_node = iterator.current_node;
            return *this;
        }

        reference operator*() {
            return static_cast<node_type>(current_node)->value;
        }

        pointer operator->() {
            return &(*(*this));
        }

        Iterator& operator++() {
            current_node = current_node->next;
            return *this;
        }

        Iterator& operator--() {
            current_node = current_node->previous;
            return *this;
        }

        Iterator operator++(int) {
            Iterator temp = *this;
            ++*this;
            return temp;
        }

        Iterator operator--(int) {
            Iterator temp = *this;
            --(*this);
            return temp;
        }

        ~Iterator() = default;

        bool operator==(const Iterator& other) const {
            return other.current_node == current_node;
        }

        bool operator!=(const Iterator& other) const = default;
    };

    BaseNode* fake_node;
    std::size_t size_ = 0;

public:
    using node_alloc = std::allocator_traits<Alloc>::template rebind_alloc<Node>;
    using node_traits = std::allocator_traits<node_alloc>;
    using iterator = Iterator<false>;
    using const_iterator = Iterator<true>;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using difference_type = std::ptrdiff_t;

    [[no_unique_address]] node_alloc alloc_;

    size_t size() const {
        return size_;
    }

    iterator insert(const_iterator pos, const T& value) {
        Node* node = node_traits::allocate(alloc_, 1);
        try {
            node_traits::construct(alloc_, node, value);
        }
        catch (...) {
            node_traits::deallocate(alloc_, node, 1);
            throw;
        }

        auto* base = static_cast<BaseNode*>(node);
        base->previous = pos.current_node->previous;
        base->next = pos.current_node;
        base->previous->next = base;
        base->next->previous = base;

        ++size_;
        return iterator(node);
    }

    iterator begin() {
        return fake_node->next;
    }

    const_iterator begin() const {
        return fake_node->next;
    }

    iterator end() {
        return iterator(fake_node);
    }

    const_iterator end() const {
        return const_iterator(fake_node);
    }

    reverse_iterator rbegin() {
        return std::reverse_iterator(end());
    }

    const_reverse_iterator rbegin() const {
        return std::reverse_iterator(end());
    }

    reverse_iterator rend() {
        return std::reverse_iterator(begin());
    }

    const_reverse_iterator rend() const {
        return std::reverse_iterator(begin());
    }

    const_iterator cbegin() const {
        return begin();
    }

    const_iterator cend() const {
        return end();
    }

    const_reverse_iterator crbegin() const {
        return rbegin();
    }

    const_reverse_iterator crend() const {
        return rend();
    }

    iterator erase(const_iterator pos) {
        pos.current_node->previous->next = pos.current_node->next;
        pos.current_node->next->previous = pos.current_node->previous;

        BaseNode* next_node = pos.current_node->next;

        node_traits::destroy(alloc_, static_cast<Node*>(pos.current_node));
        node_traits::deallocate(alloc_, static_cast<Node*>(pos.current_node), 1);

        --size_;
        return next_node;
    }

    void pop_back() {
        erase(--end());
    }

    void push_back(const T& value) {
        insert(end(), value);
    }

    void pop_front() {
        erase(begin());
    }

    void push_front(const T& value) {
        insert(begin(), value);
    }

    void clear() {
        while (size() > 0) {
            pop_front();
        }
    }

    List() {
        size_ = 0;
        fake_node = new BaseNode();
        fake_node->next = fake_node;
        fake_node->previous = fake_node;
    }

    explicit List(Alloc alloc) : List() {
        alloc_ = alloc;
    }

    explicit List(size_t size) : List() {
        try {
            while (size--) {
                insert(end());
            }
        }
        catch (...) {
            clear();
            throw;
        }
    }

    List(size_t size, Alloc alloc) : List() {
        alloc_ = alloc;
        try {
            while (size--) {
                insert(end());
            }
        }
        catch (...) {
            clear();
            throw;
        }
    }

    ~List() {
        clear();
    }

    List(size_t count, const T& value) : List(count, value, Alloc()) {
    }

    List(size_t n, const T& value, Alloc alloc_) : List(alloc_) {
        while (size() < n) {
            push_back(value);
        }
    }

    List(const List& other)
        : List(node_traits::select_on_container_copy_construction(other.alloc_)) {
        try {
            for (auto it = other.begin(); it != other.end(); ++it) {
                push_back(*it);
            }
        }
        catch (...) {
            clear();
            throw;
        }
    }

    List& operator=(const List& other) {
        if (this == &other) {
            return *this;
        }

        List copy = other;
        this->Swap(copy);

        if constexpr (node_traits::propagate_on_container_copy_assignment::value) {
            if (alloc_ != other.alloc_) {
                clear();
            }
            alloc_ = other.alloc_;
        }

        return *this;
    }

    Alloc get_allocator() const {
        return alloc_;
    }

private:
    void Swap(List& other) {
        std::swap(fake_node, other.fake_node);
        std::swap(size_, other.size_);
        std::swap(alloc_, other.alloc_);
    }

    iterator insert(iterator pos) {
        Node* node = node_traits::allocate(alloc_, 1);
        try {
            node_traits::construct(alloc_, node);
        }
        catch (...) {
            node_traits::deallocate(alloc_, node, 1);
            throw;
        }

        auto* base = static_cast<BaseNode*>(node);
        base->previous = pos.current_node->previous;
        base->next = pos.current_node;
        base->previous->next = base;
        base->next->previous = base;

        ++size_;
        return iterator(node);
    }
};

#endif  // STACKALLOCATOR_H
