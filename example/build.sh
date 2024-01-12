mkdir -p build
cd build
git clone --depth=1 https://github.com/nlohmann/json.git
g++ -std=c++11 -I./json/single_include -I../../include -o example ../example.cpp
cd -
./build/example
