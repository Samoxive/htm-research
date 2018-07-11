#include <iostream>
#include <htmxlintrin.h>
#include <mutex>
#include <thread>

#define MAX_TRANSACTION_RETRY_COUNT 10

void run() {
    int big_buffer[256];
    int try_count = MAX_TRANSACTION_RETRY_COUNT;
    TM_buff_type tm_buffer = { 0 };
    while (true) {
        if (__TM_begin(tm_buffer) == _HTM_TBEGIN_STARTED) {
            // do stuff
            __TM_end();
            break;
        } else {
            // a failure happened
            if (try_count-- <= 0 || __TM_is_failure_persistent(tm_buffer)) {
                // transaction failed for sure or we retried too much
                if (__TM_is_failure_persistent(tm_buffer)) {
                    std::cout << "Persistent fault" << std::endl;
                } else if (__TM_is_conflict(tm_buffer)) {
                    std::cout << "Conflict fault" << std::endl;
                } else if (__TM_is_footprint_exceeded(tm_buffer)) {
                    std::cout << "Footprint exceeded" << std::endl;
                } else if (__TM_is_nested_too_deep(tm_buffer)) {
                    std::cout << "Transaction nested too deep" << std::endl;
                } else if (__TM_is_illegal(tm_buffer)) {
                    std::cout << "Transaction is illegal" << std::endl;
                } else if (__TM_is_user_abort(tm_buffer)) {
                    std::cout << "User abort" << std::endl;
                } else {
                    std::cout << "Unknown transaction abort reason" << std::endl;
                }
            }
            break;
        }
    }

    std::cout << "Retried " << (MAX_TRANSACTION_RETRY_COUNT - try_count) << " times" << std::endl;
}

int main(int argc, char **argv) {
    std::thread thread(run);
    thread.join();

    return 0;
}