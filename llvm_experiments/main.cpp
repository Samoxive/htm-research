#include <iostream>
#include "tm.h"

int main() {
    int x = 0;
    __start_store_instrumentation();
    if (!__start_transaction()) {
        x = 32;
    }
    __end_store_instrumentation();
    int hash = __get_instrumentation_hash();
    return hash;
}