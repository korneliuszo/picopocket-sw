cmake_minimum_required(VERSION 3.16)
project(picopocket-configuration-tui)

set(WATCOM_PATH "$ENV{HOME}/watcom" CACHE STRING "Watcom compiler path")

add_custom_command(
	COMMAND 
	${CMAKE_COMMAND} -E env 
		"PATH=${WATCOM_PATH}/binl64/:$ENV{PATH}"
		"INCLUDE=${WATCOM_PATH}/h"
		"WATCOM=${WATCOM_PATH}"
	wmake -f makefile.wat pocket.exe
	WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
	OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/pocket.exe
	DEPENDS 
		${CMAKE_CURRENT_LIST_DIR}/picopocket.c
		${CMAKE_CURRENT_LIST_DIR}/menus.c
		${CMAKE_CURRENT_LIST_DIR}/dialogs.c
)

add_custom_target(pocket_exe DEPENDS ${CMAKE_CURRENT_LIST_DIR}/pocket.exe)
