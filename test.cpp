/*
    Do some simple test here, which is not strict unit test.
*/

// The next line can cancel check macro. 
// #define CANCEL_CHECK

#include <iostream>
#include <chrono>

#include "utils/base_config.hpp"
#include "utils/array.hpp"
#include "utils/exception.hpp" // CHECK_XXX is defined in utils/exception.hpp
#include "exp/function.hpp"
#include "tensor/shape.hpp"
#include "tensor/storage.hpp"
#include "tensor/tensor_impl.hpp"
#include "tensor/tensor.hpp"
#include "nn/init.hpp"
#include "nn/module.hpp"
#include "nn/optim.hpp"


using std::cout;
using std::endl;

void test_Alloc();
void test_Tensor();
void test_basic_operator();
void test_matrix_operator();
void test_numeric_operator();
void test_conv_operator();
void test_tensor_backward();
void test_basic_operator_backward();
void test_matrix_operator_backward();
void test_numeric_operator_backward();
void test_img2col_operator_backward();
void test_broadcasting_operator_backward();
void test_conv2d_module();
void test_linear_module();
void test_maxpool2d_module();
void test_ce_module();
void test_optimizer();

int main() {
    using namespace std::chrono;

    steady_clock::time_point start_tp = steady_clock::now();

    cout << "\033[33mtest allocator...\33[0m" << endl;
    test_Alloc();
    cout << "\033[33mtest tensor...\33[0m" << endl;
    test_Tensor();
    cout << "\033[33mtest basic operator...\033[0m" << endl;
    test_basic_operator();
    cout << "\033[33mtest matrix operator...\033[0m" << endl;
    test_matrix_operator();
    cout << "\033[33mtest numeric operator...\033[0m" << endl;
    test_numeric_operator();
    cout << "\033[33mtest conv operator...\033[0m" << endl;
    test_conv_operator();

    cout << "\033[33mtest tensor backward...\033[0m" << endl;
    test_tensor_backward();
    cout << "\033[33mtest basic operator backward...\033[0m" << endl;
    test_basic_operator_backward();
    cout << "\033[33mtest matrix operator backward...\033[0m" << endl;
    test_matrix_operator_backward();
    cout << "\033[33mtest numeric operator backward...\033[0m" << endl;
    test_numeric_operator_backward();
    cout << "\033[33mtest img2col operator backward...\033[0m" << endl;
    test_img2col_operator_backward();
    cout << "\033[33mtest broadcasting operator backward...\033[0m" << endl;
    test_broadcasting_operator_backward();

    cout << "\033[33mtest Conv2d module...\033[0m" << endl;
    test_conv2d_module();
    cout << "\033[33mtest Linear module...\033[0m" << endl;
    test_linear_module();
    cout << "\033[33mtest MaxPool2d module...\033[0m" << endl;
    test_maxpool2d_module();
    cout << "\033[33mtest CrossEntropy module...\033[0m" << endl;
    test_ce_module();
    cout << "\033[33mtest optimizer...\033[0m" << endl;
    test_optimizer();

    cout << "\033[33mcheck all memory is deallocated...\033[0m" << endl;
    CHECK_TRUE(st::Alloc::all_clear(), "check memory all clear");

    steady_clock::time_point end_tp = steady_clock::now();
    duration<double> time_span = duration_cast<duration<double>>(end_tp - start_tp);
    cout << "\033[32mTest success. Test took " << time_span.count();
    cout << " seconds.\033[0m" << endl;
    return 0;
}


struct Foo {
    static int ctr_call_counter;
    static int dectr_call_counter;

    char x_;
    char y_;
    Foo() { ++ctr_call_counter; }
    Foo(char x, char y) : x_(x), y_(y) { ++ctr_call_counter; }
    ~Foo() { ++dectr_call_counter; }
};
int Foo::ctr_call_counter = 0;
int Foo::dectr_call_counter = 0;

void test_Alloc() {
    using namespace st;
    
    void* ptr;
    {
        // No constructor call here.
        auto uptr = Alloc::unique_allocate<Foo>(sizeof(Foo));
        CHECK_EQUAL(Foo::ctr_call_counter, 0, "check 1");
        ptr = uptr.get();
    }
    CHECK_EQUAL(Foo::dectr_call_counter, 0, "check 1");

    {
        auto sptr = Alloc::shared_allocate<Foo>(sizeof(Foo));
        // The strategy of allocator.
        CHECK_EQUAL(ptr, static_cast<void*>(sptr.get()), "check 2");
    }
    
    {
        auto uptr = Alloc::unique_construct<Foo>();
        CHECK_EQUAL(Foo::ctr_call_counter, 1, "check 3");
        CHECK_EQUAL(ptr, static_cast<void*>(uptr.get()), "check 3");
    }
    CHECK_EQUAL(Foo::dectr_call_counter, 1, "check 3");

    {
        auto sptr = Alloc::shared_construct<Foo>('6', '7');
        CHECK_EQUAL(Foo::ctr_call_counter, 2, "check 4");
        CHECK_TRUE(sptr->x_ == '6' && sptr->y_ == '7', "check 4");
        CHECK_EQUAL(ptr, static_cast<void*>(sptr.get()), "check 4");
    }
    CHECK_EQUAL(Foo::dectr_call_counter, 2, "check 4");
}

void test_Tensor() {
    using namespace st;

    data_t data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    index_t idata[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

    st::Tensor t1(data, Shape({3, 4}));
    // cout << t1 << endl;
    for(index_t i = 0, idx = -1; i < 3; ++i) {
        for(index_t j = 0; j < 4; ++j) {
            data_t value = t1[{i, j}];
            CHECK_FLOAT_EQUAL(value, data[++idx], "check 1");
        }
    }
    
    auto t2 = t1.transpose(0, 1);
    // cout << t2 << endl;
    for(index_t i = 0; i < 4; ++i) {
        for(index_t j = 0; j < 3; ++j) {
            data_t value1 = t1[{j, i}];
            data_t value2 = t2[{i, j}];
            CHECK_FLOAT_EQUAL(value1, value2, "check 2");
        }
    }

    auto t3 = t2.slice(/*start=*/1, /*end=*/3, /*dim=*/1);
    auto shape_t3 = t3.size();
    CHECK_TRUE(shape_t3[0] == 4 && shape_t3[1] == 2, "check 3");
    for(index_t i = 0; i < 4; ++i) {
        for(index_t j = 0; j < 2; ++j) {
            data_t value1 = t2[{i, j+1}];
            data_t value2 = t3[{i, j}];
            CHECK_FLOAT_EQUAL(value1, value2, "check 3");
        }
    }

    auto t4 = t1.view({3, 2, 2});
    auto shape_t4 = t4.size();
    for(index_t i = 0; i < 3; ++i) {
        for(index_t j = 0; j < 2; ++j) {
            for(index_t k = 0; k < 2; ++k) {
                data_t value1 = t1[{i, j*2+k}];
                data_t value2 = t4[{i, j, k}];
                CHECK_FLOAT_EQUAL(value1, value2, "check 4");
            }
        }
    }

    auto t5 = t4.unsqueeze(/*dim=*/0).unsqueeze(/*dim=*/2);
    CHECK_EQUAL(t5.ndim(), 5, "check 5");
    Shape shape_t5({1, 3, 1, 2, 2});
    for(index_t i = 0; i < 5; ++i)
        CHECK_EQUAL(t5.size(i), shape_t5[i], "check 5");
    for(index_t i = 0; i < 3; ++i) {
        for(index_t j = 0; j < 2; ++j) {
            for(index_t k = 0; k < 2; ++k) {
                data_t value1 = t4[{i, j, k}];
                data_t value2 = t5[{0, i, 0, j, k}];
                CHECK_FLOAT_EQUAL(value1, value2, "check 5");
            }
        }
    }

    auto t6 = t5.squeeze();
    CHECK_EQUAL(t6.ndim(), t4.ndim(), "check 6");
    for(index_t i = 0; i < 3; ++i) {
        for(index_t j = 0; j < 2; ++j) {
            for(index_t k = 0; k < 2; ++k) {
                data_t value1 = t4[{i, j, k}];
                data_t value2 = t6[{i, j, k}];
                CHECK_FLOAT_EQUAL(value1, value2, "check 6");
            }
        }
    }

    auto t7 = t5.permute({0, 2, 3, 4, 1});
    CHECK_EQUAL(t7.ndim(), 5, "check7");
    Shape shape_t7({1, 1, 2, 2, 3});
    for(index_t i = 0; i < 5; ++i)
        CHECK_EQUAL(t7.size(i), shape_t7[i], "check7");
    for(index_t i = 0; i < 2; ++i)
        for(index_t j = 0; j < 2; ++j)
            for(index_t k = 0; k < 3; ++k) {
                data_t value1 = t7[{0, 0, i, j, k}];
                data_t value2 = t5[{0, k, 0, i, j}];
                CHECK_FLOAT_EQUAL(value1, value2, "check7");
            }
}

void test_basic_operator() {
    using namespace st;

    data_t data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    Tensor t1(data, Shape{3, 4});
    Tensor t2(data, Shape{3, 4});

    Tensor t3 = t1 + t2;
    for(index_t i = 0; i < 3; ++i) {
        for(index_t j = 0; j < 4; ++j) {
            data_t value1 = t3[{i, j}];
            data_t value2 = 2*t1[{i, j}];
            CHECK_FLOAT_EQUAL(value1, value2, "check 1");
        }
    }

    t3 += t1;
    for(index_t i = 0; i < 3; ++i) {
        for(index_t j = 0; j < 4; ++j) {
            data_t value1 = t3[{i, j}];
            data_t value2 = 3*t1[{i, j}];
            CHECK_FLOAT_EQUAL(value1, value2, "check 2");
        }
    }

    Tensor t4 = t1 * t2 + t3;
    for(index_t i = 0; i < 3; ++i) {
        for(index_t j = 0; j < 4; ++j) {
            data_t value1 = t4[{i, j}];
            data_t value2 = t1[{i, j}] * t2[{i, j}] + t3[{i, j}];
            CHECK_FLOAT_EQUAL(value1, value2, "check 3");
        }
    }

    auto func = [&t1, &t2](const Tensor& t3, const Tensor& t4) {
        auto add_exp = t1 + t2;
        auto mul_exp = -t1 * t2;
        return t3 * t4 - add_exp - mul_exp;
    };
    auto exp = func(t3, t4);
    // At this time, add_exp, mul_exp and other implicitly constructed Exp has 
    // been deconstructed. But we expect the BinaryExpImpl hold by them is 
    // still alive, untill the assignment of t5 completes.
    Tensor t5 = exp;
    for(index_t i = 0; i < 3; ++i) {
        for(index_t j = 0; j < 4; ++j) {
            data_t value1 = t5[{i, j}];
            data_t value2 = t3[{i, j}] * t4[{i, j}]
                        - (t1[{i, j}] + t2[{i, j}])
                        - (-t1[{i, j}] * t2[{i, j}]);
            CHECK_FLOAT_EQUAL(value1, value2, "check 3");
        }
    }

    Tensor t6 = t1.view({2, 1, 1, 2, 3});
    Tensor t7 = t1.view({2, 2, 1, 1, 3});
    Tensor t8 = t1.view({2, 2, 3});
    auto exp1 = t6 + t7;
    auto exp2 = -(t6 * t8);
    auto exp3 = t6 - t8;
    Tensor t9 = exp1 + exp2 + exp3;
    for(index_t i = 0; i < 2; ++i)
        for(index_t j = 0; j < 2; ++j)
            for(index_t k = 0; k < 3; ++k)
                for(index_t l = 0; l < 2; ++l) 
                    for(index_t m = 0; m < 3; ++m) {
                        data_t value1 = t9[{i, j, k, l, m}];
                        data_t value2 = t6[{i, 0, 0, l, m}] + t7[{i, j, 0, 0, m}];
                        value2    -= t6[{i, 0, 0, l, m}] * t8[{i, j, k}];
                        value2    += t6[{i, 0, 0, l, m}] - t8[{i, j, k}];
                        CHECK_FLOAT_EQUAL(value1, value2, "check 3");
                    }

    Tensor t10 = t1.transpose(0, 1) + op::constant(1, {4, 3});
    for(index_t i = 0; i < 4; ++i)
        for(index_t j = 0; j < 3; ++j) {
            data_t value1 = t10[{i, j}];
            data_t value2 = t1[{j, i}] + 1;
            CHECK_FLOAT_EQUAL(value1, value2, "check5");
        }

    // assignment of uncontiguous tensor
    auto t11 = t1.transpose(0, 1);
    Tensor t12 = t2.transpose(0, 1);
    Tensor t13(data, t11.size());
    t11 = t13;
    t12 = t11 + op::constant(0, {4, 3});
    for(index_t i = 0; i < 4; ++i)
        for(index_t j = 0; j < 3; ++j) {
            data_t value1 = t11[{i, j}];
            data_t value2 = t12[{i, j}];
            data_t value3 = t13[{i, j}];
            CHECK_TRUE(value1 == value2 && value1 == value3, "check6");
        }
}

void test_matrix_operator() {
    using namespace st;

    data_t data1[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    data_t data2[] = {11, 21, 31, 41, 51, 61, 71, 81, 91, 101, 111, 121};
    Tensor t1(data1, Shape{2, 6});
    Tensor t2(data2, Shape{2, 6});

    Tensor t3 = op::matrix_transpose(
        op::matrix_mul(t1, t2.transpose(/*dim1=*/0, /*dim2=*/1))
    );
    data_t t3_expect[2][2] = {{931, 2227}, {2191, 5647}};
    for(index_t i = 0; i < 2; ++i) {
        for(index_t j = 0; j < 2; ++j) {
            data_t value1 = t3[{i, j}];
            data_t value2 = t3_expect[i][j];
            CHECK_FLOAT_EQUAL(value1, value2, "check 1");
        }
    }

    Tensor t4 = t1.view({3, 2, 2});
    Tensor t5 = t2.view({3, 2, 2});
    Tensor t6 = op::batch_matrix_transpose(op::batch_matrix_mul(t4, t5));
    data_t t6_expect[3][2][2] = {{{73, 157}, {103, 227}},
                                 {{681, 925}, {791, 1075}},
                                 {{1929, 2333}, {2119, 2563}}};
    for(index_t i = 0; i < 3; ++i) {
        for(index_t j = 0; j < 2; ++j) {
            for(index_t k = 0; k < 2; ++k) {
                data_t value1 = t6[{i, j, k}];
                data_t value2 = t6_expect[i][j][k];
                CHECK_FLOAT_EQUAL(value1, value2, "check 2");
            }
        }
    }

    Tensor t7 = op::matrix_transpose(t1);
    CHECK_EQUAL(t7.ndim(), 2, "check3");
    CHECK_EQUAL(t7.size(0), 6, "check3");
    CHECK_EQUAL(t7.size(1), 2, "check3");
    for(index_t i = 0; i < 6; ++i) {
        for(index_t j = 0; j < 2; ++j) {
            data_t value1 = t1[{j, i}];
            data_t value2 = t7[{i, j}];
            CHECK_FLOAT_EQUAL(value1, value2, "check 3");
        }
    }

    Tensor t8(data1, Shape{2, 2, 3});
    Tensor t9 = op::batch_matrix_transpose(t8);
    CHECK_EQUAL(t9.ndim(), 3, "check4");
    CHECK_EQUAL(t9.size(0), 2, "check4");
    CHECK_EQUAL(t9.size(1), 3, "check4");
    CHECK_EQUAL(t9.size(2), 2, "check4");
    for(index_t i = 0; i < 2; ++i) {
        for(index_t j = 0; j < 3; ++j) {
            for(index_t k = 0; k < 2; ++k) {
                data_t value1 = t8[{i, k, j}];
                data_t value2 = t9[{i, j, k}];
                CHECK_FLOAT_EQUAL(value1, value2, "check 3");
            }
        }
    }   
}

void test_numeric_operator() {
    using namespace st;
    data_t data1[] = {0.585639, 0.612628, 0.241485, 0.097616, 0.035854, 0.723054, 
                      0.131163, 0.884268, 0.193597, 0.694748, 0.650687, 0.738797};
    Tensor t0(data1, Shape{3, 4});
    data_t t1_expect[3][4] = {{-1.208965, -1.181976, -1.553119, -1.696988},
                              {-1.860054, -1.172853, -1.764744, -1.011639},
                              {-1.784239, -1.283088, -1.327148, -1.239038}};
    Tensor t1 = op::log_softmax(t0);
    for(index_t i = 0; i < 3; ++i)
        for(index_t j = 0; j < 4; ++j) {
            data_t value1 = t1[{i, j}];
            data_t value2 = t1_expect[i][j];
            CHECK_FLOAT_EQUAL(value1, value2, "check1");
        }

    auto labels_ptr = Alloc::shared_allocate<index_t>(3 * sizeof(index_t));
    auto labels = labels_ptr.get();
    labels[0] = 2, labels[1] = 0, labels[2] = 3;
    Tensor t2 = op::nll_loss(t1, labels_ptr);
    CHECK_EQUAL(t2.ndim(), 1, "check2");
    CHECK_EQUAL(t2.size(0), t1.size(0), "check2");
    CHECK_FLOAT_EQUAL(t2[{0}], -t1_expect[0][2], "check2");
    CHECK_FLOAT_EQUAL(t2[{1}], -t1_expect[1][0], "check2");
    CHECK_FLOAT_EQUAL(t2[{2}], -t1_expect[2][3], "check2");

    data_t data2[] = {0.096237, -0.037000,  0.028076,  0.328307,  0.122271, -0.017293,
                      0.150791,  0.421008,  0.322066, -0.321352,  0.319534, -0.424081};
    Tensor t3(data2, Shape{2, 2, 3});
    Tensor t4 = op::mean(op::sigmoid(op::relu(t3)), 1);
    data_t t4_expect[][3] = {{0.552694, 0.515265, 0.503509},
                             {0.518813, 0.591467, 0.539914}};
    CHECK_TRUE(t4.ndim() == 2 && t4.size(0) == 2 && t4.size(1) == 3, "check3");
    for(index_t i = 0; i < 2; ++i)
        for(index_t j = 0; j < 3; ++j) {
            data_t value1 = t4[{i, j}];
            data_t value2 = t4_expect[i][j];
            CHECK_FLOAT_EQUAL(value1, value2, "check4");
        }

    Tensor t5 = op::mean(op::mean(t4, 0), 0);
    CHECK_TRUE(t5.ndim() == 1 && t5.size(0) == 1, "check5");
    CHECK_FLOAT_EQUAL(t5.item(), 0.536944, "check5");

    Tensor t6 = op::argmax(t3, 1);
    index_t t6_expect[][3] = {{1, 1, 0},
                              {0, 0, 0}};
    for(index_t i = 0; i < 2; ++i)
        for(index_t j = 0; j < 3; ++j) {
            index_t value1 = static_cast<index_t>(t6[{i, j}]);
            index_t value2 = t6_expect[i][j];
            CHECK_EQUAL(value1, value2, "check6");
        }
    
    Tensor t7 = op::max(t3, 1);
    for(index_t i = 0; i < 2; ++i)
        for(index_t j = 0; j < 3; ++j) {
            data_t value1 = t7[{i, j}];
            data_t value2 = t3[{i, t6_expect[i][j], j}];
            CHECK_FLOAT_EQUAL(value1, value2, "check7");
        }
}

void test_conv_operator() {
    using namespace st;
    data_t data[6][4] = {{0.4279, 0.7488, 0.3639, 0.5433}, {0.2849, 0.6536, 0.8932, 0.9341}, {0.9640, 0.4822, 0.1887, 0.9457},
                         {0.2132, 0.0185, 0.0163, 0.9874}, {0.2039, 0.8020, 0.3766, 0.6537}, {0.8543, 0.3589, 0.5178, 0.7816}};
    Tensor t0(reinterpret_cast<data_t*>(data), Shape{1, 1, 6, 4});

    Tensor t1 = op::max_pool2d(t0, {2, 2}, {1, 1}, {1, 1});
    index_t t1_size_expect[] = {1, 1, 7, 5};
    data_t t1_expect[7][5] = {{0.4279, 0.7488, 0.7488, 0.5433, 0.5433}, {0.4279, 0.7488, 0.8932, 0.9341, 0.9341},
                              {0.9640, 0.9640, 0.8932, 0.9457, 0.9457}, {0.9640, 0.9640, 0.4822, 0.9874, 0.9874},
                              {0.2132, 0.8020, 0.8020, 0.9874, 0.9874}, {0.8543, 0.8543, 0.8020, 0.7816, 0.7816},
                              {0.8543, 0.8543, 0.5178, 0.7816, 0.7816}};
    CHECK_EQUAL(t1.ndim(), 4, "check1");
    for(index_t i = 0; i < 4; ++i)
        CHECK_EQUAL(t1.size(i), t1_size_expect[i], "check1");
    for(index_t i = 0; i < 7; ++i)
        for(index_t j = 0; j < 5; ++j) {
            data_t value1 = t1[{0, 0, i, j}];
            data_t value2 = t1_expect[i][j];
            CHECK_FLOAT_EQUAL(value1, value2, "check2");
        }

    Tensor t2 = op::max_pool2d(t1, {3, 4}, {2, 3}, {0, 1});
    index_t t2_size_expect[] = {1, 1, 3, 2};
    data_t t2_expect[][2] = {{0.9640, 0.9457}, {0.9640, 0.9874}, {0.8543, 0.9874}};
    CHECK_EQUAL(t2.ndim(), 4, "check3");
    for(index_t i = 0; i < 4; ++i)
        CHECK_EQUAL(t2.size(i), t2_size_expect[i], "check3");
    for(index_t i = 0; i < 3; ++i)
        for(index_t j = 0; j < 2; ++j) {
            data_t value1 = t2[{0, 0, i, j}];
            data_t value2 = t2_expect[i][j];
            CHECK_FLOAT_EQUAL(value1, value2, "check3");
        }
    
    Tensor t3 = op::img2col(t0, /*kernel_size=*/{4, 4}, 
                            /*stride=*/{2, 2}, /*padding=*/{1, 1});
    index_t t3_shape_expect[] = {6, 16};
    CHECK_EQUAL(t3.size(0), t3_shape_expect[0], "check4");
    CHECK_EQUAL(t3.size(1), t3_shape_expect[1], "check4");
    data_t t3_expect[][16] = 
        {{0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.4279, 0.7488, 0.3639, 0.0000, 0.2849, 0.6536, 0.8932, 0.0000, 0.9640, 0.4822, 0.1887},
         {0.0000, 0.0000, 0.0000, 0.0000, 0.7488, 0.3639, 0.5433, 0.0000, 0.6536, 0.8932, 0.9341, 0.0000, 0.4822, 0.1887, 0.9457, 0.0000},
         {0.0000, 0.2849, 0.6536, 0.8932, 0.0000, 0.9640, 0.4822, 0.1887, 0.0000, 0.2132, 0.0185, 0.0163, 0.0000, 0.2039, 0.8020, 0.3766},
         {0.6536, 0.8932, 0.9341, 0.0000, 0.4822, 0.1887, 0.9457, 0.0000, 0.0185, 0.0163, 0.9874, 0.0000, 0.8020, 0.3766, 0.6537, 0.0000},
         {0.0000, 0.2132, 0.0185, 0.0163, 0.0000, 0.2039, 0.8020, 0.3766, 0.0000, 0.8543, 0.3589, 0.5178, 0.0000, 0.0000, 0.0000, 0.0000},
         {0.0185, 0.0163, 0.9874, 0.0000, 0.8020, 0.3766, 0.6537, 0.0000, 0.3589, 0.5178, 0.7816, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000}};
    for(index_t i = 0; i < 6; ++i)
        for(index_t j = 0; j < 16; ++j) {
            data_t value1 = t3[{i, j}];
            data_t value2 = t3_expect[i][j];
            CHECK_FLOAT_EQUAL(value1, value2, "check4");
        }

    data_t t4_data[2][3][6][4];
    for(index_t i = 0; i < 2; ++i)
        for(index_t j = 0; j < 3; ++j)
            for(index_t k = 0; k < 6; ++k)
                for(index_t l = 0; l < 4; ++l)
                    t4_data[i][j][k][l] = data[k][l];
    Tensor t4(reinterpret_cast<data_t*>(t4_data), Shape{2, 3, 6, 4});
    Tensor t5 = op::img2col(t4, /*kernel_size=*/{2, 3}, 
                            /*stride=*/{1, 2}, /*padding=*/{2, 1});
    data_t t5_expect[18][6] = 
        {{0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000}, {0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000}, {0.0000, 0.0000, 0.0000, 0.0000, 0.4279, 0.7488}, {0.0000, 0.0000, 0.0000, 0.7488, 0.3639, 0.5433}, {0.0000, 0.4279, 0.7488, 0.0000, 0.2849, 0.6536}, {0.7488, 0.3639, 0.5433, 0.6536, 0.8932, 0.9341},
         {0.0000, 0.2849, 0.6536, 0.0000, 0.9640, 0.4822}, {0.6536, 0.8932, 0.9341, 0.4822, 0.1887, 0.9457}, {0.0000, 0.9640, 0.4822, 0.0000, 0.2132, 0.0185}, {0.4822, 0.1887, 0.9457, 0.0185, 0.0163, 0.9874}, {0.0000, 0.2132, 0.0185, 0.0000, 0.2039, 0.8020}, {0.0185, 0.0163, 0.9874, 0.8020, 0.3766, 0.6537},
         {0.0000, 0.2039, 0.8020, 0.0000, 0.8543, 0.3589}, {0.8020, 0.3766, 0.6537, 0.3589, 0.5178, 0.7816}, {0.0000, 0.8543, 0.3589, 0.0000, 0.0000, 0.0000}, {0.3589, 0.5178, 0.7816, 0.0000, 0.0000, 0.0000}, {0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000}, {0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000}};
    CHECK_EQUAL(t5.size(0), 36, "check5");
    CHECK_EQUAL(t5.size(1), 18, "check5");
    for(index_t i = 0; i < 36; ++i)
        for(index_t j = 0; j < 18; ++j) {
            data_t value1 = t5[{i, j}];
            data_t value2 = t5_expect[i/2][j%6];
            CHECK_FLOAT_EQUAL(value1, value2, "check5");
        }
}

void test_tensor_backward() {
    using namespace st;

    data_t data1[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    Tensor t0(data1, Shape{3, 4}, true);

    Tensor t1 = t0.view({2, 2, 3});
    Tensor t2 = t1.slice(/*start_idx=*/1, /*end_idx=*/3, /*dim=*/2);
    Tensor t3 = t2.slice(1, /*dim=*/1);
    t3.backward();
    data_t t0_grad_expect1[][4] = {{0, 0, 0, 0}, {1, 1, 0, 0}, {0, 0, 1, 1}};
    auto&& grad1 = t0.grad();
    for(index_t i = 0; i < 3; ++i)
        for(index_t j = 0; j < 4; ++j) {
            data_t value1 = grad1[{i, j}];
            data_t value2 = t0_grad_expect1[i][j];
            CHECK_FLOAT_EQUAL(value1, value2, "check1");
        }

    Tensor t4 = t0.view({3, 2, 2});
    Tensor t5 = t4.transpose(0, 1);
    Tensor t6 = t5.slice(0, 1, /*dim=*/0);
    Tensor t7 = t6.permute({1, 2, 0});
    t7.backward();
    data_t t0_grad_expect2[][4] = {{1, 1, 0, 0}, {2, 2, 0, 0}, {1, 1, 1, 1}};
    auto&& grad2 = t0.grad();
    for(index_t i = 0; i < 3; ++i)
        for(index_t j = 0; j < 4; ++j) {
            data_t value1 = grad2[{i, j}];
            data_t value2 = t0_grad_expect2[i][j];
            CHECK_FLOAT_EQUAL(value1, value2, "check2");
        }
}

void test_basic_operator_backward() {
    using namespace st;

    data_t data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    Tensor t0(data, Shape{3, 4}, /*requires_grad=*/true);
    Tensor t1(data, Shape{3, 4}, /*requires_grad=*/true);

    Tensor t2 = t0 + t1;
    Tensor t3 = t0 * (-t2);
    Tensor t4 = t3 - t2;
    t4.backward();

    data_t t0_grad_expect[][4] = {{-4, -7, -10, -13}, {-16, -19, -22, -25}, {-28, -31, -34, -37}};
    data_t t1_grad_expect[][4] = {{-2, -3, -4, -5}, {-6, -7, -8, -9}, {-10, -11, -12, -13}};
    auto&& t0_grad = t0.grad();
    auto&& t1_grad = t1.grad();
    for(index_t i = 0; i < 3; ++i)
        for(index_t j = 0; j < 4; ++j) {
            data_t value1 = t0_grad[{i, j}];
            data_t value2 = t1_grad[{i, j}];
            data_t value3 = t0_grad_expect[i][j];
            data_t value4 = t1_grad_expect[i][j];
            CHECK_FLOAT_EQUAL(value1, value3, "check1");
            CHECK_FLOAT_EQUAL(value2, value4, "check1");
        }
}

void test_matrix_operator_backward() {
    using namespace st;

    data_t data1[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    data_t data2[] = {11, 21, 31, 41, 51, 61, 71, 81, 91, 101, 111, 121};
    Tensor t1(data1, Shape{2, 6}, true);
    Tensor t2(data2, Shape{2, 6}, true);

    Tensor t3 = op::matrix_mul(t1, t2.transpose(/*dim1=*/0, /*dim2=*/1));
    t3.backward();
    
    Tensor t4 = t1.view({3, 2, 2});
    Tensor t5 = t2.view({3, 2, 2});
    Tensor t6 = op::batch_matrix_mul(t4, t5);
    t6.backward();
    
    data_t t1_grad_expect[][6] = {{114., 174., 154., 214., 274., 334.}, {194., 254., 314., 374., 354., 414.}};
    data_t t2_grad_expect[][6] = {{12., 14., 18., 20., 28., 30.}, {22., 24., 32., 34., 38., 40.}};
    auto&& t1_grad = t1.grad();
    auto&& t2_grad = t2.grad();
    for(index_t i = 0; i < 2; ++i)
        for(index_t j = 6; j < 6; ++j) {
            data_t value1 = t1_grad[{i, j}];
            data_t value2 = t2_grad[{i, j}];
            CHECK_FLOAT_EQUAL(value1, value2, "check1");
        }
}

void test_numeric_operator_backward() {
    using namespace st;

    data_t data1[] = {0.585639, 0.612628, 0.241485, 0.097616, 0.035854, 0.723054, 
                      0.131163, 0.884268, 0.193597, 0.694748, 0.650687, 0.738797};
    Tensor t0(data1, Shape{3, 4}, /*requires_grad=*/true);
    Tensor t1 = op::log_softmax(t0);

    auto labels_ptr = Alloc::shared_allocate<index_t>(3 * sizeof(index_t));
    auto labels = labels_ptr.get();
    labels[0] = 2, labels[1] = 0, labels[2] = 3;
    Tensor t2 = op::nll_loss(t1, labels_ptr);

    t2.backward();
    data_t t0_grad_expect[][4] = {{0.2985, 0.3067, -0.7884, 0.1832},
                                  {-0.8443, 0.3095, 0.1712, 0.3636},
                                  {0.1679, 0.2772, 0.2652, -0.7103}};
    auto&& t0_grad = t0.grad();
    for(index_t i = 0; i < 3; ++i) 
        for(index_t j = 0; j < 4; ++j) {
            data_t value1 = t0_grad[{i, j}];
            data_t value2 = t0_grad_expect[i][j];
            CHECK_FLOAT_EQUAL(value1, value2, "check1");
        }

    data_t data2[] = {0.096237, -0.037000,  0.028076,  0.328307,  0.122271, -0.017293,
                      0.150791,  0.421008,  0.322066, -0.321352,  0.319534, -0.424081};
    Tensor t3(data2, Shape{2, 2, 3}, /*requires_grad=*/true);
    Tensor t4 = op::mean(op::sigmoid(op::relu(t3)), 1);
    Tensor t5 = op::max(t4, 1);

    t5.backward();
    data_t t3_grad_expect[][2][3] = {{{0.1247, 0.0000, 0.0000}, {0.1217, 0.0000, 0.0000}},
                                     {{0.0000, 0.1196, 0.0000}, {0.0000, 0.1219, 0.0000}}};
    auto&& t3_grad = t3.grad();
    for(index_t i = 0; i < 2; ++i)
        for(index_t j = 0; j < 2; ++j)
            for(index_t k = 0; k < 3; ++k) {
                data_t value1 = t3_grad[{i, j ,k}];
                data_t value2 = t3_grad_expect[i][j][k];
                CHECK_FLOAT_EQUAL(value1, value2, "check2");
            }
}

void test_img2col_operator_backward() {
    using namespace st;
    data_t data[6][4] = {{0.4279, 0.7488, 0.3639, 0.5433}, {0.2849, 0.6536, 0.8932, 0.9341}, {0.9640, 0.4822, 0.1887, 0.9457},
                         {0.2132, 0.0185, 0.0163, 0.9874}, {0.2039, 0.8020, 0.3766, 0.6537}, {0.8543, 0.3589, 0.5178, 0.7816}};
    Tensor t0(reinterpret_cast<data_t*>(data), Shape{1, 1, 6, 4}, true);

    Tensor t1 = op::img2col(t0, /*kernel_size=*/{5, 3},
                            /*stride=*/{1, 1}, /*padding=*/{0, 0});
    t1.backward();

    Tensor t2 = op::img2col(t0, /*kernel_size=*/{3, 3},
                            /*stride=*/{1, 1}, /*padding=*/{1, 1});
    t2.backward();

    auto&& t0_grad = t0.grad();
    data_t t0_grad_expect[][4] = {{5., 8., 8., 5.}, {8., 13., 13., 8.}, {8., 13., 13., 8.},
                                  {8., 13., 13., 8.}, {8., 13., 13., 8.}, {5., 8., 8., 5.}};
    for(index_t i = 0; i < 6; ++i) {
        for(index_t j = 0; j < 4; ++j) {
            data_t value1 = t0_grad[{0, 0, i, j}];
            data_t value2 = t0_grad_expect[i][j];
            CHECK_FLOAT_EQUAL(value1, value2, "check1");
        }
    }
}

 void test_broadcasting_operator_backward(void) {
    using namespace st;

    data_t data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    Tensor t0(data, Shape{1, 3, 4}, /*requires_grad=*/true);
    Tensor t1(data, Shape{3, 1, 4}, /*requires_grad=*/true);

    Tensor t2 = t0 + t1;
    Tensor t3 = t2 * t0;
    Tensor t4 = t3 - t1;
    t4.backward();
    auto&& t0_grad = t0.grad();
    auto&& t1_grad = t1.grad();
    data_t t0_grad_expect[][4] = {{21.0000, 30.0000, 39.0000, 48.0000}, {45.0000, 54.0000, 63.0000, 72.0000}, {69.0000, 78.0000, 87.0000, 96.0000}};
    data_t t1_grad_expect[] = {12.0000, 15.0000, 18.0000, 21.0000};
    for(index_t i = 0; i < 3; ++i) {
        for(index_t j = 0; j < 4; ++j) {
            data_t value1 = t0_grad[{0, i, j}];
            data_t value2 = t0_grad_expect[i][j];
            CHECK_FLOAT_EQUAL(value1, value2, "check1");

            value1 = t1_grad[{i, 0, j}];
            value2 = t1_grad_expect[j];
            CHECK_FLOAT_EQUAL(value1, value2, "check2");
        }
    }

    Tensor t5(data, Shape{1, 3, 1, 2, 1, 2}, true);
    Tensor t6(data, Shape{2, 1, 3, 2, 1, 1}, true);
    Tensor t7 = t5 + op::sigmoid(t5 + op::relu(t6));
    Tensor t8 = op::mean(t5 * t6, /*dim=*/0); 
    Tensor t9 = op::mean(op::max(t8.squeeze(), /*dim=*/0), /*dim=*/1);
    Tensor t10 = op::log_softmax(t9);
    Tensor t11 = t10.view({1, 3, 1, 2, 1, 1});
    Tensor t12 = t7 - t5 - t11;
    t12.backward();
    auto&& t5_grad = t5.grad();
    auto&& t6_grad = t6.grad();
    data_t t5_grad_expect[3][2][2] = {{{0.1255, 0.0529}, {0.0077, 0.0029}}, {{0.0029, 0.0011}, {0.0001, 0.0001}}, {{-107.3450, 107.3450}, {-125.1927, 125.1927}}};
    data_t t6_grad_expect[2][3][2] = {{{3.0877, 2.9434}, {3.0158, 2.9923}, {3.0022, 2.9989}}, {{2.9345, 2.9341}, {2.9911, 2.9910}, {2.9988, 2.9988}}};
    for(index_t i = 0; i < 3; ++i)
        for(index_t j = 0; j < 2; ++j)
            for(index_t k = 0; k < 2; ++k) {
                data_t value1 = t5_grad[{0, i, 0, j, 0, k}];
                data_t value2 = t5_grad_expect[i][j][k];
                CHECK_FLOAT_EQUAL(value1, value2, "check3");

                value1 = t6_grad[{j, 0, i, k, 0, 0}];
                value2 = t6_grad_expect[j][i][k];
                CHECK_FLOAT_EQUAL(value1, value2, "check4");
            }
}

void test_conv2d_module(void) {
    using namespace st;
    data_t weight_data[] = 
        {0.144233, 0.038765, -0.064723, 0.091522, -0.221822, 0.243479, 0.041969, -0.041030, 0.087458, 0.181160, -0.175163, 0.031789,
         0.128350, 0.186573, 0.171205, -0.095062, 0.164999, -0.001384, 0.056682, -0.051798, -0.021868, -0.078280, 0.213687, 0.207394, 
         -0.004414, -0.229483, 0.107253, -0.277729, 0.163448, 0.117666, 0.083151, -0.082815, -0.063118, -0.060334, 0.225444, 0.198153};
    nn::Conv2dWithReLU conv(
        /*in_channels=*/2,  /*out_channels=*/3,
        /*kernel_size=*/{2, 3}, /*stride=*/{2, 1},
        /*padding=*/{1, 0}
    );
    nn::ParamsDict params = conv.parameters();
    Tensor& weight = params["weight"];
    nn::CpyInitializer initilizer(weight, weight_data);
    initilizer.init();

    data_t img_data[2][2][7][7] = 
        {{{{0.906730, 0.916613, 0.722186, 0.386272, 0.100365, 0.618340, 0.609103}, {0.328955, 0.215732, 0.107681, 0.948013, 0.380048, 0.430663, 0.952055}, {0.193987, 0.173216, 0.952505, 0.543355, 0.794108, 0.892996, 0.298362},{0.364147, 0.519262, 0.255671, 0.286267, 0.373460, 0.638731, 0.768166},{0.238655, 0.624588, 0.365848, 0.170788, 0.957593, 0.592034, 0.195187},{0.907456, 0.690784, 0.488165, 0.208965, 0.298154, 0.160431, 0.215673},{0.082201, 0.544421, 0.673148, 0.754322, 0.053999, 0.834828, 0.282316}},
          {{0.063048, 0.014931, 0.652814, 0.747882, 0.708108, 0.689558, 0.825811},{0.642470, 0.979915, 0.715351, 0.195480, 0.097592, 0.995848, 0.853004},{0.445599, 0.872031, 0.647845, 0.977787, 0.134027, 0.028618, 0.090089},{0.861318, 0.085258, 0.144686, 0.547808, 0.198714, 0.486728, 0.214308},{0.133363, 0.289884, 0.341866, 0.149106, 0.463517, 0.422703, 0.378203},{0.204468, 0.258783, 0.282937, 0.615461, 0.572807, 0.890848, 0.987701},{0.564413, 0.419104, 0.989817, 0.934213, 0.818020, 0.931046, 0.973152}}},
         {{{0.188894, 0.422389, 0.363913, 0.017957, 0.339406, 0.686006, 0.036082},{0.499343, 0.730941, 0.075625, 0.258123, 0.145462, 0.160852, 0.877703},{0.273129, 0.795254, 0.169702, 0.263705, 0.574255, 0.985132, 0.046906},{0.491382, 0.606278, 0.363986, 0.652343, 0.050938, 0.427025, 0.859248},{0.747834, 0.483195, 0.136416, 0.186103, 0.171346, 0.355480, 0.069143},{0.427993, 0.046004, 0.433581, 0.905928, 0.488569, 0.371607, 0.220589},{0.527133, 0.720200, 0.358453, 0.598218, 0.134067, 0.765214, 0.909041}},
          {{0.518659, 0.629063, 0.760152, 0.316678, 0.783487, 0.723803, 0.640258},{0.314762, 0.950146, 0.556054, 0.492749, 0.996876, 0.352247, 0.816742},{0.381224, 0.613649, 0.634370, 0.129223, 0.346971, 0.359611, 0.038520},{0.098706, 0.768641, 0.262159, 0.443318, 0.791324, 0.880512, 0.713702},{0.313552, 0.349498, 0.481868, 0.075583, 0.179302, 0.794912, 0.768620},{0.116356, 0.613183, 0.599636, 0.231469, 0.502692, 0.471287, 0.778603},{0.297628, 0.129855, 0.114448, 0.860079, 0.003179, 0.437888, 0.744226}}}};
    Tensor img(reinterpret_cast<data_t*>(img_data), Shape{2, 2, 7, 7});
    Tensor out = conv.forward(img);
    out.backward();

    data_t out_expect[2][3][4][5] =
        {{{{0.085057, 0.000000, 0.014622, 0.197015, 0.054076},{0.257979, 0.015247, 0.168568, 0.460462, 0.017097},{0.058030, 0.126796, 0.304079, 0.000000, 0.061810},{0.259736, 0.174524, 0.045041, 0.427429, 0.013675}},
          {{0.197690, 0.324926, 0.250511, 0.214764, 0.354828},{0.365562, 0.637203, 0.468079, 0.286736, 0.313169},{0.430900, 0.230755, 0.218951, 0.541876, 0.418846},{0.652668, 0.632795, 0.476516, 0.326459, 0.538814}},
          {{0.111889, 0.203388, 0.143905, 0.233037, 0.421233},{0.272305, 0.544619, 0.000000, 0.000000, 0.000000},{0.165981, 0.000000, 0.071176, 0.337920, 0.000000},{0.269469, 0.297138, 0.173952, 0.105476, 0.404562}}},
         {{{0.020134, 0.000000, 0.219107, 0.036524, 0.000000},{0.000000, 0.255312, 0.301976, 0.153720, 0.000000},{0.071727, 0.160195, 0.229189, 0.204703, 0.000000},{0.078193, 0.149058, 0.000000, 0.536360, 0.151846}},
          {{0.302706, 0.198736, 0.138555, 0.346089, 0.306997},{0.507746, 0.232753, 0.143462, 0.263327, 0.384909},{0.357071, 0.345307, 0.184858, 0.338571, 0.574458},{0.195879, 0.423052, 0.559256, 0.236188, 0.516787}},
          {{0.320550, 0.140453, 0.122583, 0.432157, 0.264884},{0.065698, 0.000000, 0.021695, 0.197138, 0.133571},{0.000000, 0.010505, 0.000000, 0.158508, 0.281115},{0.002592, 0.101532, 0.043178, 0.000000, 0.330650}}}};
    for(index_t i = 0; i < 2; ++i)
        for(index_t j = 0; j < 3; ++j)
            for(index_t k = 0; k < 4; ++k)
                for(index_t l = 0; l < 5; ++l) {
                    data_t value1 = out[{i, j, k, l}];
                    data_t value2 = out_expect[i][j][k][l];
                    CHECK_FLOAT_EQUAL(value1, value2, "check1");
                }

    auto&& weight_grad = weight.grad();
    data_t weight_grad_expect[3][12] = 
        {{11.133665, 9.121082, 9.863847, 14.400089, 14.734153, 16.512037, 10.890715, 13.367422, 13.612727, 15.687311, 14.727955, 17.525148},
         {12.549257, 11.719290, 12.803723, 17.626467, 20.197935, 17.964199, 14.141121, 15.980512, 16.688646, 18.285843, 19.956493, 21.097359},
         {7.728555, 7.243347, 8.941869, 11.476597, 16.036528, 14.276077, 11.306244, 11.789472, 11.962565, 13.503635, 16.988863, 19.089035}};
    for(index_t i = 0; i < 3; ++i)
        for(index_t j = 0; j < 12; ++j) {
            data_t value1 = weight_grad[{i, j}];
            data_t value2 = weight_grad_expect[i][j];
            CHECK_FLOAT_EQUAL(value1, value2, "check2");
        }
}

void test_linear_module(void) {
    using namespace st;
    data_t weight_data[6][5] = 
        {0.071760,  0.263576, -0.378940, -0.424306,  0.424915,
        0.406897,  0.142503,  0.361772, -0.061179,  0.132496,
        0.226302,  0.022161, -0.021480, -0.283614, -0.442592,
        0.032238, -0.245419, -0.083803, -0.155786,  0.081459,
        -0.104956,  0.009876,  0.175388,  0.024486, -0.188793,
        0.262046, -0.425379, -0.263474,  0.102063, -0.067243};
    data_t bias_data[] = {0.053275, 0.057604, -0.233080, 0.186017, -0.003390, 0.101612};
    nn::LinearWithReLU linear(/*in_features=*/5, /*out_features=*/6);
    nn::ParamsDict params = linear.parameters();
    Tensor& weight = params["weight"];
    Tensor& bias = params["bias"];
    nn::CpyInitializer weight_initializer(weight, reinterpret_cast<data_t*>(weight_data));
    nn::CpyInitializer bias_initializer(bias, reinterpret_cast<data_t*>(bias_data));
    weight_initializer.init();
    bias_initializer.init();

    data_t input_data[3][5] = 
        {{0.524644, 0.069943, 0.090128, 0.390283, 0.264224},
         {0.360333, 0.167909, 0.272388, 0.330552, 0.947953},
         {0.735467, 0.036351, 0.184947, 0.862948, 0.818394}};
    Tensor input(reinterpret_cast<data_t*>(input_data), Shape{3, 5});

    Tensor linear_out = linear.forward(input);
    Tensor out = op::log_softmax(linear_out);
    out.backward();

    data_t out_expect[3][6] = {{-1.892938, -1.590033, -1.914817, -1.775882, -1.914817, -1.707157},
                               {-1.672153, -1.522798, -1.954868, -1.795545, -1.954868, -1.932030},
                               {-1.929682, -1.472234, -1.956825, -1.839288, -1.956825, -1.693635}};
    for(index_t i = 0; i < 3; ++i)
        for(index_t j = 0; j < 6; ++j) {
            data_t value1 = out[{i, j}];
            data_t value2 = out_expect[i][j];
            CHECK_FLOAT_EQUAL(value1, value2, "check1");
        }
    data_t weight_grad_expect[6][5] = {{0.099457, -0.009920, -0.002108, 0.106735, 0.010422},
                                       {-0.505351, -0.081136, -0.173833, -0.514122, -0.659705},
                                       {0.000000, 0.000000, 0.000000, 0.000000, 0.000000},
                                       {0.027103, 0.001202, 0.008172, 0.035058, 0.037341},
                                       {0.000000, 0.000000, 0.000000, 0.000000, 0.000000},
                                       {-0.074985, 0.012053, 0.008624, -0.080164, 0.016362}};
    auto&& weight_grad = weight.grad();
    for(index_t i = 0; i < 6; ++i) 
        for(index_t j = 0; j < 5; ++j) {
            data_t value1 = weight_grad[{i, j}];
            data_t value2 = weight_grad_expect[i][j];
            CHECK_FLOAT_EQUAL(value1, value2, "check2");
        }
    data_t bias_grad_expect[6] = {0.098009, -0.908593, 0.000000, 0.034192, 0.000000, -0.060508};
    auto&& bias_grad = bias.grad();
    for(index_t i = 0; i < 6; ++i) {
        data_t value1 = bias_grad[{0, i}];
        data_t value2 = bias_grad_expect[i];
        CHECK_FLOAT_EQUAL(value1, value2, "check3");
    }
}

void test_maxpool2d_module(void) {
    using namespace st;
    data_t data1[2][6] = {{0.138318, 0.883046, 0.093294, 0.514822, 0.359068, 0.650812},
                          {0.576113, 0.390465, 0.855900, 0.452224, 0.551624, 0.140468}};
    data_t data2[2][7] = {{0.574436, 0.286016, 0.286861, 0.392806, 0.088330, 0.456134, 0.482773},
                          {0.387206, 0.814651, 0.888812, 0.004778, 0.971438, 0.481807, 0.931557}};
    Tensor t0(reinterpret_cast<data_t*>(data1), Shape{1, 2, 1, 6}, true);
    Tensor t1(reinterpret_cast<data_t*>(data2), Shape{2, 1, 7, 1}, true);
    Tensor img = t0 + t1;

    nn::MaxPool2d maxpool(
        /*kernel_size=*/{3, 2}, /*stride=*/{1, 2}, /*padding=*/{1, 0}
    );
    Tensor maxpool_output = maxpool.forward(img);
    Tensor reduced = op::mean(op::mean(maxpool_output, 0), 2);
    Tensor output = op::matrix_mul(reduced, t1.view({7, 2}));
    output.backward();

    data_t output_expect[2][2] = {{3.787218, 5.987955}, {3.727950, 5.894424}};
    for(index_t i = 0; i < 2; ++i)
        for(index_t j = 0; j < 2; ++j) {
            data_t value1 = output[{i, j}];
            data_t value2 = output_expect[i][j];
            CHECK_FLOAT_EQUAL(value1, value2, "check1");
        }

    data_t t0_grad_expect[2][6] = {{0.000000, 2.349201, 0.000000, 2.349201, 0.000000, 2.349201},
                                   {2.349201, 0.000000, 2.349201, 0.000000, 2.349201, 0.000000}};
    auto&& t0_grad = t0.grad();
    for(index_t i = 0; i < 2; ++i)
        for(index_t j = 6; j < 6; ++j) {
            data_t value1 = t0_grad[{0, i, 0, j}];
            data_t value2 = t0_grad_expect[i][j];
            CHECK_FLOAT_EQUAL(value1, value2, "check2");
        }

    data_t t1_grad_expect[2][7] = {{4.273312, 2.733193, 2.807353, 4.221796, 2.625723, 4.329186, 5.097929},
                                   {2.708350, 3.632129, 3.995808, 2.798316, 6.347974, 2.758435, 4.171799}};
    auto&& t1_grad = t1.grad();
    for(index_t i = 0; i < 2; ++i)
        for(index_t j = 0; j < 7; ++j) {
            data_t value1 = t1_grad[{i, 0, j, 0}];
            data_t value2 = t1_grad_expect[i][j];
            CHECK_FLOAT_EQUAL(value1, value2, "check3");
        }
}

void test_ce_module() {
    using namespace st;
    data_t weight_data[3][5] = {{ 0.332016,  0.383861, -0.039896, -0.286464,  0.069793},
                                {-0.341369,  0.439378, -0.156823, -0.208273, -0.401472},
                                { 0.201601,  0.154146, -0.086722, -0.359864,  0.297248}};
    data_t bias_data[3] = {-0.240007,  0.322247, -0.051916};
    nn::Linear linear(5, 3);
    nn::ParamsDict params = linear.parameters();
    Tensor& weight = params["weight"];
    Tensor& bias = params["bias"];
    nn::CpyInitializer weight_initializer(weight, reinterpret_cast<data_t*>(weight_data));
    nn::CpyInitializer bias_initializer(bias, reinterpret_cast<data_t*>(bias_data));
    weight_initializer.init();
    bias_initializer.init();

    data_t input_data[3][5] = {{0.521807, 0.487334, 0.844843, 0.366452, 0.744550},
                               {0.861821, 0.102663, 0.949307, 0.086492, 0.588144},
                               {0.788253, 0.402394, 0.554831, 0.984794, 0.170077}};
    index_t labels[] = {2, 1, 0};
    Tensor input(reinterpret_cast<data_t*>(input_data), Shape{3, 5});

    nn::CrossEntropy criterion;
    Tensor out = linear.forward(input);
    Tensor loss = criterion.forward(out, labels);
    loss.backward();

    data_t loss_expect = 1.157702;
    CHECK_FLOAT_EQUAL(loss_expect, loss.item(), "check1");

    data_t weight_grad_expect[3][5] = {{-0.011948, -0.021014,  0.086070, -0.164264,  0.116384},
                                       {-0.080774,  0.065085, -0.098823,  0.123327, -0.059957},
                                       { 0.092722, -0.044071,  0.012753,  0.040937, -0.056427}};
    auto&& weight_grad = weight.grad();
    for(index_t i = 0; i < 3; ++i) {
        for(index_t j = 0; j < 5; ++j) {
            data_t value1 = weight_grad_expect[i][j];
            data_t value2 = weight_grad[{i, j}];
            CHECK_FLOAT_EQUAL(value1, value2, "check2");
        }
    }

    data_t bias_grad_expect[3] = {0.012001, -0.047002,  0.035001};
    auto&& bias_grad = bias.grad();
    for(index_t i = 0; i < 3; ++i) {
        data_t value1 = bias_grad_expect[i];
        data_t value2 = bias_grad[{0, i}];
        CHECK_FLOAT_EQUAL(value1, value2, "check3");
    }
}

void test_optimizer() {
    using namespace st;
    data_t weight_data[4][3] = {{ 0.5437, -0.4394, -0.0307}, {-0.3073,  0.4709,  0.1285},
                                {-0.0405,  0.5013, -0.3253}, { 0.4171, -0.2727, -0.3348}};
    data_t bias_data[4] = {0.1618, -0.4150,  0.1099,  0.2695};
    nn::Linear linear(3, 4);
    nn::ParamsDict params = linear.parameters();
    Tensor& weight = params["weight"];
    Tensor& bias = params["bias"];
    nn::CpyInitializer weight_initializer(weight, reinterpret_cast<data_t*>(weight_data));
    nn::CpyInitializer bias_initializer(bias, reinterpret_cast<data_t*>(bias_data));
    weight_initializer.init();
    bias_initializer.init();

    nn::SGDwithMomentum optimizer(linear.parameters(), 0.01, 0.9);

    data_t input_data[2][3] = {{0.4746, 0.5383, 0.2668}, {0.0405, 0.8955, 0.7365}};
    Tensor input(reinterpret_cast<data_t*>(input_data), Shape{2, 3});

    Tensor out1 = linear.forward(input);
    out1.backward();
    optimizer.step();
    optimizer.zero_grad();
    data_t weight_expect1[4][3] = {{ 0.5385, -0.4537, -0.0407}, {-0.3125,  0.4566,  0.1185}, 
                                   {-0.0457,  0.4870, -0.3354}, { 0.4119, -0.2871, -0.3448}};
    data_t bias_expect1[4] = {0.1418, -0.4350,  0.0899,  0.2495};
    for(index_t i = 0; i < 4; ++i) {
        for(index_t j = 0; j < 3; ++j) {
            data_t value1 = weight_expect1[i][j];
            data_t value2 = weight[{i, j}];
            CHECK_FLOAT_EQUAL(value1, value2, "check1");
        }
        data_t value1 = bias_expect1[i];
        data_t value2 = bias[{0, i}];
        CHECK_FLOAT_EQUAL(value1, value2, "check1");
    }

    Tensor out2 = linear.forward(input);;
    out2.backward();;
    optimizer.step();
    optimizer.zero_grad();
    data_t weight_expect2[4][3] = {{ 0.5287, -0.4809, -0.0598}, {-0.3223,  0.4293,  0.0994},
                                   {-0.0555,  0.4597, -0.3544}, { 0.4022, -0.3143, -0.3639}};
    data_t bias_expect2[4] = {0.1038, -0.4730,  0.0519,  0.2115};
    for(index_t i = 0; i < 4; ++i) {
        for(index_t j = 0; j < 3; ++j) {
            data_t value1 = weight_expect2[i][j];
            data_t value2 = weight[{i, j}];
            CHECK_FLOAT_EQUAL(value1, value2, "check1");
        }
        data_t value1 = bias_expect2[i];
        data_t value2 = bias[{0, i}];
        CHECK_FLOAT_EQUAL(value1, value2, "check1");
    }
}