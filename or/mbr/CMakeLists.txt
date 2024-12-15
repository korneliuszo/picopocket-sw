
if(PICOPOCKET_SIM)
set(BFDARG elf64-x86-64)
else()
set(BFDARG elf32-littlearm)
endif()

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mbr.o
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND
		dd if=/dev/zero of=disk.img bs=8M count=1
	COMMAND
		fdisk -C 256 -H 2 -S 256 disk.img < ${CMAKE_CURRENT_SOURCE_DIR}/fdisk.script
	COMMAND
		dd if=disk.img of=mbr.bin bs=512 count=1		
	COMMAND 
		${CMAKE_OBJCOPY} -I binary -O ${BFDARG} --rename-section .data=.flashdata mbr.bin mbr.o
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/fdisk.script
)

add_custom_target(mbr_obj DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/mbr.o)

add_library(mbr_img OBJECT IMPORTED GLOBAL)
add_dependencies(mbr_img mbr_obj)
set_target_properties(mbr_img PROPERTIES
  IMPORTED_OBJECTS "${CMAKE_CURRENT_BINARY_DIR}/mbr.o")