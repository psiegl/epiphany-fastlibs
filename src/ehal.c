// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText:  2022 Patrick Siegl <code@siegl.it>

#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#define __DEFINE_ELOGLVL
#include "ehal-print.h"
#include "loader/ehal_hdf_loader.h"
#include "memmap-epiphany-system.h"
#include "state/ident-adapteva-epiphany.h"
#include "state/ident-xilinx-zynq.h"


#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))   
#define elemsof( x ) (sizeof(x)/sizeof(x[0]))

#define MEASURE( str, X ) \
({ \
  struct timeval tbgn, tend; \
  gettimeofday(&tbgn, NULL); \
  int ret = X; \
  gettimeofday(&tend, NULL); \
  printf("%s: measured: %ld μs\n", \
         str, \
         ((tend.tv_sec * 1000000 + tend.tv_usec) \
         - (tbgn.tv_sec * 1000000 + tbgn.tv_usec))); \
  ret; \
})


// TODO: create one function that creates "the state"
eConfig_t ecfg = {
  .esys_regs_base      = (eSysRegs*)0x808f0000,

  .num_chips           = 1,
  .chip[0].eCoreRoot   = (eCoresGMemMap)0x80800000,
  .chip[0].xyDim       = 4,
  .chip[0].type        = E16G301,

  .num_ext_mems        = 1,
  .emem[0].base_address   = 0x3e000000,
  .emem[0].epi_base       = (char*)0x8e000000,
  .emem[0].size           = 0x02000000,
  .emem[0].prot           = PROT_READ|PROT_WRITE
};
unsigned epiphanyfd;



static char *fmtBytes(unsigned bytes)
{
  const char unit[] = {' ', 'K', 'M', 'G'}, *b = unit, *e = &unit[elemsof(unit)-1];
  static char o[8];
  if (bytes >> 10)
    for ( ; bytes >> 10 && b < e; ++b, bytes >>= 10);
  sprintf(o, "%4d %cB", bytes, *b);
  return o;
}









int _eCoreMmap(int fd, eCoreMemMap_t* eCoreCur, eCoreMemMap_t* eCoreBgn, eCoreMemMap_t* eCoreEnd)
{
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

/* should employ a pre- and post- */
int _eCoreIter(eCoreMemMap_t* eCoreCur, eCoreMemMap_t* eCoreBgn, eCoreMemMap_t* eCoreEnd,
               int (*fpre)(eCoreMemMap_t* eCoreCur), int (*fpost)(eCoreMemMap_t* eCoreCur, int ret))
{
  assert( eCoreCur );
  assert( eCoreBgn );
  assert( eCoreEnd );

  if(ECORE_ADDR_COLID(eCoreCur) > ECORE_ADDR_COLID(eCoreEnd)
     || ECORE_ADDR_ROWID(eCoreCur) > ECORE_ADDR_ROWID(eCoreEnd))
    return 0;

  int ret = fpre ? (*fpre)(eCoreCur) : 0;
  if( ! ret ) {
    int isColEnd = ECORE_ADDR_COLID(eCoreCur) < ECORE_ADDR_COLID(eCoreEnd);
    ret = _eCoreIter(isColEnd ? eCoreCur + 1 :
                                eCoreBgn + ECORES_MAX_DIM,
                     isColEnd ? eCoreBgn :
                                eCoreBgn + ECORES_MAX_DIM,
                     eCoreEnd, fpre, fpost);
    if( fpost )
      ret = (*fpost)(eCoreCur, ret);
  }
  return ret;
}

// externally visible
int eCoreIter(eCoreMemMap_t* eCoreBgn, eCoreMemMap_t* eCoreEnd,
              int (*fpre)(eCoreMemMap_t* eCoreCur), int (*fpost)(eCoreMemMap_t* eCoreCur, int ret))
{
  return ( !eCoreBgn || !eCoreEnd ) ? -1 : _eCoreIter(eCoreBgn, eCoreBgn, eCoreEnd, fpre, fpost);
}

int eCoreMunmapPre(eCoreMemMap_t* eCoreCur)
{
  assert( eCoreCur );

  munmap(&eCoreCur->regs, sizeof(eCoreCur->regs));
  munmap((void*)eCoreCur->bank, sizeof(eCoreCur->bank));
  return 0;
}

// needs to be run upfront a program run, otherwise the epiphany won't signal anything back
void eLinkUp(typeof(&((eSysRegs*)0x0)->esysconfig.reg) esysconfig,
             eCoresGMemMap eCoreRoot, eChip_t type)
{
  assert( esysconfig );
  assert( eCoreRoot );

	// Perform post-reset, platform specific operations
  if(type == E16G301) { // TODO: assume one chip
//	  if ((e_platform.type == E_ZEDBOARD1601) || (e_platform.type == E_PARALLELLA1601))
    assert((ELINK_REG_MASK( ELINK_REG_EAST ) << 28) == 0x50000000);
    *esysconfig = ELINK_REG_MASK( ELINK_REG_EAST ) << 28;
    asm volatile("" ::: "memory");

// The register must be written, there shall be NO read beforehand. Otherwise stall!
//                      E16G301               E64G301
//  East link offset:   0x08300000 [ 2, 3]    0x08700000 [ 2, 7]
    eCoreMemMap_t* eLinkEast = &eCoreRoot[2][3];
    volatile uint32_t* elinkmodecfgEast = &eLinkEast->regs.chipioregs.elinkmodecfg.reg;
//    assert(elinkmodecfg == (volatile uint32_t*)0x88bf0300);

//  LCLK Transmit Frequency control: Divide cclk by 0->2, 1->4, 2->8
    *elinkmodecfgEast = 1;
    asm volatile("" ::: "memory");
    *esysconfig = 0x0;
  }
}

#if 0
void eCoresReset(void)
{
  esysregs->esysreset = 0x0;
  asm volatile("" ::: "memory");
  usleep(200000);

  eLinkUp(&esysregs->esysconfig.reg); // FIXME
}
#endif


int eOpen(void)
{
  eCoresPrintf(E_DBG, "Opening EPIPHANY.\n" );
  struct { int err; const char* dev; } edev[] = {
    { -1, "/dev/epiphany/mesh0" },
    { -1, "/dev/epiphany" },
    { -1, "/dev/mem" }
  };

  int eCoresFd = -1;
  unsigned i, edevc = elemsof(edev);
  for(i=0; i<edevc; ++i) {
    if((eCoresFd = open(edev[i].dev, O_RDWR|O_SYNC|O_EXCL)) != -1) // TODO: check if file ...
      break;
    edev[i].err = errno;
  }
  if(eCoresFd == -1) {
    eCoresError("Could not open EPIPHANY!, tried:\n");
    for(i=0; i<edevc; ++i)
      eCoresError("    '%s' (%s)\n", edev[i].dev, strerror(edev[i].err));
    eCoresError("Cleaning up...\n");
    return -1;
  }
  return eCoresFd;
}

int eCoresBootstrap(int eCoresFd, eConfig_t *ecfg)
{
  // Idea: We need to read the HDF to get initial values:
  //       - esys_regs_base
  //       - first core id e.g. 0x80800000
  //       With this information, one can read the FPGA reg and compare.
  //       hardware values will take precidice.
  //       TODO: Next step could be to execute code on cores to evaluate next HDF values.
  eConfig_t ecfgHdf;
  if(load_default_hdf(&ecfgHdf) == -1) {
    eCoresError("Failed to obtain HDF file! Aborting...\n");
    return -1;
  }
  // TODO: implement some logic to combine
  if(memcmp(ecfg, &ecfgHdf, sizeof(*ecfg))) {
    eCoresError("Supplied HDF is different to default cfg! Aborting...\n");
    return -1;
  }

  eCoresPrintf(E_DBG, "Setting up Zynq FPGA regs / shm to EPIPHANY eCores\n" );
  // Can not use MAP_FIXED_NOREPLACE as it would 'normally' overlap with eCore mmap
  int flags = MAP_FIXED | MAP_LOCKED;
#ifdef MAP_SHARED_VALIDATE
  flags |= MAP_SHARED_VALIDATE;
#else
  flags |= MAP_SHARED;
#endif
  // The EPIPHANY FPGA regs visible by the Zynq, are mapped onto the [32, 8] eCore regs memory mapped page
  eSysRegs* esysregs = (eSysRegs*) mmap(ecfg->esys_regs_base, sizeof(eSysRegs), PROT_READ|PROT_WRITE, flags, eCoresFd, (off_t)ecfg->esys_regs_base );
  assert(esysregs == ecfg->esys_regs_base);
  if(esysregs != MAP_FAILED) {
    eCoresPrintf(E_DBG, "VA %p, PA %p (%s) - eCores FPGA regs\n", ecfg->esys_regs_base, ecfg->esys_regs_base, fmtBytes(sizeof(eSysRegs)) );
    typeof(&ecfg->chip[0]) chip = &ecfg->chip[0];

    // as the FPGA regs are now visible, we can check what configuration is given.
    // let us rather trust FPGA then HDF file
    eChip_t hw_eChipType = eChipVersion(esysregs);
    if(chip->type != hw_eChipType) {
      eCoresWarn("Hw highlighted different type of EPIPHANY! Will use hw!\n");
      chip->type  = hw_eChipType;
      chip->xyDim = ECHIP_GET_DIM(hw_eChipType);
    }

    eCoreMemMap_t* eCoreBgn = &chip->eCoreRoot[0][0];
    eCoreMemMap_t* eCoreEnd = &chip->eCoreRoot[chip->xyDim-1][chip->xyDim-1];
    eCoresPrintf(E_DBG, "Identified EPIPHANY eCores (%p-%p), xydim: %dx%d\n",
                 eCoreBgn, eCoreEnd, chip->xyDim, chip->xyDim);

    if(!eCoreMmap(eCoresFd, eCoreBgn, eCoreEnd)) {

      // after the mmap'ed regions are up, let us enable the east elink:
      eLinkUp(&esysregs->esysconfig.reg, chip->eCoreRoot, chip->type); // FIXME
      eCoresPrintf(E_DBG, "Enabled EAST eLink between Zynq and EPIPHANY\n");

      typeof(&ecfg->emem[0]) emem = &ecfg->emem[0];
// https://www.kernel.org/doc/html/latest/admin-guide/mm/hugetlbpage.html
#if defined(MAP_HUGETLB) && defined(MAP_HUGE_2MB) // ToDo: check if working
      if(emem->size >= 0x200000 /* 2MB */
         && !(emem->size % 0x200000)) {
        flags |= MAP_HUGETLB | MAP_HUGE_2MB;
        printf("will try hugetlb!\n");
      }
#endif
      void* eshm = mmap(emem->epi_base, emem->size, emem->prot, flags, eCoresFd, emem->base_address);
      assert(eshm == emem->epi_base);
      if(eshm != MAP_FAILED) {
        
        eCorePrintf(E_DBG, eshm, "VA %p, PA %p (%s) - eCores-Zynq shm\n", emem->epi_base, (void*)emem->base_address, fmtBytes(emem->size) );
        return 0;

        //munmap(eshm, emem->size);
      }
      else
        eCoresError("failed on mmap eshm %p (errno %d, %s)! Cleaning up...\n", eshm, errno, strerror(errno));

      eCoreIter(eCoreBgn, eCoreEnd, &eCoreMunmapPre, NULL);
    }
    else
      eCoresError("could not mmap eCores %p - %p\n", eCoreBgn, eCoreEnd);

    munmap(esysregs, sizeof(eSysRegs));
  }
  else
    eCoresError("failed on mmap esysregs %p (errno %d, %s)! Cleaning up...\n", esysregs, errno, strerror(errno));

  close(eCoresFd);

  return -1;
}

void eCoresFini(int fd)
{
  // FIXME all below
  eCoreMemMap_t* eCoreBgn = &ecfg.chip[0].eCoreRoot[0][0];
  eCoreMemMap_t* eCoreEnd = &ecfg.chip[0].eCoreRoot[ecfg.chip[0].xyDim-1][ecfg.chip[0].xyDim-1];

  eCoreIter(eCoreBgn, eCoreEnd, &eCoreMunmapPre, NULL);
  munmap(ecfg.esys_regs_base, sizeof(eSysRegs));
  munmap((void*)ecfg.emem[0].epi_base, ecfg.emem[0].size);
  close(fd);
}



__attribute__((constructor))
static void init(void)
{
  struct timeval tbgn, tend;
  gettimeofday(&tbgn, NULL);

/////////////////////////////////////
// sanity checks (solely build time)
/////////////////////////////////////
#define PAGESIZE 0x1000
// Hint:
// The code assumes that it gets 4K pages.
// 
  assert(sysconf(_SC_PAGESIZE) == PAGESIZE);

  BUILD_BUG_ON(sizeof(eCoreDMA_t)    != 0x20);
  BUILD_BUG_ON(sizeof(eCoreIVT_t)    != 0x28);
  BUILD_BUG_ON(sizeof(eCoreRegs_t)   != PAGESIZE);
  BUILD_BUG_ON(sizeof(eCoreMemMapSW_t) != 0x100000);
  BUILD_BUG_ON(sizeof(eCoreMemMap_t) != 0x100000);
  BUILD_BUG_ON(sizeof(eSysRegs)     != PAGESIZE);

  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->r[0]                            != (void*)0x0000 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->r[63]                           != (void*)0x00FC );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->config                          != (void*)0x0400 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->status                          != (void*)0x0404 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->pc                              != (void*)0x0408 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->debugstatus                     != (void*)0x040C );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->lc                              != (void*)0x0414 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->ls                              != (void*)0x0418 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->le                              != (void*)0x041C );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->iret                            != (void*)0x0420 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->imask                           != (void*)0x0424 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->ilat                            != (void*)0x0428 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->ilatst                          != (void*)0x042C );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->ilatcl                          != (void*)0x0430 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->ipend                           != (void*)0x0434 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->fstatus                         != (void*)0x0440 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->debug                           != (void*)0x0448 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->corereset                       != (void*)0x070C );

  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->ctimer[0]                       != (void*)0x0438 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->ctimer[1]                       != (void*)0x043C );

  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->memstatus                       != (void*)0x0604 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->memprotect                      != (void*)0x0608 );
    
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->dma[0].config                   != (void*)0x0500 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->dma[0].stride                   != (void*)0x0504 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->dma[0].count                    != (void*)0x0508 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->dma[0].srcaddr                  != (void*)0x050C );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->dma[0].dstaddr                  != (void*)0x0510 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->dma[0].autodma[0]               != (void*)0x0514 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->dma[0].autodma[1]               != (void*)0x0518 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->dma[0].status                   != (void*)0x051C );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->dma[1].config                   != (void*)0x0520 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->dma[1].stride                   != (void*)0x0524 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->dma[1].count                    != (void*)0x0528 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->dma[1].srcaddr                  != (void*)0x052C );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->dma[1].dstaddr                  != (void*)0x0530 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->dma[1].autodma[0]               != (void*)0x0534 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->dma[1].autodma[1]               != (void*)0x0538 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->dma[1].status                   != (void*)0x053C );
    
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->meshconfig                      != (void*)0x0700 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->coreid                          != (void*)0x0704 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->multicast                       != (void*)0x0708 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->cmeshroute                      != (void*)0x0710 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->xmeshroute                      != (void*)0x0714 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->rmeshroute                      != (void*)0x0718 );

  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->chipioregs.elinkmodecfg         != (void*)0x0300 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->chipioregs.elinktxcfg           != (void*)0x0304 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->chipioregs.elinkrxcfg           != (void*)0x0308 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->chipioregs.gpiocfg              != (void*)0x030c );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->chipioregs.flagcfg              != (void*)0x0318 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->chipioregs.chipsync             != (void*)0x031c );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->chipioregs.chiphalt             != (void*)0x0320 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->chipioregs.chipreset            != (void*)0x0324 );
  BUILD_BUG_ON( &((eCoreRegs_t*)0x0)->chipioregs.elinkdebug           != (void*)0x0328 );
  
  BUILD_BUG_ON( &((eCoreMemMapSW_t*)0x0)->ivt.sync                      != (void*)0x00000 );
  BUILD_BUG_ON( &((eCoreMemMapSW_t*)0x0)->ivt.softwareexception         != (void*)0x00004 );
  BUILD_BUG_ON( &((eCoreMemMapSW_t*)0x0)->ivt.memoryfault               != (void*)0x00008 );
  BUILD_BUG_ON( &((eCoreMemMapSW_t*)0x0)->ivt.timerinterrupt[0]         != (void*)0x0000C );
  BUILD_BUG_ON( &((eCoreMemMapSW_t*)0x0)->ivt.timerinterrupt[1]         != (void*)0x00010 );
  BUILD_BUG_ON( &((eCoreMemMapSW_t*)0x0)->ivt.message                   != (void*)0x00014 );
  BUILD_BUG_ON( &((eCoreMemMapSW_t*)0x0)->ivt.dmainterrupt[0]           != (void*)0x00018 );
  BUILD_BUG_ON( &((eCoreMemMapSW_t*)0x0)->ivt.dmainterrupt[1]           != (void*)0x0001C );
  BUILD_BUG_ON( &((eCoreMemMapSW_t*)0x0)->ivt.wandinterrupt             != (void*)0x00020 );
  BUILD_BUG_ON( &((eCoreMemMapSW_t*)0x0)->ivt.userinterrupt             != (void*)0x00024 );

  BUILD_BUG_ON( &((eCoreMemMap_t*)0x0)->bank[0]                       != (void*)0x00000 );
  BUILD_BUG_ON( &((eCoreMemMap_t*)0x0)->bank[1]                       != (void*)0x02000 );
  BUILD_BUG_ON( &((eCoreMemMap_t*)0x0)->bank[2]                       != (void*)0x04000 );
  BUILD_BUG_ON( &((eCoreMemMap_t*)0x0)->bank[3]                       != (void*)0x06000 );
  BUILD_BUG_ON( &((eCoreMemMap_t*)0x0)->regs                          != (void*)0xF0000 );

  // test at least one reg within overall mem map.
  BUILD_BUG_ON( &((eCoreMemMap_t*)0x0)->regs.corereset                != (void*)0xF070C );

  BUILD_BUG_ON( &((eCoresGMemMap)0x0)[32][32]                         != (void*)0x82000000 );
  BUILD_BUG_ON( &((eCoresGMemMap)0x0)[32][33]                         != (void*)0x82100000 );
  BUILD_BUG_ON( &((eCoresGMemMap)0x0)[35][35]                         != (void*)0x8E300000 );
  BUILD_BUG_ON( &((eCoresGMemMap)0x0)[39][39]                         != (void*)0x9E700000 );

  BUILD_BUG_ON( &((eSysRegs*)0x808f0000)->esysconfig                 != (void*)0x808f0f00 );
  BUILD_BUG_ON( &((eSysRegs*)0x808f0000)->esysreset                  != (void*)0x808f0f04 );
  BUILD_BUG_ON( &((eSysRegs*)0x808f0000)->esysinfo                   != (void*)0x808f0f08 );
  BUILD_BUG_ON( &((eSysRegs*)0x808f0000)->esysfilterl                != (void*)0x808f0f0c );
  BUILD_BUG_ON( &((eSysRegs*)0x808f0000)->esysfilterh                != (void*)0x808f0f10 );
  BUILD_BUG_ON( &((eSysRegs*)0x808f0000)->esysfilterc                != (void*)0x808f0f14 );
/////////////////////////////////////
/////////////////////////////////////

  char *eloglevels = getenv ("ELOGLEVEL");
  eloglevel = (eloglevels == NULL) ? 0 : atoi(eloglevels);

  if((epiphanyfd = eOpen()) == -1)
    exit(-1);

  if(eCoresBootstrap(epiphanyfd, &ecfg)) {
    eCoresError("Failed to initialise EPIPHANY! Aborting...\n");
    exit(-1);
  }

  gettimeofday(&tend, NULL);

  eCoreMemMap_t* eCoreBgn = &ecfg.chip[0].eCoreRoot[0][0];
  eCoreMemMap_t* eCoreEnd = &ecfg.chip[0].eCoreRoot[ecfg.chip[0].xyDim-1][ecfg.chip[0].xyDim-1];
  typeof(&ecfg.emem[0]) emem = &ecfg.emem[0];
  printf("\n"
         "(c) BSD-2-Clause 2022 Dr.-Ing. Patrick Siegl\n"
         "\n"
         "Xilinx Zynq %s (silicon rev. %0.1f)\n"
         "Adapteva %s (%s, rev. %d)\n"
         "\n"
         "Epiphany cores    [%p:%p] [%2d,%2d] to [%2d,%2d]\n"
         "Zynq Epiphany shm [%p:%p] [%2d,%2d] to [%2d,%2d]\n"
         "\n"
         
         "           (connector)\n"
         "                N\n"
         "     [%2d,%2d]┌─┬─┬─┬─┐EPIPHANY\n"
         "            ├─┼─┼─┼─┤\n"
         "(unwired) W ├─┼─┼─┼─┤ E (wired to Zynq)\n"
         "            ├─┼─┼─┼─┤            [%2d,%2d]┌─┬─┬···┬─┬─┐DRAM\n"
         "            └─┴─┴─┴─┘[%2d,%2d]            └─┴─┴···┴─┴─┘[%2d,%2d]\n"
         "                S\n"
         "           (connector)\n"
         "\n"
         "Init in %ld μs\n"
         "\n",
         xlxZynqDevice(epiphanyfd),
         xlxZynqSiliconRevision(epiphanyfd),
         eChipTypeToStr(ecfg.esys_regs_base), eChipCapsToStr(ecfg.esys_regs_base),
         eChipRevision(ecfg.esys_regs_base),
         eCoreBgn, ((uint8_t*)(eCoreEnd + 1))-1,
         ECORE_ADDR_ROWID(eCoreBgn), ECORE_ADDR_COLID(eCoreBgn),
         ECORE_ADDR_ROWID(eCoreEnd), ECORE_ADDR_COLID(eCoreEnd),
         (void*)emem->epi_base, (void*)(emem->epi_base+emem->size-1),
         ECORE_ADDR_ROWID((void*)emem->epi_base), ECORE_ADDR_COLID((void*)emem->epi_base),
         ECORE_ADDR_ROWID((void*)(emem->epi_base+emem->size-1)), ECORE_ADDR_COLID((void*)(emem->epi_base+emem->size-1)),
 
         ECORE_ADDR_ROWID(eCoreBgn), ECORE_ADDR_COLID(eCoreBgn),
         ECORE_ADDR_ROWID((void*)emem->epi_base), ECORE_ADDR_COLID((void*)emem->epi_base),
         ECORE_ADDR_ROWID(eCoreEnd), ECORE_ADDR_COLID(eCoreEnd),
         ECORE_ADDR_ROWID((void*)(emem->epi_base+emem->size-1)), ECORE_ADDR_COLID((void*)(emem->epi_base+emem->size-1)),
         ((tend.tv_sec * 1000000 + tend.tv_usec) - (tbgn.tv_sec * 1000000 + tbgn.tv_usec)));
  fflush(stdout);
}

__attribute__((destructor))
static void fini(void)
{
  eCoresFini(epiphanyfd);
}

/*
 * low power mode

  epiphany-examples (2016.11)/io/link_lowpower_mode/src/e_link_lowpower_mode.c

  //North-Link
  // ELINK_REG_MASK( ELINK_REG_NORTH ) << 12
  e_reg_write(E_REG_CONFIG, (unsigned) 0x00001000); [15:12] -> CONFIG

  addr=(unsigned *) (0x80AF0308);    // [32,10]
  (*(addr)) = 0x00000FFF;
  addr=(unsigned *) (0x80AF0304);    // [32,10]
  (*(addr)) = 0x00000FFF;

  e_reg_write(E_REG_CONFIG, (unsigned) 0x00400000); // LPMODE=aggressive power down

  /////////////////////////////////////////////////
  //South-Link
  // ELINK_REG_MASK( ELINK_REG_SOUTH ) << 12
  e_reg_write(E_REG_CONFIG, (unsigned) 0x00009000);

  addr=(unsigned *) (0x8CAF0308);   // [35,10]
  (*(addr)) = 0x00000FFF;
  addr=(unsigned *) (0x8CAF0304);   // [35,10]
  (*(addr)) = 0x00000FFF;

  e_reg_write(E_REG_CONFIG, (unsigned) 0x00400000);
  
  /////////////////////////////////////////////////
  //West-Link
  // ELINK_REG_MASK( ELINK_REG_WEST ) << 12
  e_reg_write(E_REG_CONFIG, (unsigned) 0x0000d000);

  addr=(unsigned *) (0x888F0308);   // [34, 8]
  (*(addr)) = 0x00000FFF;
  addr=(unsigned *) (0x888F0304);   // [34, 8]
  (*(addr)) = 0x00000FFF;

  e_reg_write(E_REG_CONFIG, (unsigned) 0x00400000);
*/
/*
 * Performance hints:
 * 
 * //  E16G301 EPIPHANYTM 16-CORE MICROPROCESSOR Datasheet
//  REV 14.03.11, page 19
//  https://www.adapteva.com/docs/e16g301_datasheet.pdf
//
//  E64G301 EPIPHANYTM 64-CORE MICROPROCESSOR Datasheet
//  REV 14.03.11, page 19
//  https://www.adapteva.com/docs/e64g401_datasheet.pdf
 * NOTE: Optimal eLink bandwidth utilization is achieved by
 * transmitting a sequence of 64-bit write transactions with increasing
 * address order (e.g 0x80800000, 0x80800008, 0x80800010, ..). Read
 * transactions, non 64-bit transactions, and non-sequential 64-bit write
 * transactions will result in a max bandwidth of 1/4th of peak.
 * 
 */
