add_subdirectory(8088)
add_subdirectory(config)
add_subdirectory(mbr)
	
if(PICOPOCKET_SIM)
	set(OR_HWLIBS)
	set(OR_HWSRC)
else()
	set(OR_HWLIBS 
		hardware_flash
		pico_lwip_nosys
		pico_cyw43_arch_none
		psram_pio
		ar1021_pio
	)
	set(OR_HWSRC
		disk_mapper.cpp
	)
endif()

add_library(or
	or.cpp
	int19_install.cpp
	monitor.cpp
	ramdisk.cpp
	config.cpp
	${OR_HWSRC}	
)

target_link_libraries(or 
	PRIVATE
		${WORKER}
		optionrom
		ioiface
		freedos_kernel
		config_img
		mbr_img
		${OR_HWLIBS}
		common
		cmake_git_version_tracking
	PUBLIC
		jmpcoro
)	
target_include_directories(or PUBLIC ${CMAKE_CURRENT_LIST_DIR}/)
