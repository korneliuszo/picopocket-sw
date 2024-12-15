add_subdirectory(freedos)
include(microweb/CMakeLists.txt)
include(tui/CMakeLists.txt)

FetchContent_MakeAvailable(freedos_command_zip)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/config.img
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND 
	    dd if=/dev/zero of=config.img bs=1k count=512
	COMMAND
		mformat -i config.img -t 16 -h 2 -s 32 -L12 ::
	COMMAND
		mcopy -i config.img ${freedos_command_zip_SOURCE_DIR}/BIN/COMMAND.COM ::
	COMMAND
		mcopy -i config.img ${CMAKE_CURRENT_SOURCE_DIR}/tui/pocket.exe ::
	COMMAND
		mcopy -i config.img ${CMAKE_CURRENT_SOURCE_DIR}/fdconfig.sys ::
	COMMAND
		mcopy -i config.img ${microweb_boot_disk_BINARY_DIR}/MICROWEB ::
	#places in data
	DEPENDS 
		${freedos_command_zip_SOURCE_DIR}/BIN/COMMAND.COM
		${CMAKE_CURRENT_SOURCE_DIR}/pocket.exe
		${CMAKE_CURRENT_SOURCE_DIR}/fdconfig.sys
		${microweb_boot_disk_BINARY_DIR}/MICROWEB
)

if(PICOPOCKET_SIM)
set(BFDARG elf64-x86-64)
else()
set(BFDARG elf32-littlearm)
endif()

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/config_img.o
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND ${CMAKE_OBJCOPY} -I binary -O ${BFDARG} --rename-section .data=.flashdata config.img config_img.o
	#places in data
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/config.img
)

add_custom_target(config_img_obj DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/config_img.o)

add_library(config_img OBJECT IMPORTED GLOBAL)
add_dependencies(config_img config_img_obj)
set_target_properties(config_img PROPERTIES
  IMPORTED_OBJECTS "${CMAKE_CURRENT_BINARY_DIR}/config_img.o")