#ifndef UTILS_ARRAY_H
#define UTILS_ARRAY_H

#include <initializer_list>
#include <memory>
#include <cstring>
#include <iostream>

#include "utils/base_config.hpp"
#include "utils/allocator.hpp"

namespace st {

template<typename Dtype>
class DynamicArray {
public:
    explicit DynamicArray(index_t size) 
            : size_(size),
              dptr_(Alloc::unique_allocate<Dtype>(size_ * sizeof(Dtype))) {}
    DynamicArray(std::initializer_list<Dtype> data) 
            : DynamicArray(data.size()) {
        auto ptr = dptr_.get();
        for(auto d: data) {
            *ptr = d;
            ++ptr;
        }
    }
    DynamicArray(const DynamicArray<Dtype>& other) 
            : DynamicArray(other.size()) {
        std::memcpy(dptr_.get(), other.dptr_.get(), size_ * sizeof(Dtype));
    }
    DynamicArray(const Dtype* data, index_t size) 
            : DynamicArray(size) {
        std::memcpy(dptr_.get(), data, size_ * sizeof(Dtype));
    }
    explicit DynamicArray(DynamicArray<Dtype>&& other) = default;
    ~DynamicArray() = default;

    Dtype& operator[](index_t idx) { return dptr_.get()[idx]; }
    Dtype operator[](index_t idx) const { return dptr_.get()[idx]; }
    index_t size() const { return size_; }
    void memset(int value) const { std::memset(dptr_.get(), value, size_ * sizeof(Dtype)); }
private:
    index_t size_;
    Alloc::TrivialUniquePtr<Dtype> dptr_;
};


} // namespace st

#endif