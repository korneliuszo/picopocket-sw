add_subdirectory(8088)

add_library(or
	or.cpp
	int19_install.cpp
	monitor.cpp
)
	
target_link_libraries(or 
	PRIVATE
		isa_worker
		optionrom
		ioiface
	PUBLIC
		jmpcoro
)	
target_include_directories(or PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../)
