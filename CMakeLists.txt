cmake_minimum_required(VERSION 3.4.1)

# Compiler options
add_compile_options(-Wall)

# Submodule library
add_library(gltf
            STATIC

            # Source
            gltf.c)

# Linking
target_link_libraries(gltf

                      # NDK libraries
                      log)
