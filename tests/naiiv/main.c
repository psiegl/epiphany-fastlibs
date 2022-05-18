// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText:  2022 Patrick Siegl <code@siegl.it>

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include "memmap-epiphany-system.h"
#include "memmap-epiphany-cores.h"
#include "loader/ehal-srec-loader.h"

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

// 2016.11 epiphany-hal.c

#ifndef ECORES_NEXT
#define ECORE_NEXT( ROWS, COLS ) ( (ROWS) * ECORES_MAX_DIM + (COLS) )
#endif

#define eCorePrintf( dbg, eCore, format, ... ) \
({ \
  if(dbg <= 1) { \
    fprintf (stdout, "[%2d,%2d]: " format, ECORE_ADDR_ROWID(eCore), ECORE_ADDR_COLID(eCore), ##__VA_ARGS__); \
  } \
})

// Resume a core after halt
void e_resume(eCoreMemMap_t* eCore)
{
  eCore->regs.debug.command = 0;
}

void e_halt(eCoreMemMap_t* eCore)
{
  eCore->regs.debug.command = 1;
}

int ee_soft_reset_dma(eCoreMemMap_t* eCore)
{
  /* pause DMA */
  eCore->regs.config.reg |= 0x01000000; // undocumented! (changes reserved)

  unsigned i, dmac = sizeof(eCore->regs.dma)/sizeof(eCore->regs.dma[0]);
  for(i=0; i<dmac; ++i) {
    eCore->regs.dma[i].config.dmaen = 0; // pause DMA
    eCore->regs.dma[i].config.reg = 0;
    eCore->regs.dma[i].stride = 0;
    eCore->regs.dma[i].count.reg = 0;
    eCore->regs.dma[i].srcaddr = 0;
    eCore->regs.dma[i].dstaddr = 0;
    eCore->regs.dma[i].status.reg = 0;
    eCore->regs.dma[i].config.dmaen = 1; // unpause DMA
  }

  /* unpause DMA */
  eCore->regs.config.reg &= ~0x01000000; // undocumented!

  unsigned c = 2000;
  unsigned dmamask = (0x1 << dmac) - 1;
  do {
    for(i=0; i<dmac; ++i)
      if((0x1 << i) & dmamask
         && !(eCore->regs.dma[i].status.dmastate & 7) 
         && !(dmamask &= ~(0x1 << i)) ) {
          return 0;
      }
    usleep(10);
  } while ( --c );

  for(i=0; i<dmac; ++i)
    if((0x1 << i) & dmamask)
      printf("%s(): (%d, %d) DMA%d NOT IDLE after dma reset", __func__, ECORE_ADDR_ROWID(eCore), ECORE_ADDR_COLID(eCore), i);

  return -1;
}

int ee_reset_regs(eCoreMemMap_t* eCore, int reset_dma)
{
  memset((void*)eCore->regs.r, 0, sizeof(eCore->regs.r));

  if(reset_dma != -1
     && !ee_soft_reset_dma(eCore))
    return -1;

  /* Enable clock gating */
  eCore->regs.config.lpmode = 1;
  eCore->regs.fstatus = 0;
  eCore->regs.pc = 0;
  eCore->regs.lc = 0;
  eCore->regs.ls = 0;
  eCore->regs.le = 0;
  eCore->regs.iret = 0;
  /* Mask all but SYNC irq */
  eCore->regs.imask = ~1;
  eCore->regs.ilatcl = ~0;
  unsigned i;
  for(i=0; i<sizeof(eCore->regs.ctimer)/sizeof(eCore->regs.ctimer[0]); ++i)
    eCore->regs.ctimer[i] = 0;
  eCore->regs.memstatus.reg = 0;
  eCore->regs.memprotect.reg = 0;
  /* Enable clock gating */
  eCore->regs.meshconfig.lpmode = 1;

  return 0;
}

uint8_t soft_reset_payload[] = {
  0xe8, 0x16, 0x00, 0x00, 0xe8, 0x14, 0x00, 0x00, 0xe8, 0x12, 0x00, 0x00,
  0xe8, 0x10, 0x00, 0x00, 0xe8, 0x0e, 0x00, 0x00, 0xe8, 0x0c, 0x00, 0x00,
  0xe8, 0x0a, 0x00, 0x00, 0xe8, 0x08, 0x00, 0x00, 0xe8, 0x06, 0x00, 0x00,
  0xe8, 0x04, 0x00, 0x00, 0xe8, 0x02, 0x00, 0x00, 0x1f, 0x15, 0x02, 0x04,
  0x7a, 0x00, 0x00, 0x03, 0xd2, 0x01, 0xe0, 0xfb, 0x92, 0x01, 0xb2, 0x01,
  0xe0, 0xfe
};
/*
 *        ivt:
 *   0:              b.l     clear_ipend
 *   4:              b.l     clear_ipend
 *   8:              b.l     clear_ipend
 *   c:              b.l     clear_ipend
 *  10:              b.l     clear_ipend
 *  14:              b.l     clear_ipend
 *  18:              b.l     clear_ipend
 *  1c:              b.l     clear_ipend
 *  20:              b.l     clear_ipend
 *  24:              b.l     clear_ipend
 *  28:              b.l     clear_ipend
 *        clear_ipend:
 *  2c:              movfs   r0, ipend
 *  30:              orr     r0, r0, r0
 *  32:              beq     1f
 *  34:              rti
 *  36:              b       clear_ipend
 *        1:
 *  38:              gie
 *  3a:              idle
 *  3c:              b       1b
 */

int ee_soft_reset_core(eCoreMemMap_t* eCore)
{
  if (!eCore->regs.debugstatus.halt) { // & 1
    eCorePrintf(0, eCore, "%s(): No clean previous exit\n", __func__);
    e_halt(eCore);
  }

  /* Wait for external fetch */
  unsigned i;
  for (i = 0; eCore->regs.debugstatus.ext_pend && i < 1000; ++i) { // & 2
    usleep(10);
  }
  if (eCore->regs.debugstatus.ext_pend) { // & 2
    eCorePrintf(0, eCore, "%s(): stuck. Full system reset needed\n", __func__);
    return -1;
  }

  for (i = 0; i < sizeof(eCore->regs.dma)/sizeof(eCore->regs.dma[0]); ++i)
    if (eCore->regs.dma[i].status.dmastate & 7)
      eCorePrintf(0, eCore, "%s(): DMA%d NOT IDLE\n", __func__, i);

  /* Abort DMA transfers */
  if (ee_soft_reset_dma(eCore))
    return -1;

  /* Disable timers */
  eCore->regs.config.reg = 0;
  eCore->regs.ilatcl = ~0;
  eCore->regs.imask = 0;
  eCore->regs.iret = 0x2c; /* clear_ipend */
  eCore->regs.pc = 0x2c; /* clear_ipend */

  memcpy((void*)eCore->bank, soft_reset_payload, sizeof(soft_reset_payload));

  /* Set active bit */
  eCore->regs.fstatus = 1;

  e_resume(eCore);

  i = 10000;
  do {
    if (!eCore->regs.ipend
        && !eCore->regs.ilat
        && !eCore->regs.status.active) {

      /* Reset regs, excluding DMA (already done above) */
      ee_reset_regs(eCore, -1);

      return 0;
    }
    usleep(10);
  } while( --i );

  eCorePrintf(0, eCore, "%s: Not idle\n", __func__);
  return -1;
}


// needs to be run upfront a program run, otherwise the epiphany won't signal anything back
// TODO: move the lower part of the code to elib
// only use the commands that should be issued from the esys!!!
void reset()
{
  ((eSysRegs*)0x808f0000)->esysreset = 0x0;
  asm volatile("" ::: "memory");
  usleep(200000);

	// Perform post-reset, platform specific operations
//	if (e_platform.chip[0].type == E_E16G301) // TODO: assume one chip
//	if ((e_platform.type == E_ZEDBOARD1601) || (e_platform.type == E_PARALLELLA1601))
  typeof(&((eSysRegs*)0x808f0000)->esysconfig.reg) esysconfig = &((eSysRegs*)0x808f0000)->esysconfig.reg;
  *esysconfig = 0x50000000;
  asm volatile("" ::: "memory");

// The register must be written, there shall be NO read beforehand. Otherwise stall!
//                      E16G301               E64G301
//  East link offset:   0x08300000 [ 2, 3]    0x08700000 [ 2, 7]
  eCoreMemMap_t* eLinkEast =  ((eCoreMemMap_t*)0x0) + ECORE_NEXT( 32 +2, 8 +3 );
  volatile uint32_t* elinkmodecfgEast = &eLinkEast->regs.chipioregs.elinkmodecfg.reg;
//  assert(elinkmodecfg == (volatile uint32_t*)0x88bf0300);

  //      LCLK Transmit Frequency control: Divide cclk by 0->2, 1->4, 2->8
  *elinkmodecfgEast = 1;
  asm volatile("" ::: "memory");
  *esysconfig = 0x0;


/*
  // put individual cores into reset
  eCoreMemMap_t* eCoreBgn = &eCoresGMemBaseVA[32][8];
  eCoreBgn->regs.corereset.reset = 1;
  asm volatile("" ::: "memory");
  eCoreBgn->regs.corereset.reset = 0;
*/
}


void dump_mem(eCoreMemMap_t* eCoreBgn)
{
  typeof(eCoreBgn->sram[0])* s = eCoreBgn->sram;
  for( ; s < &eCoreBgn->sram[sizeof(eCoreBgn->sram)/sizeof(eCoreBgn->sram[0])]; ++s ) {
    
    if( ((uintptr_t)s) % (16 * sizeof(uint32_t)) == 0 ) {
      for( ; s < &eCoreBgn->sram[sizeof(eCoreBgn->sram)/sizeof(eCoreBgn->sram[0])] ; ) {
        unsigned f, found_zeros = 0;
        for( f = 0; f < 16; ++f )
          found_zeros += (s[f] != 0);
        if( ! found_zeros )
          for( f = 0; f < 16; ++f )
            ++s;
        else
          break;
      }
      if( s >= &eCoreBgn->sram[sizeof(eCoreBgn->sram)/sizeof(eCoreBgn->sram[0])] )
        break;
    }
  
    if( ((uintptr_t)s) % (16 * sizeof(uint32_t)) == 0 )
      printf("dump [%p]: ", s);
    printf("%8x ", *s);
    if( ((uintptr_t)s) % (16 * sizeof(uint32_t)) == (15 * sizeof(uint32_t)) )
      printf("\n");
  }
  printf("\n");
}

int main(int argc, char* argv[])
{
  reset(); // first reset!!!!!!!!!! otherwise Zynq will hang.
  printf("attempted reset!\n");

  // TODO: get from library
  eCoreMemMap_t* eCoreBgn = 0x0;
  eCoreBgn += ECORE_NEXT( 32, 8 );
  assert(eCoreBgn == (void*)0x80800000);
  
  const char *srectest =  "S0110000655F736569736D69632E7372656362\r\n"
                          "S30900000000E82C0000E2\r\n"
                          "S3150000002800000000000000000000000000000000C2\r\n"
                          "S3150000003800000000000000000000000000000000B2\r\n"
                          "S30D000000480000000000000000AA\r\n"
                          "S30D000000500000000000000000A2\r\n"
                          "S311000000580B6EE2000B600210520D00005F\r\n"
                          
                          "S30D00001278430000000000000025\r\n";

  printf("%s\n", srectest);

  memset( (char*)eCoreBgn->sram, 0, 0x8000 );
  int ret = MEASURE("parse_srec", parse_srec(srectest, srectest + strlen(srectest),
                                  eCoreBgn, eCoreBgn));
  printf("%d\n", ret);
  printf("--> 80800058 %x\n", *(uint32_t*)0x80800058);
  printf("--> 80800058 %x\n", *(char*)0x80800058);
  dump_mem(eCoreBgn);

  printf("should fail: %d %d\n", load_srec(NULL, NULL, NULL), load_srec("", NULL, NULL));

/*
  uint32_t val = 0xdeadbeef;

  volatile uint32_t *mem;
  for(mem = (volatile uint32_t*)&eCoreBgn->regs; mem < (volatile uint32_t*)(&eCoreBgn->regs + 1); ++mem) {
  
    if( mem == (unsigned*)0x808f0548
        || mem == (unsigned*)0x808f06fc
        || mem == (unsigned*)0x808f0708
        || mem == (unsigned*)0x808f070c )
      continue;
  
    *mem = val;
    if( val == *mem ) {
      reset();
      
      gettimeofday(&tbgn, NULL);

      unsigned i = 900000;
      while( *mem == val && --i );
      
      gettimeofday(&tend, NULL);

      if( i == 0 && *mem == val )
        printf("%p: timeout\n", mem);
      else {
        unsigned long us = ((tend.tv_sec * 1000000 + tend.tv_usec) - (tbgn.tv_sec * 1000000 + tbgn.tv_usec));
        printf("%p: required %ld μs\n", mem, us);
      }
    }
//    else
//      printf("Did not work!\n");
  }
*/
////////////////////////////////



  struct timeval tbgn, tend;
  gettimeofday(&tbgn, NULL);

  e_halt(eCoreBgn);
  ee_soft_reset_core(eCoreBgn);
 
  gettimeofday(&tend, NULL);
 
  printf("Init in %ld μs\n", ((tend.tv_sec * 1000000 + tend.tv_usec) - (tbgn.tv_sec * 1000000 + tbgn.tv_usec)));
 
  fprintf (stdout, "[%2d,%2d]\n", ECORE_ADDR_ROWID((void*)0x888F0308), ECORE_ADDR_COLID((void*)0x888F0308));

  return 0;
}
