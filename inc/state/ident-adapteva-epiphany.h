// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText:  2022 Patrick Siegl <code@siegl.it>

#ifndef __IDENT_ADAPTEVA_EPIPHANY__H
#define __IDENT_ADAPTEVA_EPIPHANY__H

#include "memmap-epiphany-system.h"
#include "state/ehal-state.h"

eChip_t eChipType(eSysRegs* esysregs);
const char* eChipTypeToStr(eSysRegs* esysregs);
const char* eChipCapsToStr(eSysRegs* esysregs);
unsigned eChipRevision(eSysRegs* esysregs);

#endif /* __IDENT_ADAPTEVA_EPIPHANY__H */
