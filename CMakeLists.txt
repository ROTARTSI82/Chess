cmake_minimum_required(VERSION 3.7)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set (CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Scacus)
add_compile_options(-fno-exceptions -flto -Ofast -march=x86-64-v3 -std=c++20 -Wall -Wextra -Wpedantic)
add_link_options(-fno-exceptions -flto -Ofast -march=x86-64-v3 -std=c++20 -Wall -Wextra -Wpedantic)

# stockfish flags
# CXXFLAGS:  -Wall -Wcast-qual -fno-exceptions -std=c++17  -pedantic -Wextra -Wshadow -m64 -DUSE_PTHREADS -DNDEBUG -O3 -fexperimental-new-pass-manager -DIS_64BIT -msse -msse3 -mpopcnt -DUSE_POPCNT -DUSE_AVX2 -mavx2 -DUSE_SSE41 -msse4.1 -DUSE_SSSE3 -mssse3 -DUSE_SSE2 -msse2 -DUSE_PEXT -mbmi2 -flto
# LDFLAGS:   -latomic -m64 -lpthread  -Wall -Wcast-qual -fno-exceptions -std=c++17  -pedantic -Wextra -Wshadow -m64 -DUSE_PTHREADS -DNDEBUG -O3 -fexperimental-new-pass-manager -DIS_64BIT -msse -msse3 -mpopcnt -DUSE_POPCNT -DUSE_AVX2 -mavx2 -DUSE_SSE41 -msse4.1 -DUSE_SSSE3 -mssse3 -DUSE_SSE2 -msse2 -DUSE_PEXT -mbmi2 -flto
add_compile_options(-m64 -msse -msse3 -mpopcnt -mavx2 -msse4.1 -mssse3 -msse2 -mbmi2)
add_link_options(-m64 -msse -msse3 -mpopcnt -mavx2 -msse4.1 -mssse3 -msse2 -mbmi2)

option(USE_ASAN "" false)
option(USE_SDL "" false)
option(USE_DBG "" false)



# profile optimization: -fprofile-instr-generate
# merge profile data: llvm-profdata merge -output=stockfish.profdata *.profraw
# use profile to optimize:
#   CXXFLAGS: -fprofile-instr-use=stockfish.profdata
#   LDFLAGS: -fprofile-use
if (false)
    add_compile_options(-fprofile-instr-generate)
    add_link_options(-fprofile-instr-generate)
elseif()
    add_compile_options(-fprofile-instr-use=scacus.profdata -fprofile-use)
    add_link_options(-fprofile-instr-use=scacus.profdata -fprofile-use)
endif()


set(USE_DBG false)
if (USE_DBG)
    message("-- !! Debug symbols enabled!!")
    add_compile_options(-g3 -glldb -fno-omit-frame-pointer)
    add_link_options(-g3 -glldb -fno-omit-frame-pointer)
endif()

set(USE_ASAN false)
if (USE_ASAN)
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
endif()

set(USE_SDL false)
if (USE_SDL)
    find_package(SDL2 REQUIRED)
    find_package(SDL2_image REQUIRED)

    include_directories(${SDL2_INCLUDE_DIR} ${SDL2_IMAGE_INCLUDE_DIR})
    link_libraries(${SDL2_LIBRARY} ${SDL2_IMAGE_LIBRARY})

    add_compile_definitions(SDL_AVAIL)
endif()

file(GLOB_RECURSE SCACUS_SOURCES CONFIGURE_DEPENDS src/*.cpp)

message("-- sources = ${SCACUS_SOURCES}")
add_executable(Scacus ${SCACUS_SOURCES})
target_include_directories(Scacus PUBLIC include src)
# target_include_directories(Chess PUBLIC dep/SDL/include dep/SDL_image)
