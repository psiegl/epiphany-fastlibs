// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText:  2022 Patrick Siegl <code@siegl.it>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "ehal-backward-comp.h"
#include "loader/ehal-srec-loader.h"
#include "state/ehal-state.h"



#define elemsof( x ) (sizeof(x)/sizeof(x[0]))
#define ECORE_ADDR_ROWCOLID( addr ) (( ((uintptr_t)addr) >> 20 ) & ((ECORES_MAX_DIM-1) << 6 | (ECORES_MAX_DIM-1)))



extern eConfig_t ecfg;
eConfig_t *cfg = &ecfg;

e_platform_t e_platform = {
  .objtype      = E_EPI_PLATFORM,
	.hal_ver      = 0x050d0705, // TODO: update ver
	.initialized  = E_FALSE
};




int ee_set_platform_params(e_platform_t *platform)
{
  struct {
	  e_platformtype_t type;        // Epiphany platform part number
	  const char*      version;     // version name of Epiphany chip
  } platform_params_table[] = {
//   type              version
	  {E_GENERIC,        "GENERIC"},
	  {E_EMEK301,        "EMEK301"},
	  {E_EMEK401,        "EMEK401"},
	  {E_ZEDBOARD1601,   "ZEDBOARD1601"},
	  {E_ZEDBOARD6401,   "ZEDBOARD6401"},
	  {E_PARALLELLA1601, "PARALLELLA1601"},
	  {E_PARALLELLA6401, "PARALLELLA6401"},
  };

	for (unsigned v = 0; v < elemsof(platform_params_table); v++)
		if (!strcmp(platform->version, platform_params_table[v].version)) {
    	platform->type = platform_params_table[v].type;
			return E_OK;
		}

  // Fallback
	platform->type = platform_params_table[0].type;

	return E_ERR;
}

int ee_set_chip_params(e_chip_t *chip, eConfigChip_t *chipCfg)
{
  struct {
	  e_chiptype_t	 oldtype;		  // Epiphany chip part number
	  eChip_t    type;
	  off_t			 ioregs_n;	  // base address of north IO register
	  off_t			 ioregs_e;	  // base address of east IO register
	  off_t			 ioregs_s;	  // base address of south IO register
	  off_t			 ioregs_w;	  // base address of west IO register
  } chip_params_table[] = {
  // type                io_n        io_e        io_s        io_w
	  {E_E16G301, E16G301, 0x002f0000, 0x083f0000, 0x0c2f0000, 0x080f0000},
	  {E_E64G401, E64G401, 0x002f0000, 0x087f0000, 0x1c2f0000, 0x080f0000},
  };

	unsigned v, ret = E_OK;
	for (v = 0; v < elemsof(chip_params_table); v++)
		if(chipCfg->type == chip_params_table[v].type)
			break;

	if (v == elemsof(chip_params_table)) {
	  ret = E_ERR;
		v = 0;
	}

	chip->arch      = ECHIP_GET_ARCH( chipCfg->type );
	chip->rows      = ECHIP_GET_DIM( chipCfg->type );
	chip->cols      = ECHIP_GET_DIM( chipCfg->type );
	chip->num_cores = chip->rows * chip->cols;
	chip->sram_base = (uintptr_t)chipCfg->eCoreRoot[0][0].sram - (uintptr_t)&chipCfg->eCoreRoot[0][0];
	chip->sram_size = sizeof(chipCfg->eCoreRoot[0][0].sram);
	chip->regs_base = (uintptr_t)&chipCfg->eCoreRoot[0][0].regs - (uintptr_t)&chipCfg->eCoreRoot[0][0];
	chip->regs_size = sizeof(chipCfg->eCoreRoot[0][0].regs);

  __typeof__(chip_params_table[0])* cdb = &chip_params_table[v];
	chip->type      = cdb->oldtype;
	chip->ioregs_n  = cdb->ioregs_n;
	chip->ioregs_e  = cdb->ioregs_e;
	chip->ioregs_s  = cdb->ioregs_s;
	chip->ioregs_w  = cdb->ioregs_w;

	return ret;
}

int e_init(char *hdf)
{
  (void)hdf;

  if(cfg == NULL
     || cfg->fd == -1)
    return E_ERR;

	e_platform.chip = (e_chip_t *) malloc (cfg->num_chips * sizeof(e_chip_t)
	                                       * cfg->num_ext_mems + sizeof(e_memseg_t));
  if(!e_platform.chip)
    return E_ERR;

	e_platform.num_chips = cfg->num_chips;
	e_platform.num_emems = cfg->num_ext_mems;
	e_platform.emem = (e_memseg_t *) &e_platform.chip[cfg->num_chips];
  strcpy(e_platform.version, "PARALLELLA1601");

	e_platform.chip[0].row = ECORE_ADDR_ROWID(cfg->lchip->eCoreRoot);
	e_platform.chip[0].col = ECORE_ADDR_COLID(cfg->lchip->eCoreRoot);
	strcpy(e_platform.chip[0].version, "E16G301");

	e_platform.emem[0].phy_base = cfg->lemem->base_address;
	e_platform.emem[0].ephy_base = cfg->lemem->epi_base;
	e_platform.emem[0].size = cfg->lemem->size;
	e_platform.emem[0].type = (cfg->lemem->prot == (PROT_READ|PROT_WRITE)) ? E_RDWR
                            : ((cfg->lemem->prot == PROT_READ) ? E_RD 
                               : ((cfg->lemem->prot == PROT_WRITE) ? E_WR
                                  : 0));

  // Populate the missing platform parameters according to platform version.
	ee_set_platform_params(&e_platform);

	e_platform.row  = 0x3f;
	e_platform.col  = 0x3f;
	e_platform.rows = 0;
	e_platform.cols = 0;
	for (unsigned i=0; i< 1/* cfg->num_chips */; i++) { // In case HDF provides 2, we get here a problem row[1] and col[1] are not set.
	  // Populate the missing chip parameters according to chip version.
		ee_set_chip_params(&e_platform.chip[i], &cfg->chip[i]);
	
	  // Find the minimal bounding box of Epiphany chips. This defines the reference frame for core-groups.
		if (e_platform.row > e_platform.chip[i].row)
			e_platform.row = e_platform.chip[i].row;

		if (e_platform.col > e_platform.chip[i].col)
			e_platform.col = e_platform.chip[i].col;

		if (e_platform.rows < (e_platform.chip[i].row + e_platform.chip[i].rows - 1))
			e_platform.rows =  e_platform.chip[i].row + e_platform.chip[i].rows - 1;

		if (e_platform.cols < (e_platform.chip[i].col + e_platform.chip[i].cols - 1))
			e_platform.cols =  e_platform.chip[i].col + e_platform.chip[i].cols - 1;
	}
	e_platform.rows = e_platform.rows - e_platform.row + 1;
	e_platform.cols = e_platform.cols - e_platform.col + 1;

	e_platform.initialized = E_TRUE;

	return E_OK;
}

int e_finalize()
{
	if (e_platform.initialized == E_FALSE) {
		fprintf(stderr, "e_finalize(): Platform was not initiated!\n");
		return E_ERR;
	}

	e_platform.initialized = E_FALSE;

	free(e_platform.chip);

	return E_OK;
}

// ------------------------------------------------------------

// TODO: assumes one chip type in platform
// TODO: assumes first chip + a single chip type
int e_open(e_epiphany_t *dev, unsigned row, unsigned col, unsigned rows, unsigned cols)
{
  if (e_platform.initialized == E_FALSE) {
		fprintf(stderr, "e_open(): Platform was not initialized. Use e_init()!\n");
		return E_ERR;
	}

	// Map individual cores to virtual memory space
	dev->core = (e_core_t**) malloc (rows * (sizeof(e_core_t*)
                                           + cols * sizeof(e_core_t)));
	if (!dev->core) {
		fprintf(stderr, "e_open(): Error while allocating eCore descriptors!\n");
		return E_ERR;
	}

	dev->objtype = E_EPI_GROUP;
	dev->type	 = e_platform.chip[0].type; 

	// Set device geometry
	// TODO: check if coordinates and size are legal.
	dev->memfd   = -1;
	dev->row		 = row + ECORE_ADDR_ROWID( &cfg->lchip->eCoreRoot[0][0] );
	dev->col		 = col + ECORE_ADDR_COLID( &cfg->lchip->eCoreRoot[0][0] );
	dev->rows		 = rows;
	dev->cols		 = cols;
	dev->num_cores	 = rows * cols;
	dev->base_coreid = ECORE_ADDR_ROWCOLID( &cfg->lchip->eCoreRoot[0][0] );

	for (unsigned irow=0; irow<rows; irow++) {
		dev->core[irow] = &((e_core_t*)&dev->core[rows])[cols * irow];
		for (unsigned icol=0; icol<cols; icol++) {
			e_core_t *curr_core = &dev->core[irow][icol];

			curr_core->objtype = E_EPI_CORE;
			curr_core->row = irow;
			curr_core->col = icol;
			curr_core->id  = ECORE_ADDR_ROWCOLID( &cfg->lchip->eCoreRoot[irow][icol] );

			// e-core regs
			curr_core->regs.objtype = E_NULL;
			curr_core->regs.map_size = sizeof(cfg->lchip->eCoreRoot[irow][icol].regs);
			curr_core->regs.page_offset = 0;
			curr_core->regs.base =
			  curr_core->regs.mapped_base =
			    curr_core->regs.page_base =
			      curr_core->regs.phy_base = (uintptr_t)&cfg->lchip->eCoreRoot[irow][icol].regs; 

			// SRAM array
			curr_core->mems.objtype = E_NULL;
			curr_core->mems.map_size = sizeof(cfg->lchip->eCoreRoot[irow][icol].sram);
			curr_core->mems.page_offset = 0;
			curr_core->mems.base =
			  curr_core->mems.mapped_base =
			    curr_core->mems.page_base =
            curr_core->mems.phy_base = (uintptr_t)&cfg->lchip->eCoreRoot[irow][icol].sram[0];
		}
	}

	return E_OK;
}

// Close an e-core workgroup
int e_close(e_epiphany_t *dev)
{
	if (!dev) {
		fprintf(stderr, "e_close(): Core group was not opened!\n");
		return E_ERR;
	}

	free(dev->core);

	return E_OK;
}

// ------------------------------------------------------------

unsigned long ee_rndl_page(unsigned long size)
{
	// Get OS memory page size
	unsigned long page_size = sysconf(_SC_PAGE_SIZE);

	// Find lower integral number of pages
	return (size / page_size) * page_size;
}

int e_alloc(e_mem_t *mbuf, off_t offset, size_t size)
{
	if (e_platform.initialized == E_FALSE) {
		fprintf(stderr, "e_alloc(): Platform was not initialized. Use e_init()!\n");
		return E_ERR;
	}

	mbuf->objtype = E_EXT_MEM;
	mbuf->memfd   = -1;

	mbuf->phy_base = e_platform.emem[0].phy_base + offset; // TODO: this takes only the 1st segment into account
	mbuf->page_base = ee_rndl_page(mbuf->phy_base);
	mbuf->page_offset = mbuf->phy_base - mbuf->page_base;
	mbuf->map_size = size + mbuf->page_offset;

  mbuf->mapped_base = (void*)cfg->lemem->epi_base;

	mbuf->base = (void*)(((char*)mbuf->mapped_base) + mbuf->page_offset);

	mbuf->ephy_base = e_platform.emem[0].ephy_base + offset; // TODO: this takes only the 1st segment into account
	mbuf->emap_size = size;

	return E_OK;
}

// Free a memory buffer in external memory
int e_free(e_mem_t *mbuf)
{
  (void)mbuf;
	return E_OK;
}

// ------------------------------------------------------------

ssize_t ee_write_esys(off_t to_addr, int data)
{
  uintptr_t sys_regs_base = (uintptr_t)cfg->esys_regs_base | 0xf00;
  assert(sys_regs_base == 0x808f0f00);
	*(int*)(sys_regs_base + to_addr) = data;

	return sizeof(int);
}

// Write a memory block to SRAM of a core in a group
ssize_t ee_write_buf(e_epiphany_t *dev, unsigned row, unsigned col, off_t to_addr, const void *buf, size_t size)
{
	volatile unsigned char *pto = cfg->lchip->eCoreRoot[row][col].sram + to_addr;
	assert(dev->core[row][col].mems.base == cfg->lchip->eCoreRoot[row][col].sram);
	memcpy((char*)pto, buf, size);

	return size;
}

ssize_t ee_write_reg(e_epiphany_t *dev, unsigned row, unsigned col, off_t to_addr, int data)
{
	if (to_addr >= E_REG_R0)
		to_addr -= E_REG_R0;

  eCoreRegs_t* regs = &cfg->lchip->eCoreRoot[row][col].regs;
  assert(regs == dev->core[row][col].regs.base);

  *(int*)(((char*)regs) + to_addr) = data;

	return sizeof(int);
}

// Write a block to an external memory buffer
ssize_t ee_mwrite_buf(e_mem_t *mbuf, off_t to_addr, const void *buf, size_t size)
{
	void* pto = mbuf->base + to_addr;
	memcpy(pto, buf, size);
	return size;
}

// Write a memory block to a core in a group
ssize_t e_write(void *dev, unsigned row, unsigned col, off_t to_addr, const void *buf, size_t size)
{
	ssize_t       wcount;
	e_epiphany_t *edev;
	e_mem_t      *mdev;

	switch (*(e_objtype_t*) dev) {
	case E_EPI_GROUP:
		edev = (e_epiphany_t*) dev;
		if (to_addr < edev->core[row][col].mems.map_size)
			wcount = ee_write_buf(edev, row, col, to_addr, buf, size);
		else
			wcount = ee_write_reg(edev, row, col, to_addr, *(unsigned*) buf);
		break;

	case E_EXT_MEM:
		mdev = (e_mem_t *) dev;
		wcount = ee_mwrite_buf(mdev, to_addr, buf, size);
		break;

	default:
		wcount = 0;
	}

	return wcount;
}

// ------------------------------------------------------------

int e_reset_system(e_epiphany_t *dev)
{
//	printf("Writing 0 to E_SYS_RESET:\n");
	ee_write_esys(/* E_SYS_RESET =  E_SYS_REG_BASE + 0x0004, E_SYS_REG_BASE  = 0x00000000 */0x4, 0); // 0x4
	usleep(200000);

	// Perform post-reset, platform specific operations
//	if (e_platform.chip[0].type == E_E16G301) // TODO: assume one chip
	if ((e_platform.type == E_ZEDBOARD1601) || (e_platform.type == E_PARALLELLA1601))
	{
//  	printf("Writing 0x50000000 to E_SYS_CONFIG:\n");
		ee_write_esys(/* E_SYS_CONFIG =  E_SYS_REG_BASE + 0x0000, E_SYS_REG_BASE  = 0x00000000 */0x0, 0x50000000); // 0x0
//	  printf("Writing 1 to E_REG_IO_LINK_MODE_CFG [2,3]:\n");
		int data = 1;
		e_write(dev, 2,3,/*0, 0,*/ /* E_REG_IO_LINK_MODE_CFG = E_CHIP_REG_BASE + 0x0300, E_CHIP_REG_BASE        = 0xf0000 */ 0xf0000 | 0x0300, &data, sizeof(int));
//  	printf("Writing 0x00000000 to E_SYS_CONFIG:\n");
		ee_write_esys(/* E_SYS_CONFIG =  E_SYS_REG_BASE + 0x0000, E_SYS_REG_BASE  = 0x00000000 */0x0, 0x00000000); // 0x0
	}

	return E_OK;
}

// ------------------------------------------------------------

int ee_set_core_config_range(e_epiphany_t *pEpiphany,
                               eCoreMemMap_t* eCoreBgn, eCoreMemMap_t* eCoreEnd)
{
  struct {
    e_group_config_t e_group_config;
    e_emem_config_t  e_emem_config;
  } backComp;
  assert(sizeof(backComp) == (sizeof(e_group_config_t) + sizeof(e_emem_config_t)));
  
  backComp.e_group_config.objtype     = E_EPI_GROUP;
  backComp.e_group_config.chiptype    = pEpiphany->type;
  backComp.e_group_config.group_id    = pEpiphany->base_coreid;
  backComp.e_group_config.group_row   = pEpiphany->row;
  backComp.e_group_config.group_col   = pEpiphany->col;
  backComp.e_group_config.group_rows  = pEpiphany->rows;
  backComp.e_group_config.group_cols  = pEpiphany->cols;
  backComp.e_group_config.alignment_padding = 0xdeadbeef;

  backComp.e_emem_config.objtype   = E_EXT_MEM;
  backComp.e_emem_config.base      = cfg->lemem->epi_base/*TODO!!! pEMEM->ephy_base from Alloc*/;


  eCoreMemMap_t* eCoreRoot = &cfg->lchip->eCoreRoot[0][0];

#if 0
  for(uintptr_t r = ECORE_MASK_ROWID( eCoreBgn );
      r <= ECORE_MASK_ROWID( eCoreEnd ); r += ECORE_ONE_ROW) {
    backComp.e_group_config.core_row = ECORE_ADDR_ROWID( r ) - ECORE_ADDR_ROWID( eCoreRoot );
    for(uintptr_t c = ECORE_MASK_COLID( eCoreBgn );
        c <= ECORE_MASK_COLID( eCoreEnd ); c += ECORE_ONE_COL) {
#else // somehow from back does not work.
  for(uintptr_t r = ECORE_MASK_ROWID( eCoreEnd );
      r >= ECORE_MASK_ROWID( eCoreBgn ); r -= ECORE_ONE_ROW) {
    backComp.e_group_config.core_row = ECORE_ADDR_ROWID( r ) - ECORE_ADDR_ROWID( eCoreRoot );

    for(uintptr_t c = ECORE_MASK_COLID( eCoreEnd );
        c >= ECORE_MASK_COLID( eCoreBgn ); c -= ECORE_ONE_COL) {
#endif
	    backComp.e_group_config.core_col = ECORE_ADDR_COLID( c ) - ECORE_ADDR_COLID( eCoreRoot );

      eCoreMemMapSW_t* cur = (eCoreMemMapSW_t*)(r | c);
      //memcpy(&cur->grpcfg, &backComp, sizeof(backComp));
      memcpy(&cur->____PADDING[0], &backComp, sizeof(backComp));
    }
  }

	return 0;
}

int e_load_group(char *executable, e_epiphany_t *dev,
                 unsigned row, unsigned col,
                 unsigned rows, unsigned cols,
                 e_bool_t start)
{
	if (!dev) {
		fprintf(stderr, "ERROR: Can't connect to Epiphany or external memory.\n");
		return E_ERR;
	}

  eCoreMemMap_t* eCoreBgn = &cfg->lchip->eCoreRoot[row][col];
  eCoreMemMap_t* eCoreEnd = &cfg->lchip->eCoreRoot[row+rows-1][col+cols-1];
	if (load_srec(executable, eCoreBgn, eCoreEnd)
	    || ee_set_core_config_range(dev, eCoreBgn, eCoreEnd))
	  return E_ERR;

  if(start == E_TRUE) {
    int SYNC = (1 << E_SYNC);

#if 1
    for(uintptr_t r = ECORE_MASK_ROWID( eCoreBgn );
        r <= ECORE_MASK_ROWID( eCoreEnd ); r += ECORE_ONE_ROW) {
      for(uintptr_t c = ECORE_MASK_COLID( eCoreBgn );
          c <= ECORE_MASK_COLID( eCoreEnd ); c += ECORE_ONE_COL) {
#else // somehow from back does not work.
    for(uintptr_t r = ECORE_MASK_ROWID( eCoreEnd );
        r >= ECORE_MASK_ROWID( eCoreBgn ); r -= ECORE_ONE_ROW) {
      for(uintptr_t c = ECORE_MASK_COLID( eCoreEnd );
          c >= ECORE_MASK_COLID( eCoreBgn ); c -= ECORE_ONE_COL) {
#endif
        eCoreMemMap_t* cur = (eCoreMemMap_t*)(r | c);
        cur->regs.ilatst = SYNC;
      }
    }
  }

  return E_OK;
}

// ------------------------------------------------------------

int e_get_platform_info(e_platform_t *platform)
{
	if (e_platform.initialized == E_FALSE) {
		fprintf(stderr, "e_get_platform_info(): Platform was not initialized. Use e_init()!\n");
		return E_ERR;
	}

	*platform = e_platform;
	platform->chip = NULL;
	platform->emem = NULL;

	return E_OK;
}
