// a few allocations in multiples of 4 bytes followed by frees
#include <assert.h>
#include <stdlib.h>
#include "myHeap.h"

int main() {
    assert(myInit(4096) == 0);

    int* ptr = (int*) myAlloc(sizeof(int));
    // int* ptr2 = (int*) myAlloc(sizeof(int));
    // int* ptr3 = (int*) myAlloc(sizeof(int));

    *ptr  = 50;
    // *ptr2 = 50;
    // *ptr3 = 50;

    dispMem();

    myFree(ptr);

    // dispMem();

    dispMem();

   exit(0);
}
