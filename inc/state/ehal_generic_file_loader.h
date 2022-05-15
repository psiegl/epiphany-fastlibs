// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText:  2022 Patrick Siegl <code@siegl.it>

#ifndef __EHAL_GENERIC_FILE_LOADER__H
#define __EHAL_GENERIC_FILE_LOADER__H

int load_file(const char *fname, unsigned extc, const char** ext,
              int (*fnc)(char* fileBgn, char* fileEnd, void *pass),
              void *pass);

#endif /* __EHAL_GENERIC_FILE_LOADER__H */
