# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText:  2022 Patrick Siegl <code@siegl.it>

#include_directories(${CMAKE_SOURCE_DIR}/inc)
link_directories(${CMAKE_BINARY_DIR}/)
add_executable(helloworld.elf helloworld.c)
target_link_libraries(helloworld.elf PRIVATE libehal.so)
add_test(NAME helloworld
	COMMAND sudo ELOGLEVEL=0 EPIPHANY_HDF=${CMAKE_SOURCE_DIR}/misc/platform.hdf ${CMAKE_CURRENT_BINARY_DIR}/helloworld.elf)

add_executable(naiiv.elf main.c)
target_link_libraries(naiiv.elf PRIVATE libehal.so)
add_test(NAME naiiv
	COMMAND sudo ELOGLEVEL=5 EPIPHANY_HDF=${CMAKE_SOURCE_DIR}/misc/platform.hdf ${CMAKE_CURRENT_BINARY_DIR}/naiiv.elf) 
