include_directories(${librg_SOURCE_DIR}/include)

add_library(multigain-static STATIC gain_analysis)
set_target_properties(multigain-static PROPERTIES OUTPUT_NAME multigain CLEAN_DIRECT_OUTPUT 1)

add_library(multigain-shared SHARED gain_analysis)
set_target_properties(multigain-shared PROPERTIES OUTPUT_NAME multigain CLEAN_DIRECT_OUTPUT 1 VERSION 1.0.0 SOVERSION 1)

install(TARGETS multigain-static multigain-shared LIBRARY DESTINATION "lib${LIB_SUFFIX}" ARCHIVE DESTINATION "lib${LIB_SUFFIX}")
