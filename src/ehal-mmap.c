// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText:  2022 Patrick Siegl <code@siegl.it>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <asm-generic/mman.h> /* MAP_LOCKED */
#include "ehal-mmap.h"
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
  void *ebank = mmap(eCoreCur->bank, sizeof(eCoreCur->bank), PROT_READ|PROT_WRITE, flags, fd, (off_t)eCoreCur->bank);
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


int eShmMmap(int fd, __typeof__(&((eConfig_t*)0x0)->emem[0]) emem)
{
  assert( fd >= 0 );
  assert( emem );

  // Can not use MAP_FIXED_NOREPLACE as it would 'normally' overlap with eCore mmap
  int flags = MAP_FIXED | MAP_LOCKED;
#ifdef MAP_SHARED_VALIDATE
  flags |= MAP_SHARED_VALIDATE;
#else
  flags |= MAP_SHARED;
#endif

// https://www.kernel.org/doc/html/latest/admin-guide/mm/hugetlbpage.html
#if defined(MAP_HUGETLB) && defined(MAP_HUGE_2MB) // ToDo: check if working
  if(emem->size >= 0x200000 /* 2MB */
     && !(emem->size % 0x200000)) {
    flags |= MAP_HUGETLB | MAP_HUGE_2MB;
    printf("will try hugetlb!\n");
  }
#endif
  void* eshm = mmap(emem->epi_base, emem->size, emem->prot, flags, fd, emem->base_address);
  assert(eshm == emem->epi_base);
  if(eshm == MAP_FAILED) {
    eCoresError("failed on mmap eshm %p (errno %d, %s)! Cleaning up...\n", emem->epi_base, errno, strerror(errno));
    return -1;
  }
  
// In case MAP_HUGETLB is not defined, let us try the 2nd option:
// Quote:      "It is mostly intended for
//              embedded systems, where MADV_HUGEPAGE-style behavior may
//              not be enabled by default in the kernel."
// https://man7.org/linux/man-pages/man2/madvise.2.html
// Do not care about return value, as it is just hint to Linux
#if ! (defined(MAP_HUGETLB) && defined(MAP_HUGE_2MB)) && defined(MADV_HUGEPAGE)
  int madvHugepage = madvise(eshm, emem->size, MADV_HUGEPAGE);
  eCoresPrintf(E_DBG, "Zynq <-> eCores shm: madvise(MADV_HUGEPAGE): %d%s%s%s\n",
               madvHugepage,
               madvHugepage ? " (" : "",
               madvHugepage ? strerror(errno) : "",
               madvHugepage ? ")" : "" );
#endif
  
  eCorePrintf(E_DBG, eshm, "VA %p, PA %p (%s) - Zynq <-> eCores shm\n", eshm, (void*)emem->base_address, fmtBytes(emem->size) );

  return 0;
}

int eShmMunmap(__typeof__(&((eConfig_t*)0x0)->emem[0]) emem)
{
  assert( emem );

  return munmap(emem->epi_base, emem->size);
}


int eSysRegsMmap(int fd, eSysRegs* esys_regs_base)
{
  assert( fd >= 0 );
  assert( esys_regs_base );

  // Can not use MAP_FIXED_NOREPLACE as it would 'normally' overlap with eCore mmap
  int flags = MAP_FIXED | MAP_LOCKED;
#ifdef MAP_SHARED_VALIDATE
  flags |= MAP_SHARED_VALIDATE;
#else
  flags |= MAP_SHARED;
#endif
  // The EPIPHANY FPGA regs visible by the Zynq, are mapped onto the [32, 8] eCore regs memory mapped page
  eSysRegs* esysregs = (eSysRegs*) mmap(esys_regs_base, sizeof(eSysRegs), PROT_READ|PROT_WRITE, flags, fd, (off_t)esys_regs_base );
  assert(esysregs == esys_regs_base);
  if(esysregs != MAP_FAILED) {
    eCoresPrintf(E_DBG, "VA %p, PA %p (%s) - eCores FPGA regs\n",
                 esys_regs_base, esys_regs_base, fmtBytes(sizeof(eSysRegs)) );
    return 0;
  }
  else {
    eCoresError("failed on mmap esysregs %p (errno %d, %s)! Cleaning up...\n", esysregs, errno, strerror(errno));
    return -1;
  }
}

int eSysRegsMunmap(eSysRegs* esys_regs_base)
{
  assert( esys_regs_base );

  return munmap(esys_regs_base, sizeof(*esys_regs_base));
}

