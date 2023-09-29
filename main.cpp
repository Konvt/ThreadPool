#include <iostream>
#include <chrono>
#include <mutex>
#include "MultiplePool.hpp"

std::mutex mtx;

void task(int id) {
    {
        std::lock_guard<std::mutex> lock {mtx};
        std::cout << "Task " << id << " started" << std::endl;
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
    {
        std::lock_guard<std::mutex> lock {mtx};
        std::cout << "Task " << id << " completed" << std::endl;
    }
}

int main() {
    Gadgetry::MultiplePool thread_pool;

    for (int i=1; i<=10; ++i) {
        thread_pool.submit([i]() {
            task(i);
        });
    }

    thread_pool.shutdown();
    std::cout << "All tasks completed" << std::endl;

    return 0;
}