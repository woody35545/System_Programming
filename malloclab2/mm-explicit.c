/*
 * mm-explicit.c - an empty malloc package
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 *
 * @id : 학번 
 * @name : 이름 
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif


/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

#define MINSIZE 24

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)
/* Define Word Size(4bytes) */
#define WSIZE 4
/* Define Double Word Size(8bytes)*/ 
#define DSIZE 8
#define HDRSIZE 4
#define FTRSIZE 4
#define OVERHEAD 8
#define CHUNKSIZE (1<<12)

/* The PACK macro is a function that creates a 1-word bit string for size and allocation.*/
#define PACK(size,alloc) ((size)|(alloc))

/* Get a value as much as 1 word size from a specific address value */
#define GET(p) (*(unsigned int *)(p))
#define GET8(p) (*(unsigned long *)(p))
/* Assign a value to a specific address */
#define PUT(p,val) (*(unsigned int *)(p) = (val)) 
#define PUT8(p,val) (*(unsigend long *)(p) = (unsigned long)(val))

/* Get pointer of block's header / footer */
#define HDRP(bp)    ((char *)(bp) - WSIZE)
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp))- DSIZE)

/* Get pointer of next or previous block */
#define NEXT_BLKP(bp)   (((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE))) 
#define PREV_BLKP(bp)   (((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE)))

/* Given block pointer bp, compute address of next and previous free block */
#define NEXT_FREEP(bp) ((char*)(bp))
#define PREV_FREEP(bp) ((char*)(bp) + DSIZE)

/* Given free block pointer bp, compute address of next and previous free blocks */
#define NEXT_FREE_BLKP(bp) ((char*)GET8((char*)(bp)))
#define PREV_FREE_BLKP(bp) ((char*)GET8((char*)(bp)+DSIZE))

#define NEXT_FREEP(bp) (*(void **) (bp + DSIZE))
#define PREV_FREEP(bp) (*(void **)(bp))

#define MIN(x,y) ((x)>(y)?(x):(y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

// [!] 확인 필요
static char *h_ptr = 0;
static char *heap_start_ptr;
static char *epilogue_ptr;
static char *free_listp=0;
static char *heap_listp=0;

static void *extend_heap(size_t words);
static void place(void *block_ptr, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *block_ptr);
static void remove_block(void *block_ptr);

/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {
if((heap_listp = mem_sbrk(2*MINSIZE)) == NULL)
    return -1;

PUT(heap_listp,0);
PUT(heap_listp+WSIZE, PACK(MINSIZE,1));
PUT(heap_listp+DSIZE,0);
PUT(heap_listp+DSIZE+WSIZE, 0);
PUT(heap_listp+MINSIZE, PACK(MINSIZE,1));

PUT(heap_listp+WSIZE+MINSIZE, PACK(0,1));
free_listp =heap_listp+DSIZE;
if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
    return -1;
return 0;
}

static void *extend_heap(size_t words){
    // 메모리에 heap 추가 공간 할당하는 함수

    char *block_ptr; /* 확장 후의 블록 주소를 저장할 pointer*/
    size_t size; 
    size = (words%2) ?(words+1)*WSIZE : words*WSIZE; 

    if(size < MINSIZE) size = MINSIZE;
    if((long)(block_ptr = mem_sbrk(size)) == -1)
        return NULL;

    // free block의 header, footer, epilogue 초기화
    PUT(HDRP(block_ptr), PACK(size,0));
    PUT(FTRP(block_ptr), PACK(size,0));
    PUT(HDRP(NEXT_BLKP(block_ptr)), PACK(0,1));
    return coalesce(block_ptr);
}   

static void *coalesce(void *block_ptr){
    // 이전 block이 allocated block 인지 확인
    size_t isPrevBlkAlloc = GET_ALLOC(FTRP(PREV_BLKP(block_ptr))) || PREV_BLKP(block_ptr) == block_ptr;
    size_t isNextBLkAlloc = GET_ALLOC(HDRP(NEXT_BLKP(block_ptr)));
    size_t size = GET_SIZE(HDRP(block_ptr));

    if(isPrevBlkAlloc && !isNextBLkAlloc){
        // PREV: alloc, NEXT: not alloc
        // Merge with NEXT BLOCK
        size += GET_SIZE(HDRP(NEXT_BLKP(block_ptr)));
        PUT(HDRP(block_ptr), PACK(size,0));
        PUT(FTRP(block_ptr), PACK(size,0)); 
    }
    else if(!isPrevBlkAlloc && isNextBLkAlloc){ 
        // PREV: not alloc, NEXT: alloc
        // Merge with PREV BLOCK
        size += GET_SIZE(HDRP(PREV_BLKP(block_ptr)));
        block_ptr = PREV_BLKP(block_ptr);
        remove_block(block_ptr);
        PUT(HDRP(block_ptr), PACK(size,0));
        PUT(FTRP(block_ptr), PACK(size,0));
    }
    else if(!isPrevBlkAlloc && isNextBLkAlloc){
        // PREV: not alloc, NEXT: not alloc
        size += GET_SIZE(HDRP(PREV_BLKP(block_ptr))) + GET_SIZE(HDRP(NEXT_BLKP(block_ptr)));
        remove_block(PREV_BLKP(block_ptr));
        remove_block(NEXT_BLKP(block_ptr));
        block_ptr = PREV_BLKP(block_ptr);
        PUT(HDRP(block_ptr), PACK(size,0));
        PUT(FTRP(block_ptr), PACK(size,0));
    }
    NEXT_FREEP(block_ptr) = free_listp;
    PREV_FREEP(free_listp) = block_ptr;
    PREV_FREEP(block_ptr) = NULL;
    free_listp = block_ptr;
    return block_ptr;
}

static void remove_block(void *block_ptr){
    if(PREV_FREEP(block_ptr))
        NEXT_FREEP(PREV_FREEP(block_ptr)) = NEXT_FREEP(block_ptr);
    else
        free_listp = NEXT_FREEP(block_ptr);
    PREV_FREEP(NEXT_FREEP(block_ptr)) = PREV_FREEP(block_ptr);
}   

static void *find_fit(size_t asize){
    void *block_ptr;
    for (block_ptr = free_listp; GET_ALLOC(HDRP(block_ptr)) == 0; block_ptr=NEXT_FREE_BLKP(block_ptr)){
        if(asize <=(size_t)GET_SIZE(HDRP(block_ptr)))
            return block_ptr;     
    }
    return NULL;
}

static void place(void *block_ptr, size_t asize){
    size_t csize = GET_SIZE(HDRP(block_ptr));
    if((csize-asize)>= MINSIZE){
        PUT(HDRP(block_ptr), PACK(asize,1));
        PUT(FTRP(block_ptr), PACK(asize,1));
        remove_block(block_ptr);
        block_ptr = NEXT_BLKP(block_ptr);
        PUT(HDRP(block_ptr), PACK(csize-asize,0));
        PUT(FTRP(block_ptr), PACK(csize-asize,0));
        coalesce(block_ptr);
    }
    else{
        PUT(HDRP(block_ptr), PACK(csize,1));
        PUT(FTRP(block_ptr), PACK(csize,1));
        remove_block(block_ptr);
    }
}
/*
 * malloc
 */
void *malloc (size_t size) {
        size_t asize;
        size_t extendSize;
        char *block_ptr;
    
    if(size <= 0) return NULL;

    asize = MAX(ALIGN(size) + DSIZE,MINSIZE);
    if((block_ptr = find_fit(asize))){
        place(block_ptr,asize);
        return block_ptr;
    }
    extendSize = MAX(asize,CHUNKSIZE);
    if((block_ptr = extend_heap(extendSize/WSIZE))==NULL)
        return NULL;
   
    place(block_ptr,asize);
    return block_ptr;

    }

/*
 * free
 */
void free (void *block_ptr) { // block_ptr: pointer of block
    // pointer가 null 인지 check
    if(!block_ptr) return;     
    
    // size는 원래 block size를 그대로 넣어줌
    size_t size = GET_SIZE(HDRP(block_ptr));

    // Header와 Footer의 Allocate 비트를 free(=0)로 변경
    PUT(HDRP(block_ptr),PACK(size,0)); // set allocated bit 0(free block)
    PUT(FTRP(block_ptr),PACK(size,0)); // set allocated bit 0(free block)

    coalesce(block_ptr);
}

/*
 * realloc - you may want to look at mm-naive.c
 */
void *realloc(void *ptr, size_t size) {
    size_t oldsize;
    void *newptr;
    size_t asize = MAX(ALIGN(size) + DSIZE, MINSIZE);

    if(size<=0){
        free(ptr);
        return 0;
    }
    if(ptr == NULL){
        return malloc(size);
    }

    if(asize == oldsize){
        return ptr;
    }
    if(asize <= oldsize){
        size = asize;
        if(oldsize -size< MINSIZE) return ptr;
        PUT(HDRP(ptr), PACK(size,1));
        PUT(FTRP(ptr), PACK(size,1));
        PUT(HDRP(NEXT_BLKP(ptr)), PACK(oldsize-size,1));
        free(NEXT_BLKP(ptr));
        return ptr;
    }
    if(!newptr){
        return 0;

    }
    if(size < oldsize) oldsize = size;
    memcpy(newptr,ptr,oldsize);
    free(ptr);
    return newptr;
}

/*
 * calloc - you may want to look at mm-naive.c
 * This function is not tested by mdriver, but it is
 * needed to run the traces.
 */
void *calloc (size_t nmemb, size_t size) {
    return NULL;
}


/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 */
static int in_heap(const void *p) {
    return p < mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Return whether the pointer is aligned.
 * May be useful for debugging.
 */
static int aligned(const void *p) {
    return (size_t)ALIGN(p) == (size_t)p;
}

/*
 * mm_checkheap
 */
void mm_checkheap(int verbose) {
}
