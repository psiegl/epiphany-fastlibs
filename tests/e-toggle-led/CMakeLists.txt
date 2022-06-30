# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText:  2022 Patrick Siegl <code@siegl.it>

link_directories(${CMAKE_BINARY_DIR}/)

add_executable(e-toggle-led.elf e-toggle-led)
target_link_libraries(e-toggle-led.elf PRIVATE libe-hal.so)
target_include_directories(e-toggle-led.elf PRIVATE ${LIB_E_HAL_INCLUDE_DIR})

add_test(NAME e-toggle-led-on
	COMMAND sudo ELOGLEVEL=0 EPIPHANY_HDF=${CMAKE_SOURCE_DIR}/misc/platform.hdf ${CMAKE_CURRENT_BINARY_DIR}/e-toggle-led.elf 1)

add_test(NAME e-toggle-led-off
	COMMAND sudo ELOGLEVEL=0 EPIPHANY_HDF=${CMAKE_SOURCE_DIR}/misc/platform.hdf ${CMAKE_CURRENT_BINARY_DIR}/e-toggle-led.elf 0)
