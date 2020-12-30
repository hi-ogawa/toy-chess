Building and Testing

```
CC=clang CXX=clang++ LDFLAGS=-fuse-ld=lld cmake -B build/Debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/Debug -j 4
./build/Debug/main_test

CC=clang CXX=clang++ LDFLAGS=-fuse-ld=lld cmake -B build/Release -DCMAKE_BUILD_TYPE=Release
cmake --build build/Release -j 4
./build/Release/main_test
```
