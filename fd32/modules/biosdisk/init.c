/**************************************************************************
 * FreeDOS 32 BIOSDisk Driver                                             *
 * Disk drive support via BIOS                                            *
 * by Salvo Isaja                                                         *
 *                                                                        *
 * Copyright (C) 2001-2003, Salvatore Isaja                               *
 *                                                                        *
 * This is "init.c" - Initialization code for the FreeDOS32 kernel        *
 *                    by Luca Abeni                                       *
 *                                                                        *
 *                                                                        *
 * This file is part of the FreeDOS32 BIOSDisk Driver.                    *
 *                                                                        *
 * The FreeDOS32 BIOSDisk Driver is free software; you can redistribute   *
 * it and/or modify it under the terms of the GNU General Public License  *
 * as published by the Free Software Foundation; either version 2 of the  *
 * License, or (at your option) any later version.                        *
 *                                                                        *
 * The FreeDOS32 BIOSDisk Driver is distributed in the hope that it will  *
 * be useful, but WITHOUT ANY WARRANTY; without even the implied warranty *
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the       *
 * GNU General Public License for more details.                           *
 *                                                                        *
 * You should have received a copy of the GNU General Public License      *
 * along with the FreeDOS32 BIOSDisk Driver; see the file COPYING;        *
 * if not, write to the Free Software Foundation, Inc.,                   *
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA                *
 **************************************************************************/

#include <ll/i386/hw-func.h>
#include <ll/i386/pic.h>
#include <ll/i386/x-bios.h>
#include <ll/i386/error.h>
#include <../drivers/dpmi/include/dpmi.h>
#include "biosdisk.h"

extern void biosdisk_reflect(unsigned intnum, union regs r);

void biosdisk_init(void)
{
    message("Initing BIOSDisk...\n");
    /* TODO: Add command line switches for floppy and hard disk operation */

    vm86_init();

//    if (want_fd)
    {
        /* In some bioses, INT 0x13 calls INT 0x40 */
        l1_int_bind(0x40, biosdisk_reflect);
        l1_irq_bind(6, biosdisk_reflect);
        irq_unmask(6);
//        biosdisk_detect_fd();
    }
//  if (want_hd)
    {
        l1_irq_bind(15, biosdisk_reflect);
        l1_irq_bind(14, biosdisk_reflect);
        irq_unmask(14);
        irq_unmask(15);
//        biosdisk_detect_hd();
    }
    /* Are these needed for both floppies and hard disks? */
    l1_int_bind(0x15, biosdisk_reflect);
//    l1_int_bind(0x1C, biosdisk_reflect);
//    l1_irq_bind(0, biosdisk_reflect);
//    irq_unmask(0);

    biosdisk_detect();
    message("BIOSDisk initialized.\n");
}
