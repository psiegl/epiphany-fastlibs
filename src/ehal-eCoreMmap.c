// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText:  2022 Patrick Siegl <code@siegl.it>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include "ehal-eCoreMmap.h"
#include "ehal-print.h"

int _eCoreMmap(int fd, eCoreMemMap_t* eCoreCur, eCoreMemMap_t* eCoreBgn, eCoreMemMap_t* eCoreEnd)
{
  assert( fd >= 0 );
  assert( eCoreCur );
  assert( eCoreBgn );
  assert( eCoreEnd );

  if(ECORE_ADDR_COLID(eCoreCur) > ECORE_ADDR_COLID(eCoreEnd)
     || ECORE_ADDR_ROWID(eCoreCur) > ECORE_ADDR_ROWID(eCoreEnd))
    return 0;

  int flags = MAP_LOCKED;
#ifdef MAP_FIXED_NOREPLACE
  flags |= MAP_FIXED_NOREPLACE;
#else
  flags |= MAP_FIXED;
#endif
#ifdef MAP_SHARED_VALIDATE
  flags |= MAP_SHARED_VALIDATE;
#else
  flags |= MAP_SHARED;
#endif    
  void *ebank = mmap((void*)eCoreCur->bank, sizeof(eCoreCur->bank), PROT_READ|PROT_WRITE, flags, fd, (off_t)eCoreCur->bank);
  assert(ebank == eCoreCur->bank);
  if(ebank != MAP_FAILED) {
    eCorePrintf(E_DBG, eCoreCur, "VA %p, PA %p (%s) - eCore bank\n", eCoreCur->bank, eCoreCur->bank, fmtBytes(sizeof(eCoreCur->bank)) );

    void *eregs = mmap(&eCoreCur->regs, sizeof(eCoreCur->regs), PROT_READ|PROT_WRITE, flags, fd, (off_t)&eCoreCur->regs);
    assert(eregs == &eCoreCur->regs);
    if(eregs != MAP_FAILED) {
      eCorePrintf(E_DBG, eCoreCur, "VA %p, PA %p (%s) - eCore regs\n", &eCoreCur->regs, &eCoreCur->regs, fmtBytes(sizeof(eCoreCur->regs)) );

      int isColEnd = ECORE_ADDR_COLID(eCoreCur) < ECORE_ADDR_COLID(eCoreEnd);
      if(!_eCoreMmap(fd, isColEnd ? eCoreCur + 1
                                  : eCoreBgn + ECORES_MAX_DIM,
                         isColEnd ? eCoreBgn
                                  : eCoreBgn + ECORES_MAX_DIM,
                         eCoreEnd))
        return 0;

      munmap(eregs, sizeof(eCoreCur->regs));
    }
    else
      eCoreError(eCoreCur, "mmap for %p regs %p failed (errno %d, %s)! Cleaning up...\n", eCoreCur, &eCoreCur->regs, errno, strerror(errno));

    munmap(ebank, sizeof(eCoreCur->bank));
  }
  else
    eCoreError(eCoreCur, "mmap for %p bank %p failed (errno %d, %s)! Cleaning up...\n", eCoreCur, eCoreCur->bank, errno, strerror(errno));

  return -1;
}

int eCoreMmap(int fd, eCoreMemMap_t* eCoreBgn, eCoreMemMap_t* eCoreEnd)
{
  assert( eCoreBgn );
  assert( eCoreEnd );

  eCoresPrintf(E_DBG, "Setting up EPIPHANY eCores mmap (%p-%p)\n", eCoreBgn, eCoreEnd);
  return _eCoreMmap(fd, eCoreBgn, eCoreBgn, eCoreEnd);
}

int _eCoreMunmap(eCoreMemMap_t* eCoreCur, eCoreMemMap_t* eCoreBgn, eCoreMemMap_t* eCoreEnd)
{
  assert( eCoreCur );
  assert( eCoreBgn );
  assert( eCoreEnd );

  if(ECORE_ADDR_COLID(eCoreCur) > ECORE_ADDR_COLID(eCoreEnd)
     || ECORE_ADDR_ROWID(eCoreCur) > ECORE_ADDR_ROWID(eCoreEnd))
    return 0;

  munmap(&eCoreCur->regs, sizeof(eCoreCur->regs));
  munmap((void*)eCoreCur->bank, sizeof(eCoreCur->bank));

  int isColEnd = ECORE_ADDR_COLID(eCoreCur) < ECORE_ADDR_COLID(eCoreEnd);
  return _eCoreMunmap(isColEnd ? eCoreCur + 1 :
                               eCoreBgn + ECORES_MAX_DIM,
                    isColEnd ? eCoreBgn :
                               eCoreBgn + ECORES_MAX_DIM,
                    eCoreEnd);
}

int eCoreMunmap(eCoreMemMap_t* eCoreBgn, eCoreMemMap_t* eCoreEnd)
{
  return _eCoreMunmap(eCoreBgn, eCoreBgn, eCoreEnd);
}


