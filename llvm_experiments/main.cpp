#include <iostream>
#include "tm.h"

int main() {
    int x = 0, hash = 0, *ptr = nullptr;
    __start_transaction();
    hash = 16;
    x = 32;
    ptr = &x;
    __end_transaction();
    std::cout << "Expected hash: " << __crc32(__crc32(__crc32(0, (long) hash), (long) x), (long) ptr) << std::endl;
    std::cout << "Ptr: " << ptr << " Sum: " << ((long) ptr + (long) x + (long) hash) << " Hash: " << __get_hash();
    return x;
}