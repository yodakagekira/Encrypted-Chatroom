cd ~/Documents/C++/multiclient-chatserver/proj
rm -rf build          # ← delete the broken/incompatible build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel $(nproc)