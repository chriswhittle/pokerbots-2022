rm -r build
mkdir -p build
BUILD_TYPE=Release
if command -v cmake3 &> /dev/null
then
    cd build && cmake3 -DCMAKE_BUILD_TYPE=${BUILD_TYPE} .. && make VERBOSE=1
    exit
else
    cd build && cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} .. && make VERBOSE=1
    exit
fi
