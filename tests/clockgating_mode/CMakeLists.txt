# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText:  2022 Patrick Siegl <code@siegl.it>

if(EHAL_BACKWARD_COMPATIBILITY)
	link_directories(${CMAKE_BINARY_DIR}/)

	add_executable(clockgating_mode.elf clockgating_mode)
	target_link_libraries(clockgating_mode.elf PRIVATE libe-hal.so)
	target_include_directories(clockgating_mode.elf PRIVATE ${LIB_E_HAL_INCLUDE_DIR})

	add_test(NAME clockgating_mode
		COMMAND sudo ELOGLEVEL=0 EPIPHANY_HDF=${CMAKE_SOURCE_DIR}/misc/platform.hdf ${CMAKE_CURRENT_BINARY_DIR}/clockgating_mode.elf)
endif(EHAL_BACKWARD_COMPATIBILITY)
