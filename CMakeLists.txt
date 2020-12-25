cmake_minimum_required(VERSION 3.9)
project(DiffAndCopy CXX)

set(CMAKE_CXX_STANDARD 17)

if(CMAKE_COMPILER_IS_GNUCC)
	option(ENABLE_COVERAGE "Enable coverage reporting for gcc/clang" FALSE)
	
	if(ENABLE_COVERAGE)
		add_compile_options(--coverage -O0)
	endif()
endif()

if(MSVC)
	add_compile_options(/W4)
else()
	add_compile_options(-Wall -Wextra -Wpedantic)
endif()

#adding json11 library
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/json11/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/json11/)

file(GLOB CPP_FILES *.cpp *.c *.hpp)
add_executable(DiffAndCopy ${CPP_FILES})

#if(CMAKE_BUILD_TYPE MATCHES DEBUG)
target_compile_definitions(DiffAndCopy PUBLIC -DDebug=1)
message("Debug configuration. #define DEBUG")
set_target_properties(
    DiffAndCopy
    PROPERTIES
    COMPILE_DEFINITIONS DEBUG=1
    COMPILE_DEFINITIONS TESTS=1
   )
#endif()

target_link_libraries(DiffAndCopy json11 cryptopp-shared --coverage)