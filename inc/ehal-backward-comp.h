// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText:  2022 Patrick Siegl <code@siegl.it>

#ifndef __EHAL_BACKWARD_COMP__H
#define __EHAL_BACKWARD_COMP__H

#include "epiphany-hal-data.h"
#include "epiphany-hal-data-local.h"

int e_init(char *hdf);
int e_finalize();

int e_open(e_epiphany_t *dev,
           unsigned row, unsigned col,
           unsigned rows, unsigned cols);
int e_close(e_epiphany_t *dev);

int e_alloc(e_mem_t *mbuf, off_t offset, size_t size);
int e_free(e_mem_t *mbuf);

ssize_t e_write(void *dev, unsigned row, unsigned col,
                off_t to_addr, const void *buf, size_t size);

int e_reset_system(e_epiphany_t *dev);

int e_load_group(char *executable, e_epiphany_t *dev,
                 unsigned row, unsigned col,
                 unsigned rows, unsigned cols,
                 e_bool_t start);
#define e_load( executable, dev, row, col, start ) e_load_group( executable, dev, row, col, 1, 1, start )

#define e_set_host_verbosity( lvl ) {}

int e_get_platform_info(e_platform_t *platform);

#endif /* __EHAL_BACKWARD_COMP__H */
