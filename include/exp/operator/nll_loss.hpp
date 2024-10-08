#ifndef EXP_OPERATOR_NLL_LOSS_H
#define EXP_OPERATOR_NLL_LOSS_H

#include "utils/base_config.hpp"
#include "utils/allocator.hpp"
#include "utils/array.hpp"

namespace st{
namespace op {

struct NLLLoss {
    template<typename OperandType>
    static index_t ndim(const OperandType& operand) { return 1; }

    template<typename OperandType>
    static index_t size(index_t idx, const OperandType& operand) {
        return operand.size(0);
    }

    template<typename OperandType>
    static data_t map(IndexArray& inds, const OperandType& operand, 
                      index_t* batch_label) {
        index_t idx = inds[0];
        index_t label = batch_label[idx];
        IndexArray operand_inds{idx, label};
        return -operand.eval(operand_inds);
    }

    struct Grad {
        using allow_broadcast = std::false_type;
        using is_lhs = std::false_type;
        using is_rhs = std::false_type;

        template<typename GradType, typename OperandType>
        static data_t map(IndexArray& inds, const GradType& grad, 
                          const OperandType& operand, 
                          const index_t* batch_label) {
            index_t idx = inds[0];
            index_t cls = inds[1];
            if(cls == batch_label[idx])
                return -grad.eval(inds);
            else
                return 0;
        }
    };
};

}  // namespace op
}  // namespace st

#endif