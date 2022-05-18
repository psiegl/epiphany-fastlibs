// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText:  2022 Patrick Siegl <code@siegl.it>

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <memmap-epiphany-cores.h>
#include <loader/ehal-srec-loader.h>

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


int main(int argc, char* argv[])
{
  // TODO: get from library
  eCoreMemMap_t* eCoreBgn = 0x0;
  eCoreBgn += ECORE_NEXT( 32, 8 );
  assert(eCoreBgn == (void*)0x80800000);
  
  const char *srectest =  "S0110000655F736569736D69632E7372656362\r\n";

  printf("%s\n", srectest);

  int ret = MEASURE("parse_srec", parse_srec(srectest, srectest + strlen(srectest),
                                  eCoreBgn, eCoreBgn));
  printf("%d\n", ret);

  return 0;
}
