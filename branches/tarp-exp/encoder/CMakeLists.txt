include_directories(${rududu_SOURCE_DIR}/lib)
link_directories(${rududu_BINARY_DIR}/librududu)

add_executable(rudenc rudenc.cpp)
add_executable(ruddec ruddec.cpp)

target_link_libraries(rudenc rududu avcodec avformat avutil)
target_link_libraries(ruddec rududu SDL)
