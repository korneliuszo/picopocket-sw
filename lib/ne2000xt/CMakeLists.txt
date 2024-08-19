cmake_minimum_required(VERSION 3.29)

include(FetchContent)

FetchContent_Declare(
  ne2000xt_bin
  URL      https://github.com/korneliuszo/ne2000xt/releases/download/v0.8/optionrom.bin
  URL_HASH MD5=3296465e867ce706f178ea177f37f625
  DOWNLOAD_NO_EXTRACT true
)

FetchContent_MakeAvailable(ne2000xt_bin)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/optionrom.o
	WORKING_DIRECTORY ${ne2000xt_bin_SOURCE_DIR}
	COMMAND ${CMAKE_LINKER} -r -b binary -o ${CMAKE_CURRENT_BINARY_DIR}/optionrom.o optionrom.bin
	#places in data
	DEPENDS ${ne2000xt_bin_SOURCE_DIR}/optionrom.bin
)

add_custom_target(ne2000xt_obj DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/optionrom.o)

add_library(ne2000xt OBJECT IMPORTED GLOBAL)
add_dependencies(ne2000xt ne2000xt_obj)
set_target_properties(ne2000xt PROPERTIES
  IMPORTED_OBJECTS "${CMAKE_CURRENT_BINARY_DIR}/optionrom.o")