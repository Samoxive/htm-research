#include <iostream>
#include <htmxlintrin.h>
#include <mutex>
#include <thread>

inline uint32_t crc32(const void* data, size_t length, uint32_t previousCrc32)
{
    uint32_t crc = ~previousCrc32; // same as previousCrc32 ^ 0xFFFFFFFF
    const uint8_t* current = (const uint8_t*) data;
    while (length-- != 0)
    {
        uint8_t s = uint8_t(crc) ^ *current++;
        uint32_t low = (s ^ (s << 6)) & 0xFF;
        uint32_t a   = (low * ((1 << 23) + (1 << 14) + (1 << 2)));
        crc = (crc >> 8) ^
              (low * ((1 << 24) + (1 << 16) + (1 << 8))) ^
              a ^
              (a >> 1) ^
              (low * ((1 << 20) + (1 << 12))) ^
              (low << 19) ^
              (low << 17) ^
              (low >>  2);
        }
    return ~crc;
}

#define MAX_TRANSACTION_RETRY_COUNT 10
#define BUBBLE_SORT_ARRAY_SIZE 32

inline void bubble_sort(int *data, int length) {
    for (int i = 0; i < length - 1; i++) {
        for (int k = 0; k < length - i - 1; k++) {
            if (data[k] > data[k + 1]) {
                int a = data[k];
                data[k] = data[k + 1];
                data[k + 1] = a;
            }
        }
    }
}

struct SortData {
    __attribute__((aligned(128))) int numbers[BUBBLE_SORT_ARRAY_SIZE] = { 0 };
    char pad[128 - (sizeof(int) * BUBBLE_SORT_ARRAY_SIZE)];
};

struct SharedData {
    __attribute__((aligned(128))) uint32_t hash;
    bool is_successful;
    bool trailer_done;
    char pad[128 - (sizeof(uint32_t) + (sizeof(bool) * 2))];
};

struct SharedMutex {
    __attribute__((aligned(128))) std::mutex header_mutex;
    std::mutex trailer_mutex;
    char pad[128 - (sizeof(std::mutex) * 2)] = { 0 };
};

void run(SharedData &data, SharedMutex &mutex, SortData &sort_data) {
    bool is_header = false;
    if (mutex.header_mutex.try_lock()) {
        is_header = true;
    }

    int try_count = MAX_TRANSACTION_RETRY_COUNT;
    TM_buff_type tm_buffer = { 0 };
    std::this_thread::sleep_for(std::chrono::seconds(1));
    while (true) {
        if (__TM_begin(tm_buffer) == _HTM_TBEGIN_STARTED) {
            bubble_sort(sort_data.numbers, BUBBLE_SORT_ARRAY_SIZE);
            int *arr_ptr = sort_data.numbers;
            uint32_t crc32hash = 0;
            for (int i = 0; i < BUBBLE_SORT_ARRAY_SIZE; i++) {
                crc32hash = crc32(arr_ptr, sizeof(int), crc32hash);
                crc32hash = crc32(&arr_ptr, sizeof(int*), crc32hash);
                arr_ptr++;
            }

            __TM_suspend();
            if (is_header) {
                data.hash = crc32hash;
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
                data.is_successful = crc32hash == data.hash;
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
    SortData sort_data;
    data.hash = 0;
    data.is_successful = false;
    data.trailer_done = false;
    srand(time(0));
    for (int i = 0; i < BUBBLE_SORT_ARRAY_SIZE; i++) {
        sort_data.numbers[i] = (rand() % 100);
        std::cout << sort_data.numbers[i] << " ";
    }
    std::cout << std::endl;

    std::thread thread1(run, std::ref(data), std::ref(mutex), std::ref(sort_data)),
            thread2(run, std::ref(data), std::ref(mutex), std::ref(sort_data));
    thread1.join();
    thread2.join();


    std::cout << "Is successful? " << data.is_successful << std::endl;
    std::cout << "Is trailer done? " << data.trailer_done << std::endl;
    std::cout << "Hash: " << data.hash << std::endl;
    for (int i = 0; i < BUBBLE_SORT_ARRAY_SIZE; i++) {
        std::cout << sort_data.numbers[i] << " ";
    }
    std::cout << std::endl;

    return 0;
}
