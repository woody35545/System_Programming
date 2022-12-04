/*
 * mm-implicit.c - an empty malloc package
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 * 
 * @id :
 * @name :
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

/* Basic constants and macros */
#define WSIZE 4
#define DSIZE 8 
#define CHUNKSIZE (1<<12)
#define OVERHEAD 8

#define MAX(x,y) ((x) > (y) ? (x): (y))

/* Pack a size and allocate bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)      (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields of address p */
#define GET_SIZE(p)   (GET(p) & ~0x7) // read without last 3 bits
#define GET_ALLOC(p)  (GET(p) & 0x1) // is allocated?

/* Get block header and footer pointer */
#define HDRP(bp)    ((char *)(bp) - WSIZE)
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp))- DSIZE)

/* get next and previous block pointer */
#define NEXT_BLKP(bp)   (((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE))) 
#define PREV_BLKP(bp)   (((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE)))


static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void place(void *bp, size_t asize);

static char *heap_listp = 0; // usable block heap for init
static char *next_listp;


/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {
    
    // 0. make empty heap for padding, prologue, epilogue
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1){ // extend 4word to heap_listp
        return -1;
    };

    // 1. make init block
    PUT(heap_listp, 0); // padding 0
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); // prologue header
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); // prologue footer
    PUT(heap_listp + (3*WSIZE), PACK(0, 1)); // epilogue header
    heap_listp += (2*WSIZE); // heap_listp points third block

    // 2. extend heap as CHUNKSIZE : (1) heap initialize (2) mm_malloc cannot find fit space
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL){
        return -1;
    }
    next_listp = heap_listp;

    return 0;
}

/*
 * extend_heap
 */
static void *extend_heap(size_t words){
    char *bp;
    size_t size;

    // round size as 8bytes... and ask for more heap
    size = (words % 2) ? (words + 1)*WSIZE : words*WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1){
        return NULL;
    }

    PUT(HDRP(bp), PACK(size, 0)); // make header to new block
    PUT(FTRP(bp), PACK(size, 0)); // make footer to new block
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // make epilogue header to new block

    return coalesce(bp); // if prev block is finished as usable block, combine two usable blocks
}

/*
 * find_fit
 */
static void *find_fit(size_t asize){
    char *bp;

    for (bp = next_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){
            return bp;
        }
    }
    return NULL;
}

/*
 * coalesce
 */
static void *coalesce(void *bp){
    // 0 no, 1 yes
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));

    size_t size = GET_SIZE(HDRP(bp));

    /* 
     * case 1 : prev block, next block both have 1 as last bit
     * no block combine and return bp
     */
    if (prev_alloc && next_alloc){
        return bp;
    }

    /* 
     * case 2 : prev block have 1, next block have 0 as last bit
     * block combine with next block and return bp
     */
    else if (prev_alloc && !next_alloc){
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); // add next block size to this block size
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    /* 
     * case 3 : prev block have 0, next block have 1 as last bit
     * block combine with prev block and return bp
     */
    else if (!prev_alloc && next_alloc){
        size += GET_SIZE(HDRP(PREV_BLKP(bp))); // add next block size to this block size
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    /* 
     * case 4 : prev block, next block both have 0 as last bit
     * block combine with prev, next block and return bp
     */
    else if (!prev_alloc && !next_alloc){
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))) + GET_SIZE(HDRP(PREV_BLKP(bp))); // add next, prev block size to this block size
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    return bp;
}

/*
 * place
 */
static void place(void *bp, size_t asize){
    size_t size = GET_SIZE(HDRP(bp));

    if ((size-asize) >= (2*DSIZE)){
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(size-asize, 0));
        PUT(FTRP(bp), PACK(size-asize, 0));
    }else{
        PUT(HDRP(bp), PACK(size, 1));
        PUT(FTRP(bp), PACK(size, 1));
    }

    return 0;
}

/*
 * malloc
 */
void *malloc (size_t size) {
    size_t asize;
    size_t extendsize;
    char *bp;

    // make enough space for header, footer using size
    // 8 -> min
    // 8 -> header, footer overhead
    if (size <= DSIZE){
        // if allocating size is smaller than DSIZE+OVERHEAD, set block size as DSIZE to align
        asize = 2*DSIZE;
    } else {
        // else set block size by 
        asize = DSIZE*((size+(DSIZE) + (DSIZE-1)) / DSIZE);
    }

    // search usable block and allocate requested block
    if ((bp = find_fit(asize)) != NULL){
        place(bp, asize);
        return bp;
    }

    // if there's nothing found, extend to new usable block and allocate
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL){
        return NULL;
    }
    place(bp, asize);

    return bp;
}

/*
 * free
 */
void free (void *ptr) {
    if(!ptr) return;

    size_t size = GET_SIZE(HDRP(ptr)); // read block size from ptr's header

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    next_listp = coalesce(ptr);
}

/*
 * realloc - you may want to look at mm-naive.c
 */
void *realloc(void *oldptr, size_t size) {
    size_t oldsize;
    char *newptr;

    if (size == 0){
        free(oldptr);
        return 0;
    }
    if (oldptr == NULL){
        return malloc(size);
    }

    newptr = malloc(size);
    if (!newptr){
        return 0;
    }
    
    oldsize = GET_SIZE(HDRP(oldptr));
    if (size < oldsize){
        oldsize = size;
    }
    
    memcpy(newptr, oldptr, oldsize);
    free(oldptr);

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