Building and Testing

```
CC=clang CXX=clang++ LDFLAGS=-fuse-ld=lld cmake -B build/Debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/Debug
./build/Debug/main_test

CC=clang CXX=clang++ LDFLAGS=-fuse-ld=lld cmake -B build/Release -DCMAKE_BUILD_TYPE=Release
cmake --build build/Release
./build/Release/main_test
```

Benchmark

```
./build/Release/main_bench --benchmark-samples 5
```
