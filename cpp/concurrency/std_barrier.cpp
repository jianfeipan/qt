#include <iostream>
#include <thread>
#include <vector>
#include <barrier>

void task(int id, std::barrier<>& sync_point) {
    std::cout << "Thread " << id << " reached the barrier.\n";
    
    // 线程在此阻塞，直到 n 个线程都调用了 arrive_and_wait
    sync_point.arrive_and_wait();
    
    /*
    #include <barrier>

    // 定义一个 barrier，设定参与线程数为 n
    // 也可以传入一个 Lambda 作为阶段完成时的回调
    std::barrier sync_point(n, []() noexcept { 
        // 这部分代码由最后一个到达的线程执行一次
        std::cout << "所有线程已到达，准备进入下一阶段...\n"; 
    });
    */

    std::cout << "Thread " << id << " passed the barrier!\n";
}

int main() {
    const int n = 5;
    // 初始化屏障，参与者数量为 n
    std::barrier sync_point(n);
    std::vector<std::thread> threads;

    for (int i = 0; i < n; ++i) {
        threads.emplace_back(task, i, std::ref(sync_point));
    }

    for (auto& t : threads) {
        t.join();
    }

    return 0;
}


//Mutex:
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <thread>

class Barrier {
public:
    explicit Barrier(std::size_t count) : threshold(count), count(count), generation(0) {}

    void wait() {
        std::unique_lock<std::mutex> lock(mtx);
        auto gen = generation;

        if (--count == 0) {
            // 最后一个到达的线程进入这里
            generation++;    // 进入下一代
            count = threshold; // 重置计数器
            cv.notify_all();   // 唤醒所有线程
        } else {
            // 未满 n 个线程，持续等待，直到 generation 改变
            cv.wait(lock, [this, gen] { return gen != generation; });
        }
    }

private:
    std::mutex mtx;
    std::condition_variable cv;
    std::size_t threshold;  // 总线程数
    std::size_t count;      // 剩余等待数
    std::size_t generation; // 代次，用于区分不同的同步轮次
};

// 使用示例
void worker(int id, Barrier& barrier) {
    std::cout << "Thread " << id << " working...\n";
    barrier.wait();
    std::cout << "Thread " << id << " moving to next phase.\n";
}

int main() {
    Barrier b(3);
    std::vector<std::thread> threads;
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back(worker, i, std::ref(b));
    }
    for (auto& t : threads) t.join();
    return 0;
}

// atomic:
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <string>

/**
 * C++20 Atomic 实现的同步屏障 (Synchronization Barrier)
 * 作用：让 n 个线程在屏障点汇合，直到所有线程到达后再一起继续。
 */
class AtomicBarrier {
private:
    const int total_threads;
    std::atomic<int> arrived_count; // 到达屏障的线程数
    std::atomic<int> generation;    // 屏障周期计数，用于区分不同的汇合点

public:
    explicit AtomicBarrier(int count) 
        : total_threads(count), arrived_count(0), generation(0) {}

    void arrive_and_wait() {
        // 记录当前所在的周期
        int current_gen = generation.load();
        
        // 增加到达数并检查是否是最后一个
        if (arrived_count.fetch_add(1) + 1 == total_threads) {
            // 最后一个到达的线程：重置计数并推进周期
            arrived_count.store(0);
            generation.fetch_add(1);
            // 唤醒所有在旧周期等待的线程
            generation.notify_all();
        } else {
            // 非最后到达的线程：在当前周期上等待
            // 只要 generation 还是 current_gen，线程就会阻塞
            while (generation.load() == current_gen) {
                generation.wait(current_gen);
            }
        }
    }
};

void thread_task(int id, AtomicBarrier& barrier) {
    // 第一阶段工作
    std::cout << "线程 " << id << " 正在执行第一阶段任务...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100 * (id + 1))); 

    std::cout << "线程 " << id << " 到达屏障点，等待他人...\n";
    barrier.arrive_and_wait();

    // 所有线程汇合后执行第二阶段
    std::cout << "线程 " << id << " 已通过屏障，开始第二阶段工作！\n";
}

int main() {
    const int num_threads = 5;
    AtomicBarrier barrier(num_threads);
    std::vector<std::thread> threads;

    std::cout << "启动同步屏障示例，总计线程数: " << num_threads << "\n\n";

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(thread_task, i, std::ref(barrier));
    }

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "\n所有线程任务已完成。\n";
    return 0;
}