#include <iostream>
#include <htmxlintrin.h>
#include <mutex>
#include <thread>

#define MAX_TRANSACTION_RETRY_COUNT 10

struct SharedData {
    __attribute__((aligned(128))) int value;
    bool is_successful;
    bool trailer_done;
    char pad[128 - (sizeof(int) + (sizeof(bool) * 2))] = { 0 };
};

struct SharedMutex {
    __attribute__((aligned(128))) std::mutex header_mutex;
    std::mutex trailer_mutex;
    char pad[128 - (sizeof(std::mutex) * 2)] = { 0 };
};

void run(SharedData &data, SharedMutex &mutex, int *number_ptr) {
    bool is_header = false;
    if (mutex.header_mutex.try_lock()) {
        is_header = true;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    int try_count = MAX_TRANSACTION_RETRY_COUNT;
    TM_buff_type tm_buffer = { 0 };
    while (true) {
        if (__TM_begin(tm_buffer) == _HTM_TBEGIN_STARTED) {
            (*number_ptr) = (*number_ptr) + 25;

            __TM_suspend();
            if (is_header) {
                data.value = (*number_ptr);
                mutex.header_mutex.unlock();

                while (true) {
                    mutex.trailer_mutex.lock();
                    if (data.trailer_done) {
                        mutex.trailer_mutex.unlock();
                        break;
                    }
                    mutex.trailer_mutex.unlock();
                }

                if (data.is_successful) {
                    __TM_resume();
                    __TM_end();
                } else {
                    __TM_resume();
                    __TM_abort();
                }
            } else {
                mutex.header_mutex.lock();
                mutex.header_mutex.unlock();

                mutex.trailer_mutex.lock();
                data.is_successful = (*number_ptr) == (data.value);
                data.trailer_done = true;
                mutex.trailer_mutex.unlock();
                __TM_abort();
            }

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

                break;
            }
        }
    }

    std::cout << "Retried " << (MAX_TRANSACTION_RETRY_COUNT - try_count) << " times" << std::endl;
    std::cout << "I am header? " << is_header << std::endl;
}

int main(int argc, char **argv) {
    SharedData data;
    SharedMutex mutex;
    data.is_successful = false;
    data.trailer_done = false;
    data.value = -1;
    int number = 0;
    std::thread thread1(run, std::ref(data), std::ref(mutex), &number),
                thread2(run, std::ref(data), std::ref(mutex), &number);
    thread1.join();
    thread2.join();

    std::cout << "Is successful? " << data.is_successful << std::endl;
    std::cout << "Is trailer done? " << data.trailer_done << std::endl;
    std::cout << "Commonly found value: " << data.value << std::endl;
    std::cout << "Final number: " << number << std::endl;

    return 0;
}