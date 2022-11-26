mkdir build
cd build
cmake -GNinja -DCMAKE_C_COMPILER=$(which clang) -DCMAKE_CXX_COMPILER=$(which g++) -DCMAKE_LINKER=$(which ld.lld) ..
cmake --build . -j8

