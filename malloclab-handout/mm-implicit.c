/*
 * mm-implicit.c - an empty malloc package
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


/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)

/* Define Word Size(4bytes) */
#define WSIZE 4
/* Define Double Word Size(8bytes)*/ 
#define DSIZE 8
#define OVERHEAD 8
#define CHUNKSIZE (1<<12)

/* The PACK macro is a function that creates a 1-word bit string for size and allocation.*/
#define PACK(size,alloc) ((size)|(alloc))

/* Get a value as much as 1 word size from a specific address value */
#define GET(p) (*(unsigned int *)(p))

/* Assign a value to a specific address */
#define PUT(p,value) (*(unsigned int *)(p) = (value)) 

/* Get pointer of block's header */
#define HDRP(bp)    ((char *)(bp) - WSIZE)
/* Get pointer of block's footer */
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp))- DSIZE)

/* Get pointer of next or previous block */
#define NEXT_BLKP(bp)   (((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE))) 
#define PREV_BLKP(bp)   (((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE)))

/* A pointer to store the starting address of the heap */
static char *heap_listp = 0;


/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {
     if((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1){
        return -1;
    };
    PUT(heap_listp, 0); // Padding                         
    PUT(heap_listp + WSIZE, PACK(OVERHEAD, 1)); // Prologue header 
    PUT(heap_listp + DSIZE, PACK(OVERHEAD, 1)); // Prologue footer
    PUT(heap_listp + WISZE + DSIZE, PACK(0, 1)); // Epilogue header
    heap_listp += (2*WSIZE);  
    
    if(extend_heap(CHUNKSIZE/WSIZE)==NULL)
        return -1; 
    return 0;
}
static void *extend_heap(size_t words)
{   
    /* Round the requested size to a multiple of a double word and expand the heap by that size */
    char *bp;
    size_t size;
    if(words%2)
        size = (words+1) * WSIZE;
    else
        size = words * WSIZE;
    
    if((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    // Put header of free block
    PUT(HDRP(bp), PACK(size, 0)); 
    
    // Put footer of free block
    PUT(FTRP(bp), PACK(size, 0));  //free 블록의 footer
    // Put epilogue header    
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); 

    return coalesce(bp);
};

static void *find_fit(size_t asize){

    void *bp;
    /* First Fit */
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
        /* If it is not an allocated block and the size of the block is greater than the size of the value to be allocated, the corresponding address (pointer) is returned. */
        if((GET_ALLOC(HDRP(bp))==0) && (asize <= GET_SIZE(HDRP(bp)))){
            return bp;
        }
    }
    return NULL;
}

/*
 * coalesce
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc){           
        // 앞뒤 블럭이 모두 allocate 상태일 경우(Free Block이 아닐 경우)
        return bp;
    }
    else if(prev_alloc && !next_alloc){         
        // next block이  free block일 경우, next block과 병합 
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if(!prev_alloc && next_alloc){        
        // previous block이  free block일 경우, previous block과 병합
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp);
    }
    else{                                       
        // previous, next 모두 free block 일 경우 모두 병합
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }   
    return bp;
}

/*
 * malloc
 */
void *malloc (size_t size) {
    size_t asize; // 실제로 할당할 size
    size_t extendsize;
    char *bp;
    if(size< DSIZE){
        // 만약 Size가 DSIZE 보다 작은 경우 최소 블럭 크기인 16bytes(2*DWORD)로 할당
        asize = 2*DWORD;
    }
    else{
        // 이외의 경우 적당히 align
         asize = DSIZE*((size+(DSIZE) + (DSIZE-1)) / DSIZE);
    }

    // find fit 함수를 이용하여 가용블록 찾기
    if(bp=find_fit(asize) != NULL){
        // block을 찾았을 경우 해당 위치에 배치
        place(bp,asize);
        return bp;
    }
    // 가용 block을 찾지 못했을 경우. 힙확장
    extendsize = MAX(asize, CHUNKSIZE);
    if((bp=extend_heap(extendsize/WSIZE)) == NULL)
        // 확장 실패시 NULL return
        return NULL;
    // 확장에 성공했을 경우 확장된 위치에 할당
    place(bp,asize);

    return bp;
}

/*
 * place
 */
static void place(void *bp, size_t asize){
    // block point에 asize를 작성해주는 함수
    
    // 가용블록 크기와 할당할 블럭 크기의 차이를 구해서 블럭 할당전에 블럭을 분할해줄지 여부 판단
    size_t diff = GET_SIZE(HDRP(bp)) - asize; 

    if(diff >= (2*DSIZE)){
        // diff가 최소 블럭 크기인 16bytes(2*DSIZE)보다 크면 남은 부분을 새로운 가용 블럭으로 분할
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(diff, 0));
        PUT(FTRP(bp), PACK(diff, 0));
    }else{
        // diff가 최소 블럭 크기인 16bytes(2*DSIZE)보다 작으면 분할할 수 없으므로 그냥 할당
        PUT(HDRP(bp), PACK(GET_SIZE(HDRP(bp)), 1));
        PUT(FTRP(bp), PACK(GET_SIZE(HDRP(bp)), 1));
    }
}
/*
 * free
 */
void free (void *ptr) {
    //if(!ptr) return;
    size_t size = GET_SIZE(HDRP(bp));
    // header와 footer의 최하위 비트를 0으로 설정하여 free block으로 전환
    // header의 alloc 비트 0으로 설정
    PUT(HDRP(ptr), PACK(size, 0)); 
    // footer의 alloc 비트 0으로 설정
    PUT(FTRP(ptr), PACK(size, 0));

    // 주위에 빈 블럭이 있으면 병합
    coalesce(ptr);
}

/*
 * realloc - you may want to look at mm-naive.c
 */
void *realloc(void *oldptr, size_t size) {
    return NULL;
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
