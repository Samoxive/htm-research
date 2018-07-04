#include <htmintrin.h>
#include <htmxlintrin.h>
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>

bool is_successful = false;
bool trailer_done = false;
int value = -1;
std::mutex header_mutex;
std::mutex trailer_mutex;

void thread1(int *ptr) {
    bool is_header = false;
    if (header_mutex.try_lock()) {
        is_header = true;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    int tries = 10;
    while (!__builtin_tbegin(0) && tries > 0) {
        tries--;
    }
    
    if (tries <= 0) {
        std::cout << "Failed to create transaction." << std::endl;
        return;
    }

    (*ptr) = (*ptr) + 25;

    __builtin_tsuspend();

    if (is_header) {
        value = (*ptr);
        header_mutex.unlock();

        while (true) {
            trailer_mutex.lock();
            if (trailer_done) {
                trailer_mutex.unlock();
                break;
            }
            trailer_mutex.unlock();
        }

        if (is_successful) {
            __builtin_tresume();
            __builtin_tend(0);
        } else {
            __builtin_tresume();
            __builtin_tabort(0);
        }
    } else {
        header_mutex.lock();
        header_mutex.unlock();

        trailer_mutex.lock();
        is_successful = (*ptr) == value;
        trailer_done = true;
        trailer_mutex.unlock();
        __builtin_tabort(0);
    }
}

int main() {
    int number = 0;
    std::thread t1(thread1, &number), t2(thread1, &number);
    t1.join();
    t2.join();

    std::cout << number << " " << value << " " << is_successful << std::endl;
    return 0;
}
