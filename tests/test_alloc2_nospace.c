// second allocation is too big to fit
#include <assert.h>
#include <stdlib.h>
#include "myHeap.h"
#include <stdio.h>

int main() {
   assert(myInit(4096) == 0);
   assert(myAlloc(2048) != NULL);
   assert(myAlloc(2047) == NULL);
   dispMem();
   exit(0);
}
