#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

/**
 * 1. Meyers' Singleton (C++11 及以后最推荐的做法)
 * 这种写法利用了 C++11 标准：如果多个线程试图同时初始化同一个静态局部变量，
 * 初始化行为只会在一个线程中发生，其他线程会等待直到初始化完成。
 */
class MeyersSingleton {
public:
    // 获取实例的静态方法
    static MeyersSingleton& getInstance() {
        // C++11 保证了这里是线程安全的
        static MeyersSingleton instance;
        return instance;
    }

    void doSomething() {
        std::cout << "Meyers' Singleton 正在运行，线程 ID: " << std::this_thread::get_id() << std::endl;
    }

    // 禁用拷贝构造和赋值操作
    MeyersSingleton(const MeyersSingleton&) = delete;
    MeyersSingleton& operator=(const MeyersSingleton&) = delete;

private:
    MeyersSingleton() {
        std::cout << "Meyers' Singleton 构造函数被调用" << std::endl;
    }
    ~MeyersSingleton() {
        std::cout << "Meyers' Singleton 析构函数被调用" << std::endl;
    }
};

/**
 * 2. 双检锁单例 (Double-Checked Locking Pattern)
 * 在 C++11 之前常用的方法，或者在某些需要手动控制生命周期的复杂场景中使用。
 * 注意：必须配合 std::atomic 使用以防止指令重排。
 */
class DCLPSingleton {
public:
    static DCLPSingleton* getInstance() {
        DCLPSingleton* tmp = instance.load(std::memory_order_acquire);
        if (tmp == nullptr) { // 第一次检查
            std::lock_guard<std::mutex> lock(mtx);
            tmp = instance.load(std::memory_order_relaxed);
            if (tmp == nullptr) { // 第二次检查
                tmp = new DCLPSingleton();
                instance.store(tmp, std::memory_order_release);
            }
        }
        return tmp;
    }

    void doSomething() {
        std::cout << "DCLP Singleton 正在运行，线程 ID: " << std::this_thread::get_id() << std::endl;
    }

    DCLPSingleton(const DCLPSingleton&) = delete;
    DCLPSingleton& operator=(const DCLPSingleton&) = delete;

private:
    DCLPSingleton() {
        std::cout << "DCLP Singleton 构造函数被调用" << std::endl;
    }
    
    static std::atomic<DCLPSingleton*> instance;
    static std::mutex mtx;
};

// 初始化静态成员
std::atomic<DCLPSingleton*> DCLPSingleton::instance{nullptr};
std::mutex DCLPSingleton::mtx;

// --- 测试代码 ---

void testMeyers() {
    MeyersSingleton::getInstance().doSomething();
}

void testDCLP() {
    DCLPSingleton::getInstance()->doSomething();
}

int main() {
    std::cout << "--- 开始多线程测试 ---" << std::endl;

    std::vector<std::thread> threads;
    
    // 启动多个线程同时访问单例
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back(testMeyers);
        threads.emplace_back(testDCLP);
    }

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "--- 测试结束 ---" << std::endl;
    return 0;
}