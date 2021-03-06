/* FD32 Abort code
 * by Luca Abeni
 * 
 * This is free software; see GPL.txt
 */

#include <ll/i386/hw-instr.h>
#include <ll/i386/hw-func.h>
#include <ll/i386/stdlib.h>

#include "kernel.h"

void fd32_abort(void)
{
  l1_end();
  sti();
  l1_exit(1);
}

void fd32_reboot(void)
{
  cold_reboot();
}
