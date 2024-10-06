#include <memory>
#include <utility>
#include <iostream>

#include "tensor/tensor_impl.hpp"
#include "tensor/grad_meta.hpp"
#include "utils/exception.hpp"
#include "utils/allocator.hpp"

namespace st {

TensorImpl::TensorImpl(const Storage& storage, 
               const Shape& shape, 
               const IndexArray& stride, 
               bool requires_grad)
        : storage_(storage),
          shape_(shape),
          stride_(stride),
          requires_grad_(requires_grad),
          gradmeta_ptr_(nullptr) {
    if(requires_grad_)
        gradmeta_ptr_ = Alloc::unique_construct<AutoGradMeta>(shape_);
}

TensorImpl::TensorImpl(const Storage& storage, const Shape& shape, bool requires_grad) 
        : storage_(storage), 
          shape_(shape), 
          stride_(shape_.ndim()), 
          requires_grad_(requires_grad),
          gradmeta_ptr_(nullptr) {
    // if shape_[i] == 1, set stride_[i] = 0. For broadcasting operatoion.
    for(int i = 0; i < stride_.size(); ++i)
        stride_[i] = shape_[i] == 1 ? 0 : shape_.subsize(i + 1);
    if(requires_grad_)
        gradmeta_ptr_ = Alloc::unique_construct<AutoGradMeta>(shape_);
}

TensorImpl::TensorImpl(const Shape& shape, bool requires_grad)
        : TensorImpl(Storage(shape.dsize()), shape, requires_grad) {}

TensorImpl::TensorImpl(const data_t* data, const Shape& shape, bool requires_grad)
        : TensorImpl(Storage(data, shape.dsize()), shape, requires_grad) {}

TensorImpl::TensorImpl(Storage&& storage, 
       Shape&& shape, 
       IndexArray&& stride, 
       bool requires_grad)
        : storage_(std::move(storage)),
          shape_(std::move(shape)),
          stride_(std::move(stride)),
          requires_grad_(requires_grad),
          gradmeta_ptr_(nullptr) {
    if(requires_grad_)
        gradmeta_ptr_ = Alloc::unique_construct<AutoGradMeta>(shape_);
}

bool TensorImpl::is_contiguous(void) const {
    for(index_t i = 0; i < stride_.size(); i++)
        if(stride_[i] != 0 && stride_[i] != shape_.subsize(i+1))
            return false;
    return true;
}

data_t& TensorImpl::operator[](std::initializer_list<index_t> inds) {
    CHECK_EQUAL(ndim(), inds.size(),
        "Invalid %dD indices for %dD tensor", inds.size(), ndim());

    index_t offset = 0, i = 0;
    for(auto idx: inds) {
        CHECK_IN_RANGE(idx, 0, size(i),
            "Index %d is out of bound for dimension %d with size %d", idx, i, size(i));
        offset += idx * stride_[i++];
    }
    storage_.increment_version();
    return storage_[offset]; 
}

data_t TensorImpl::operator[](std::initializer_list<index_t> inds) const {
    CHECK_EQUAL(ndim(), inds.size(),
        "Invalid %dD indices for %dD tensor", inds.size(), ndim());

    index_t offset = 0, i = 0;
    for(auto idx: inds) {
        CHECK_IN_RANGE(idx, 0, size(i),
            "Index %d is out of bound for dimension %d with size %d", idx, i, size(i));
        offset += idx * stride_[i++];
    }
    return storage_[offset]; 
}

data_t TensorImpl::item(void) const {
    CHECK_TRUE(ndim() == 1 && size(0) == 1,
        "Only one element tensors can be converted to scalars");
    return storage_[0];
}

Alloc::NontrivialUniquePtr<TensorImpl>
TensorImpl::slice(index_t idx, index_t dim) const {
    CHECK_IN_RANGE(dim, 0, ndim(),
        "Dimension out of range (expected to be in range of [0, %d), but got %d)", 
        ndim(), dim);
    CHECK_IN_RANGE(idx, 0, size(dim),
        "Index %d is out of bound for dimension %d with size %d", 
        idx, dim, size(dim));
    
    // new_dptr = dptr + idx * stride_[dim]
    index_t offset = stride_[dim] * idx;
    Storage storage(storage_, stride_[dim] * idx);
    // new_shape is the same as shape_ except missing the size on #dim dimension,
    // and new_stride is similiar.
    Shape shape(shape_, dim);
    IndexArray stride(shape_.ndim() - 1);

    int i = 0;
    for(; i < dim; i++)
        stride[i] = stride_[i];
    for(; i < stride.size(); i++)
        stride[i] = stride_[i+1];

    auto ret_ptr = Alloc::unique_construct<TensorImpl>(
        std::move(storage), std::move(shape), std::move(stride), false
    );
    if(requires_grad_) {
        ret_ptr->requires_grad_ = true;
        ret_ptr->gradmeta_ptr_ = Alloc::unique_construct<AutoGradMeta>(
            gradmeta_ptr_->grad_, offset
        );
        ret_ptr->gradmeta_ptr_->set_from_view(true);
        ret_ptr->gradmeta_ptr_->set_grad_fn(*this);
    }
    return ret_ptr;
}

Alloc::NontrivialUniquePtr<TensorImpl>
TensorImpl::slice(index_t start_idx, index_t end_idx, index_t dim) const {
    CHECK_IN_RANGE(dim, 0, ndim(),
        "Dimension out of range (expected to be in range of [0, %d), but got %d)",
        ndim(), dim);
    CHECK_IN_RANGE(start_idx, 0, size(dim),
        "Index %d is out of bound for dimension %d with size %d", 
        start_idx, dim, size(dim));
    CHECK_IN_RANGE(end_idx, 0, size(dim)+1,
        "Range end %d is out of bound for dimension %d with size %d", 
        end_idx, dim, size(dim));

    // new_dptr = dptr + start_idx * stride_[dim]
    index_t offset = stride_[dim] * start_idx;
    Storage storage(storage_, offset);
    // new_stride is the same as stride
    IndexArray stride(stride_);
    // new_shape and shape_ are only different on #dim dimension
    Shape shape(shape_);
    shape[dim] = end_idx - start_idx;

    auto ret_ptr =  Alloc::unique_construct<TensorImpl>(
        std::move(storage), std::move(shape), std::move(stride), false
    );
    if(requires_grad_) {
        ret_ptr->requires_grad_ = true;
        ret_ptr->gradmeta_ptr_ = Alloc::unique_construct<AutoGradMeta>(
            gradmeta_ptr_->grad_, offset
        );
        ret_ptr->gradmeta_ptr_->set_from_view(true);
        ret_ptr->gradmeta_ptr_->set_grad_fn(*this);
    }
    return ret_ptr;
}

Alloc::NontrivialUniquePtr<TensorImpl>
TensorImpl::transpose(index_t dim1, index_t dim2) const {
    CHECK_IN_RANGE(dim1, 0, ndim(),
        "Dimension out of range (expected to be in range of [0, %d), but got %d)", 
        ndim(), dim1);
    CHECK_IN_RANGE(dim2, 0, ndim(),
        "Dimension out of range (expected to be in range of [0, %d), but got %d)", 
        ndim(), dim2);
    
    // new_dptr = dptr
    // Exchange the value in shape_ and stride_ on #dim1 and #dim2
    Shape shape(shape_);
    shape[dim1] = shape_[dim2];
    shape[dim2] = shape_[dim1];

    IndexArray stride(stride_);
    stride[dim1] = stride_[dim2];
    stride[dim2] = stride_[dim1];

    auto ret_ptr = Alloc::unique_construct<TensorImpl>(
        Storage(storage_), std::move(shape), std::move(stride), false
    );
    if(requires_grad_) {
        ret_ptr->requires_grad_ = true;
        ret_ptr->gradmeta_ptr_ = Alloc::unique_construct<AutoGradMeta>(
            gradmeta_ptr_->grad_, 0
        );
        ret_ptr->gradmeta_ptr_->set_from_view(true);
        ret_ptr->gradmeta_ptr_->set_grad_fn(*this);
    }
    return ret_ptr;
}

Alloc::NontrivialUniquePtr<TensorImpl>
TensorImpl::permute(std::initializer_list<index_t> dims) const {
    CHECK_EQUAL(dims.size(), ndim(),
        "Dimension not match (expected dims of %d, but got %d)",
        ndim(), dims.size());

    IndexArray shape(ndim());
    IndexArray stride(ndim());
    index_t i = 0;
    for(index_t idx: dims) {
        shape[i] = shape_[idx];
        stride[i] = stride_[idx];
        ++i;
    }
    auto ret_ptr = Alloc::unique_construct<TensorImpl>(
        Storage(storage_), std::move(shape), std::move(stride), false
    );
    if(requires_grad_) {
        ret_ptr->requires_grad_ = true;
        ret_ptr->gradmeta_ptr_ = Alloc::unique_construct<AutoGradMeta>(
            gradmeta_ptr_->grad_, 0
        );
        ret_ptr->gradmeta_ptr_->set_from_view(true);
        ret_ptr->gradmeta_ptr_->set_grad_fn(*this);
    }
    return ret_ptr;
}

Alloc::NontrivialUniquePtr<TensorImpl>
TensorImpl::view(const Shape& shape) const {
    CHECK_TRUE(is_contiguous(),
        "view() is only supported to contiguous tensor");
    CHECK_EQUAL(shape.dsize(), shape_.dsize(),
        "Shape of size %d is invalid for input tensor with size %d", 
        shape.dsize(), shape_.dsize());
    // new_dptr = dptr
    // Just use new shape and adjust stride.
    auto ret_ptr = Alloc::unique_construct<TensorImpl>(
        storage_, shape, false
    );
    if(requires_grad_) {
        ret_ptr->requires_grad_ = true;
        ret_ptr->gradmeta_ptr_ = Alloc::unique_construct<AutoGradMeta>(
            gradmeta_ptr_->grad_, 0
        );
        ret_ptr->gradmeta_ptr_->set_from_view(true);
        ret_ptr->gradmeta_ptr_->set_grad_fn(*this);
    }
    return ret_ptr;
}

Alloc::NontrivialUniquePtr<TensorImpl>
TensorImpl::squeeze(void) const {
    index_t count = 0;
    auto squeeze_dims_ptr = Alloc::unique_allocate<index_t>(ndim() * sizeof(index_t));
    auto squeeze_dims = squeeze_dims_ptr.get();

    for(index_t i = 0; i < shape_.ndim(); i++)
        if(shape_[i] != 1)
            squeeze_dims[count++] = shape_[i];
    Shape squeeze_shape(squeeze_dims, count);
    return view(squeeze_shape);
}

Alloc::NontrivialUniquePtr<TensorImpl>
TensorImpl::unsqueeze(index_t dim) const {
    index_t new_ndim = ndim() + 1;
    CHECK_IN_RANGE(dim, 0, new_ndim,
        "Dimension out of range (expected to be in range of [0, %d), but got %d)", 
        new_ndim, dim);

    auto unsqueeze_dims_ptr = 
        Alloc::unique_allocate<index_t>(new_ndim * sizeof(index_t));
    auto unsqueeze_dims = unsqueeze_dims_ptr.get();

    index_t i = 0;
    for(; i != dim; i++)
        unsqueeze_dims[i] = shape_[i];
    unsqueeze_dims[dim] = 1;
    for(i++; i < new_ndim; i++)
        unsqueeze_dims[i] = shape_[i-1];
    return view(Shape(unsqueeze_dims, new_ndim));
}

Alloc::NontrivialUniquePtr<TensorImpl>
TensorImpl::grad(void) const {
    CHECK_TRUE(requires_grad_, "The tensor don't require grad.");
    return Alloc::unique_construct<TensorImpl>(
        gradmeta_ptr_->grad_, shape_, stride_, false
    );
}

data_t TensorImpl::eval(IndexArray& inds) const {
    index_t offset = 0;
    for(int i = 0; i < ndim(); ++i) {
        offset += inds[i] * stride_[i];
    }
    return storage_[offset];
}

data_t TensorImpl::eval(index_t idx) const {
    return storage_[idx];
}

std::ostream& operator<<(std::ostream& out, const TensorImpl& src) {
    TensorImpl t(src.size());
    t = src;

    std::ios_base::fmtflags flags = out.flags();
    out.setf(std::ios::fixed);
    out.precision(4);

    out << '[';
    if(t.ndim() == 1) {
        out << t[{0}];
        for(index_t i = 1; i < t.size(0); i++) {
            out << ", " << t[{i}];
        } 
    } else if(t.ndim() == 2) {
        out << *t.slice(0);
        for(index_t i = 1; i < t.size(0); i++) {
            out << ',' << std::endl;
            out << *t.slice(i);
        }
    } else {
        out << *t.slice(0);
        for(index_t i = 1; i < t.size(0); i++) {
            out << ',' << std::endl;
            out << std::endl;
            out << *t.slice(i);
        }
    }
    out << ']';

    out.setf(flags);
    return out;
}

}  // namespace st