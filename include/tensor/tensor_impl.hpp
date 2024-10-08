#ifndef TENSOR_TENSOR_IMPL_H
#define TENSOR_TENSOR_IMPL_H

#include <initializer_list>
// #include <utility>

#include "exp/exp_impl.hpp"
#include "tensor/storage.hpp"
#include "tensor/shape.hpp"
#include "utils/exception.hpp"


namespace st {

// foward declaration
struct AutoGradMeta;
namespace nn {
    class InitializerBase;
    class OptimizerBase;
}
namespace op {
    struct Identity;
}

class TensorImpl : public ExpImpl<TensorImpl> {
public:
    // To be consistent with UnaryImpl
    using op = op::Identity;
    using operand_type = TensorImpl;

    // constructor
    TensorImpl(const Storage& storage, const Shape& shape, const IndexArray& stride,
               bool requires_grad=false);
    TensorImpl(const Storage& storage, const Shape& shape, 
               bool requires_grad=false);
    TensorImpl(const data_t* data, const Shape& shape, 
               bool requires_grad=false);
    explicit TensorImpl(const Shape& shape, 
                        bool requires_grad=false);
    TensorImpl(Storage&& storage, Shape&& shape, IndexArray&& stride, 
               bool requires_grad=false);
    template<typename ImplType> TensorImpl(const ImplType& impl);
    
    TensorImpl(const TensorImpl& other) = delete;
    TensorImpl(TensorImpl&& other) = default;
    TensorImpl& operator=(const TensorImpl& other);

    // inline function
    index_t ndim(void) const { return shape_.ndim(); }
    index_t size(index_t idx) const { return shape_[idx]; }
    const Shape& size(void) const { return shape_; }
    index_t offset(void) const { return storage_.offset(); }
    const IndexArray& stride(void) const { return stride_; }
    index_t version(void) const { return storage_.version(); }
    bool requires_grad(void) const { return requires_grad_; }

    // other method
    bool is_contiguous(void) const;
    Alloc::NontrivialUniquePtr<TensorImpl> grad(void) const;
    
    data_t& operator[](std::initializer_list<index_t> ids);
    data_t operator[](std::initializer_list<index_t> ids) const;
    data_t item(void) const;

    Alloc::NontrivialUniquePtr<TensorImpl> slice(index_t idx, index_t dim=0) const;
    Alloc::NontrivialUniquePtr<TensorImpl> slice(index_t start_idx, 
                                                 index_t end_idx, index_t dim) const;
    Alloc::NontrivialUniquePtr<TensorImpl> transpose(index_t dim1, index_t dim2) const;
    Alloc::NontrivialUniquePtr<TensorImpl> view(const Shape& shape) const;
    Alloc::NontrivialUniquePtr<TensorImpl> squeeze(void) const;
    Alloc::NontrivialUniquePtr<TensorImpl> unsqueeze(index_t dim) const;
    Alloc::NontrivialUniquePtr<TensorImpl> 
    permute(std::initializer_list<index_t> dims) const;

    // member function for expression template
    data_t eval(IndexArray& inds) const;
    data_t eval(index_t idx) const;
    template<typename ImplType> TensorImpl& operator=(const ImplType& exp_impl);
    template<typename ImplType> TensorImpl& operator+=(const ImplType& exp_impl);

    // friend function
    friend std::ostream& operator<<(std::ostream& out, const TensorImpl& t);
    friend ExpImplPtr<TensorImpl>;
    friend class nn::InitializerBase;
    friend class nn::OptimizerBase;
private:

    template<typename ImplType> void backward(const ImplType& grad);
    void backward(void);

    Storage storage_;
    Shape shape_;
    IndexArray stride_;

    bool requires_grad_;
    Alloc::NontrivialUniquePtr<AutoGradMeta> gradmeta_ptr_;
};

// Template specialization for ExpImplPtr
template<> 
class ExpImplPtr<TensorImpl> {
public:
    ExpImplPtr(Alloc::NontrivialUniquePtr<TensorImpl>&& ptr, bool with_grad)
            : ptr_(ptr.release()),
              with_grad_(with_grad && static_cast<TensorImpl*>(ptr_)->requires_grad()),
              version_(static_cast<TensorImpl*>(ptr_)->version()) {
        increment_counters();
    }
    ExpImplPtr(const TensorImpl& impl, bool with_grad)
            : ptr_(const_cast<TensorImpl*>(&impl)),
              with_grad_(with_grad && static_cast<TensorImpl*>(ptr_)->requires_grad()),
              version_(static_cast<TensorImpl*>(ptr_)->version()) {
        increment_counters();
    }
    ExpImplPtr(const ExpImplPtr& other, bool with_grad)
            : ptr_(other.ptr_),
              with_grad_(with_grad && static_cast<TensorImpl*>(ptr_)->requires_grad()),
              version_(static_cast<TensorImpl*>(ptr_)->version()) { 
        increment_counters(); 
    }
    ~ExpImplPtr() { decrease_refcount(); }

    TensorImpl* operator->(void) const { return static_cast<TensorImpl*>(ptr_); }
    const TensorImpl& operator*(void) const { return *static_cast<TensorImpl*>(ptr_); }
    explicit operator bool() const { return ptr_ != nullptr; }

    template<typename GradImplType>
    void invoke_backward(const GradImplType& grad) {
        TensorImpl* ptr = static_cast<TensorImpl*>(ptr_);
        if(ptr->requires_grad()) {
            CHECK_EQUAL(version_, ptr->version(),
                "Leaf variable has been moved into the graph interior");
            if(with_grad_)
                --ptr->gradcount_;
            ptr->backward(grad);
        }
    }

    void invoke_backward(void) {
        TensorImpl* ptr = static_cast<TensorImpl*>(ptr_);
        if(ptr->requires_grad()) {
            CHECK_EQUAL(version_, ptr->version(),
                "Leaf variable has been moved into the graph interior");
            if(with_grad_)
                --ptr->gradcount_;
            ptr->backward();
        }
    }
private:
    void increment_counters() { 
        ++ ptr_->refcount_; 
        if(with_grad_)
            ++ ptr_->gradcount_;
    }

    void decrease_refcount() {
        -- ptr_->refcount_;
        if(ptr_->refcount_ == 0)
            delete_handler(static_cast<void*>(ptr_));
    }

    ExpImpl<TensorImpl>* ptr_;
    bool with_grad_;
    index_t version_;
    Alloc::nontrivial_delete_handler<TensorImpl> delete_handler;
};
}  // namespace st
#include "tensor/grad_meta.hpp"

namespace st {

template<typename ImplType> 
class __GradFn: public GradFn {
public:
    __GradFn(const ImplType& impl) : next_exp_(impl, false) {}
    ~__GradFn() = default;

    void operator()(void) override { 
        THROW_ERROR("Need grad when invoke backward method of a expression.");
    }

    void operator()(const Storage& grad, const Shape& shape, 
                    const IndexArray& stride) override {
        TensorGradImpl grad_exp_impl(grad, shape, stride);
        next_exp_.invoke_backward(grad_exp_impl);
    }
private:
    ExpImplPtr<ImplType> next_exp_;
};

template<>
struct __GradFn<TensorImpl>: public GradFn {
public:
    __GradFn(const TensorImpl& impl) : next_exp_(impl, false) {}
    ~__GradFn() = default;

    void operator()(void) override {
        next_exp_.invoke_backward();
    }

    void operator()(const Storage& grad, const Shape& shape,
                    const IndexArray& stride) override {
        TensorGradImpl grad_exp_impl(grad, shape, stride);
        next_exp_.invoke_backward(grad_exp_impl);
    }
private:
    ExpImplPtr<TensorImpl> next_exp_;
};


struct AutoGradMeta {

    Storage grad_;
    bool from_view_;
    std::shared_ptr<GradFn> grad_fn_ptr_;

    AutoGradMeta(const Shape& tensor_shape)
            : grad_(tensor_shape.dsize(), 0),
              from_view_(false),
              grad_fn_ptr_(nullptr) {}
    
    AutoGradMeta(const Storage& grad, index_t offset)
            : grad_(grad, offset),
              from_view_(false),
              grad_fn_ptr_(nullptr) {}

    void set_from_view(bool from_view) { from_view_ = from_view; }

    template<typename ImplType>
    void set_grad_fn(const ImplType& impl) {
        auto ptr = Alloc::shared_construct<__GradFn<ImplType>>(impl);
        grad_fn_ptr_ = ptr;
    }
};

template<typename ImplType> 
void __assign(Storage& dist_storage, const Shape& dist_shape, 
              const IndexArray& dist_stride, const ImplType& src_exp);
template<typename ImplType>
void __inplacement_add(Storage& dist_storage, const Shape& dist_shape, 
                       const IndexArray& dist_stride,const ImplType& src_exp);
template<typename ImplType>
void __assign_uncontiguous(Storage& dist_storage, const Shape& dist_shape, 
                           const IndexArray& dist_stride, const ImplType& src_exp);
template<typename ImplType> 
void __inplacement_add_uncontiguous(Storage& dist_storage, const Shape& dist_shape,
                                    const IndexArray& dist_stride, 
                                    const ImplType& src_exp);


// member template function definition
template<typename ImplType>
TensorImpl::TensorImpl(const ImplType& impl)
        : TensorImpl(impl.size(), impl.requires_grad()) {
    this->operator=(impl);
}

template<typename ImplType> 
TensorImpl& TensorImpl::operator=(const ImplType& exp_impl) {
    CHECK_EXP_SAME_SHAPE(*this, exp_impl);

    if(requires_grad_) {
        gradmeta_ptr_->set_grad_fn(exp_impl);
        gradmeta_ptr_->set_from_view(false);
        storage_.increment_version();
    }

    if(is_contiguous())
        __assign(storage_, shape_, stride_, exp_impl);
    else
        __assign_uncontiguous(storage_, shape_, stride_, exp_impl);
    return *this;
}

template<typename ImplType>
TensorImpl& TensorImpl::operator+=(const ImplType& exp_impl) {
    CHECK_EXP_SAME_SHAPE(*this, exp_impl);

    if(requires_grad_) {
        gradmeta_ptr_->set_grad_fn(exp_impl);
        gradmeta_ptr_->set_from_view(false);
        storage_.increment_version();
    }
    
    if(is_contiguous())
        __inplacement_add(storage_, shape_, stride_, exp_impl);
    else
        __inplacement_add_uncontiguous(storage_, shape_, stride_, exp_impl);
    return *this;
}

inline TensorImpl& TensorImpl::operator=(const TensorImpl& other) {
    CHECK_EXP_SAME_SHAPE(*this, other);

    if(requires_grad_) {
        gradmeta_ptr_->set_grad_fn(other);
        gradmeta_ptr_->set_from_view(false);
        storage_.increment_version();
    }

    if(is_contiguous())
        __assign(storage_, shape_, stride_, other);
    else
        __assign_uncontiguous(storage_, shape_, stride_, other);
    return *this;
}

template<typename ImplType>
void TensorImpl::backward(const ImplType& grad) {
    // If the gradient is from a non-broadcasting operation,
    // shape will be the same to this->shape_;
    // Otherwise, shape will be broadcasted.
    Shape shape(grad.grad_size());
    if(is_contiguous() && shape == shape_) {
        __inplacement_add(gradmeta_ptr_->grad_, shape, stride_, grad);
    } else {
        __inplacement_add_uncontiguous(
            gradmeta_ptr_->grad_, shape, stride_, grad
        );
    }
    backward();
}

inline void TensorImpl::backward(void) {
    if(bool(gradmeta_ptr_->grad_fn_ptr_) && gradcount() == 0) {
        auto& grad_fn = *(gradmeta_ptr_->grad_fn_ptr_);
        if(gradmeta_ptr_->from_view_)
            grad_fn();
        else
            grad_fn(gradmeta_ptr_->grad_, shape_, stride_);
    }
}

template<typename ImplType>
void __assign(Storage& dist_storage, const Shape& dist_shape, 
              const IndexArray& dist_stride, const ImplType& src_exp) {
    IndexArray inds(dist_shape.ndim());
    for(index_t i = 0; i < dist_shape.dsize(); ++i) {
        for(index_t ii = i, j = 0; j < dist_shape.ndim(); ++j) {
            if(dist_stride[j] != 0) {
                inds[j] = ii / dist_stride[j];
                ii %= dist_stride[j];
            } else {
                inds[j] = 0;
            }
        }
        dist_storage[i] = src_exp.eval(inds);
    }
}

template<typename ImplType>
void __inplacement_add(Storage& dist_storage, const Shape& dist_shape, 
                       const IndexArray& dist_stride, const ImplType& src_exp) {
    IndexArray inds(dist_shape.ndim());
    for(index_t i = 0; i < dist_shape.dsize(); ++i) {
        for(index_t ii = i, j = 0; j < dist_shape.ndim(); ++j) {
            if(dist_stride[j] != 0) {
                inds[j] = ii / dist_stride[j];
                ii %= dist_stride[j];
            } else {
                inds[j] = 0;
            }
        }
        dist_storage[i] += src_exp.eval(inds);
    }
}

template<typename ImplType>
void __assign_uncontiguous(Storage& dist_storage, const Shape& dist_shape, 
                           const IndexArray& dist_stride, const ImplType& src_exp) {
    IndexArray inds(dist_shape.ndim());
    IndexArray cur(dist_shape.ndim());
    index_t idx = 0;
    cur.memset(0);

    while(true) {
        if(idx == dist_shape.ndim()) {
            --idx;
            index_t offset = 0;
            for(index_t i = 0; i < inds.size(); ++i)
                offset += dist_stride[i] * inds[i];
            dist_storage[offset] = src_exp.eval(inds);
        } else {
            while(idx < dist_shape.ndim() && cur[idx] == dist_shape[idx]) {
                cur[idx] = 0;
                --idx;
            }
            if(idx > dist_shape.ndim()) break;

            inds[idx] = cur[idx];
            ++cur[idx];
            ++idx;
        }
    }
}

template<typename ImplType>
void __inplacement_add_uncontiguous(Storage& dist_storage, const Shape& dist_shape, 
                                    const IndexArray& dist_stride, const ImplType& src_exp) {
    IndexArray inds(dist_shape.ndim());
    IndexArray cur(dist_shape.ndim());
    index_t idx = 0;
    cur.memset(0);

    while(true) {
        if(idx == dist_shape.ndim()) {
            --idx;
            index_t offset = 0;
            for(index_t i = 0; i < inds.size(); ++i)
                offset += dist_stride[i] * inds[i];
            dist_storage[offset] += src_exp.eval(inds);
        } else {
            while(idx < dist_shape.ndim() && cur[idx] == dist_shape[idx]) {
                cur[idx] = 0;
                --idx;
            }
            if(idx > dist_shape.ndim()) break;

            inds[idx] = cur[idx];
            ++cur[idx];
            ++idx;
        }
    }
}
}  // namespace st
#endif