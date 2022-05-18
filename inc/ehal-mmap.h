// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText:  2022 Patrick Siegl <code@siegl.it>

#ifndef __EHAL_MMAP__H
#define __EHAL_MMAP__H

#include "memmap-epiphany-cores.h"
#include "state/ehal-state.h"

int eCoreMmap(int fd, eCoreMemMap_t* eCoreBgn, eCoreMemMap_t* eCoreEnd);
int eCoreMunmap(eCoreMemMap_t* eCoreBgn, eCoreMemMap_t* eCoreEnd);

int eShmMmap(int fd, typeof(&((eConfig_t*)0x0)->emem[0]) emem);
int eShmMunmap(typeof(&((eConfig_t*)0x0)->emem[0]) emem);

int eSysRegsMmap(int fd, eSysRegs* esys_regs_base);
int eSysRegsMunmap(eSysRegs* esys_regs_base);

#endif /* __EHAL_MMAP__H */
