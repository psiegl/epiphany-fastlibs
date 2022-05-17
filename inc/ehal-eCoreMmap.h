// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText:  2022 Patrick Siegl <code@siegl.it>

#ifndef __EHAL_ECOREMMAP__H
#define __EHAL_ECOREMMAP__H

#include "memmap-epiphany-cores.h"

int eCoreMmap(int fd, eCoreMemMap_t* eCoreBgn, eCoreMemMap_t* eCoreEnd);
int eCoreMunmap(eCoreMemMap_t* eCoreBgn, eCoreMemMap_t* eCoreEnd);

#endif /* __EHAL_ECOREMMAP__H */
