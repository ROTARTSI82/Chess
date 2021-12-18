cmake_minimum_required(VERSION 3.7)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set (CMAKE_CXX_STANDARD 20)

project(Scacus)
add_compile_options(-Ofast -std=c++17) # -g3 -fsanitize=address -fno-omit-frame-pointer)
add_link_options(-Ofast -std=c++17) #-g3 -fsanitize=address -fno-omit-frame-pointer)

find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)

include_directories(${SDL2_INCLUDE_DIR} ${SDL2_IMAGE_INCLUDE_DIR})
link_libraries(${SDL2_LIBRARY} ${SDL2_IMAGE_LIBRARY})

file(GLOB_RECURSE SCACUS_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_executable(Scacus ${SCACUS_SOURCES})
target_include_directories(Scacus PUBLIC include src)
# target_include_directories(Chess PUBLIC dep/SDL/include dep/SDL_image)
