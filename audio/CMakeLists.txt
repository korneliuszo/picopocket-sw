add_library(audio
	sbdsp.cpp
)

target_link_libraries(audio
	PRIVATE
		audio_dma
		${WORKER}
		jmpcoro
)


target_include_directories(audio PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../)
