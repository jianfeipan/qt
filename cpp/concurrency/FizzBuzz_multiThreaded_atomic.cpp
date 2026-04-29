#include <iostream>
#include <thread>
#include <atomic>
#include <functional>

/**
 * C++20 Atomic Implementation of Multithreaded FizzBuzz
 * * This version uses std::atomic<int>::wait() and notify_all(), which is
 * more lightweight than a mutex/condition_variable pair but provides
 * similar "blocking" behavior to prevent high CPU usage (spinning).
 */
class MultithreadedFizzBuzz {
private:
    int n;
    std::atomic<int> current_num; // Atomic variable for synchronization

public:
    MultithreadedFizzBuzz(int n) : n(n), current_num(1) {}

    // Generic worker function to reduce code duplication
    // logic_check: boolean function to determine if this thread should act
    // printer: function to print the specific string/number
    void run_worker(std::function<bool(int)> logic_check, std::function<void(int)> printer) {
        while (true) {
            int expected = current_num.load();

            // Wait until the condition is met OR we exceed n
            // .wait() blocks the thread if current_num == expected
            while (expected <= n && !logic_check(expected)) {
                current_num.wait(expected);
                expected = current_num.load();
            }

            if (expected > n) return;

            // Perform the output
            printer(expected);

            // Increment and notify all waiting threads
            current_num.fetch_add(1);
            current_num.notify_all();
        }
    }

    void fizz() {
        run_worker(
            [](int i) { return i % 3 == 0 && i % 5 != 0; },
            [](int i) { std::cout << "Fizz\n"; }
        );
    }

    void buzz() {
        run_worker(
            [](int i) { return i % 5 == 0 && i % 3 != 0; },
            [](int i) { std::cout << "Buzz\n"; }
        );
    }

    void fizzbuzz() {
        run_worker(
            [](int i) { return i % 15 == 0; },
            [](int i) { std::cout << "FizzBuzz\n"; }
        );
    }

    void number() {
        run_worker(
            [](int i) { return i % 3 != 0 && i % 5 != 0; },
            [](int i) { std::cout << i << "\n"; }
        );
    }
};

int main() {
    int max_number = 20;
    
    std::cout << "Starting C++20 Atomic FizzBuzz up to " << max_number << "...\n\n";
    
    MultithreadedFizzBuzz fb(max_number);

    // Launch threads
    std::thread t1(&MultithreadedFizzBuzz::fizz, &fb);
    std::thread t2(&MultithreadedFizzBuzz::buzz, &fb);
    std::thread t3(&MultithreadedFizzBuzz::fizzbuzz, &fb);
    std::thread t4(&MultithreadedFizzBuzz::number, &fb);

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    std::cout << "\nDone!\n";
    return 0;
}