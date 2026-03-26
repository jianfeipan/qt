```
#include <iostream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <chrono>

template <typename T>
class ThreadSafeQueue {
private:
    mutable std::mutex mtx;
    std::queue<T> data_queue;
    std::condition_variable cv;

public:
    ThreadSafeQueue() = default;
    ThreadSafeQueue(const ThreadSafeQueue& other) = delete; // 禁止拷贝
    ThreadSafeQueue& operator=(const ThreadSafeQueue& other) = delete;

    // 入队：支持右值引用以优化性能
    void push(T new_value) {
        {
            std::lock_guard<std::mutex> lk(mtx);
            data_queue.push(std::move(new_value));
        }
        cv.notify_one();
    }

    // 阻塞式弹出：如果队列为空则无限等待
    void wait_and_pop(T& value) {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait(lk, [this] { return !data_queue.empty(); });
        value = std::move(data_queue.front());
        data_queue.pop();
    }

    // 阻塞式弹出：返回智能指针，可以避免某些情况下的数据拷贝
    std::shared_ptr<T> wait_and_pop() {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait(lk, [this] { return !data_queue.empty(); });
        std::shared_ptr<T> res(std::make_shared<T>(std::move(data_queue.front())));
        data_queue.pop();
        return res;
    }

    // 非阻塞式弹出：尝试获取，失败立即返回 false
    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lk(mtx);
        if (data_queue.empty()) return false;
        value = std::move(data_queue.front());
        data_queue.pop();
        return true;
    }

    // 检查是否为空
    bool empty() const {
        std::lock_guard<std::mutex> lk(mtx);
        return data_queue.empty();
    }
};

```
// --- 使用示例：生产者-消费者模型 ---
#include <thread>
#include <vector>

int main() {
    ThreadSafeQueue<int> q;

    // 生产者线程
    std::thread producer([&q]() {
        for (int i = 0; i < 5; ++i) {
            std::cout << "[Producer] Pushing: " << i << std::endl;
            q.push(i);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    // 消费者线程
    std::thread consumer([&q]() {
        for (int i = 0; i < 5; ++i) {
            int val;
            q.wait_and_pop(val);
            std::cout << "[Consumer] Popped: " << val << std::endl;
        }
    });

    producer.join();
    consumer.join();

    return 0;
}
```
# mutable
`mutable` Key word: 
a const member function promises not to change any of the object's data members. However, there are cases where you need to change "internal" state that doesn't affect the "external" (logical) state of the object.

