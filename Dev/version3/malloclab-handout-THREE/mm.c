/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

#define VERBOSE 0
#ifdef DEBUG
#define VERBOSE 1
#endif

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */  // 对 ALIGNMENT 倍数上取整的计算
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* 自定义的宏，有便于操作常量和指针运算 */   
#define WSIZE       4        //字、脚部或头部的大小（字节）
#define DSIZE       8        //双字大小（字节）
#define CHUNKSIZE  (1<<12)   //扩展堆时的默认大小
#define MINBLOCK (DSIZE + 2*WSIZE + 2*WSIZE)  //头部、脚部、两指针、8字节数据

#define MAX(x, y)  ((x) > (y) ? (x) : (y))

#define PACK(size, alloc)  ((size) | (alloc))         //将 size 和 allocated bit 合并为一个字

#define GET(p)             (*(unsigned int *)(p))          //读地址p处的一个字
#define PUT(p, val)        (*(unsigned int *)(p) = (val))  //向地址p处写一个字
#define GETADDR(p)         (*(unsigned int **)(p))   //读地址p处的一个指针
#define PUTADDR(p, addr)   (*(unsigned int **)(p) = (unsigned int *)(addr))  //向地址p处写一个指针


#define GET_SIZE(p)   (GET(p) & ~0x07)    //得到地址p处的 size
#define GET_ALLOC(p)  (GET(p) & 0x1)      //得到地址p处的 allocated bit
//block point --> bp指向有效载荷块指针
#define HDRP(bp)     ((char*)(bp) - WSIZE)                       //获得头部的地址
#define FTRP(bp)     ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)  //获得脚部的地址, 与宏定义HDRP有耦合

#define NEXT_BLKP(bp)    ((char*)(bp) + GET_SIZE((char*)(bp) - WSIZE))  //计算后块的地址
#define PREV_BLKP(bp)    ((char*)(bp) - GET_SIZE((char*)(bp) - DSIZE))  //计算前块的地址

#define PRED_POINT(bp)   (bp)            //指向祖先指针的指针
#define SUCC_POINT(bp)   ((char*)(bp) + WSIZE)  //指向后继指针的指针

static void* heap_listp;    //指向序言块
static void* head_free;     //空闲链表的头结点，存放在堆区开关的对齐块中
/* private functions */
static void *extend_heap(size_t size);     //拓展堆块
static void *find_fit(size_t size);        //寻找空闲块   first fit
static void place(void *bp, size_t size);  //分割空闲块
static void *coalesce(void *bp);           //合并空闲块
/* 链表操作 */
static void insert_freelist(void *bp);
static void remove_freelist(void *bp);
static void place_freelist(void *bp);
//check
static void mm_printblock(int verbose, const char* func);

/* 
 * mm_init - initialize the malloc package.
 */
//设立序言块、结尾块，以及序言块前的对齐块（4B），总共需要4个4B的空间
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void*)-1) 
        return -1;
    PUTADDR(heap_listp, NULL);                     //堆起绐位置的对齐块，使bp对齐8字节
    PUT(heap_listp + 1*WSIZE, PACK(8, 1));  //序言块
    PUT(heap_listp + 2*WSIZE, PACK(8, 1));  //序言块
    PUT(heap_listp, PACK(0, 1));            //结尾块
    head_free = heap_listp;                 //利用对齐块存放空闲链表的头结点
    PUTADDR(head_free, NULL);
    heap_listp += (2*WSIZE);     //小技巧：使heap_listp指向下一块, 即两个序主块中间

    if (extend_heap(CHUNKSIZE) == NULL)   //拓展堆块
        return -1;
    mm_printblock(VERBOSE, __func__);
    return 0;
}

//使用头插法，将空闲块插入空闲链表中
static void insert_freelist(void *bp) {   //LIFO  后进先出，头插法
    if (GETADDR(head_free) == NULL) {
        PUTADDR(SUCC_POINT(bp), NULL);
        PUTADDR(PRED_POINT(bp), head_free);
        PUTADDR(head_free, bp);
    } else {
        void *tmp;
        tmp = GETADDR(head_free);
        PUTADDR(SUCC_POINT(bp), tmp);
        PUTADDR(PRED_POINT(bp), head_free);
        PUTADDR(head_free, bp);
        PUTADDR(PRED_POINT(tmp), bp);
        tmp = NULL;
    }
}

//将 bp 所指的空闲块从空闲链表中移除（进行合并操作中会用到）
static void remove_freelist(void *bp) {
    void *pre_block, *post_block;
    pre_block = GETADDR(PRED_POINT(bp));
    post_block = GETADDR(SUCC_POINT(bp));
    //处理前序结点
    if (pre_block == head_free) {
        PUTADDR(head_free, post_block);
    } else {
        PUTADDR(SUCC_POINT(pre_block), post_block);
    }
    //处理后序结点
    if (post_block != NULL) {
        PUTADDR(PRED_POINT(post_block), pre_block);
    }
}

//对空闲链表中的空闲块进行前部分割
static void place_freelist(void *bp) {
    void *pre_block, *post_block, *next_bp;
    //存储前后结点地址
    pre_block = GETADDR(PRED_POINT(bp));
    post_block = GETADDR(SUCC_POINT(bp));
    next_bp = NEXT_BLKP(bp);
    //处理新的bp，进行前后连接
    PUTADDR(PRED_POINT(next_bp), pre_block);
    PUTADDR(SUCC_POINT(next_bp), post_block);
    //处理前序结点  针对head_free是前序结点的特殊处理
    if (pre_block == head_free) {
        PUTADDR(head_free, next_bp);
    } else {
        PUTADDR(SUCC_POINT(pre_block), next_bp);
    }
    //处理后序结点
    if (post_block != NULL) {
        PUTADDR(PRED_POINT(post_block), next_bp);
    }
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;     //ajusted size
    size_t extendsize;  //若无适配块则拓展堆的大小
    void *bp = NULL;

    if (size == 0)    //无效的申请
        return NULL;

    asize = ALIGN(size + 2*WSIZE);
    
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }
    
    //无足够空间的空闲块用来分配
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize)) == NULL) {
        return NULL;
    }    
    place(bp, asize);
    mm_printblock(VERBOSE, __func__);
    return bp;
}

static void *extend_heap(size_t size) {
    size_t asize;   
    void *bp;

    asize = ALIGN(size);
    if ((long)(bp = mem_sbrk(asize)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(asize, 0));          //HDRP(bp)指向原结尾块
    PUT(FTRP(bp), PACK(asize, 0));          
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   //新结尾块
 
    return coalesce(bp);
}

//放置策略搜索   首次适配搜索
static void *find_fit(size_t size) {         
    void *curbp;
    for (curbp = GETADDR(head_free); curbp != NULL; curbp = GETADDR(SUCC_POINT(curbp))) {
        if (GET_SIZE(HDRP(curbp)) >= size)
            return curbp;
    }
    return NULL;    //未适配
} 


//分割空闲块
static void place(void *bp, size_t asize) {     //注意最小块的限制（24B == MINBLOCK）
    size_t total_size = GET_SIZE(HDRP(bp));
    size_t remainder_size = total_size - asize;
    if (remainder_size >= MINBLOCK) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        void *next_bp = NEXT_BLKP(bp);
        PUT(HDRP(next_bp), PACK(remainder_size, 0));
        PUT(FTRP(next_bp), PACK(remainder_size, 0));
        place_freelist(bp);
    } else {          //没有已分配块或空闲块可以比最小块更小
        PUT(HDRP(bp), PACK(total_size, 1));
        PUT(FTRP(bp), PACK(total_size, 1));
        remove_freelist(bp);
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
    mm_printblock(VERBOSE, __func__);
}
/*
* coalesce - 合并内存块
*/
static void *coalesce(void *bp) {
    char *pre_block, *post_block;
    int pre_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    int post_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    if (pre_alloc && post_alloc) {
        insert_freelist(bp);
        return bp;
    } else if (pre_alloc && !post_alloc) {   //与后块合并
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        post_block = NEXT_BLKP(bp);  //记录后块的指针
        remove_freelist(post_block);
        insert_freelist(bp);
    } else if (!pre_alloc && post_alloc) {   //与前块合并
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
        remove_freelist(bp);
        insert_freelist(bp);
    } else {  //前后块都合并
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        pre_block = PREV_BLKP(bp);
        post_block = NEXT_BLKP(bp);
        bp = PREV_BLKP(bp);
        remove_freelist(pre_block);
        remove_freelist(post_block);
        insert_freelist(bp);
    }
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    return bp;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    size_t old_size, new_size, extendsize;
    void *old_ptr, *new_ptr;

    if (ptr == NULL) {
        return mm_malloc(size);
    }
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    new_size = ALIGN(size + 2*WSIZE);
    old_size = GET_SIZE(HDRP(ptr));
    old_ptr = ptr;
    if (old_size >= new_size) {
        if (old_size - new_size >= MINBLOCK) {  //分割内存块
            place(old_ptr, new_size);
            mm_printblock(VERBOSE, __func__);
            return old_ptr;
        } else {   //剩余块小于最小块大小，不分割
            mm_printblock(VERBOSE, __func__);
            return old_ptr;
        }
    } else {  //释放原内存块，寻找新内存块
        if ((new_ptr = find_fit(new_size)) == NULL) {  //无合适内存块
            extendsize = MAX(new_size, CHUNKSIZE);
            if ((new_ptr = extend_heap(extendsize)) == NULL)   //拓展堆空间
                return NULL;
        }
    place(new_ptr, new_size);
    memcpy(new_ptr, old_ptr, old_size - 2*WSIZE);
    mm_free(old_ptr);
    mm_printblock(VERBOSE, __func__);
    return new_ptr;
    }
}

static void mm_printblock(int verbose, const char* func) {
    if (!verbose) return;
    void *curbp;
    printf("\n=========================== $%s$ ===========================\n" ,func);
    printf("================ block ================\n");
    for (curbp = heap_listp; GET_SIZE(HDRP(curbp)) > 0; curbp = NEXT_BLKP(curbp)) {
        printf("address = %p\n", curbp);
        printf("hsize = %d, fsize = %d\n", GET_SIZE(HDRP(curbp)), GET_SIZE(FTRP(curbp)));
        printf("halloc = %d, falloc = %d\n", GET_ALLOC(HDRP(curbp)), GET_ALLOC(FTRP(curbp)));
        printf("\n");
    }
    //epilogue blocks
    printf("address = %p\n", curbp);
    printf("hsize = %d\n", GET_SIZE(HDRP(curbp)));
    printf("halloc = %d\n", GET_ALLOC(HDRP(curbp)));
    printf("================ block ================\n");
    printf("\n");
    printf("=============== freelist ===============\n");
    for (curbp = GETADDR(head_free); curbp != NULL; curbp = GETADDR(SUCC_POINT(curbp))) {
        printf("address = %p, size = %d,%d, alloc = %d,%d\n",
         curbp, GET_SIZE(HDRP(curbp)), GET_SIZE(FTRP(curbp)), GET_ALLOC(HDRP(curbp)), GET_ALLOC(FTRP(curbp)));
    }
    printf("address = %p\n", curbp);
    printf("=============== freelist ===============\n");
    printf("=========================== $%s$ ===========================\n" ,func);
}

