/**************************************************************************
 * FreeDOS 32 FAT Driver                                                  *
 * by Salvo Isaja                                                         *
 *                                                                        *
 * Copyright (C) 2001-2002, Salvatore Isaja                               *
 *                                                                        *
 * This is "fatreq.c" - Driver request function of the FAT driver         *
 *                                                                        *
 *                                                                        *
 * This file is part of the FreeDOS 32 FAT Driver.                        *
 *                                                                        *
 * The FreeDOS 32 FAT Driver is free software; you can redistribute it    *
 * and/or modify it under the terms of the GNU General Public License     *
 * as published by the Free Software Foundation; either version 2 of the  *
 * License, or (at your option) any later version.                        *
 *                                                                        *
 * The FreeDOS 32 FAT Driver is distributed in the hope that it will be   *
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 * GNU General Public License for more details.                           *
 *                                                                        *
 * You should have received a copy of the GNU General Public License      *
 * along with the FreeDOS 32 FAT Driver; see the file COPYING;            *
 * if not, write to the Free Software Foundation, Inc.,                   *
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA                *
 **************************************************************************/

#include "fat.h"

int fat_request(DWORD Function, void *Params)
{
  switch (Function)
  {
    case FD32_READ:
    {
      fd32_read_t *X = (fd32_read_t *) Params;
      if (X->Size < sizeof(fd32_read_t)) return FD32_EFORMAT;
      if (((tFile *) X->DeviceId)->FilSig != FAT_FILSIG) return FD32_EBADF;
      return fat_read((tFile *) X->DeviceId, X->Buffer, X->BufferBytes);
    }
    case FD32_WRITE:
    {
      #ifdef FATWRITE
      fd32_write_t *X = (fd32_write_t *) Params;
      if (X->Size < sizeof(fd32_write_t)) return FD32_EFORMAT;
      if (((tFile *) X->DeviceId)->FilSig != FAT_FILSIG) return FD32_EBADF;
      return fat_write((tFile *) X->DeviceId, X->Buffer, X->BufferBytes);
      #else
      return FD32_EROFS;
      #endif
    }
    case FD32_LSEEK:
    {
      fd32_lseek_t *X = (fd32_lseek_t *) Params;
      if (X->Size < sizeof(fd32_lseek_t)) return FD32_EFORMAT;
      if (((tFile *) X->DeviceId)->FilSig != FAT_FILSIG) return FD32_EBADF;
      return fat_lseek((tFile *) X->DeviceId, &X->Offset, X->Origin);
    }
    case FD32_OPENFILE:
    {
      fd32_openfile_t *X = (fd32_openfile_t *) Params;
      if (X->Size < sizeof(fd32_openfile_t)) return FD32_EFORMAT;
      if (((tVolume *) X->DeviceId)->VolSig != FAT_VOLSIG) return FD32_ENODEV;
      return fat_open((tVolume *) X->DeviceId, X->FileName, X->Mode, X->Attr, X->AliasHint, (tFile **) &X->FileId);
    }
    case FD32_CLOSE:
    {
      fd32_close_t *X = (fd32_close_t *) Params;
      if (X->Size < sizeof(fd32_close_t)) return FD32_EFORMAT;
      if (((tFile *) X->DeviceId)->FilSig != FAT_FILSIG) return FD32_EBADF;
      return fat_close((tFile *) X->DeviceId);
    }
    case FD32_READDIR:
    {
      fd32_readdir_t *X = (fd32_readdir_t *) Params;
      if (X->Size < sizeof(fd32_readdir_t)) return FD32_EFORMAT;
      if (((tFile *) X->DirId)->FilSig != FAT_FILSIG) return FD32_EBADF;
      return fat_readdir((tFile *) X->DirId, (fd32_fs_lfnfind_t *) X->Entry);
    }
    case FD32_FFLUSH:
    {
      #ifdef FATWRITE
      fd32_fflush_t *X = (fd32_fflush_t *) Params;
      if (X->Size < sizeof(fd32_fflush_t)) return FD32_EFORMAT;
      if (((tFile *) X->DeviceId)->FilSig != FAT_FILSIG) return FD32_EBADF;
      return fat_fflush((tFile *) X->DeviceId);
      #else
      return FD32_EROFS;
      #endif
    }
    case FD32_OPEN:
    {
      fd32_open_t *X = (fd32_open_t *) Params;
      if (X->Size < sizeof(fd32_open_t)) return FD32_EFORMAT;
      if (((tFile *) X->DeviceId)->FilSig != FAT_FILSIG) return FD32_EBADF;
      return ++((tFile *) X->DeviceId)->References;
    }
    case FD32_GETATTR:
    {
      fd32_getattr_t *X = (fd32_getattr_t *) Params;
      if (X->Size < sizeof(fd32_getattr_t)) return FD32_EFORMAT;
      if (((tFile *) X->FileId)->FilSig != FAT_FILSIG) return FD32_EBADF;
      return fat_get_attr((tFile *) X->FileId, (fd32_fs_attr_t *) X->Attr);
    }
    case FD32_SETATTR:
    {
      #ifdef FATWRITE
      fd32_setattr_t *X = (fd32_setattr_t *) Params;
      if (X->Size < sizeof(fd32_setattr_t)) return FD32_EFORMAT;
      if (((tFile *) X->FileId)->FilSig != FAT_FILSIG) return FD32_EBADF;
      return fat_set_attr((tFile *) X->FileId, (fd32_fs_attr_t *) X->Attr);
      #else
      return FD32_EROFS;
      #endif
    }
    case FD32_REOPENDIR:
    {
      fd32_reopendir_t *X = (fd32_reopendir_t *) Params;
      if (X->Size < sizeof(fd32_reopendir_t)) return FD32_EFORMAT;
      if (((tVolume *) X->DeviceId)->VolSig != FAT_VOLSIG) return FD32_ENODEV;
      return fat_reopendir((tVolume *) X->DeviceId, (tFindRes *) X->DtaReserved, (tFile **) &X->DirId);
    }
    case FD32_UNLINK:
    {
      #ifdef FATWRITE
      fd32_unlink_t *X = (fd32_unlink_t *) Params;
      if (X->Size < sizeof(fd32_unlink_t)) return FD32_EFORMAT;
      if (((tVolume *) X->DeviceId)->VolSig != FAT_VOLSIG) return FD32_ENODEV;
      return fat_unlink((tVolume *) X->DeviceId, X->FileName, X->Flags);
      #else
      return FD32_EROFS;
      #endif
    }
    case FD32_RENAME:
    {
      #ifdef FATWRITE
      fd32_rename_t *X = (fd32_rename_t *) Params;
      if (X->Size < sizeof(fd32_rename_t)) return FD32_EFORMAT;
      if (((tVolume *) X->DeviceId)->VolSig != FAT_VOLSIG) return FD32_ENODEV;
      return fat_rename((tVolume *) X->DeviceId, X->OldName, X->NewName);
      #else
      return FD32_EROFS;
      #endif
    }
    case FD32_MKDIR:
    {
      #ifdef FATWRITE
      fd32_mkdir_t *X = (fd32_mkdir_t *) Params;
      if (X->Size < sizeof(fd32_mkdir_t)) return FD32_EFORMAT;
      if (((tVolume *) X->DeviceId)->VolSig != FAT_VOLSIG) return FD32_ENODEV;
      return fat_mkdir((tVolume *) X->DeviceId, X->DirName);
      #else
      return FD32_EROFS;
      #endif
    }
    case FD32_RMDIR:
    {
      #ifdef FATWRITE
      fd32_rmdir_t *X = (fd32_rmdir_t *) Params;
      if (X->Size < sizeof(fd32_rmdir_t)) return FD32_EFORMAT;
      if (((tVolume *) X->DeviceId)->VolSig != FAT_VOLSIG) return FD32_ENODEV;
      return fat_rmdir((tVolume *) X->DeviceId, X->DirName);
      #else
      return FD32_EROFS;
      #endif
    }
    case FD32_MOUNT:
    {
      fd32_mount_t *X = (fd32_mount_t *) Params;
      if (X->Size < sizeof(fd32_mount_t)) return FD32_EFORMAT;
      return fat_mount(X->hDev, (tVolume **) &X->FsDev);
    }
    case FD32_UNMOUNT:
    {
      fd32_unmount_t *X = (fd32_unmount_t *) Params;
      if (X->Size < sizeof(fd32_unmount_t)) return FD32_EFORMAT;
      if (((tVolume *) X->DeviceId)->VolSig != FAT_VOLSIG) return FD32_ENODEV;
      return fat_unmount((tVolume *) X->DeviceId);
    }
    case FD32_PARTCHECK:
    {
      fd32_partcheck_t *X = (fd32_partcheck_t *) Params;
      if (X->Size < sizeof(fd32_partcheck_t)) return FD32_EFORMAT;
      return fat_partcheck(X->PartId);
    }
    case FD32_GETFSINFO:
    {
      fd32_getfsinfo_t *X = (fd32_getfsinfo_t *) Params;
      if (X->Size < sizeof(fd32_getfsinfo_t)) return FD32_EFORMAT;
      return fat_get_fsinfo((fd32_fs_info_t *) X->FSInfo);
    }
  }
  return FD32_EINVAL;
}