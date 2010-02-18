if(COMMAND cmake_policy)
	cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

include_directories(${libmultigain_SOURCE_DIR}/include)
add_definitions(
	-D_FILE_OFFSET_BITS=64
	)

add_library(multigain-static STATIC
	decode.cpp errors.cpp gain_analysis.c tag_locate.cpp )
set_target_properties(multigain-static PROPERTIES
	CLEAN_DIRECT_OUTPUT 1
	OUTPUT_NAME multigain
	)

add_library(multigain-shared SHARED
	decode.cpp errors.cpp gain_analysis.c tag_locate.cpp )
set_target_properties(multigain-shared PROPERTIES
	CLEAN_DIRECT_OUTPUT 1
	OUTPUT_NAME multigain
	SOVERSION 1
	VERSION 1.0.0
	)

add_executable(gaintool gaintool.cpp)
set_target_properties(gaintool PROPERTIES
	OUTPUT_NAME multigain
	)
target_link_libraries(gaintool multigain-shared mpg123)

install(TARGETS gaintool multigain-static multigain-shared
	ARCHIVE DESTINATION "lib${LIB_SUFFIX}"
	LIBRARY DESTINATION "lib${LIB_SUFFIX}"
	RUNTIME DESTINATION "bin"
	)
