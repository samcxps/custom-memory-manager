#include <assert.h>
#include <stdlib.h>
#include "myHeap.h"
#include <stdio.h>

int main() {
    assert(myInit(4096)  == 0);

    void *ptr;

    // pointer null
    ptr = NULL;
    assert(myFree(ptr) == -1);

    // ptr not multuple of 8
    ptr = (void*)0x03;
    assert(myFree(ptr) == -1);

    // ptr multiple of 8 but outside heap space
    ptr= (char *)0x08;
    assert(myFree(ptr) == -1);

    // valid pointer but will fail because free not working yet
    int* ptr2 = (int*) myAlloc(sizeof(int));
    *ptr2 = 42;
    // myFree(ptr2);

    dispMem();

    myFree(ptr2);

    dispMem();

    // assert(myFree(ptr) == 1);

    exit(0);
}
