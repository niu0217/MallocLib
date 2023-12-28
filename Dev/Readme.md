# 我的开发文档

## 1. 版本文件

完成所有准备工作：`Dev/version1/malloclab-handout-ONE`

隐式空闲链表：`Dev/version1/malloclab-handout-TWO`

## 2. 预备知识

### 2.1 需求

#### 2.1.1 功能要求

修改`mm.c`文件，其中包括四个函数：

+ `int mm_init(void);` 初始化堆空间，错误返回-1，否则返回0。
+ `void *mm_malloc(size_t size); `返回指向 size 字节有效载荷的指针，其中堆块保持为 8字节 对齐。
+ `void mm_free(void *prt);` 释放由 ptr 所指向堆块的空间。
+ `void *mm_realloc(void *ptr, size_t size);` 尝试重新调整之前调用 malloc 或 calloc 所分配的 ptr 所指向的内存块的大小，size 为新的内存空间的大小。
  描述：
  + 如果 prt 是 NULL，则重新分配内存空间，与 mm_malloc(size) 等价。
  + 如果 size 是 NULL，则释放该内存空间，与 mm_free(ptr) 等价。
  + 新内存块的地址与旧内存块的地址可能相同，也可能不同，取决于分割策略，需求大小以及原内存块的内部碎片。
  + 新内存块与旧内存块内容相同的大小取决于新旧内存块大小的最小值。

#### 2.1.2 编程规则

- 不可改变`mm.c`中的函数接口。
- 不可以调用任何与内存管理相关的库函数。（`malloc`，`calloc`，`free`，`realloc`，`sbrk`，`brk`等）
- 不可以在`mm.c`中定义任何聚合的变量类型（array，structs，trees，lists等）

#### 2.1.3 提示

+ 在最初时可以先使用简单的文件进行测试（eg. `short1,2-bal.rep`）

+ `unix> mdriver -V -f short1-bal.rep`

+ 理解书中基于隐式空闲列表实现的每一行代码。

+ 在C预处理器宏中封装指针算法。（通过写宏可以降低指针运算的复杂度）

+ 使用一些性能分析器。（eg.`gprof`）

### 2.2 需要注意的点

- 在空闲链表中的每一块是否都是空闲的？
- 是否存在一些连续的空闲块而没有合并？
- 每一个空闲块是否都在空闲链表中？
- 在空闲链表中的指针是否指向有效的空闲块？
- 已分配的块是否存在重叠的现象？
- 堆块中的指针是否指向有效的堆地址？？？？

### 2.3 支持函数

`memlib.c`

+ `void *mem_sbrk(int incr); `按incr个字节来扩充堆，其中incr为正数，返回新区域第一个字节的地址。

+ `void *mem_heap_lo(void); `返回指向堆第一个字节的空指针。

+ `void *mem_heap_hi(void);` 返回指向堆最后一个字节的空指针。

+ `size_t mem_heapsize(void);` 返回当前堆的总大小（以字节为单位）。

+ `size_t mem_pagesize(void); `返回系统页桢的大小（linux系统中为4K）。

## 3. 隐式空闲链表

