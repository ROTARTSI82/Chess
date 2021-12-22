mkdir build
cd build
cmake -DCMAKE_C_COMPILER=$(which clang) -DCMAKE_CXX_COMPILER=$(which clang++) ..
cmake --build .

