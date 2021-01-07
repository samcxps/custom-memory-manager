#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include "myHeap.h"

#define MINIMUM_BLOCK_SIZE    8
#define DOUBLE_ALIGNMENT_SIZE 8

#define GET(ptr)       (*(unsigned int *)(ptr))
#define GET_SIZE(ptr)  (GET(ptr) & ~0x7)
#define GET_ALLOC(ptr) (GET(ptr) & 0x1)
#define GET_PREV(ptr)  (GET(ptr) & 0x2)
 
/*
 * This structure serves as the header for each allocated and free block.
 * It also serves as the footer for each free block but only containing size.
 */
typedef struct blockHeader {           
    int size_status;
    /*
    * Size of the block is always a multiple of 8.
    * Size is stored in all block headers and free block footers.
    *
    * Status is stored only in headers using the two least significant bits.
    *   Bit0 => least significant bit, last bit
    *   Bit0 == 0 => free block
    *   Bit0 == 1 => allocated block
    *
    *   Bit1 => second last bit 
    *   Bit1 == 0 => previous block is free
    *   Bit1 == 1 => previous block is allocated
    * 
    * End Mark: 
    *  The end of the available memory is indicated using a size_status of 1.
    * 
    * Examples:
    * 
    * 1. Allocated block of size 24 bytes:
    *    Header:
    *      If the previous block is allocated, size_status should be 27
    *      If the previous block is free, size_status should be 25
    * 
    * 2. Free block of size 24 bytes:
    *    Header:
    *      If the previous block is allocated, size_status should be 26
    *      If the previous block is free, size_status should be 24
    *    Footer:
    *      size_status should be 24
    */
} blockHeader;         

/* 
 * Global variable - DO NOT CHANGE. It should always point to the first block,
 * i.e., the block at the lowest address.
 */
blockHeader *heapStart = NULL;     

/* 
 * Size of heap allocation padded to round to nearest page size.
 */
int allocsize;

/*
 * Additional global variables may be added as needed below
 */
blockHeader *lastAlloc = NULL;     

/*
 * Function for coalescing blocks
 * Argument ptr: address of the block to coalescing
 */
int coalesce(void* ptr) {
    blockHeader *currHeader = NULL;
    currHeader = (blockHeader*)((char*)ptr - 4);    

    blockHeader *nextHeader = NULL;
    blockHeader *prevFooter = NULL;
    blockHeader *prevHeader = NULL;
    
    nextHeader = (blockHeader*)((char*)currHeader + GET_SIZE(currHeader));

    // If prev used, next used
    if (GET_PREV(currHeader) && GET_ALLOC(nextHeader)) {
        // cannot coalesce but still need to change p bit of next block
        nextHeader->size_status -= 2;

        return 0;
    }

    // If prev used, next free
    if (GET_PREV(currHeader) && !GET_ALLOC(nextHeader)) {
        // change size in curr block header
        currHeader->size_status += GET_SIZE(nextHeader);

        // add new footer
        blockHeader *newFooter = (blockHeader*)((char*)currHeader + GET_SIZE(currHeader) - 4);
        newFooter->size_status = currHeader->size_status;

        return 0;
    }

    prevFooter = (blockHeader*)((char*)currHeader - 4);
    prevHeader = (blockHeader*)((char*)prevFooter - GET_SIZE(prevFooter) + 4);

    // If prev free, next used
    if (!GET_PREV(currHeader) && GET_ALLOC(nextHeader)) {
        // change size in prev block header
        prevHeader->size_status += GET_SIZE(currHeader);

        // add new footer
        blockHeader *newFooter = (blockHeader*)((char*)prevHeader + GET_SIZE(prevHeader) - 4);
        newFooter->size_status = prevHeader->size_status;

        blockHeader *newNextHeader = (blockHeader*)((char*)prevHeader + GET_SIZE(prevHeader));
        newNextHeader->size_status -= 2;

        return 0;
    }

    prevHeader->size_status += GET_SIZE(currHeader) + GET_SIZE(nextHeader);

    // add new footer
    blockHeader *newFooter = (blockHeader*)((char*)prevHeader + GET_SIZE(prevHeader) - 4);
    newFooter->size_status = prevHeader->size_status;

    return 0;
}

/* 
 * Function for allocating 'size' bytes of heap memory.
 * Argument size: requested size for the payload
 *
 * Returns address of allocated block on success.
 * Returns NULL on failure.
 *
 * This function should:
 * - Check size - Return NULL if not positive or if larger than heap space.
 * - Determine block size rounding up to a multiple of 8 and possibly adding padding as a result.
 * - Use NEXT-FIT PLACEMENT POLICY to chose a free block
 * - Use SPLITTING to divide the chosen free block into two if it is too large.
 * - Update header(s) and footer as needed.
 * Tips: Be careful with pointer arithmetic and scale factors.
 */
void* myAlloc(int size) {
    // only happens on first call to myAlloc
    if (lastAlloc == NULL) {
        lastAlloc = heapStart;
    }

    /*
    * Check the size is valid (not negative or zero and not larger than heap space)
    */
    if (size <= 0) {
        return NULL;
    }
    if (size > allocsize) {
        return NULL;
    }

    /*
    * Calculate the size of the block we need based on the requests size
    * Minimum block size is 8 (header + 4 byte payload)
    * All blocks must be multiple of 8
    */
    int requested_blocksize = size + sizeof(blockHeader);
    if (requested_blocksize <= MINIMUM_BLOCK_SIZE) {
        requested_blocksize = MINIMUM_BLOCK_SIZE;
    } else {
        // Rounds up to next multiple of 8
        requested_blocksize = ((requested_blocksize + DOUBLE_ALIGNMENT_SIZE - 1) / DOUBLE_ALIGNMENT_SIZE) * DOUBLE_ALIGNMENT_SIZE;                       
    }

    /*
    * Iterate over list and try to find something that works
    */
    blockHeader *current = lastAlloc;

    char *curr_begin = NULL;
    int curr_a_bit = -1;
    int curr_p_bit = -1;
    int curr_size;

    while (1) {
        curr_begin = (char*)current;
        curr_size = current->size_status;

        /*
        * Calculate size of memory and check a/p bits
        */
        if (curr_size & 1) {
            // used block so subtract one from size and curr_a_bit = 1
            curr_size = curr_size - 1;
            curr_a_bit = 1;
        } else {
            // block is free so curr_a_bit = 0
            curr_a_bit = 0;
        }

        if (curr_size & 2) {
            // used previous block so subtract two from size and curr_p_bit = 1
            curr_size = curr_size - 2;
            curr_p_bit = 1;
        } else {
            // previous block is free so curr_p_bit = 0
            curr_p_bit = 0;
        }

        // check if current block is free
        if (curr_a_bit == 0) {
            
            // if block size is perfect fit
            if (curr_size == requested_blocksize) {
                blockHeader *newBlock = (blockHeader*)curr_begin;
                newBlock->size_status = requested_blocksize;
                current->size_status += 1;
                if (curr_p_bit == 1) {
                    current->size_status += 2;
                }

                lastAlloc = (blockHeader*)newBlock;
                return (void*)curr_begin + 4;

            // else if current block size is larger than requested size
            } else if (curr_size > requested_blocksize) {

                // check if splitting makes sense
                // only want to split if the new free block will be at least minimum size allowed
                if ((curr_size - requested_blocksize) >= MINIMUM_BLOCK_SIZE) {

                    // If we can split, set current header to requested block size
                    // set a bit to 1 because we know it is allocated
                    // set p bit to old headers p bit because previous block will not change on splitting
                    current->size_status = requested_blocksize;                  
                    current->size_status += 1;
                    if (curr_p_bit == 1) {
                        current->size_status += 2;
                    }

                    // create new header for new free block
                    // a bit will always be 0 if we are splitting, new block is free
                    // p bit will always be 1 if we are splitting, previous block is used
                    int newBlockSize = curr_size - requested_blocksize;
                    blockHeader *newBlock = (blockHeader*)((char *)curr_begin + requested_blocksize);
                    newBlock->size_status = newBlockSize;
                    newBlock->size_status += 2;

                    // create and set footer (same size_status as header) 
                    blockHeader *footer = (blockHeader*) ((char*)newBlock + newBlockSize - 4);
                    footer->size_status = newBlockSize;
                    footer->size_status += 2;

                    // finally change lastAlloc to new free block
                    lastAlloc = (blockHeader*)newBlock;

                    return (void*)curr_begin + 4;
                } // else cannot split because size will not be >= 8

            } // else the current block is too small for requested size    

        } // else the current block is not free

        // set current to next block since we did not find fit yet
        current = (blockHeader*)((char*)current + curr_size);

        // check if current is last block in heap so set current back to heap start
        if (current->size_status == 1) {
            current = heapStart;
        }

        // if we end up back at lastAlloc, so space is available
        if (current == lastAlloc) {
            return NULL;
        }
    }
    return NULL;
} 
 
/* 
 * Function for freeing up a previously allocated block.
 * Argument ptr: address of the block to be freed up.
 * Returns 0 on success.
 * Returns -1 on failure.
 * This function should:
 * - Return -1 if ptr is NULL.
 * - Return -1 if ptr is not a multiple of 8.
 * - Return -1 if ptr is outside of the heap space.
 * - Return -1 if ptr block is already freed.
 * - USE IMMEDIATE COALESCING if one or both of the adjacent neighbors are free.
 * - Update header(s) and footer as needed.
 */                    
int myFree(void *ptr) {    

    blockHeader *currHeader = NULL;
    currHeader = (blockHeader*)((char*)ptr - 4);

    /*
     * Ensure ptr not null
    */ 
    if (ptr == NULL) {
        return -1;
    }

    /*
     * Ensure ptr is multiple of 8
    */ 
    if (!((((int)ptr) % 8) == 0)) {        
        return -1;
    }

    /*
     * Ensure ptr is inside heap space
    */
    if (!((unsigned int)ptr >= (unsigned int)heapStart && 
          (unsigned int)ptr <= ((unsigned int)heapStart + (unsigned int)allocsize))) {
        return -1;
    }

    /*
     * Check if ptr already freed
     * get header of ptr and check a-bit
     */
    if (GET_ALLOC(currHeader) == 0) {
        return -1;
    }

    /*
     * Actually frees pointer by setting a-bit to 0
     */
    currHeader->size_status = currHeader->size_status - 1;
    // printf("CURR HEADER SIZE: %i\n", GET_SIZE(currHeader));

    /*
     * Sets footer of now free block
     */
    blockHeader *currFooter = NULL;
    currFooter = (blockHeader*)((char*)currHeader + GET_SIZE(currHeader) - 4);
    currFooter->size_status = currHeader->size_status;
    // printf("CURR FOOTER SIZE: %i\n", GET_SIZE(currFooter));

    return coalesce(ptr);
} 
 
/*
 * Function used to initialize the memory allocator.
 * Intended to be called ONLY once by a program.
 * Argument sizeOfRegion: the size of the heap space to be allocated.
 * Returns 0 on success.
 * Returns -1 on failure.
 */                    
int myInit(int sizeOfRegion) {    
 
    static int allocated_once = 0; //prevent multiple myInit calls
 
    int pagesize;  // page size
    int padsize;   // size of padding when heap size not a multiple of page size
    void* mmap_ptr; // pointer to memory mapped area
    int fd;

    blockHeader* endMark;
  
    if (0 != allocated_once) {
        fprintf(stderr, 
        "Error:mem.c: InitHeap has allocated space during a previous call\n");
        return -1;
    }
    if (sizeOfRegion <= 0) {
        fprintf(stderr, "Error:mem.c: Requested block size is not positive\n");
        return -1;
    }

    // Get the pagesize
    pagesize = getpagesize();

    // Calculate padsize as the padding required to round up sizeOfRegion 
    // to a multiple of pagesize
    padsize = sizeOfRegion % pagesize;
    padsize = (pagesize - padsize) % pagesize;

    allocsize = sizeOfRegion + padsize;

    // Using mmap to allocate memory
    fd = open("/dev/zero", O_RDWR);
    if (-1 == fd) {
        fprintf(stderr, "Error:mem.c: Cannot open /dev/zero\n");
        return -1;
    }
    mmap_ptr = mmap(NULL, allocsize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (MAP_FAILED == mmap_ptr) {
        fprintf(stderr, "Error:mem.c: mmap cannot allocate space\n");
        allocated_once = 0;
        return -1;
    }
  
    allocated_once = 1;

    // for double word alignment and end mark
    allocsize -= 8;

    // Initially there is only one big free block in the heap.
    // Skip first 4 bytes for double word alignment requirement.
    heapStart = (blockHeader*) mmap_ptr + 1;

    // Set the end mark
    endMark = (blockHeader*)((void*)heapStart + allocsize);
    endMark->size_status = 1;

    // Set size in header
    heapStart->size_status = allocsize;

    // Set p-bit as allocated in header
    // note a-bit left at 0 for free
    heapStart->size_status += 2;

    // Set the footer
    blockHeader *footer = (blockHeader*) ((void*)heapStart + allocsize - 4);
    footer->size_status = allocsize;
  
    return 0;
} 
                  
/* 
 * Function to be used for DEBUGGING to help you visualize your heap structure.
 * Prints out a list of all the blocks including this information:
 * No.      : serial number of the block 
 * Status   : free/used (allocated)
 * Prev     : status of previous block free/used (allocated)
 * t_Begin  : address of the first byte in the block (where the header starts) 
 * t_End    : address of the last byte in the block 
 * t_Size   : size of the block as stored in the block header
 */                     
void dispMem() {     
 
    int counter;
    char status[5];
    char p_status[5];
    char *t_begin = NULL;
    char *t_end   = NULL;
    int t_size;

    blockHeader *current = heapStart;
    counter = 1;

    int used_size = 0;
    int free_size = 0;
    int is_used   = -1;

    fprintf(stdout, "************************************Block list***\
                    ********************************\n");
    fprintf(stdout, "No.\tStatus\tPrev\tt_Begin\t\tt_End\t\tt_Size\n");
    fprintf(stdout, "-------------------------------------------------\
                    --------------------------------\n");
  
    while (current->size_status != 1) {
        t_begin = (char*)current;
        t_size = current->size_status;
    
        if (t_size & 1) {
            // LSB = 1 => used block
            strcpy(status, "used");
            is_used = 1;
            t_size = t_size - 1;
        } else {
            strcpy(status, "Free");
            is_used = 0;
        }

        if (t_size & 2) {
            strcpy(p_status, "used");
            t_size = t_size - 2;
        } else {
            strcpy(p_status, "Free");
        }

        if (is_used) 
            used_size += t_size;
        else 
            free_size += t_size;

        t_end = t_begin + t_size - 1;
    
        fprintf(stdout, "%d\t%s\t%s\t0x%08lx\t0x%08lx\t%d\n", counter, status, 
        p_status, (unsigned long int)t_begin, (unsigned long int)t_end, t_size);
    
        current = (blockHeader*)((char*)current + t_size);
        counter = counter + 1;
    }

    fprintf(stdout, "---------------------------------------------------\
                    ------------------------------\n");
    fprintf(stdout, "***************************************************\
                    ******************************\n");
    fprintf(stdout, "Total used size = %d\n", used_size);
    fprintf(stdout, "Total free size = %d\n", free_size);
    fprintf(stdout, "Total size = %d\n", used_size + free_size);
    fprintf(stdout, "***************************************************\
                    ******************************\n");
    fflush(stdout);

    return;  
} 

// end of myHeap.c (fall 2020)
