## LLVM Pass to instrument binaries to catch store instructions

How to build:

```bash
mkdir build
cd build
cmake ..
make
clang++ -Xclang -load -Xclang ./libllvm_experiments.so ./file_to_compile.cpp -o a.out
```

If you are in a machine without global LLVM installation, you can use the given makefile, `make llvm=/path/to/llvm` and you will have `libllvm_experiments.so` compiled.

For ubuntu, llvm is located in `/usr/lib/llvm-*`.

How to use:

To use instrumentation, you must include `tm.h` and call `__start_store_instrumentation` to start recording store instructions and call `__end_store_instrumentation` to stop recording. You can use `__get_hash` at any moment after starting the instrumentation and it will return an integer value that had stored values accumulated via crc32 hashing.