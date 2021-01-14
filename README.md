Build and Run

```
python script.py init --clang --build-type Release
python script.py run
```

Test

```
python script.py run --e main_test -- -s
```

Benchmark

```
python script.py run --e main_bench -- --benchmark-samples 5
```
