#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "sfmm.h"
#include <errno.h>
#define ALLOC_MASK 0x8
#define PREV_ALLOC_MASK 0x4
#define block_size_MASK 0x0FFFFFFF0
#define payload_size_MASK 0xFFFFFFFF00000000
static int is_initialized = 0;
#define PACK(size, alloc)  ((size) | (alloc))

static size_t max_payload = 0;
void fix_bug(sf_block *block){
    size_t block_size = block->header & block_size_MASK;
    //check if next block within the boundary
    if((char*)block + block_size == NULL) {return;}
    sf_block* next_block = (sf_block*)((char*)block + block_size);
    size_t next_block_size = next_block->header & block_size_MASK;
    if(next_block_size == 0){return;}
    if (((char*)next_block < ((char*)sf_mem_start()+32)) ||
        (char*)next_block  > ((char*)sf_mem_end()-16-next_block_size)) {
        return;
    }
    //check if next next block within the boundary
    sf_block* next_next_block = (sf_block*)((char*)next_block + next_block_size);
    if(next_next_block == NULL) {return;}
    size_t next_next_block_size = next_next_block->header & block_size_MASK;
    if(next_next_block_size == 0){return;}
    if (((char*)next_next_block < ((char*)sf_mem_start()+32)) ||
        (char*)next_next_block  > ((char*)sf_mem_end()-16-next_next_block_size)) {
        return;
    }

    size_t prev_block_payload = (block->prev_footer & payload_size_MASK)>>32;
    size_t block_payload = (block->header & payload_size_MASK)>>32;

    //if prev block payload is not empty
    if (prev_block_payload){
        block->header = block-> header | PREV_ALLOC_MASK;
    }
    else{
        block->header = block-> header & ~PREV_ALLOC_MASK;
    }
    if(block_payload){
        next_block->header |= PREV_ALLOC_MASK;
        next_next_block->prev_footer |= PREV_ALLOC_MASK;
    }
    else{
        next_block->header &= ~PREV_ALLOC_MASK;
        next_next_block->prev_footer &= ~PREV_ALLOC_MASK;
    }

}

//initialzied the free_list
void sf_init(){
    if (is_initialized) {
        return;
    }
    for (int i = 0; i < NUM_FREE_LISTS; i++) {
        sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
        sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
    }
    is_initialized = 1;
}

//4, totaly size
int get_free_list_index(size_t size) {
    if (size <= 32) {
        return 0;
    }
    int index = 0;
    size_t M = 32;
    size_t fib_sequence[] = {M, 2*M, 3*M, 5*M, 8*M, 13*M, 21*M, 34*M};

    for (int i = 0; i < 8; i++) {
        if (size <= fib_sequence[i]) {
            index = i;
            break;
        }
    }
    if (size > 34*M) {
        index = NUM_FREE_LISTS - 2;
    }
    return index;
}

//3 insert block to free list
void insert_block(sf_block *block) {

    /*/////////////////////////////////////////////////////////////////
    //handle the allocated of next block
    size_t block_size = block->header & block_size_MASK;
    sf_block* next_block = (sf_block*)((char*)block + block_size);
    block->header &= ~ALLOC_MASK;
    next_block -> prev_footer &= ~ALLOC_MASK;

    //handle the pre_allocated of next block
    size_t next_block_size = next_block->header & block_size_MASK;
    sf_block* next_next_block = (sf_block*)((char*)next_block + next_block_size);
    next_block->header &= ~PREV_ALLOC_MASK;
    next_next_block->prev_footer &= ~PREV_ALLOC_MASK;*/

    //insert
    //if it is initial free block
    sf_block* epilogue = (sf_block*)((char*)sf_mem_end() - 16);
    size_t free_block_size = epilogue->prev_footer & block_size_MASK;
    sf_block *initial_free_block = (sf_block*) ((char*)sf_mem_end() - 16 - free_block_size);


    if(block == initial_free_block){
        //insert_block(new initial_free_block);
        sf_block *dummy = &sf_free_list_heads[NUM_FREE_LISTS-1];
        dummy->body.links.prev = block;
        block->body.links.next = dummy;
        block->body.links.prev = dummy;
        dummy->body.links.next = block;
    }
    else{
    //if it's not
    int list_index = get_free_list_index(block->header & block_size_MASK);
    sf_block *dummy3 = &sf_free_list_heads[list_index];
    sf_block *temp = dummy3->body.links.next;
    temp->body.links.prev = block;
    block->body.links.next = temp;
    block->body.links.prev = dummy3;
    dummy3->body.links.next = block;
    }
    //sf_show_heap();

}
//6 remove block from free list
void remove_block(sf_block *block) {

    //remove
    sf_block *temp_pre = block->body.links.prev;
    sf_block *temp_nex = block->body.links.next;
    temp_pre->body.links.next = temp_nex;
    temp_nex->body.links.prev = temp_pre;

    block->body.links.next = NULL;
    block->body.links.prev = NULL;
}


sf_block *find_fit(size_t size) {

    int index = get_free_list_index(size);
    sf_block *dummy = &sf_free_list_heads[index];
    sf_block *current_block = dummy->body.links.next;
    //tralversal the list
    while (current_block != dummy) {
            size_t current_block_size = (current_block->header & block_size_MASK);
            if (current_block_size >= size) {
                remove_block(current_block);

                /*//modified prev allocated
                size_t block_size = block->header & block_size_MASK;
                sf_block* next_block = (sf_block*)((char*)block + block_size);
                size_t next_block_size = next_block->header & block_size_MASK;
                sf_block* next_next_block = (sf_block*)((char*)next_block + next_block_size);
                next_block->header &= ~PREV_ALLOC_MASK;
                next_next_block->prev_footer &= ~PREV_ALLOC_MASK;
                //////*/
                return current_block;
            }
            current_block = current_block->body.links.next;
    }

    sf_block *dummy2 = &sf_free_list_heads[NUM_FREE_LISTS-1];
    sf_block* initial_free_block = dummy2->body.links.next;
    size_t free_block_size = (initial_free_block->header& block_size_MASK);

    if(free_block_size-size >= 32){

        //reduce the size of initial free block
        remove_block(initial_free_block);
        size_t new_initial_free_block_size = free_block_size-size;
        sf_header new_initial_free_block_header = (initial_free_block->header & ~block_size_MASK)|new_initial_free_block_size|PREV_ALLOC_MASK;
        sf_block* new_initial_free_block = (sf_block*)((char*)initial_free_block+size);

        //the block need to reutrn; prev footer don't need to change.
        sf_block* current_block2 = initial_free_block;
//sf_show_heap();
        current_block2->header= (current_block2->header & ~block_size_MASK) | size | ALLOC_MASK;
        
        //keep doing the initial free block
        new_initial_free_block->header = new_initial_free_block_header;

        new_initial_free_block->prev_footer = current_block2->header;

        sf_block* epilogue = (sf_block*)((char*)sf_mem_end() - 16);
        epilogue->prev_footer = new_initial_free_block->header;

        //insert_block(new initial_free_block);
        sf_block *dummy = &sf_free_list_heads[NUM_FREE_LISTS-1];
        dummy->body.links.prev = new_initial_free_block;
        new_initial_free_block->body.links.next = dummy;
        new_initial_free_block->body.links.prev = dummy;
        dummy->body.links.next = new_initial_free_block;
        return current_block2;
    }
    return NULL;
}

//extend initial free block after mem_grow
sf_block *create_block(void *start, size_t size) {
    sf_block *block = (sf_block *) ((char*)start -16);
    sf_header epilogue_header = block->header;
    //new free block's prev foot is already ok in initial heap fucntion, setteing the header now
    block->header = block->header & ~ALLOC_MASK;
    block->header = block->header | size;
    // Create the epilogue block.
    sf_block *epilogue = (sf_block*)((char*)sf_mem_end()-16);
    epilogue->prev_footer = block->header;
    epilogue->header = epilogue_header;
    if (( epilogue->prev_footer) & ALLOC_MASK ) {
        epilogue->header = epilogue ->header | PREV_ALLOC_MASK;
    }else{
        epilogue->header = epilogue ->header & ~PREV_ALLOC_MASK;
    }

    insert_block(block);
    return block;
}
//block just come from free list; the real size we need
void split(sf_block *block, size_t desired_size) {
    size_t block_size = (block->header& block_size_MASK);
    if ((block_size - desired_size) >= 32) {
        //allocated smaller block
        block->header = (block->header & ~block_size_MASK) | ALLOC_MASK | desired_size;

        //bring new block to the free list
        sf_block *new_block = (sf_block *)((char *)block + desired_size);
        size_t new_size = block_size - desired_size;
        new_block->prev_footer = block->header;
        new_block->header = (new_block->header & 0)  | new_size | PREV_ALLOC_MASK;

        size_t new_block_size = new_block->header & block_size_MASK;
        sf_block* next_block = (sf_block*)((char*)new_block + new_block_size);
        next_block->prev_footer = new_block->header;
        insert_block(new_block);
    }
}

//block just come from free list; block size are the atually size we need
void place(sf_block *block, size_t block_size) {
    split(block, block_size);
    //find epilogue and initial free block
    sf_block *epilogue = (sf_block*)((char*)sf_mem_end()-16);
    size_t initial_block_size = epilogue->prev_footer & block_size_MASK;
    sf_block *initial_free_block = (sf_block*) ((char*)sf_mem_end() - 16 - initial_block_size);

    //insert block beween free block and the one before free block
    block->prev_footer = initial_free_block->prev_footer;
    size_t payload = (block_size - 16)<<32;
    block->header = (block->header & ~payload_size_MASK) | payload | ALLOC_MASK;
    initial_free_block->prev_footer = block->header;
}

//coalesce free block
sf_block *coalesce(sf_block *block) {
    size_t size = (block->header& block_size_MASK);
    sf_block* next_block = (sf_block *)((char *)block + size);
    //sf_block* epilogue = (sf_block*)((char*)sf_mem_end() - 16);
    //size_t free_block_size = epilogue->prev_footer & block_size_MASK;
    // prev alloc
    bool prev_alloc = block->header & PREV_ALLOC_MASK;
    bool next_alloc = next_block->header & ALLOC_MASK;

    size_t prev_size = (block->prev_footer & block_size_MASK);
    sf_block* prev_block = (sf_block *)((char *)block - prev_size);

    //coalescing
    if (prev_alloc && next_alloc) {
        //sf_show_heap();
        // Case 1: Both allocated
        return block;
    } else if (prev_alloc && !next_alloc) {
        // Case 2: Previous allocated, next free
        //Remove from the free list, then merge, then add to free list
        //sf_show_heap();
        remove_block(next_block);
        remove_block(block);
        size = size + (next_block->header& block_size_MASK);
        block->header = (block->header & ~block_size_MASK) | size;

        ((sf_block*)((char*)block + size))->prev_footer = block->header;

        insert_block(block);
        return block;

    } else if (!prev_alloc && next_alloc) {
        // Case 3: Previous free, next allocated
        remove_block(prev_block);
        remove_block(block);
        size += prev_size;
        prev_block->header =  (prev_block->header & ~block_size_MASK) |size;
        ////
        ((sf_block*)((char*)prev_block + size))->prev_footer = prev_block->header;
        /////
        insert_block(prev_block);
        return prev_block;
    } else {
        //sf_show_heap();
        // Case 4: Both free
        //Remove both blocks from the free list, then merge them with the current block
        size = size + prev_size + (next_block->header& block_size_MASK);
        remove_block(prev_block);
        remove_block(next_block);
        remove_block(block);
        prev_block->header = (prev_block->header & ~block_size_MASK) |size;
        ((sf_block*)((char*)prev_block + size))->prev_footer = prev_block->header;

        insert_block(prev_block);
        return prev_block;
    }
}

void initialize_heap(void *new_page) {
    sf_block *prologue = (sf_block *)new_page;
    prologue->prev_footer = 0;
    prologue->header = PACK(0, ALLOC_MASK);
    prologue->header |= 32;
    sf_block *initial_free_block = (sf_block*)((char*)prologue + 32);
    initial_free_block->prev_footer = prologue->header;
    //prologue + footer + epilogue
    size_t initial_block_size = PAGE_SZ - (32 + 8 + 8);
    initial_free_block->header =  (initial_free_block->header & 0) | initial_block_size | PREV_ALLOC_MASK;

    // Create the epilogue block.
    sf_block *epilogue = (sf_block*)((char*)sf_mem_end()-16);
    epilogue->prev_footer = initial_free_block->header;
    epilogue->header = PACK(0, ALLOC_MASK);
    if (( epilogue->prev_footer) & ALLOC_MASK ) {
        epilogue->header = epilogue ->header | PREV_ALLOC_MASK;
    }else{
        epilogue->header = epilogue ->header & ~PREV_ALLOC_MASK;
    }

    //insert_block(initial_free_block);
    sf_block *dummy = &sf_free_list_heads[NUM_FREE_LISTS-1];
    dummy->body.links.prev = initial_free_block;
    initial_free_block->body.links.next = dummy;
    initial_free_block->body.links.prev = dummy;
    dummy->body.links.next = initial_free_block;


}
void *sf_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    if (!is_initialized) {
        sf_init();
        // The first call should initialize the heap with a page of memory.
        void *new_page = sf_mem_grow();
        if (!new_page) {
            sf_errno = ENOMEM;
            return NULL;
        }
        initialize_heap(new_page);
    }

    size_t block_size = size + 2*8;
    size_t padding = 0;
    if (block_size % 16 != 0) {
        padding = 16 - (block_size % 16);
    }
    block_size += padding;
    if (block_size < 32) {
        block_size = 32;
    }
    while (size > ((char*)sf_mem_end()- (char*)sf_mem_start() -32-32-16)) {
        void *new_page = sf_mem_grow();
        if (new_page == NULL) {
            sf_errno = ENOMEM;
            return NULL;
        }
        sf_block* new = create_block(new_page, PAGE_SZ);
        coalesce(new);
    }

    sf_block *block = find_fit(block_size);

    if (block != NULL) {
        place(block, block_size);

        ///
        size_t payload = (block->header & payload_size_MASK)>>32;
        max_payload  += payload;
        ///

        return block->body.payload;
    }
    sf_errno = ENOMEM;
    return NULL;
}

void sf_free(void *pp) {
    sf_block *block = (sf_block *)((char *)pp - 16);
    size_t block_size = (block->header & block_size_MASK);
    if (pp == NULL) {
        //printf("1\n");
        abort();
    }
    if (block_size < 32) {
        abort();
    }
    if (block_size % 16 != 0) {
        abort();
    }
    if (((char*)block < ((char*)sf_mem_start()+32)) ||
        (char*)block  > ((char*)sf_mem_end()-16-block_size)) {
        sf_errno = EINVAL;
        abort();
    }
    if ((block->header & ALLOC_MASK) != ALLOC_MASK) {
        abort();
    }
    if ((block->header & PREV_ALLOC_MASK) !=  PREV_ALLOC_MASK) {
        sf_block *prev_block = (sf_block *)((char *)block - (block->prev_footer & block_size_MASK));
        if ((prev_block->header & ALLOC_MASK) == ALLOC_MASK) {
            abort();
        }
    }
    //clear everything except the size and pre_allocated
    size_t pre_alloc = block->header & PREV_ALLOC_MASK;
    block->header = (block->header & block_size_MASK) | pre_alloc;
    sf_block* next_block = (sf_block*)((char*)block + block_size);
    next_block->prev_footer = block->header;

    block = coalesce(block);
    insert_block(block);

}
void *sf_realloc(void *pp, size_t rsize) {

    sf_block *block = (sf_block *)((char *)pp - 16);
    size_t block_size = block->header & block_size_MASK;
    if (!is_initialized) {
        sf_init();
    }
    if (pp == NULL) {
        abort();
    }
    if (rsize % 16 != 0) {
        abort();
    }
    if (((char*)block < ((char*)sf_mem_start()+32)) ||
        (char*)block  > ((char*)sf_mem_end()-16-block_size)) {
        sf_errno = EINVAL;
        return NULL;
    }
    if ((block->header & ALLOC_MASK) != ALLOC_MASK) {
        abort();
    }
    if ((block->header & PREV_ALLOC_MASK) !=  PREV_ALLOC_MASK) {
        sf_block *prev_block = (sf_block *)((char *)block - (block->prev_footer & block_size_MASK));
        if ((prev_block->header & ALLOC_MASK) == ALLOC_MASK) {
           abort();
        }
    }
    if (rsize < 32) {
        if (rsize == 0) {
        sf_free(pp);
        return NULL;
        }
        abort();
    }

    size_t new_size = rsize+16;
    if (new_size == block_size) {
        return pp;
    }

    if (new_size > block_size) {
        sf_block *new_block = sf_malloc(rsize);
        if (new_block == NULL) {
            return NULL;
        }
        memcpy(new_block, pp, block_size);
        sf_free(pp);
        ///
        size_t payload = (new_block->header & payload_size_MASK)>>32;
        max_payload += payload;
        ///
        return new_block;
    }

    if ((block_size - new_size) < 32) {
        return pp;
    } else {
        split(block, new_size);
        sf_block *new_block = (sf_block *)((char *)block + new_size);
        coalesce(new_block);
        return pp;
    }
}


double sf_fragmentation() {
    size_t total_payload = 0;
    size_t total_allocated_block_size = 0;
    sf_block *current_block = (sf_block *)(char*)(sf_mem_start() + 32);
    while (current_block < ((sf_block *)(char*)sf_mem_end()-16)) {
        size_t current_block_size = (current_block->header& block_size_MASK);
        size_t current_block_payload_size = ((current_block->header& payload_size_MASK)>>32) ;
        if (current_block->header & ALLOC_MASK) {
            total_allocated_block_size += current_block_size;
            total_payload += current_block_payload_size;
        }
        current_block = (sf_block *)((char *)current_block + current_block_size);
    }
    if (total_allocated_block_size == 0) {
        return 0.0;
    }
    return (double)((total_payload) / total_allocated_block_size);
}

double sf_utilization() {
    size_t current_heap_size = (char *)sf_mem_end() - (char *)sf_mem_start();
    if (current_heap_size == 0) {
        return 0.0;
    }
    return (double)max_payload / current_heap_size;
}
