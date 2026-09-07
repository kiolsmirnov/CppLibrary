#ifndef DEQUE_H
#define DEQUE_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <vector>

template <typename T>
class Deque {
public:
    Deque() = default;

    explicit Deque(const int size) : size_(size) {
        int64_t jend = size / kSubVectorSize + (size % kSubVectorSize != 0);

        for (int j = 1; j <= jend; ++j) {
            ptrs_.push_back(Allocate());
            int64_t iend = j < jend
                ? kSubVectorSize
                : size % kSubVectorSize != 0 ? size % kSubVectorSize
                : kSubVectorSize;
            for (int i = 0; i < iend; ++i) {
                try {
                    new (ptrs_.back() + i) T();
                }
                catch (...) {
                    size_ = 0;
                    for (int l = 1; l <= j; ++l) {
                        for (int k = 0; k < (l == j ? i : kSubVectorSize); ++k) {
                            ptrs_[l - 1][k].~T();
                        }
                        Dealloc(ptrs_[l - 1]);
                    }
                    throw;
                }
            }
        }

        back_size_ =
            size % kSubVectorSize != 0 ? size % kSubVectorSize : kSubVectorSize;
    }
    Deque(const int size, const T& val) : size_(size) {
        int64_t jend = size / kSubVectorSize + (size % kSubVectorSize != 0);
        for (int j = 1; j <= jend; ++j) {
            ptrs_.push_back(Allocate());
            int64_t iend = j < jend
                ? kSubVectorSize
                : size % kSubVectorSize != 0 ? size % kSubVectorSize
                : kSubVectorSize;
            for (int i = 0; i < iend; ++i) {
                try {
                    new (ptrs_.back() + i) T(val);
                }
                catch (...) {
                    size_ = 0;
                    for (int l = 0; l < j; ++l) {
                        for (int k = 0; k < (l + 1 == j ? i : kSubVectorSize); ++k) {
                            ptrs_[l][k].~T();
                        }
                        Dealloc(ptrs_[l]);
                    }
                    throw;
                }
            }
        }

        back_size_ =
            size % kSubVectorSize != 0 ? size % kSubVectorSize : kSubVectorSize;
    }

    Deque(const Deque& deque)
        : size_(deque.size_),
        front_offset_(deque.front_offset_),
        back_offset_(deque.back_offset_),
        front_ptr_ind_(deque.front_ptr_ind_),
        reversed_size_(deque.reversed_size_),
        ptrs_(deque.ptrs_.size(), nullptr),
        back_size_(deque.back_size_),
        reverse_start_(deque.reverse_start_) {
        for (int64_t i = front_ptr_ind_; i < static_cast<int64_t>(ptrs_.size());
            ++i) {
            ptrs_[i] = Allocate();
            int64_t j_start =
                i <= reverse_start_
                ? (i == front_ptr_ind_ ? reversed_size_ - 1 : kSubVectorSize - 1)
                : 0;
            int64_t j_end = i <= reverse_start_
                ? -1
                : (i == NumPtrs() - 1 ? back_size_ : kSubVectorSize);
            int64_t add = j_end - j_start > 0 ? 1 : -1;
            for (int64_t j = j_start; std::abs(j - j_end) > 0; j += add) {
                new (ptrs_[i] + j) T(deque.ptrs_[i][j]);
            }
        }
    }

    Deque& operator=(const Deque& other) {
        Clear();
        for (auto it = other.begin(); it != other.end(); ++it) {
            push_back(*it);
        }
        return *this;
    }

    size_t size() const {  // NOLINT
        return static_cast<size_t>(size_);
    }

    T& operator[](const int64_t index) { return Get(index); }

    const T& operator[](const int64_t index) const { return Get(index); }

    T& at(const int64_t index) {  // NOLINT
        if (index >= size_ || index < 0) {
            throw std::out_of_range("Index >= Deque size");
        }

        return Get(index);
    }

    const T& at(const int64_t index) const {  // NOLINT
        if (index >= size_ || index < 0) {
            throw std::out_of_range("Index >= Deque size");
        }

        return Get(index);
    }

    void push_back(const T& value) {  // NOLINT
        bool created_new = false;

        if (ptrs_.empty() ||
            (reverse_start_ == NumPtrs() - 1 && back_offset_ == 0) ||
            back_size_ == kSubVectorSize) {
            ptrs_.push_back(Allocate());
            created_new = true;
            back_size_ = 0;
        }

        try {
            if (back_offset_ > 0) {
                new (ptrs_[reverse_start_] + back_offset_ - 1) T(value);
                --back_offset_;
            }
            else {
                new (ptrs_.back() + back_size_) T(value);
                back_size_++;
            }
        }
        catch (...) {
            if (created_new) {
                Dealloc(ptrs_.back());
                ptrs_.pop_back();
            }
            // std::cout << "Error when creating new variable of T"<<std::endl;
            throw;
        }

        size_++;
    }

    void pop_back() {  // NOLINT
        if (size_ == 0) {
            throw std::out_of_range("Called pop_back on empty Deque");
        }

        if (NumPtrs() - 1 <= reverse_start_) {
            (ptrs_[reverse_start_] + back_offset_++)->~T();

            if (back_offset_ == kSubVectorSize) {
                Dealloc(ptrs_.back());
                ptrs_.pop_back();

                reverse_start_--;
                back_offset_ = 0;
            }
        }
        else {
            (ptrs_.back() + --back_size_)->~T();

            if (back_size_ == 0) {
                Dealloc(ptrs_.back());
                ptrs_.pop_back();

                if (NumPtrs() - 1 > reverse_start_) {
                    back_size_ = kSubVectorSize;
                }
            }
        }

        size_--;

        if (size_ == 0) {
            ptrs_.clear();
        }
    }

    void push_front(const T& value) {  // NOLINT
        bool created_new = false;
        bool adjusted = false;
        auto prev_rev_size = reversed_size_;

        if (ptrs_.empty() ||
            (front_offset_ == 0 && front_ptr_ind_ - 1 == reverse_start_) ||
            (reversed_size_ == kSubVectorSize &&
                front_ptr_ind_ <= reverse_start_)) {
            if (front_ptr_ind_ == 0) {
                AdjustPointers();
                adjusted = true;
                front_ptr_ind_ = NumPtrs() / 2 + (NumPtrs() == 1);
                reverse_start_ += NumPtrs() / 2 + (NumPtrs() == 1);
            }

            ptrs_[--front_ptr_ind_] = Allocate();
            reversed_size_ = 0;
            created_new = true;
        }

        try {
            if (front_offset_ != 0) {
                new (ptrs_[front_ptr_ind_] + front_offset_ - 1) T(value);
                front_offset_--;
            }
            else {
                new (ptrs_[front_ptr_ind_] + reversed_size_) T(value);
                reversed_size_++;
            }
        }
        catch (...) {
            if (created_new) {
                if (adjusted) {
                    reverse_start_ -= NumPtrs() / 2 + (NumPtrs() == 1);
                    ptrs_.erase(ptrs_.cbegin(), ptrs_.cbegin() + front_ptr_ind_ + 1);
                    front_ptr_ind_ = 0;
                }
                else {
                    front_ptr_ind_++;
                }
                reverse_start_ = front_ptr_ind_ - 1;
                reversed_size_ = prev_rev_size;
                front_offset_ = 0;
            }
            // std::cout << "Error creating new varibale of T";
            throw;
        }

        ++size_;
    }

    void pop_front() {  // NOLINT
        if (size_ == 0) {
            throw std::out_of_range("Called pop_front on empty Deque");
        }

        if (front_ptr_ind_ <= reverse_start_) {
            ptrs_[front_ptr_ind_][reversed_size_ - 1].~T();
            reversed_size_--;

            if (reversed_size_ == 0) {
                Dealloc(ptrs_[front_ptr_ind_++]);

                if (front_ptr_ind_ <= reverse_start_) {
                    reversed_size_ = kSubVectorSize;
                }
            }
        }
        else {
            ptrs_[front_ptr_ind_][front_offset_].~T();
            if (++front_offset_ == kSubVectorSize) {
                front_offset_ = 0;
                Dealloc(ptrs_[front_ptr_ind_++]);
                reverse_start_++;
            }
        }

        size_--;

        if (size_ == 0) {
            ptrs_.clear();
        }
    }

private:
    template <bool Const>
    class iterator_template {  // NOLINT
    public:
        using value_type = T;                                       // NOLINT
        using difference_type = std::ptrdiff_t;                     // NOLINT
        using pointer = std::conditional_t<Const, const T*, T*>;    // NOLINT
        using reference = std::conditional_t<Const, const T&, T&>;  // NOLINT
        using iterator_category = std::random_access_iterator_tag;  // NOLINT
        using PtrsType = std::conditional_t<Const, const T* const*, T**>;

        iterator_template(int64_t local_ind, PtrsType ptrs, int64_t sub_arr_ind,
            int64_t reverse_start)
            : ptrs_(ptrs),
            local_ind_(local_ind),
            sub_arr_ind_(sub_arr_ind),
            reverse_start_(reverse_start) {}

        iterator_template(const iterator_template&) = default;
        iterator_template& operator=(const iterator_template&) = default;

        iterator_template(const iterator_template<false>& it) requires(Const)
            : iterator_template(it.local_ind_, it.ptrs_, it.sub_arr_ind_,
                it.reverse_start_, it.sub_arr_size_) {}

        iterator_template& operator++() {
            AddToGlobalInd(1);
            return *this;
        }

        iterator_template operator++(int) {
            auto retval = *this;
            ++* this;
            return retval;
        }

        iterator_template& operator--() {
            SubtractFromGlobalInd(1);
            return *this;
        }

        iterator_template operator--(int) {
            auto retval = *this;
            --* this;
            return retval;
        }

        difference_type operator-(const iterator_template& rhs) const {
            if (*this < rhs) {
                return -(rhs - *this);
            }

            if (rhs.sub_arr_ind_ <= reverse_start_) {
                int sub_arr_dist = sub_arr_ind_ - rhs.sub_arr_ind_ -
                    (sub_arr_ind_ != rhs.sub_arr_ind_);
                difference_type res = sub_arr_dist * kSubVectorSize;

                if (sub_arr_ind_ > reverse_start_) {
                    res += local_ind_ + rhs.local_ind_ + 1;
                }
                else if (sub_arr_ind_ == rhs.sub_arr_ind_) {
                    res += rhs.local_ind_ - local_ind_;
                }
                else {
                    res += rhs.local_ind_ + kSubVectorSize - local_ind_;
                }
                return res;
            }

            return sub_arr_ind_ * kSubVectorSize + local_ind_ -
                rhs.sub_arr_ind_ * kSubVectorSize - rhs.local_ind_;
        }

        friend iterator_template operator+(const iterator_template& lhs,
            const int rhs) {
            iterator_template res(lhs);
            res.AddToGlobalInd(rhs);
            return res;
        }

        friend iterator_template operator+(int lhs, const iterator_template& rhs) {
            return rhs + lhs;
        }

        iterator_template& operator+=(int rhs) { return *this = *this + rhs; }

        friend iterator_template operator-(const iterator_template& lhs,
            const int rhs) {
            return lhs + -rhs;
        }

        iterator_template& operator-=(const int rhs) { return *this = *this - rhs; }

        friend bool operator==(const iterator_template& lhs,
            const iterator_template& rhs) {
            return lhs.ptrs_ == rhs.ptrs_ && lhs.local_ind_ == rhs.local_ind_ &&
                lhs.sub_arr_ind_ == rhs.sub_arr_ind_;
        }

        friend bool operator!=(const iterator_template& lhs,
            const iterator_template& rhs) {
            return !(lhs == rhs);
        }

        friend bool operator<(const iterator_template& lhs,
            const iterator_template& rhs) {
            if (rhs.ptrs_ != lhs.ptrs_ || lhs.sub_arr_ind_ > rhs.sub_arr_ind_) {
                return false;
            }

            if (lhs.sub_arr_ind_ < rhs.sub_arr_ind_) {
                return true;
            }

            if (lhs.sub_arr_ind_ <= lhs.reverse_start_) {
                return lhs.local_ind_ > rhs.local_ind_;
            }

            return lhs.local_ind_ < rhs.local_ind_;
        }

        friend bool operator>(const iterator_template& lhs,
            const iterator_template& rhs) {
            return rhs < lhs;
        }

        friend bool operator>=(const iterator_template& lhs,
            const iterator_template& rhs) {
            return !(lhs < rhs);
        }

        friend bool operator<=(const iterator_template& lhs,
            const iterator_template& rhs) {
            return !(lhs > rhs);
        }

        reference operator*() { return ptrs_[sub_arr_ind_][local_ind_]; }

        pointer operator->() { return &ptrs_[sub_arr_ind_][local_ind_]; }

    protected:
        PtrsType ptrs_;
        int64_t local_ind_;
        int64_t sub_arr_ind_;
        int64_t reverse_start_;

        void AddToGlobalInd(int addition) {
            if (addition < 0) {
                SubtractFromGlobalInd(-addition);
                return;
            }

            if (sub_arr_ind_ <= reverse_start_) {
                if (local_ind_ - addition < 0) {
                    addition -= local_ind_ + 1;
                    sub_arr_ind_ += addition / kSubVectorSize + 1;
                    local_ind_ = sub_arr_ind_ > reverse_start_
                        ? addition % kSubVectorSize
                        : kSubVectorSize - addition % kSubVectorSize - 1;
                }
                else {
                    local_ind_ -= addition;
                }
            }
            else {
                sub_arr_ind_ += (local_ind_ + addition) / kSubVectorSize;
                local_ind_ = (local_ind_ + addition) % kSubVectorSize;
            }
        }

        void SubtractFromGlobalInd(int addition) {
            if (addition < 0) {
                AddToGlobalInd(-addition);
                return;
            }

            if (sub_arr_ind_ <= reverse_start_) {
                sub_arr_ind_ -= (local_ind_ + addition) / kSubVectorSize;
                local_ind_ = (local_ind_ + addition) % kSubVectorSize;
            }
            else {
                addition -= local_ind_;
                if (addition <= 0) {
                    local_ind_ -= addition + local_ind_;
                    return;
                }

                sub_arr_ind_ -= 1 + (addition - 1) / kSubVectorSize;
                addition = (addition - 1) % kSubVectorSize;
                if (sub_arr_ind_ <= reverse_start_) {
                    local_ind_ = addition;
                }
                else {
                    local_ind_ = kSubVectorSize - 1 - addition;
                }
            }
        }
    };

public:
    using iterator = iterator_template<false>;  // NOLINT

    using const_iterator = iterator_template<true>;  // NOLINT

    using reverse_iterator = std::reverse_iterator<iterator>;  // NOLINT

    using const_reverse_iterator =
        std::reverse_iterator<const_iterator>;  // NOLINT

    iterator begin() {  // NOLINT
        if (front_ptr_ind_ <= reverse_start_) {
            return iterator(reversed_size_ - 1, ptrs_.data(), front_ptr_ind_,
                reverse_start_);
        }

        return iterator(front_offset_, ptrs_.data(), front_ptr_ind_,
            reverse_start_);
    }

    const_iterator begin() const {  // NOLINT
        if (front_ptr_ind_ <= reverse_start_) {
            return const_iterator(reversed_size_ - 1, ptrs_.data(), front_ptr_ind_,
                reverse_start_);
        }

        return const_iterator(front_offset_, ptrs_.data(), front_ptr_ind_,
            reverse_start_);
    }

    const_iterator cbegin() const {  // NOLINT
        return begin();
    }

    iterator end() {  // NOLINT
        if (NumPtrs() - 1 > reverse_start_) {
            return ++iterator(back_size_ - 1, ptrs_.data(), NumPtrs() - 1,
                reverse_start_);
        }

        return ++iterator(back_offset_, ptrs_.data(), NumPtrs() - 1,
            reverse_start_);
    }

    const_iterator end() const {  // NOLINT
        if (NumPtrs() - 1 > reverse_start_) {
            return ++const_iterator(back_size_ - 1, ptrs_.data(), NumPtrs() - 1,
                reverse_start_);
        }

        return ++const_iterator(back_offset_, ptrs_.data(), NumPtrs() - 1,
            reverse_start_);
    }

    const_iterator cend() const {  // NOLINT
        return end();
    }

    reverse_iterator rbegin() {  // NOLINT
        return std::make_reverse_iterator(end());
    }

    const_reverse_iterator rbegin() const {  // NOLINT
        return std::make_reverse_iterator(cend());
    }

    const_reverse_iterator crbegin() const {  // NOLINT
        return rbegin();
    }

    reverse_iterator rend() {  // NOLINT
        return std::make_reverse_iterator(begin());
    }

    const_reverse_iterator rend() const {  // NOLINT
        return std::make_reverse_iterator(cbegin());
    }

    const_reverse_iterator crend() const {  // NOLINT
        return rend();
    }

    void insert(iterator it, const T& value) {  // NOLINT
        T tmp = value;
        while (it != end()) {
            std::swap(*it++, tmp);
        }

        push_back(tmp);
    }

    void erase(iterator it) {  // NOLINT
        while (it != end() - 1) {
            std::swap(*it, *(it + 1));
            ++it;
        }

        pop_back();
    }

    ~Deque() { Clear(); }

private:
    int64_t size_ = 0;
    int8_t front_offset_ = 0;
    int8_t back_offset_ = 0;
    int64_t front_ptr_ind_ = 0;
    int64_t reversed_size_ = 0;
    static constexpr int8_t kSubVectorSize = 16;
    std::vector<T*> ptrs_;
    int8_t back_size_ = 0;
    int64_t reverse_start_ = -1;

    void AdjustPointers() {
        ptrs_.insert(ptrs_.begin(), ptrs_.size() + ptrs_.empty(), Allocate());
    }

    T& Get(int64_t index) {
        if (index < reversed_size_) {
            return ptrs_[front_ptr_ind_][reversed_size_ - index - 1];
        }

        index += reversed_size_ > 0 ? kSubVectorSize - back_size_ : front_offset_;
        int64_t sub_arr = index / kSubVectorSize + front_ptr_ind_;
        int64_t local_ind = sub_arr <= reverse_start_
            ? kSubVectorSize - 1 - index % kSubVectorSize
            : index % kSubVectorSize;

        return ptrs_[sub_arr][local_ind];
    }

    T* Allocate() {
        return reinterpret_cast<T*>(new char[sizeof(T) * kSubVectorSize]);
    }

    void Clear() {
        while (size_ != 0) {
            pop_back();
        }
    }

    void Dealloc(T* data) { delete[] reinterpret_cast<char*>(data); }

    const T& Get(int64_t index) const {
        if (index < reversed_size_) {
            return ptrs_[front_ptr_ind_][reversed_size_ - index - 1];
        }

        index += reversed_size_ > 0 ? kSubVectorSize - back_size_ : front_offset_;
        int64_t sub_arr = index / kSubVectorSize + front_ptr_ind_;
        int64_t local_ind = sub_arr <= reverse_start_
            ? kSubVectorSize - 1 - index % kSubVectorSize
            : index % kSubVectorSize;

        return ptrs_[sub_arr][local_ind];
    }

    int64_t NumPtrs() const { return static_cast<int64_t>(ptrs_.size()); }
};

#endif  // DEQUE_H
