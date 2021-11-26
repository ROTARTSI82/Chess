cmake_minimum_required(VERSION 3.7)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set (CMAKE_CXX_STANDARD 20)

project(Chess)

# add_subdirectory(dep/SDL)
# add_subdirectory(dep/SDL_image)
add_compile_options(-Ofast) # -g3 -fsanitize=address -fno-omit-frame-pointer)
add_link_options(-Ofast) # -g3 -fsanitize=address -fno-omit-frame-pointer)

find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)

include_directories(${SDL2_INCLUDE_DIR})
include_directories(${SDL2_IMAGE_INCLUDE_DIR})

add_executable(Chess src/main.cpp src/chess.cpp src/stockfish_engine.cpp src/popen2.cpp src/engine.cpp)
target_include_directories(Chess PUBLIC include src)
target_link_libraries(Chess ${SDL2_LIBRARY} ${SDL2_MIXER_LIBRARY} ${SDL2_IMAGE_LIBRARY})
# target_include_directories(Chess PUBLIC dep/SDL/include dep/SDL_image)



project(ConnectFour)
add_executable(ConnectFour src/connect4/c4_main.cpp src/connect4/connect4.cpp src/connect4/neural_net.cpp)
target_include_directories(ConnectFour PUBLIC include src)
target_link_libraries(ConnectFour ${SDL2_LIBRARY} ${SDL2_MIXER_LIBRARY} ${SDL2_IMAGE_LIBRARY})
