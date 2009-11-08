include_directories(${rududu_SOURCE_DIR}/lib)
link_directories(${rududu_BINARY_DIR}/librududu)

add_executable(huffgen huffgen.cpp)
add_executable(sgi2rgb sgi2rgb.cpp)

target_link_libraries(huffgen rududu)
