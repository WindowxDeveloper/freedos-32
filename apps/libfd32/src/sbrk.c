#include <unistd.h>

#include <ll/i386/hw-data.h>
#include <ll/i386/error.h>
#include <kmem.h>
#include <stubinfo.h>

void *sbrk(int incr)
{
  char *prev_heap_end = NULL;
  int res;
  extern DWORD mem_limit;

  if (mem_limit != 0) {
    res = mem_get_region(mem_limit, incr);
    if (res == -1) {
      message("Ermmm... SBRK problem: cannot memget(%lx %x)\n", mem_limit, incr);
      mem_dump();
      return 0;
    }
    prev_heap_end = (char *)mem_limit;
    mem_limit += incr;
  } else {
    message("sbrk error: Memory Limit == 0!\n");
    fd32_abort();
  }
  
  return prev_heap_end;
}