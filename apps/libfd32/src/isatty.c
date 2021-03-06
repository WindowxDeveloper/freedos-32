#include <unistd.h>

#include <ll/i386/hw-data.h>
#include <filesys.h>

int isatty(int fd)
{
  int res;

  res = fd32_get_dev_info(fd);
  if ((res & 0x83) == 0x83) {
    return 1;
  }

  return 0;
}
