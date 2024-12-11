cmake_minimum_required(VERSION 3.29)

include(FetchContent)

FetchContent_Declare(
  freedos_kernel_zip
  URL      https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/dos/kernel/2043/ke2043_86f16.zip
  URL_HASH MD5=8cc70d36fa83181481c7fe8512ed19aa
  DOWNLOAD_NO_EXTRACT false
)

FetchContent_MakeAvailable(freedos_kernel_zip)

FetchContent_Declare(
  freedos_command_zip
  URL      https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/dos/command/0.85a/command.zip
  URL_HASH MD5=f3fb8bd1c80e19a48ee6878025d90364
  DOWNLOAD_NO_EXTRACT false
)

FetchContent_MakeAvailable(freedos_command_zip)


if(PICOPOCKET_SIM)
set(BFDARG elf64-x86-64)
else()
set(BFDARG elf32-littlearm)
endif()

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fdoskernel.o
	WORKING_DIRECTORY ${freedos_kernel_zip_SOURCE_DIR}
	COMMAND ${CMAKE_OBJCOPY} -I binary -O ${BFDARG} --rename-section .data=.flash bin/KERNEL.SYS ${CMAKE_CURRENT_BINARY_DIR}/fdoskernel.o
	#places in data
	DEPENDS ${freedos_kernel_zip_SOURCE_DIR}/bin/KERNEL.SYS
)

add_custom_target(freedos_kernel_obj DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/fdoskernel.o)

add_library(freedos_kernel OBJECT IMPORTED GLOBAL)
add_dependencies(freedos_kernel freedos_kernel_obj)
set_target_properties(freedos_kernel PROPERTIES
  IMPORTED_OBJECTS "${CMAKE_CURRENT_BINARY_DIR}/fdoskernel.o")