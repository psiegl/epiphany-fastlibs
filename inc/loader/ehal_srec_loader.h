// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText:  2022 Patrick Siegl <code@siegl.it>

#ifndef __EHAL_SREC_LOADER__PUBLIC_API__H
#define __EHAL_SREC_LOADER__PUBLIC_API__H

#include "memmap-epiphany-cores.h"

int parse_srec(unsigned char *srecBgn, unsigned char *srecEnd,
               eCoreMemMap_t* eCoreBgn, eCoreMemMap_t* eCoreEnd);
int load_srec(const char *srecFile, eCoreMemMap_t* eCoreBgn, eCoreMemMap_t* eCoreEnd);

#endif /* __EHAL_SREC_LOADER__PUBLIC_API__H */
