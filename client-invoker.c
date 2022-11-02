#include <stdio.h>
#include "data-sender.h"
// #include "rdma-common.h"

int main() {
  char * mapped_mem = mmap(NULL, 4*4096, PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  start_invocation(mapped_mem);
  while(1) {
    sleep(1);
    mapped_mem[0] = 'z';
  }
}