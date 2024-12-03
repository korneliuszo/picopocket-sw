add_library(ioiface
	STATIC
	ioiface.cpp
)
	
target_link_libraries(ioiface
	PRIVATE
		isa_worker
)	
target_include_directories(ioiface PUBLIC ${CMAKE_CURRENT_LIST_DIR})
