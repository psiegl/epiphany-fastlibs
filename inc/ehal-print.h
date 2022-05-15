// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText:  2022 Patrick Siegl <code@siegl.it>

#ifndef __EHAL_PRINT__H
#define __EHAL_PRINT__H

#include "memmap-epiphany-cores.h"

#define E_ERR 0x1
#define E_WRN 0x2
#define E_DBG 0x4

#ifdef __DEFINE_ELOGLVL
unsigned eloglevel = 0;
#else
extern unsigned eloglevel;
#endif /* __DEFINE_ELOGLVL */


#define eCoresPrintf( dbg, format, ... ) \
({ \
  if(__builtin_expect(eloglevel >= dbg, 0)) { \
    fprintf (stdout, "[xx,xx] " format, ##__VA_ARGS__); \
  } \
})
#define eCoresWarn( format, ... ) \
({ \
  if(__builtin_expect(eloglevel >= E_WRN, 0)) { \
    fprintf (stderr, "[xx,xx] WRN: " format, ##__VA_ARGS__); \
  } \
})
#define eCoresError( format, ... ) \
({ \
  fprintf (stderr, "[xx,xx] ERR: " format, ##__VA_ARGS__); \
})
#define eCorePrintf( dbg, eCore, format, ... ) \
({ \
  if(__builtin_expect(eloglevel >= dbg, 0)) { \
    fprintf (stdout, "[%2d,%2d] " format, ECORE_ADDR_ROWID(eCore), ECORE_ADDR_COLID(eCore), ##__VA_ARGS__); \
  } \
})
#define eCoreError( eCore, format, ... ) \
({ \
  fprintf (stderr, "[%2d,%2d] ERR: " format, ECORE_ADDR_ROWID(eCore), ECORE_ADDR_COLID(eCore), ##__VA_ARGS__); \
})

#endif /* __EHAL_PRINT__H */
