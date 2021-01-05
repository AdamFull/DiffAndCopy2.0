file(GLOB hashlib_files *.cpp *.c *.hpp *.h)
add_library(hashlib ${hashlib_files})

add_library(hashlib-static STATIC ${hashlib_files})