
if(PICOPOCKET_SIM)
set(BFDARG elf64-x86-64)
else()
set(BFDARG elf32-littlearm)
endif()

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mbr.o
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}	
	COMMAND 
		${CMAKE_OBJCOPY} -I binary -O ${BFDARG} --rename-section .data=.flashdata mbr.bin ${CMAKE_CURRENT_BINARY_DIR}/mbr.o
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/mbr.bin
)

add_custom_target(mbr_obj DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/mbr.o)

add_library(mbr_img OBJECT IMPORTED GLOBAL)
add_dependencies(mbr_img mbr_obj)
set_target_properties(mbr_img PROPERTIES
  IMPORTED_OBJECTS "${CMAKE_CURRENT_BINARY_DIR}/mbr.o")