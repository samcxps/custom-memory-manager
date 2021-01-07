// first pointer returned is 8-byte aligned
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "myHeap.h"

int main() {
   assert(myInit(4096) == 0);
   int* ptr = (int*) myAlloc(sizeof(int));
   assert(ptr != NULL);
   
   dispMem();
   exit(0);
}
