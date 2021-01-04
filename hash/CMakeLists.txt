file(GLOB hash_files *.cpp *.c *.hpp *.h)
add_library(hash ${hash_files})

add_library(hash-static STATIC ${hash_files})