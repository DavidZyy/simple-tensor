#ifndef NN_OPTIM_H
#define NN_OPTIM_H

#include <vector>
#include <functional>

#include "nn/module.hpp"

namespace st {
namespace nn{

class OptimizerBase {
public:
    OptimizerBase(const ParamsDict& params_dict);
    void zero_grad(void);
    virtual void step(void) = 0;
protected:
    static index_t data_size(const TensorImpl& t) {
        return t.shape_.dsize();
    }
    static data_t* get_storage(TensorImpl& t) {
        return t.storage_.dptr_;
    };
    static data_t* get_grad(TensorImpl& t) {
        return t.gradmeta_ptr_->grad_.dptr_;
    }

    std::vector<std::reference_wrapper<TensorImpl>> params_;
};

class SGD : public OptimizerBase {
public:
    SGD(const ParamsDict& params_dict, data_t lr);
    void step(void);
    data_t lr(void) { return lr_; }
    void set_lr(data_t lr) { lr_ = lr; } 
private:
    data_t lr_;
};

class SGDwithMomentum : public OptimizerBase {
public:
    SGDwithMomentum(const ParamsDict& params_dict, data_t lr, data_t momentum);
    void step(void);
    data_t lr(void) { return lr_; }
    void set_lr(data_t lr) { lr_ = lr; } 
    void lr_decay(data_t factor) { lr_ *= factor; }
private:
    data_t lr_;
    data_t momentum_;
    bool first_step_;
    std::vector<Alloc::TrivialUniquePtr<data_t>> running_means_;
};
}  // namespace nn
}  // namespace st
#endif