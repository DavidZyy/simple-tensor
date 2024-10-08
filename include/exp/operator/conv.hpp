#ifndef EXP_OPERATOR_CONV_H
#define EXP_OPERATOR_CONV_H

#include <utility>
#include <algorithm>
#include <iostream>
#include <type_traits>

#include "utils/base_config.hpp"
#include "utils/array.hpp"
#include "utils/exception.hpp"

namespace st {
namespace op {

struct Img2col {
    using Wsize = std::pair<index_t, index_t>;

    template<typename OperandType>
    static index_t ndim(const OperandType& operand) { return 2; }

    template<typename OperandType>
    static index_t size(index_t idx, const OperandType& operand, const Wsize& size) {
        return idx == 0 ? size.first : size.second;
    }

    template<typename OperandType>
    static data_t map(IndexArray& inds, const OperandType& operand, 
                      const Wsize& kernel_size, const Wsize& stride_size,
                      const Wsize& padding_size, const Wsize& out_size) {
        index_t n_batch = operand.size(0);
        index_t h = operand.size(2);
        index_t w = operand.size(3);
        index_t col = inds[0];
        index_t row = inds[1];

        // size(0) = oh * ow * b
        index_t h_idx = col / (out_size.second * n_batch);
        col %= (n_batch * out_size.second);
        index_t w_idx = col / n_batch;
        index_t b_idx = col % n_batch;

        // size(1) = c * kh * kw
        index_t c_idx = row / (kernel_size.first * kernel_size.second);
        row %= kernel_size.first * kernel_size.second;
        index_t kh_idx = row / kernel_size.second;
        index_t kw_idx = row % kernel_size.second;

        // In fact, index_t is unsigned int, which can't be negative.
        // So we can't substract padding_size here.
        h_idx = h_idx * stride_size.first /*- padding_size.first*/ + kh_idx;
        w_idx = w_idx * stride_size.second /*- padding_size.second*/ + kw_idx;

        if(h_idx < padding_size.first || h_idx >= h + padding_size.first
                || w_idx < padding_size.second || w_idx >= w + padding_size.second)
            return 0;

        h_idx -= padding_size.first;
        w_idx -= padding_size.second;
        IndexArray operand_inds{b_idx, c_idx, h_idx, w_idx};

        return operand.eval(operand_inds);
    }

    struct Grad {
        using allow_broadcast = std::false_type;
        using is_lhs = std::false_type;
        using is_rhs = std::false_type;

        template<typename GradType, typename OperandType>
        static data_t map(IndexArray& inds, const GradType& grad, 
                          const OperandType& operand, const Wsize& kernel_size, 
                          const Wsize& stride_size, const Wsize& padding_size, 
                          const Wsize& out_size) {
            // operand size: (b, c, h, w)
            // grad size: (oh*ow*b, c*kh*kw)
            index_t n_batch = operand.size(0);
            index_t img_h = operand.size(2) + (padding_size.first << 1);
            index_t img_w = operand.size(3) + (padding_size.second << 1);
            index_t kh_idx, kw_idx;  // location in a patch
            index_t ph_idx, pw_idx;  // location of the left top point of a patch
            IndexArray grad_inds(2);
            data_t total_grad = 0;

            index_t c_step = kernel_size.first * kernel_size.second;
            index_t kh_step = kernel_size.second;
            index_t oh_step = out_size.second * n_batch;
            index_t ow_step = n_batch;

            /* The two for-loops below has the same meaning. 
                for(kh_idx = 0; kh_idx < kernel_size.first; ++kh_idx) {
                    for(kw_idx = 0; kw_idx < kernel_size.second; ++kw_idx) {
                        ph_idx = inds[2] - kh_idx + padding_size.first;
                        pw_idx = inds[3] - kw_idx + padding_size.second;
                        // neglect that ph_idx and pw_idx is unsigned int
                        if(ph_idx < 0 || pw_idx < 0)
                            continue;
                        
                        ... ...
                    }
                }
            */
            inds[2] += padding_size.first;
            inds[3] += padding_size.second;
            for(kh_idx = 0; kh_idx < kernel_size.first && kh_idx <= inds[2]; ++kh_idx) {
                for(kw_idx = 0; kw_idx < kernel_size.second && kw_idx <= inds[3]; ++kw_idx) {
                    ph_idx = inds[2] - kh_idx;
                    pw_idx = inds[3] - kw_idx;

                    if(ph_idx + kernel_size.first > img_h 
                    || pw_idx + kernel_size.second > img_w
                    || ph_idx % stride_size.first 
                    || pw_idx % stride_size.second)
                        continue;

                    grad_inds[0] = ph_idx / stride_size.first * oh_step
                                 + pw_idx / stride_size.second * ow_step
                                 + inds[0];
                    grad_inds[1] = inds[1] * c_step
                                 + kh_idx * kh_step
                                 + kw_idx;
                    total_grad += grad.eval(grad_inds);
                }
            }
            return total_grad;
        }
    };
};

struct MaxPool2d {
    using Wsize = std::pair<index_t, index_t>;

    template<typename OperandType>
    static index_t ndim(const OperandType& operand) { return 4; }

    template<typename OperandType>
    static index_t size(index_t idx, const OperandType& operand, 
                        const Wsize& out_size) {
        switch(idx) {
            case 1: return operand.size(1);  // num_channel
            case 2: return out_size.first;
            case 3: return out_size.second;
            default: return operand.size(0);  // num_batch
        }
    }

    template<typename OperandType>
    static data_t map(IndexArray& inds, const OperandType& operand,
                      const Wsize& kernel_size, const Wsize& stride_size,
                      const Wsize& padding_size) {
        index_t h = operand.size(2);
        index_t w = operand.size(3);
        index_t h_start = inds[2] * stride_size.first;
        index_t w_start = inds[3] * stride_size.second;
        index_t h_end = h_start + kernel_size.first;
        index_t w_end = w_start + kernel_size.second;
        IndexArray operand_inds(inds);

        data_t value, max_value = DATA_MIN;
        for(index_t i = h_start; i < h_end; ++i) {
            if(i < padding_size.first || i >= h + padding_size.first) {
                max_value = std::max(max_value, 0.);
                continue;
            }
            for(index_t j = w_start; j < w_end; ++j) {
                if(j < padding_size.second || j >= w + padding_size.second) {
                    value = 0;
                } else {
                    operand_inds[2] = i - padding_size.first;
                    operand_inds[3] = j - padding_size.second;
                    value = operand.eval(operand_inds);
                }
                max_value = std::max(max_value, value);
            }
        }

        return max_value;
    }

    struct Grad {
        using allow_broadcast = std::false_type;
        using is_lhs = std::false_type;
        using is_rhs = std::false_type;

        static data_t map(IndexArray& inds, data_t value) {
            THROW_ERROR("NotImplementError");
            return 0;
        }
    };
};

}  // namespace op
}  // namespace st


#endif