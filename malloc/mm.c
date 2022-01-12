/**
 * @file mm.c
 * @brief A 64-bit struct-based implicit free list memory allocator
 *
 * 15-213: Introduction to Computer Systems
 * 
 * A memory allocation functions with 16 byte aligned pointers.
 *
 * Main functions included: malloc, calloc, realloc, free
 * 
 * Implemented memory allocating with a segregated list with 15 buckets.
 * The first bucket of the seglist is for a special 16 byte "mini block".
 * Other than miniblocks all blocks are 32 bytes or greater.
 * 
 * All blocks contain a header with with relevant information such as its size,
 * allocation status, and whether it is a mini block. Free blocks also contain a 
 * footer which mirrors the header at the end of the block so the
 * start location of the block can be found.
 * 
 * 
 *
 * @author Jack Stellwagen <jstellwa@andrew.cmu.edu>
 */

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "memlib.h"
#include "mm.h"

/* Do not change the following! */

#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mem_memset
#define memcpy mem_memcpy
#endif /* def DRIVER */

/* You can change anything from here onward */

/*
 *****************************************************************************
 * If DEBUG is defined (such as when running mdriver-dbg), these macros      *
 * are enabled. You can use them to print debugging output and to check      *
 * contracts only in debug mode.                                             *
 *                                                                           *
 * Only debugging macros with names beginning "dbg_" are allowed.            *
 * You may not define any other macros having arguments.                     *
 *****************************************************************************
 */
#ifdef DEBUG
/* When DEBUG is defined, these form aliases to useful functions */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_requires(expr) assert(expr)
#define dbg_assert(expr) assert(expr)
#define dbg_ensures(expr) assert(expr)
#define dbg_printheap(...) print_heap(__VA_ARGS__)
#else
/* When DEBUG is not defined, no code gets generated for these */
/* The sizeof() hack is used to avoid "unused variable" warnings */
#define dbg_printf(...) (sizeof(__VA_ARGS__), -1)
#define dbg_requires(expr) (sizeof(expr), 1)
#define dbg_assert(expr) (sizeof(expr), 1)
#define dbg_ensures(expr) (sizeof(expr), 1)
#define dbg_printheap(...) ((void)sizeof(__VA_ARGS__))
#endif

/* Basic constants */

typedef uint64_t word_t;

/** @brief How many elements we want in the seglist **/
static const size_t seglist_length = 15;

/** @brief Word and header size (bytes) */
static const size_t wsize = sizeof(word_t);

/** @brief Double word size (bytes) */
static const size_t dsize = 2*wsize;

/** @brief Minimum block size (bytes) */
static const size_t min_block_size = dsize;

/**
 * @brief THe initial increment of the heap
 * (Must be divisible by dsize)
 */
static const size_t chunksize = (1 << 12);

/**
 * @brief The mask to isolate the allocation bit in the header
 */
static const word_t alloc_mask = 0x1;

/**
 * @brief The mask to isolate the previous allocation bit in the header
 */
static const word_t alloc_prev_mask = 0x2;


/**
 * @brief The mask to isolate the mini block bit in the header
 */
static const word_t mini_mask = 0x4;

/**
 * @brief The mask to extract the size from a header
 */
static const word_t size_mask = ~(word_t)0xF;

/** @brief Represents the header and payload of one block in the heap */
typedef struct block {
    /** @brief Header contains size + allocation flag */
    word_t header;

    /**
     * @brief A pointer to the block payload.
     *
     * WARNING: A zero-length array must be the last element in a struct, so
     * there should not be any struct fields after it. For this lab, we will
     * allow you to include a zero-length array in a union, as long as the
     * union is the last field in its containing struct. However, this is
     * compiler-specific behavior and should be avoided in general.
     *
     * WARNING: DO NOT cast this pointer to/from other types! Instead, you
     * should use a union to alias this zero-length array with another struct,
     * in order to store additional types of data in the payload memory.
     */
    union {
        struct { 
            struct block* successor;
            struct block* predecessor;
        };
        char payload[0];
    };

    
} block_t;

/* Global variables */

/** @brief Pointer to first block in the heap */
static block_t *heap_start = NULL;

static block_t* free_root[seglist_length];

/*
 *****************************************************************************
 * The functions below are short wrapper functions to perform                *
 * bit manipulation, pointer arithmetic, and other helper operations.        *
 *                                                                           *
 * We've given you the function header comments for the functions below      *
 * to help you understand how this baseline code works.                      *
 *                                                                           *
 * Note that these function header comments are short since the functions    *
 * they are describing are short as well; you will need to provide           *
 * adequate details for the functions that you write yourself!               *
 *****************************************************************************
 */

/*
 * ---------------------------------------------------------------------------
 *                        BEGIN SHORT HELPER FUNCTIONS
 * ---------------------------------------------------------------------------
 */

/**
 * @brief Returns the maximum of two integers.
 * @param[in] x
 * @param[in] y
 * @return `x` if `x > y`, and `y` otherwise.
 */
static size_t max(size_t x, size_t y) {
    return (x > y) ? x : y;
}


/**
 * @brief Returns the minimum of two integers.
 * @param[in] x
 * @param[in] y
 * @return `x` if `x < y`, and `y` otherwise.
 */
static size_t min(size_t x, size_t y) {
    return (x < y) ? x : y;
}

/**
 * @brief Rounds `size` up to next multiple of n
 * @param[in] size
 * @param[in] n
 * @return The size after rounding up
 */
static size_t round_up(size_t size, size_t n) {
    return n * ((size + (n - 1)) / n);
}

/**
 * @brief Packs the `size` and `alloc` of a block into a word suitable for
 *        use as a packed value.
 *
 * Packed values are used for both headers and footers.
 *
 * The allocation status is packed into the lowest bit of the word.
 *
 * @param[in] size The size of the block being represented
 * @param[in] alloc True if the block is allocated
 * @return The packed value
 */
static word_t pack(size_t size, bool alloc, bool prev_alloc) {
    word_t word = size;
    if (alloc) {
        word |= alloc_mask;
    }
    if (prev_alloc) {
        word |= alloc_prev_mask;
    }
    if (size == min_block_size){
        word |= mini_mask;
    }
    //if (size == min_block_size) printf("HEX THING: %lx \n", word);
    return word;
}

/**
 * @brief Extracts the size represented in a packed word.
 *
 * This function simply clears the lowest 4 bits of the word, as the heap
 * is 16-byte aligned.
 *
 * @param[in] word
 * @return The size of the block represented by the word
 */
static size_t extract_size(word_t word) {
    return (word & size_mask);
}



/**
 * @brief Returns the allocation status of the previous block given a header value.
 *
 * This is based on the second lowest bit of the header value.
 *
 * @param[in] word
 * @return The allocation status correpsonding to the word
 */
static bool extract_mini(word_t word) {
    return (bool)((word & mini_mask) >> 2);
}
/**
 * @brief Returns the allocation status of a block, based on its header.
 * @param[in] block
 * @return The allocation status of the previous block
 */
static bool get_mini(block_t *block) {
    dbg_requires(block != NULL);
    return extract_mini(block->header);
}


static void set_mini(block_t *block, bool mini){

    if (mini){
        block->header = block->header | mini_mask;
    }
    else {
        block->header = block->header & (~mini_mask);
    }
}


/**
 * @brief Extracts the size of a block from its header.
 * @param[in] block
 * @return The size of the block
 */
static size_t get_size(block_t *block) {
    if (extract_mini(block->header)){
        return min_block_size;
    }
    return extract_size(block->header);
}

/**
 * @brief Given a payload pointer, returns a pointer to the corresponding
 *        block.
 * @param[in] bp A pointer to a block's payload
 * @return The corresponding block
 */
static block_t *payload_to_header(void *bp) {
    return (block_t *)((char *)bp - offsetof(block_t, payload));
}

/**
 * @brief Given a block pointer, returns a pointer to the corresponding
 *        payload.
 * @param[in] block
 * @return A pointer to the block's payload
 * @pre The block must be a valid block, not a boundary tag.
 */
static void *header_to_payload(block_t *block) {
    dbg_requires(get_size(block) != 0);
    return (void *)(block->payload);
}


/**
 * @brief Given a block pointer, returns a pointer to the corresponding
 *        footer.
 * @param[in] block
 * @return A pointer to the block's footer
 * @pre The block must be a valid block, not a boundary tag.
 */
static word_t *header_to_footer(block_t *block) {
    dbg_requires(get_size(block) != 0 &&
                 "Called header_to_footer on the epilogue block");
    if (get_mini(block)) printf("No buen 1\n");
    return (word_t *)(block->payload + get_size(block) - dsize);
}

/**
 * @brief Given a block footer, returns a pointer to the corresponding
 *        header.
 * @param[in] footer A pointer to the block's footer
 * @return A pointer to the start of the block
 * @pre The footer must be the footer of a valid block, not a boundary tag.
 */
static block_t *footer_to_header(word_t *footer) {
    size_t size = extract_size(*footer);
    dbg_assert(size != 0 && "Called footer_to_header on the prologue block");
    if (extract_mini(*footer)) printf("No buen \n");
    return (block_t *)((char *)footer + wsize - size);
}

/**
 * @brief Returns the payload size of a given block.
 *
 * The payload size is equal to the entire block size minus the sizes of the
 * block's header and footer.
 *
 * @param[in] block
 * @return The size of the block's payload
 */
static size_t get_payload_size(block_t *block) {
    size_t asize = get_size(block);
    return asize - wsize;
}

/**
 * @brief Returns the allocation status of a given header value.
 *
 * This is based on the lowest bit of the header value.
 *
 * @param[in] word
 * @return The allocation status correpsonding to the word
 */
static bool extract_alloc(word_t word) {
    return (bool)(word & alloc_mask);
}


/**
 * @brief Returns the allocation status of a block, based on its header.
 * @param[in] block
 * @return The allocation status of the block
 */
static bool get_alloc(block_t *block) {
    dbg_requires(block != NULL);
    return extract_alloc(block->header);
}


/**
 * @brief Returns the allocation status of the previous block given a header value.
 *
 * This is based on the second lowest bit of the header value.
 *
 * @param[in] word
 * @return The allocation status correpsonding to the word
 */
static bool extract_prev_alloc(word_t word) {
    return (bool)((word & alloc_prev_mask) >> 1);
}
/**
 * @brief Returns the allocation status of a block, based on its header.
 * @param[in] block
 * @return The allocation status of the previous block
 */
static bool get_prev_alloc(block_t *block) {
    dbg_requires(block != NULL);
    return extract_prev_alloc(block->header);
}

/**
 * @brief Sets the previous allocation bit of a block.
 * @param[in] block
 * @param[in] prev_alloc
 */
static void set_prev_alloc(block_t *block, bool prev_alloc){

    if (prev_alloc){
        block->header = block->header | alloc_prev_mask;
    }
    else {
        block->header = block->header & (~alloc_prev_mask);
    }
}







/**
 * @brief Writes an epilogue header at the given address.
 *
 * The epilogue header has size 0, and is marked as allocated.
 *
 * @param[out] block The location to write the epilogue header
 */
static void write_epilogue(block_t *block) {
    dbg_requires(block != NULL);
    dbg_requires((char *)block == mem_heap_hi() - 7);
    block->header = pack(0, true,false);
}

/**
 * @brief Writes a block starting at the given address.
 *
 * This function writes both a header and footer, where the location of the
 * footer is computed in relation to the header.
 *
 * TODO: Are there any preconditions or postconditions?
 *
 * @param[out] block The location to begin writing the block header
 * @param[in] size The size of the new block
 * @param[in] alloc The allocation status of the new block
 */
static void write_block(block_t *block, size_t size, bool alloc, bool prev_alloc) {
    dbg_requires(block != NULL);
    dbg_requires(size > 0);
    block->header = pack(size, alloc, prev_alloc);
    if (!alloc && !(size == min_block_size)){
        word_t *footerp = header_to_footer(block);
        *footerp = pack(size, alloc,prev_alloc);
    }
}


static void write_footer(block_t* block){
     word_t *footerp = header_to_footer(block);
     *footerp = block->header;

}




/**
 * @brief Finds the next consecutive block on the heap.
 *
 * This function accesses the next block in the "implicit list" of the heap
 * by adding the size of the block.
 *
 * @param[in] block A block in the heap
 * @return The next consecutive block on the heap
 * @pre The block is not the epilogue
 */
static block_t *find_next(block_t *block) {
    dbg_requires(block != NULL);
    dbg_requires(get_size(block) != 0 &&
                 "Called find_next on the last block in the heap");
    return (block_t *)((char *)block + get_size(block));
}

/**
 * @brief Finds the footer of the previous block on the heap.
 * @param[in] block A block in the heap
 * @return The location of the previous block's footer
 */
static word_t *find_prev_footer(block_t *block) {
    // Compute previous footer position as one word before the header
    return &(block->header) - 1;
}

/**
 * @brief Finds the previous consecutive block on the heap.
 *
 * This is the previous block in the "implicit list" of the heap.
 *
 * If the function is called on the first block in the heap, NULL will be
 * returned, since the first block in the heap has no previous block!
 *
 * The position of the previous block is found by reading the previous
 * block's footer to determine its size, then calculating the start of the
 * previous block based on its size.
 *
 * @param[in] block A block in the heap
 * @return The previous consecutive block in the heap.
 */
static block_t *find_prev(block_t *block) {
    dbg_requires(block != NULL);
    dbg_requires(!get_prev_alloc(block));

    //if(get_mini(block)) printf( "FUCK \n");
    word_t *footerp = find_prev_footer(block);
    if (extract_mini(*footerp)){
        return (block_t*)((size_t) footerp - 8);
    }

    // Return NULL if called on first block in the heap
    if (extract_size(*footerp) == 0) {
        return NULL;
    }

    return footer_to_header(footerp);
}


/**
 * @brief Finds next block in the explicit list
 * @param[in] block A block in the heap
 * @return A pointer to the next block in the explicit list
 *
 * Requires the block is free 
 **/
static block_t *get_next_free(block_t *block){
    dbg_requires(block != NULL);
    dbg_requires(! get_alloc(block));

    return (block_t*)((size_t) block->successor & (~0x7));

}

/**
 * @brief Finds which explicit list where a block of the given size belongs
 * @param[in] size the size of a given block
 * @return The index of the seglist where the size belongs
 *
 **/
size_t get_seglist_ind(size_t size){
    dbg_requires(size != 0);

    size_t count = 0;
    if (size <= 16){
        count = 1;
    }else{
        for(size_t i = (size >>4) ; i !=0; i = i>>1) count++;
    }

    return min(count - 1, seglist_length -1);

}


/**
 * @brief Finds the previous block in the explicit list
 * @param[in] block A block in the heap
 * @return A pointer to the previous block in the explicit list
 *
 * Requires the block is free 
 */
static block_t *get_prev_free(block_t *block){
        dbg_requires(block != NULL);
        dbg_requires(! get_alloc(block));
        if (get_mini(block)){
            return (block_t*) (block->header & ~(0x7));
            //return (block_t*) extract_size(block->header);
        }
        return block->predecessor;

}

/**
 * @brief Sets the previous pointer of a block in the explicit list
 * @param[in] block A block in the heap
 *
 * Requires the block is free and not NULL
 */

static void set_prev_free(block_t *block, block_t *prev){
    dbg_requires(block != NULL);
    dbg_requires(!get_alloc(block));
    if (get_mini(block)){
        if (get_alloc(block)) printf("PROBLEMO \n");

        block->header = (block->header & ((size_t)0x7)) | ((size_t) prev);
    }
    else{
        block->predecessor = prev;
    }
}

/**
 * @brief Sets the next pointer of a block in the explicit list
 * @param[in] block A block in the heap
 *
 * Requires the block is free and not NULL
 */
static void set_next_free(block_t *block, block_t *next){
    dbg_requires(block != NULL);
    dbg_requires(! get_alloc(block));

    block->successor = next;

    if (get_mini(block)) {
        size_t s = (size_t) block->successor;
        s |= mini_mask;
        block->successor = (block_t*) s;
    }
}

/**
 * @brief removes a block from the free list
 * @param[in] block A block in the free list
 *
 * Requires the block is free and not NULL
 */
static void remove_from_free(block_t *block){
    dbg_requires(block != NULL);
    dbg_requires(! get_alloc(block));

    block_t *prev = get_prev_free(block);
    block_t *next = get_next_free(block);


    size_t seglist_ind = get_seglist_ind(get_size(block));

    if (next == NULL && prev == NULL){
        free_root[seglist_ind] = NULL;
    }else if (next == NULL){
        set_next_free(prev, NULL);
    }
    else if (prev == NULL){
        set_prev_free(next,NULL);
        free_root[seglist_ind] = next;
    }else{
        set_next_free(prev, next);
        set_prev_free(next, prev);
    }

}


/**
 * @brief Adds a block to the free list
 * @param[in] block A block in the heap
 * 
 * Adds a given block to the beginning of its respective list
 *
 * Requires the block is free and not NULL
 */
static void add_to_free(block_t *block){
    dbg_requires(block != NULL);
    dbg_requires(! get_alloc(block));

    size_t seglist_ind = get_seglist_ind(get_size(block)); 
    

    if (free_root[seglist_ind] == NULL){
        free_root[seglist_ind] = block;

        set_prev_free(free_root[seglist_ind],NULL);
        

        set_next_free(free_root[seglist_ind],NULL);

    }else{
        set_prev_free(free_root[seglist_ind],block);

        set_next_free(block, free_root[seglist_ind]);

        set_prev_free(block,NULL);

        free_root[seglist_ind] = block;
    }

}




/*
 * ---------------------------------------------------------------------------
 *                        END SHORT HELPER FUNCTIONS
 * ---------------------------------------------------------------------------
 */

/******** The remaining content below are helper and debug routines ********/

/**
 * @brief Joins consecutive free blocks
 *
 * Given a free block coalesce checks that blocks neighbors 
 * and concatenates them into a larger block if they are free
 * The argument block must be free
 *
 * @param[in] block 
 * @return The block that is formed by joining with free neighbors
 */
static block_t *coalesce_block(block_t *block) {
    dbg_requires(! get_alloc(block));

    block_t* prev;
    block_t* next;

    size_t new_size;
    
    bool prev_alloced = get_prev_alloc(block);



    next = find_next(block);

    bool next_alloced = get_alloc(next);



    if (!next_alloced && !prev_alloced){
        prev = find_prev(block);

        new_size = get_size(prev) + get_size(block) + get_size(next);
        

        remove_from_free(prev);
        remove_from_free(next);
        remove_from_free(block);

        write_block(prev,new_size,false, true);
        add_to_free(prev);
        return prev;

    }else if (!next_alloced){
        new_size = get_size(next) + get_size(block);

        remove_from_free(next);
        remove_from_free(block);

        write_block(block,new_size,false, true);
        add_to_free(block);
        return block;

    } else if (!prev_alloced){
        prev = find_prev(block);
        new_size = get_size(block) + get_size(prev);

        remove_from_free(prev);
        remove_from_free(block);

        write_block(prev, new_size, false,true);
        add_to_free(prev);
        return prev;
    }





    return block;
}



/**
 * @brief Prints relevant debugging information about a block to the screen
 * @param[in] block the block to print
 * @param[in] block_num an integer identifier to be printed with the block
 */
void print_block(block_t *block, int block_num){
    printf("BLOCK: %d \n", block_num);
    printf("MINI: %u \n", get_mini(block));
    printf("Allocated: %d \n", get_alloc(block));
    printf("Prev Allocated: %d \n", get_prev_alloc(block));
    printf("Address: %lx \n",(size_t) block );
    printf("SIZE: %lu \n", get_size(block));
    if (!get_alloc(block)){
        printf("NEXT: %lx \n", (size_t) get_next_free(block));
        printf("PREV: %lx \n", (size_t) get_prev_free(block));
    }
    printf("\n");


}


/**
 * @brief Prints the heap block by block for both the implicit and explicit list
 */
void print_heap(){
    int i = 0;
    printf("PRINTING HEAP \n _______________________ \n");
    for (block_t *block = heap_start; get_size(block) > 0; block = find_next(block)) {
        print_block(block,i);
        i++;

    }
    printf("Explicit free list \n");
    i = 0;
    for (size_t seglist_ind = 0; seglist_ind< seglist_length ;seglist_ind++){
        printf("SEGLIST %lu \n", seglist_ind);
         for( block_t *block = free_root[seglist_ind]; block != NULL; block = get_next_free(block)){
            print_block(block, i);
            i++;
         }
    }
        
    printf("Prologue: \n");
    print_block((block_t*) mem_heap_lo(),0);

    printf("Epilogue: \n");
    print_block((block_t*) (mem_heap_hi() -7),0);
}






/**
 * @brief Makes the heap size bytes larger
 *
 * 
 *
 * @param[in] size the amount to extend the heap by 
 * @return a pointer to the block created by extending the heap
 */
static block_t *extend_heap(size_t size) {
    void *bp;

    // Allocate an even number of words to maintain alignment
    size = round_up(size, dsize);
    if ((bp = mem_sbrk(size)) == (void *)-1) {
        return NULL;
    }

    /*
     * TODO: delete or replace this comment once you've thought about it.
     * Think about what bp represents. Why do we write the new block
     * starting one word BEFORE bp, but with the same size that we
     * originally requested?
     */

    // Initialize free block header/footer
    block_t *block = payload_to_header(bp);
    block_t *old_epi = (block_t*)(((size_t) bp ) -8);
    write_block(block, size, false, get_prev_alloc(old_epi));


    add_to_free(block);
    

    // Create new epilogue header
    block_t *block_next = find_next(block);
    write_epilogue(block_next);

    // Coalesce in case the previous block was free
    block = coalesce_block(block);

    return block;
}

/**
 * @brief Cuts the end off an allocated block and creates a free block
 * if there is sufficient padding
 *
 * 
 * Requires asize <= block size 
 * @param[in] block The newly allocated block
 * @param[in] asize The minimum size the block could be without padding
 */
static void split_block(block_t *block, size_t asize) {
    dbg_requires(get_alloc(block));
    dbg_requires(asize <= get_size(block));

    size_t block_size = get_size(block);

    if ((block_size - asize) >= min_block_size) {
        block_t *block_next;
        write_block(block, asize, true,true);

        block_next = find_next(block);
        write_block(block_next, block_size - asize, false, true);

        add_to_free(block_next);

    }

    dbg_ensures(get_alloc(block));
}

/**
 * @brief Finds a free block of at least size asize
 *
 * 
 *
 * @param[in] asize the size you want to allocate
 * @return a block of at least asize if available and NULL otherwise 
 */
static block_t *find_fit(size_t asize) {
    int max_check = 9;
    block_t *block;
    block_t *min = NULL;
    int checked = 0;

    size_t seglist_ind = get_seglist_ind(asize);



    for( block = free_root[seglist_ind]; block != NULL; block = get_next_free(block)){
        if (asize <= get_size(block)) {
            if (min == NULL){
                min = block;
            }
            if (get_size(block) < get_size(min)){
                min = block;
            }
            if (min != NULL && checked > max_check){
                return min;
            }
        }
        checked++;
    }
    //if block not found in the corresponding seglist
    if (min == NULL){
        for( size_t i = seglist_ind + 1; i < seglist_length; i++){
            // just returns the first one. Could be optimized
            if (free_root[i] != NULL) {
                return free_root[i];
            }
        }
    }
    return min;
}

/**
 * ____________________________________
 *      Heapcheck Helper Functions
 *_____________________________________
 **/



/**
 * @brief Ensures a blocks payload is properly 16 bit aligned
 *
 * @param[in] block the block to check
 * @return false if improperly aligned, true otherwise
 */
bool check_address_alignment(block_t *block){
    size_t pload = (size_t) header_to_payload(block);
    return ! (pload & 0xF ) ;
}

/**
 * @brief Ensures the prologue dummy node is size 0 and allocated
 *
 * @return false if conditions not met, true otherwise
 */
bool check_prologue(){
    //if (heap_start == NULL) return false;
    block_t *prologue = (block_t *) mem_heap_lo();
    return get_size(prologue) == 0 && get_alloc(prologue) && get_prev_alloc(prologue);
}

/**
 * @brief Ensures the epilogue dummy node is size 0 and allocated
 *
 * @return false if conditions not met, true otherwise
 */
bool check_epilogue(){
    block_t *epilogue = (block_t*) (mem_heap_hi() - 7);

    return get_size(epilogue) == 0 && get_alloc(epilogue);
}

/**
 * @brief checks if the previous block and currect block are both free
 * @param[in] block the block to check
 * @return false if conditions not met, true otherwise
 * requires the block is not the first dummy node
 */

bool consecutive_free(block_t *block){
    return !get_alloc(block) && !get_prev_alloc(block);
    return ! ( get_alloc(block) || get_alloc(find_prev(block)));
}


/**
 * @brief checks if the block is within the bounds of the heap
 * @param[in] block the block to check
 * @return false if outside of heap, true otherwise
 * requires the block is not NULL
 */
bool within_heap_boundaries(block_t* block){
    size_t block_pointer = (size_t) block;
    bool above_min = block_pointer > (size_t) mem_heap_lo();
    bool below_max = block_pointer < (size_t) mem_heap_hi();
    return  below_max && above_min;
}

/**
 * @brief checks if the header and footer match as well as 
 * if the previosu block points to the current block
 * @param[in] block the block to check
 * @return false if conditions not met, true otherwise
 * requires the block is not the first dummy node
 */
bool check_header_and_footer(block_t *block){
    if (get_size(block)< min_block_size){
        printf("MIN SIZE: %lu", min_block_size);
        print_block(block, 100);
        printf("Block size below min");
        return false;
    }

    if (!get_alloc(block) && *(header_to_footer(block)) != block->header){
        printf("Block header and footer inconsistent");
        return false;
     }



    return true;
}


/**
 * @brief counts the number of free blocks according to the implicit list
 * @return The number of free blocks
 *
 */
int count_free(){
    int num_free = 0;
    for (block_t *block = heap_start; get_size(block) > 0; block = find_next(block)){
       if (!get_alloc(block)) num_free++;
   }
   return num_free;
}
/**
 * @brief Checks if the previous block and currect block point to eachother in
 * the explicit list
 * @param[in] block the block to check
 * @return false if pointers do not match, true otherwise
 * requires the block is not the first in the explicit list
 */
bool explicit_list_pointer_consistency(block_t *block){

    block_t *prev = get_prev_free(block);
    if (prev == NULL){
        printf("Previous block NULL \n");
        return false;
    }
    block_t *current = get_next_free(prev);
    if (current != block){
        printf("Previous pointer does not point to current \n");
        return false;
    }

    return true;
}

/**
 * @brief Checks each block of an explicit list for variosu invariants
 * @param[in] seglist_ind which explicit list within the segregated list to check
 * @returns false if the invariants are broken, true otherwise
 */
bool check_explicit_list(size_t seglist_ind){
    for( block_t *block = free_root[seglist_ind]; block != NULL; block = get_next_free(block)){
        if (get_alloc(block)){
            printf("Block is marked as allocated in the free list\n");
            return false;
        }
        if (get_mini(block) && !(get_size(block) == min_block_size)){
            printf("Block labeled mini with size greater than min size \n");
            return false;
        }

        if ((block != free_root[seglist_ind]) && !explicit_list_pointer_consistency(block)){
            printf("Explicit list pointers inconsistent \n");
            return false;
        }
        if (!within_heap_boundaries(block)){
            printf("Explicit list block not within boundaries \n");
            return false;
        }
        //Make sure the seglist function says this block should be in this list
        if (seglist_ind != get_seglist_ind(get_size(block))){
            printf("Block in seglist %lu of size %lu \n", seglist_ind, get_size(block));
            return false;
        }
    }
    return true;
}


/**
 * @brief Ensures the heap meets various invariants
 *
 * Checks both the implicit and explicit lists
 *
 * @param[in] line the line at which the function is called
 * @return false if the heap violates an invariant, true otherwise
 */
bool mm_checkheap(int line) {

    if (!check_prologue()){
        printf("There is a problem with the prologue node at %d\n", line);
        print_heap();
        return false;
    }
    if (!check_epilogue()){
        printf("There is a problem with the epilogue node at %d \n",line);
        print_heap();
        return false;
    }

    // Imlicit list checks
    for (block_t *block = heap_start; get_size(block) > 0; block = find_next(block)) {
        
        if (!check_address_alignment(block)){
            printf("Improper Address Alignment at %d\n", line);
            print_heap();
            return false;
        }
        if (!within_heap_boundaries(block)){
            printf("Block outside of heap boundaries at line %d \n.", line);
            print_heap();
            return false;
        }
        if (block!= heap_start && !check_header_and_footer(block)){
            printf("There is a problem with a blocks header or footer at %d\n", line);
            print_heap();
            return false;
        }

        if (block != heap_start && consecutive_free(block)){
            printf("Coalesce error. 2 free blocks in a row\n");
            print_heap();
            return false;
        }

    }

    //Check explicit free list

    int num_free = 0;
    for (size_t seglist_ind = 0; seglist_ind<seglist_length; seglist_ind++){
        if (!check_explicit_list(seglist_ind)){
            printf("problem with seglist number %lu \n", seglist_ind);
            print_heap();
            return false;
        }
        //Count free nodes via explicit list
        for( block_t *block = free_root[seglist_ind]; block != NULL; block = get_next_free(block)) num_free++;
    }
    

    //Check to make sure the implicit and explicit lists are showing the same number of free blocks
    if (num_free != count_free()){
        printf("Implicit list and explicit list free count disagree at line %d \n",line);
        printf("Explicit List: %d,   Implicit list: %d \n ", num_free,count_free());
        print_heap();
        return false;
    }

    return true;
}



void update_next(block_t *block, bool alloc){

    block_t *next_block = find_next(block);

    set_prev_alloc(next_block, alloc);

    if (!alloc && !get_mini(block)){
        write_footer(block);
    }


}


/**
 * @brief Initialize all explicit lists to NULL
 *
 */

void free_root_init(){

    for(size_t i = 0; i< seglist_length; i++){
        free_root[i] = NULL;
    }
}


/**
 * @brief
 *
 * Initialize all variable and data
 * 
 * Sets all of the initial values for relecant data
 * Initializes the seglist, creats the epilog and prologue,
 * extends the heap the necessary amount
 * 
 *
 * @return bool indicating whether initilzation was successful
 */
bool mm_init(void) {
    // Create the initial empty heap
    word_t *start = (word_t *)(mem_sbrk(2 * wsize));


    free_root_init();


    if (start == (void *)-1) {
        return false;
    }



    start[0] = pack(0, true, true); // Heap prologue (block footer)
    start[1] = pack(0, true, true); // Heap epilogue (block header)




    // Heap starts with first "block header", currently the epilogue
    heap_start = (block_t *)&(start[1]);

    // Extend the empty heap with a free block of chunksize bytes
    if (extend_heap(chunksize) == NULL) {
        return false;
    }

    return true;
}

/**
 * @brief
 *
 * Allocates size bytes of memory 
 * 
 * Memory is not garbage collected and must be freed 
 * at some point using free().
 *
 * The number of bytes to allocate 
 * @param[in] size
 * 
 * @return
 * Returns a 16 byte aligned pointer to the newly allocated memory
 */
void *malloc(size_t size) {
    dbg_requires(mm_checkheap(__LINE__));

    size_t asize;      // Adjusted block size
    size_t extendsize; // Amount to extend heap if no fit is found
    block_t *block;
    void *bp = NULL;

    // Initialize heap if it isn't initialized
    if (heap_start == NULL) {
        mm_init();
    }

    // Ignore spurious request
    if (size == 0) {
        dbg_ensures(mm_checkheap(__LINE__));
        return bp;
    }

    // Adjust block size to include overhead and to meet alignment requirements
    asize = max(round_up(size + wsize, dsize), min_block_size);

    // Search the free list for a fit
    block = find_fit(asize);
    

    // If no fit is found, request more memory, and then and place the block
    if (block == NULL) {
        // Always request at least chunksize
        extendsize = max(asize, chunksize);
        block = extend_heap(extendsize);
        // extend_heap returns an error
        if (block == NULL) {
            return bp;
        }
    }

    // The block should be marked as free
    dbg_assert(!get_alloc(block));


    //remove from explicit list

    remove_from_free(block);

    // Mark block as allocated
    size_t block_size = get_size(block);
    write_block(block, block_size, true,true);

    // Try to split the block if too large
    split_block(block, asize);

    update_next(block, true);

    bp = header_to_payload(block);


    dbg_ensures(mm_checkheap(__LINE__));
    return bp;
}

/**
 * @brief
 *
 * Given a 16 byte aligned pointer, free unallocates the memory
 * 
 * free requires that the memory given as input is allocated
 *
 * A 16 byte aligned pointer to the payload of allocated memory 
 * @param[in] bp
 */
void free(void *bp) {
    dbg_requires(mm_checkheap(__LINE__));

    if (bp == NULL) {
        return;
    }

    block_t *block = payload_to_header(bp);
    size_t size = get_size(block);

    // The block should be marked as allocated
    dbg_assert(get_alloc(block));


    bool prev_alloc = get_prev_alloc(block);


    // Mark the block as free
    write_block(block, size, false, prev_alloc);
    add_to_free(block);

    // Try to coalesce the block with its neighbors
    //print_heap();
    block = coalesce_block(block);

    update_next(block, false);


    dbg_ensures(mm_checkheap(__LINE__));
}

/**
 * @brief
 * Reallocates the given area of memory. 
 * 
 * The area of memory must have previously allocated by malloc(), 
 * calloc() or realloc() and not yet freed 
 * 
 * if ptr is NULL realloc has the same function as malloc(size)
 * 
 *  The pointer to reallocate
 * @param[in] ptr
 * 
 * The new size 
 * @param[in] size
 * 
 * @return a new pointer
 */
void *realloc(void *ptr, size_t size) {
    block_t *block = payload_to_header(ptr);
    size_t copysize;
    void *newptr;

    // If size == 0, then free block and return NULL
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    // If ptr is NULL, then equivalent to malloc
    if (ptr == NULL) {
        return malloc(size);
    }

    // Otherwise, proceed with reallocation
    newptr = malloc(size);

    // If malloc fails, the original block is left untouched
    if (newptr == NULL) {
        return NULL;
    }

    // Copy the old data
    copysize = get_payload_size(block); // gets size of old payload
    if (size < copysize) {
        copysize = size;
    }
    memcpy(newptr, ptr, copysize);

    // Free the old block
    free(ptr);

    return newptr;
}

/**
 * @brief
 *
 * Wrapper for malloc to allocate memory for an array 
 *
 * The size of the individual elements in the array
 * @param[in] elements
 * The number of elements in the array 
 * @param[in] size
 * 
 * 
 * @return A pointer to the start of the array 
 * Returns NULL if size of elements  equal 0.
 */
void *calloc(size_t elements, size_t size) {
    void *bp;
    size_t asize = elements * size;

    if (elements == 0) {
        return NULL;
    }
    if (asize / elements != size) {
        // Multiplication overflowed
        return NULL;
    }

    bp = malloc(asize);
    if (bp == NULL) {
        return NULL;
    }

    // Initialize all bits to 0
    memset(bp, 0, asize);

    return bp;
}

/*
 *****************************************************************************
 * Do not delete the following super-secret(tm) lines!                       *
 *                                                                           *
 * 53 6f 20 79 6f 75 27 72 65 20 74 72 79 69 6e 67 20 74 6f 20               *
 *                                                                           *
 * 66 69 67 75 72 65 20 6f 75 74 20 77 68 61 74 20 74 68 65 20               *
 * 68 65 78 61 64 65 63 69 6d 61 6c 20 64 69 67 69 74 73 20 64               *
 * 6f 2e 2e 2e 20 68 61 68 61 68 61 21 20 41 53 43 49 49 20 69               *
 *                                                                           *
 * 73 6e 27 74 20 74 68 65 20 72 69 67 68 74 20 65 6e 63 6f 64               *
 * 69 6e 67 21 20 4e 69 63 65 20 74 72 79 2c 20 74 68 6f 75 67               *
 * 68 21 20 2d 44 72 2e 20 45 76 69 6c 0a c5 7c fc 80 6e 57 0a               *
 *                                                                           *
 *****************************************************************************
 */
