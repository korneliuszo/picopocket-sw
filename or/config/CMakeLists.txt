add_subdirectory(freedos)

FetchContent_MakeAvailable(freedos_command_zip)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/config.img
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND 
	    dd if=/dev/zero of=config.img bs=1M count=8
	COMMAND
		mformat -i config.img -t 256 -h 2 -s 32 -L12 ::
	COMMAND
		mcopy -i config.img ${freedos_command_zip_SOURCE_DIR}/BIN/COMMAND.COM ::
	#places in data
	DEPENDS 
		${freedos_command_zip_SOURCE_DIR}/BIN/COMMAND.COM
)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/config.img.spy
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND 
		snappy -c ${CMAKE_CURRENT_BINARY_DIR}/config.img > ${CMAKE_CURRENT_BINARY_DIR}/config.img.spy
	#places in data
	DEPENDS 
		${CMAKE_CURRENT_BINARY_DIR}/config.img
)

if(PICOPOCKET_SIM)
set(BFDARG elf64-x86-64)
else()
set(BFDARG elf32-littlearm)
endif()

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/config_img.o
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND ${CMAKE_OBJCOPY} -I binary -O ${BFDARG} --rename-section .data=.flash config.img.spy config_img.o
	#places in data
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/config.img.spy
)

add_custom_target(config_img_obj DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/config_img.o)

add_library(config_img OBJECT IMPORTED GLOBAL)
add_dependencies(config_img config_img_obj)
set_target_properties(config_img PROPERTIES
  IMPORTED_OBJECTS "${CMAKE_CURRENT_BINARY_DIR}/config_img.o")