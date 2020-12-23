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

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/json11/)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/json11/)

file(GLOB CPP_FILES *.cpp *.c *.hpp)
add_executable(DiffAndCopy ${CPP_FILES})

target_link_libraries(DiffAndCopy json11 --coverage)