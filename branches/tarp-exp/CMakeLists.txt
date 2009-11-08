cmake_minimum_required(VERSION 2.4)
cmake_policy(SET CMP0003 NEW)

project(rududu CXX)

SET(CMAKE_VERBOSE_MAKEFILE FALSE)
SET(CMAKE_CXX_FLAGS "-Wall")
IF(NOT CMAKE_BUILD_TYPE)
	SET(CMAKE_BUILD_TYPE Release CACHE STRING
		"Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
		FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)

add_subdirectory(lib)
add_subdirectory(ric)
add_subdirectory(encoder)
add_subdirectory(utils)
