#include <iostream>

inline int begin_transaction(int *ptr) {
    begin:
    asm("tbegin. 0");
    asm goto( "beq %l[fail]" : /* no outputs */ : /* inputs */ : "memory" /* clobbers */ : fail /* any labels used */ );
    *ptr = 50;
    asm("tsuspend.");
    asm("tresume.");
    asm("tend.");
    return 0;
    fail:
    return 1;
}

int main() {
    int a = 0;
    std::cout << "Transaction Status: " << begin_transaction(&a) << std::endl << "Result: " << a << std::endl;
}