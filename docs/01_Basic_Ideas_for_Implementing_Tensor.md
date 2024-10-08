### Basic Ideas for Implementing Tensor

使用Pytorch等深度学习框架时，我们常常会用view、transpose等函数得到不同形状的Tensor，或者在某一维上进行索引、切片来截取部分数据。无论操作的Tensor含有多少数据，这些操作都可以很快地完成，时间复杂度几乎为常数。显然，要做到这一点，必须共享这些派生出来的Tensor底层的数据。那么形状各异的Tensor是如何共享数据的呢？下面来介绍Tensor内部的基本概念。

#### 1. Concepts

不加额外要求的情况下，Tensor的成员变量可以简化如下：

```c++
struct Tensor {
    double* data;
    int* offset;
    int ndim;
    int* shape;
    int* stride;
};
```

- `Tensor::data`，浮点型数据的指针，指向储存数据的内存（这里只考虑数据是浮点数的情况）；
- `Tensor::offset`，表示Tensor存储数据的开始地址到`Tensor::data`之间的偏移，也就是说`offset + data`得到的才是Tensor储存数据的开始地址；
- `Tensor::ndim`，表示Tensor的维数，形状是`(2,3,4)`的Tensor，这个值就应该是3；
- `Tensor::shape`，可以与`Tensor::ndim`配合，视为一个整型数组，存储着Tensor的形状。
- `Tensor::stride`，与`Tensor::shape`类似，表示Tensor每一维的步长。

下面举一个形状为`(2,3,4)`的Tensor为例子进行说明：

```c++
Tensor t;

// data指向包含24个元素的double数组，元素为0到23.
t.data = new double[24];
for(int i = 0; i < 24; ++i)
    data[i] = i;

t.ndim = 3;
t.shape = new int[3]{2, 3, 4};
t.stride = new int[3]{12, 4, 1};
```

其他各项都很直观，只有`t.stride`需要稍微找一下规律。

1. `t.stride[2] = 1`就是设定如此；
2. `t.stride[1] = 4`是从`t.shape[2] = 4`得到的；
3. `t.stride[0] = 12`是从`t.shape[2] * t.shape[1] = 12`得到的。

也就是说，最后一维的stride设置为1，其他维数的stride等于把此维度后的每一维的尺寸乘起来。规范地把规律写出来就是：
$$
t.stride[i] = \begin{cases}
1, &\text{if } i=t.ndim-1, \\
\prod_{k=i+1}^{ndim-1}t.shape[k], &\text{otherwise}.
\end{cases}
$$
用代码表示：

```c++
void init_stride(Tensor& t) {
    int stride = 1;
    for(int i = ndim - 1; i >= 0; --i) {
        t.stride[i] = stride;
        stride *= t.shape[i];
    }
}
```

有了这个结构之后，下面结合具体的操作来进行说明如何使用。

#### 2. Operation

##### 2.1 取值

这里的取值，指的是取出Tensor中指定位置的元素，用Pytorch语句就是`t[i, j, k]`。

这个过程主要是`Tensor::stride`在发挥作用，我们可以参考编译器是如何从多维数组里取值的。从一个多维数组里取值，可以转化为从一维数组里的某个位置取值。比如：

```c++
int array3d[2][3][4];
int* array1d = reinterpret_cast<int*>(array);
ASSERT(array3d[i][j][k] == array1d[i*12 + j*4 + k])
```

`Tensor::stride`就是一样的东西，记录了如何把一维的`Tensor::data`解释成多维。

```c++
data_t& get_value(Tensor& t, const std::vector<int>& inds) {
    int offset = t.offset;
    for(int i = 0; i < t.ndim; ++i)
        offset += inds[i] * t.stride[i];
    return t.data[offset];
}
```

当然，我们可以在`get_value()`中加上一些Assert，对`inds`的长度及其取值进行约束，这时候才会用到`t.shape`。

##### 2.2 transpose

这里的transpose指的是交换Tensor的两个维度，用Pytorch的接口是`torch.transpose(t,dim0,dim1)`。

如果说取值时对于`Tensor::stride`的使用很自然，那么在transpose中就稍微有点惊喜了。在`Tensor::stride`的帮助下，transpose变得格外简单：仅需要交换`t.shape`和`t.stride`对应两个维度的元素即可得到结果。代码如下，同样地，这里省略了对参数的检查。

```c++
Tensor tranpose(const Tensor& t, int dim0, int dim1) {
    Tensor nt {
        /*data=*/t.data, /*offset=*/t.offset, /*ndim=*/t.ndim, 
        /*shape=*/new int[t.ndim], 
        /*stride=*/new int[t.ndim]
    };
    // 分别复制t.shape和t.stride的值到nt.shape和nt.stride 
    for(int i = 0; i < nt.ndim; ++i) {
        nt.shape[i] = t.shape[i];
        nt.stride[i] = t.stride[i];
    }
    std::swap(nt.shape[dim0], nt.shape[dim1]);
    std::swap(nt.stride[dim0], nt.stride[dim1]);
    return nt;
}
```

大致上可以这么理解这件事：假设`nt`是由`t`在第0维和第1维上transpose得到的，那么我们期待的是

```c++
nt[i, j, k] == t[j, i, k]
```

对所有`[i, j, k]`成立，而取值操作如2.1所述，是由`Tensor::stride`和`inds`逐个元素相乘后相加计算出偏移的。那么同时对调`Tensor::stride`和`inds`里的元素之后，所得的偏移一定是不变的。

同样地，`torch.permute(*dims)`也是这个道理，将`t.shape`和`t.stride`的元素按照对应的顺序重新排列即可得到结果。

```c++
Tensor permute(const Tensor& t, const std::vector<int>& dims) {
    Tensor nt {
        /*data=*/t.data, /*offset=*/t.offset, /*ndim=*/t.ndim,
        /*shape=*/new int[t.ndim],
        /*stride=*/new int[t.ndim]
    };
    for(int i = 0; i < nt.ndim; ++i) {
        nt.shape[i] = t.shape[dims[i]];
        nt.stride[i] = t.stride[dims[i]];
    }
    return nt;
}
```

##### 2.3 view

这里的view指的是直接改变Tensor的形状，给一维的`Tensor::data`换一种解释方式，Pytorch的接口是`t.view(*shape)`。

view和transpose有很大不同。在Pytorch中，view只能在连续（contiguous）的维度上进行，而transpose会把Tensor变成不连续的（uncontiguous）。比如下面的代码就会得到Error：

```python
# python code
t0 = torch.rand(3, 4)
t1 = t0.transpose(1, 0)
t2 = t1.view(3, 4)  # RuntimeError
```

这里连续与不连续的定义也是通过`Tensor::stride`来定义的。

```c++
bool is_contiguous(const Tensor& t) {
    int stride = 1;
    for(int i = t.ndim - 1; i >= 0; --i) {
        if(t.stride[i] != stride)
            return false;
        stride *= t.shape[i];
    }
    return true;
}
```

直观地说，如果`t.stride`满足前面讲的初始化公式，
$$
t.stride[i] = \begin{cases}
1, &\text{if } i=t.ndim-1, \\
\prod_{k=i+1}^{ndim-1}t.shape[k], &\text{otherwise}.
\end{cases}
$$
那么，`t`就是连续的，否则就是不连续的。在Pytorch里，可以通过`t.contiguous()`来得到一个连续的Tensor，但是其中涉及到数据的拷贝，就无法在常数时间复杂度内完成了。

如果已知view前后的Tensor都是连续的（Pytorch里好像可以对局部连续的Tensor做view，这里就不探讨了），那么view操作就同样简单了，只需根据新的shape初始化stride就行。

```c++
Tensor view(const Tensor& t, const std::vector<int>& shape) {
    Tensor nt {
        /*data=*/t.data, /*offset=*/t.offset, /*ndim=*/shape.size(),
        /*shape=*/new int[shape.size()],
        /*stride=*/new int[shape.size()]
    };
    // 下面的操作和之前init_stride()做的事情一样
    int stride = 1;
    for(int i = shape.size() - 1; i >= 0; --i) {
        t.shape[i] = shape[i];
        t.stride[i] = stride;
        stride *= shape[i];
    }
    return nt;
}
```

如果想要对参数进行检查的话，需要检查`is_contiguous(t)`，以及`shape`对应的数据量和`t.shape`对应的数据量是否相等。

##### 2.4 索引

这里的索引指的是在某一维度上取数据，用Pytorch语句更容易表述`t[:, j, :]`。

这个操作对Tensor的改变比较大，首先维度减少了一维，那么对应的`Tensor::shape`和`Tensor::stride`也要变化，而且`Tensor::offset`也要调整。假设`nt=t[:, j, :]`，则具体变化如下：

1. `nt.data = t.data`
2. `nt.offset = t.offset + j * t.stride[1]`
3. `nt.shape[0] = t.shape[0], nt.shape[1] = t.shape[2]`，跳过了`t.shape[1]`
4. `nt.stride[0] = t.stride[0], nt.stride[1] = t.stride[2]`，同样跳过了`t.stride[1]`

具体的代码如下：

```c++
Tensor slice(const Tensor& t, int idx, int dim) {
    Tensor nt {
        /*data=*/t.data, 
        /*offset=*/t.offset + idx * t.stride[dim],
        /*ndim=*/t.ndim - 1,
        /*shape=*/new int[t.ndim - 1],
        /*stride=*/new int[t.ndim - 1]
    };
    // 同样是复制t.stride和t.shape，只是需要跳过dim上的元素
    int i = 0;
    for(; i < dim; ++i) {
        nt.shape[i] = t.shape[i];
        nt.stride[i] = t.stride[i];
    }
    for(; i < nt.ndim; ++i) {
        nt.shape[i] = t.shape[i+1];
        nt.stride[i] = t.stride[i+1];
    }
    return nt;
}
```

##### 2.5 切片

这里所说的切片和上面的索引类似，只是不会让Tensor减少一维，对应的Pytorch语句是`t[:, j0:j1, :]`。

实现起来也和索引类似，`nt.stride`完全复制`t.stride`；`nt.shape`基本复制`t.shape`，只在做索引的维度上，根据`j0:j1`确定新值，这一维度上的尺寸通常会缩小。具体实现代码如下：

```c++
Tensor slice(const Tensor& t, int start_idx, int end_idx, int dim) {
    Tensor nt {
        /*data=*/t.data, 
        /*offset=*/t.offset + start_idx * t.stride[dim],
        /*shape=*/new int[t.ndim],
        /*stride=*/new int[t.ndim]
    };
    for(int i = 0; i < nt.ndim; ++i) {
        nt.shape[i] = t.shape[i];
        nt.stride[i] = t.stride[i];
    }
    nt.shape[dim] = end_dix - start_idx;
    return nt;
}
```

##### 2.6 Broadcasting

广播机制也是Tensor运算的一大特色，可以使不具有相同形状的Tensor进行加减乘除运算。下面是[Pytorch官网的例子](https://pytorch.org/docs/stable/notes/broadcasting.hpptml?highlight=broadcasting)：

```python
# python code
# can line up trailing dimensions to make reading easier
>>> x=torch.empty(5,1,4,1)
>>> y=torch.empty(  3,1,1)
>>> (x+y).size()
torch.Size([5, 3, 4, 1])

# but not necessary:
>>> x=torch.empty(1)
>>> y=torch.empty(3,1,7)
>>> (x+y).size()
torch.Size([3, 1, 7])

>>> x=torch.empty(5,2,4,1)
>>> y=torch.empty(3,1,1)
>>> (x+y).size()  # Runtime Error
```

为了支持广播机制，我们需要修改一下stride初始化的方式：在原有初始化的基础上，将size为1的维度对应的strde设为0。
$$
t.stride[i]=\begin{cases}
0, &\text{if } t.shape[i] = 1, \\
1, &\text{elif } i =t.ndim-1, \\
\prod^{t.ndim-1}_{k=i+1}t.shape[k] &\text{otherwise}.
\end{cases}
$$
用代码表示依然很简单：

```c++
void init_stride(Tensor& t) {
    int stride = 1;
    for(int i = ndim - 1; i >= 0; --i) {
        t.stride[i] = t.shape[i] == 1 ? 0 : stride;
        stride *= t.shape[i];
    }
}
```

首先，这样设置对正常取值是没有影响的，因为这一维的尺寸是1，那么能够取的索引值只有0，无论stride设置成怎样的值，这一维对偏移的贡献都是0。因为上面的代码没有对传进来的参数进行检查，所以这一点没有体现出来。比如形状是`(2, 1, 4)`的Tensor，那么对它取值的时候，能取得索引值只有`[i, 0, k]`，也就是第二维上的值一定是0，那么这一维上的stride其实可以取任意值。

其次，在broadcasting的场景中，考虑这样的一个运算：

```python
# python code
>>> t0.size()
(2, 3, 4)
>>> t1.size()
(2, 1, 4)
>>> t2 = t0 + t1
>>> t2.size()
(2, 3, 4)
```

我们期待的是，

```python
t2[i, j, k] == t0[i, j, k] + t1[i, 0, k]
```

因为`t1.size(1)==1`，所以`j`对于`t1`只能取0。如果按照这种直观的思路实现的话，就需要对`t.shape`在某一维度上是否为1进行判断，非常麻烦。而如果从`t1.stride`入手，这个过程就会简单很多。因为初始化时已经将`t1.stride[1]`设为0，所以这一维度上的索引实际上是无效，那么就可以简化如下：

```python
t2[i, j, k] == t0[i, j, k] + t1[i, j, k]
```

也就是说，可以使用相同的索引对结果和两个操作数取值，而无需多余的判断。

#### 3. My Implement

在Simple-Tensor中，这部分对应的实现位于[include/tensor_impl.hpp](https://github.com/XuHQ1997/simple-tensor/blob/main/include/tensor/tensor_impl.hpp)和[src/tensor_impl.cpp](https://github.com/XuHQ1997/simple-tensor/blob/main/src/tensor/tensor_impl.cpp)中。因为用`Storage`封装了`Tensor::data`和`Tensor::offset`，用`Shape`封装了`Tensor::shape`，用`IndexArray`封装了`Tensor::stride`，并且进行了参数检查，所以代码稍有不同，但原理大致相同。
