#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

class MultithreadedFizzBuzz {
private:
    int n;               // The maximum number to count up to
    int current_num;     // The current number being evaluated
    std::mutex mtx;      // Mutex to protect shared state (current_num)
    std::condition_variable cv; // Condition variable to coordinate threads

public:
    // Constructor initializes the target 'n' and sets the starting number to 1
    MultithreadedFizzBuzz(int n) : n(n), current_num(1) {}

    // Thread function to print "Fizz" (Divisible by 3, but not 5)
    void fizz() {
        while (true) {
            std::unique_lock<std::mutex> lock(mtx);
            
            // Wait until it's time to process this condition, or we've exceeded 'n'
            cv.wait(lock, [this]() { 
                return current_num > n || (current_num % 3 == 0 && current_num % 5 != 0); 
            });
            
            // If we have reached the limit, exit the thread
            if (current_num > n) return;
            
            std::cout << "Fizz\n";
            current_num++;
            
            // Wake up all other threads to check their conditions
            cv.notify_all();
        }
    }

    // Thread function to print "Buzz" (Divisible by 5, but not 3)
    void buzz() {
        while (true) {
            std::unique_lock<std::mutex> lock(mtx);
            
            cv.wait(lock, [this]() { 
                return current_num > n || (current_num % 3 != 0 && current_num % 5 == 0); 
            });
            
            if (current_num > n) return;
            
            std::cout << "Buzz\n";
            current_num++;
            
            cv.notify_all();
        }
    }

    // Thread function to print "FizzBuzz" (Divisible by both 3 and 5)
    void fizzbuzz() {
        while (true) {
            std::unique_lock<std::mutex> lock(mtx);
            
            cv.wait(lock, [this]() { 
                return current_num > n || (current_num % 15 == 0); 
            });
            
            if (current_num > n) return;
            
            std::cout << "FizzBuzz\n";
            current_num++;
            
            cv.notify_all();
        }
    }

    // Thread function to print the number (Not divisible by 3 or 5)
    void number() {
        while (true) {
            std::unique_lock<std::mutex> lock(mtx);
            
            cv.wait(lock, [this]() { 
                return current_num > n || (current_num % 3 != 0 && current_num % 5 != 0); 
            });
            
            if (current_num > n) return;
            
            std::cout << current_num << "\n";
            current_num++;
            
            cv.notify_all();
        }
    }
};

int main() {
    int max_number = 20; // Let's run FizzBuzz up to 20
    
    std::cout << "Starting Multithreaded FizzBuzz up to " << max_number << "...\n\n";
    
    // Create the shared FizzBuzz instance
    MultithreadedFizzBuzz fizzBuzz(max_number);

    // Launch the four concurrent threads
    std::thread t1(&MultithreadedFizzBuzz::fizz, &fizzBuzz);
    std::thread t2(&MultithreadedFizzBuzz::buzz, &fizzBuzz);
    std::thread t3(&MultithreadedFizzBuzz::fizzbuzz, &fizzBuzz);
    std::thread t4(&MultithreadedFizzBuzz::number, &fizzBuzz);

    // Wait for all threads to finish execution
    t1.join();
    t2.join();
    t3.join();
    t4.join();

    std::cout << "\nDone!\n";
    return 0;
}