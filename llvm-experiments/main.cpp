#include <iostream>
#include "tm.h"

int main() {
    int x = 0, hash = 0;
    __start_transaction();
    x = 32;
    __end_transaction();
    return x;
}