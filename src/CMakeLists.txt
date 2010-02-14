include_directories(${libmultigain_SOURCE_DIR}/include)

add_library(multigain-static STATIC
	decode.cpp errors.cpp gain_analysis.c tag_locate.cpp )
set_target_properties(multigain-static PROPERTIES
	CLEAN_DIRECT_OUTPUT 1
	DEFINE_SYMBOL _FILE_OFFSET_BITS=64
	OUTPUT_NAME multigain
	)

add_library(multigain-shared SHARED
	decode.cpp errors.cpp gain_analysis.c tag_locate.cpp )
set_target_properties(multigain-shared PROPERTIES
	CLEAN_DIRECT_OUTPUT 1
	DEFINE_SYMBOL _FILE_OFFSET_BITS=64
	OUTPUT_NAME multigain
	SOVERSION 1
	VERSION 1.0.0
	)

add_executable(multigain-bin multigain.cpp)
set_target_properties(multigain-bin PROPERTIES
	DEFINE_SYMBOL _FILE_OFFSET_BITS=64
	OUTPUT_NAME multigain
	)
target_link_libraries(multigain-bin mpg123 multigain-shared)

install(TARGETS multigain-bin multigain-static multigain-shared
	ARCHIVE DESTINATION "lib${LIB_SUFFIX}"
	LIBRARY DESTINATION "lib${LIB_SUFFIX}"
	RUNTIME DESTINATION "bin"
	)
