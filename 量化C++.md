量化交易（Quant Trading/HFT）面试对 C++ 的要求极高，核心逻辑只有四个字：**极致性能**。他们不在乎你能不能写出业务逻辑，他们在乎的是你的代码在 CPU 上执行时，能不能比对手快那几纳秒。

为了帮你系统梳理，我将这些知识点拆解为四个核心维度：

---

## 一、 C++ 语言底层与现代特性

在量化领域，C++11 及之后的特性不是为了“好用”，而是为了“零开销抽象”。

### 1. 移动语义与完美转发

* **std::move**: 核心是**右值引用**。它并不移动任何东西，只是将一个左值强制转换为右值，从而触发“移动构造函数”，避免深拷贝。
* **完美转发 (std::forward)**: 配合模板中的“万能引用”（Universal Reference），确保参数在传递过程中保持其原始的左右值属性，这在编写高性能插件化框架时至关重要。
```cpp
// 移动语义示例
class Order {
    std::vector<double> prices;  // 大数据
public:
    // 移动构造函数 - 窃取资源而非拷贝
    Order(Order&& other) noexcept
        : prices(std::move(other.prices)) {}

    // 移动赋值运算符
    Order& operator=(Order&& other) noexcept {
        prices = std::move(other.prices);
        return *this;
    }
};

// 完美转发示例 - 通用工厂函数
template<typename T, typename... Args>
std::unique_ptr<T> make_order(Args&&... args) {
    // std::forward 保持参数的左右值属性
    return std::make_unique<T>(std::forward<Args>(args)...);
}

// 使用场景
void process_order() {
    Order temp_order;
    Order final_order = std::move(temp_order);  // 避免深拷贝，直接窃取资源

    // 完美转发创建对象
    auto order_ptr = make_order<Order>(/* 构造参数 */);
}
```
### 2. 编译时优化 (constexpr & 模板元编程)

* **constexpr**: 将计算从**运行期推迟到编译期**。量化系统经常需要预计算各种因子或查找表，使用 `constexpr` 可以让程序启动时就带上结果，消除运行开销。
* **模板元编程 (TMP)**: 实现静态多态。相比虚函数，模板在编译时确定类型，方便编译器进行内联优化，彻底消除虚函数表查询的开销。

```cpp
// constexpr 编译时计算
constexpr double calculate_fee(double price, double rate) {
    return price * rate;
}

constexpr double TRADING_FEE = calculate_fee(100.0, 0.001);  // 编译时计算完成

// constexpr 查找表
constexpr std::array<int, 256> generate_lookup_table() {
    std::array<int, 256> table{};
    for (int i = 0; i < 256; ++i) {
        table[i] = i * i;  // 预计算平方值
    }
    return table;
}

constexpr auto SQUARE_TABLE = generate_lookup_table();

// 模板元编程 - 静态多态（无虚函数开销）
template<typename Strategy>
class StrategyExecutor {
public:
    void execute(const MarketData& data) {
        // 编译时确定具体类型，可以内联
        static_cast<Strategy*>(this)->on_data(data);
    }
};

class MomentumStrategy : public StrategyExecutor<MomentumStrategy> {
public:
    void on_data(const MarketData& data) {
        // 具体策略逻辑
    }
};

// 编译时 if (C++17)
template<typename T>
void process(T value) {
    if constexpr (std::is_integral_v<T>) {
        // 整数处理路径 - 未使用的分支不会编译
    } else if constexpr (std::is_floating_point_v<T>) {
        // 浮点数处理路径
    }
}
```

### 3. 编译器优化与代码生成

* **分支预测提示**: 使用 `__builtin_expect` (GCC/Clang) 或 `[[likely]]`/`[[unlikely]]` (C++20)。在价格判断、订单验证等高频分支中，告诉 CPU 哪个分支更可能执行，减少分支预测失败。
* **强制内联**: `__attribute__((always_inline))` 或 `__forceinline`。关键路径上消除函数调用开销，尤其是那些被频繁调用但编译器没有自动内联的小函数。
* **链接时优化 (LTO)**: `-flto` 标志。跨编译单元的内联和死代码消除，让编译器看到整个程序的上下文。
* **PGO (Profile-Guided Optimization)**: 用真实交易数据训练编译器，让它知道哪些路径是热路径，从而做出更精准的优化决策。
* **禁用异常/RTTI**: `-fno-exceptions` 和 `-fno-rtti`。异常展开和动态类型识别在延迟敏感路径不可接受，许多 HFT 系统完全禁用这些特性。

```cpp
// 分支预测提示
bool validate_order(const Order& order) {
    // C++20 方式
    if (order.price > 0) [[likely]] {
        return true;
    } else [[unlikely]] {
        log_error("Invalid price");
        return false;
    }
}

// GCC/Clang 方式
bool fast_path_check(int value) {
    if (__builtin_expect(value > 0, 1)) {  // 1 表示条件很可能为真
        // 热路径
        return true;
    } else {
        // 冷路径
        return false;
    }
}

// 强制内联
__attribute__((always_inline))
inline double calculate_pnl(double entry, double exit, int quantity) {
    return (exit - entry) * quantity;
}

// 或使用跨平台宏
#ifdef _MSC_VER
    #define FORCE_INLINE __forceinline
#else
    #define FORCE_INLINE __attribute__((always_inline)) inline
#endif

FORCE_INLINE double tick_to_price(int tick, double tick_size) {
    return tick * tick_size;
}

// 编译选项示例 (CMakeLists.txt)
/*
# 链接时优化
add_compile_options(-flto)
add_link_options(-flto)

# 禁用异常和RTTI
add_compile_options(-fno-exceptions -fno-rtti)

# PGO 两步构建
# 步骤1: 生成 profile 数据
add_compile_options(-fprofile-generate)
# 运行程序收集数据
# 步骤2: 使用 profile 优化
add_compile_options(-fprofile-use)
*/
```

### 4. 内存布局与缓存优化

* **虚函数表 (vtable)**: 每次调用虚函数都有一次间接寻址开销，且**无法内联**。在极低延迟路径（Hot Path）上，通常禁掉虚函数。
* **内存对齐 (Memory Alignment)**: 现代 CPU 一次读取 64 字节（Cache Line）。如果数据跨越了两个 Cache Line，需要两次内存访问。
* **伪共享 (False Sharing)**: 两个线程修改同一个 Cache Line 里的不同变量，会导致该 Line 在不同核心间反复失效。解决方法是使用 `alignas(64)` 将变量隔开。
* **Cache Line Padding**: 不只是避免伪共享，还要让热数据紧凑排列在同一个 Cache Line 内，提高空间局部性。
* **SoA vs AoS**: 处理 Order Book 时，是用 `struct Order[]`（Array of Structures）还是 `{price[], qty[], time[]}`（Structure of Arrays）？SoA 对 SIMD 和缓存更友好。
* **数据预取** (`__builtin_prefetch`): 在处理下一批订单前，提前把数据加载进 L1 Cache，隐藏内存延迟。

```cpp
// 避免伪共享 - Cache Line 对齐
struct alignas(64) ThreadData {
    std::atomic<int> counter;
    char padding[60];  // 填充到 64 字节
};

ThreadData thread_data[4];  // 每个线程独占一个 Cache Line

// 更优雅的 padding 方式
struct alignas(std::hardware_destructive_interference_size) AtomicCounter {
    std::atomic<uint64_t> value;
};

// SoA vs AoS 对比
// AoS - Array of Structures (缓存不友好)
struct Order {
    double price;
    int quantity;
    uint64_t timestamp;
};
std::vector<Order> orders_aos;

// SoA - Structure of Arrays (SIMD 友好)
struct OrderBook {
    std::vector<double> prices;
    std::vector<int> quantities;
    std::vector<uint64_t> timestamps;
};
OrderBook orders_soa;

// 数据预取示例
void process_orders(const Order* orders, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        // 预取下一个订单（提前3个）
        if (i + 3 < count) {
            __builtin_prefetch(&orders[i + 3], 0, 3);  // 0=读, 3=高局部性
        }
        // 处理当前订单
        process_single_order(orders[i]);
    }
}

// 热数据集中在同一结构体（提高空间局部性）
struct HotOrderData {
    double price;      // 8 bytes
    int quantity;      // 4 bytes
    uint32_t flags;    // 4 bytes
    // 总计 16 字节，紧凑排列
};

// 冷数据分离
struct ColdOrderData {
    std::string client_id;
    std::string notes;
    // 不常访问的数据
};

// 避免虚函数的替代方案 - 函数指针表
struct Strategy {
    void (*on_tick)(void*, double);
    void (*on_trade)(void*, int);
    void* context;
};

// 或使用 std::function（稍有开销但更灵活）
struct FastStrategy {
    std::function<void(double)> on_tick;
};
```

### 5. SIMD 向量化计算

* **SSE/AVX/AVX512**: 单指令多数据。批量计算因子、技术指标时，一条 AVX512 指令可以同时处理 16 个 float 或 8 个 double。
* **自动向量化**: 如何写出能被编译器自动向量化的循环：连续内存访问、无数据依赖、循环边界已知。
* **内在函数 (Intrinsics)**: 当自动向量化不够时，使用 `_mm256_add_ps` 等内在函数手动编写 SIMD 代码。
* **典型应用**: 计算均值、标准差、相关性、技术指标时，SIMD 可以提供 4x-16x 的性能提升。

```cpp
#include <immintrin.h>  // AVX/AVX2/AVX512
#include <vector>
#include <numeric>

// 普通标量计算
float sum_scalar(const float* data, size_t n) {
    float sum = 0.0f;
    for (size_t i = 0; i < n; ++i) {
        sum += data[i];
    }
    return sum;
}

// AVX2 向量化 - 一次处理 8 个 float
float sum_avx2(const float* data, size_t n) {
    __m256 vec_sum = _mm256_setzero_ps();  // 初始化为0

    size_t i = 0;
    // 每次处理 8 个元素
    for (; i + 7 < n; i += 8) {
        __m256 vec = _mm256_loadu_ps(&data[i]);  // 加载8个float
        vec_sum = _mm256_add_ps(vec_sum, vec);    // 向量加法
    }

    // 水平求和：将8个lane的结果相加
    alignas(32) float temp[8];
    _mm256_store_ps(temp, vec_sum);
    float sum = temp[0] + temp[1] + temp[2] + temp[3] +
                temp[4] + temp[5] + temp[6] + temp[7];

    // 处理剩余元素
    for (; i < n; ++i) {
        sum += data[i];
    }

    return sum;
}

// 计算移动平均 (VWAP - 成交量加权平均价)
void calculate_vwap_avx2(const float* prices, const float* volumes,
                         float* results, size_t n) {
    for (size_t i = 0; i + 7 < n; i += 8) {
        __m256 p = _mm256_loadu_ps(&prices[i]);
        __m256 v = _mm256_loadu_ps(&volumes[i]);
        __m256 r = _mm256_mul_ps(p, v);  // 向量乘法: price * volume
        _mm256_storeu_ps(&results[i], r);
    }
}

// 自动向量化友好的代码写法
// 编译器可以自动向量化这个循环
void normalize_prices(float* prices, size_t n, float mean, float stddev) {
    // 使用 restrict 提示编译器数据不重叠
    for (size_t i = 0; i < n; ++i) {
        prices[i] = (prices[i] - mean) / stddev;
    }
}

// 编译时: g++ -O3 -mavx2 -ftree-vectorize

// SIMD 计算相关系数
float correlation_avx2(const float* x, const float* y, size_t n) {
    __m256 sum_x = _mm256_setzero_ps();
    __m256 sum_y = _mm256_setzero_ps();
    __m256 sum_xy = _mm256_setzero_ps();
    __m256 sum_xx = _mm256_setzero_ps();
    __m256 sum_yy = _mm256_setzero_ps();

    for (size_t i = 0; i + 7 < n; i += 8) {
        __m256 vx = _mm256_loadu_ps(&x[i]);
        __m256 vy = _mm256_loadu_ps(&y[i]);

        sum_x = _mm256_add_ps(sum_x, vx);
        sum_y = _mm256_add_ps(sum_y, vy);
        sum_xy = _mm256_add_ps(sum_xy, _mm256_mul_ps(vx, vy));
        sum_xx = _mm256_add_ps(sum_xx, _mm256_mul_ps(vx, vx));
        sum_yy = _mm256_add_ps(sum_yy, _mm256_mul_ps(vy, vy));
    }

    // 水平求和并计算相关系数
    // ... (省略最终计算代码)
    return 0.0f;  // placeholder
}
```

---

## 二、 并发、多线程与低延迟

### 1. 原子操作与内存屏障 (Memory Barrier)

* **std::atomic**: 比起互斥锁（Mutex），原子操作是硬件级别的同步。
* **内存模型 (Memory Order)**: 了解 `memory_order_relaxed`, `acquire`, `release`。量化面试常问：如何在不使用锁的情况下保证两个核心看到的数据是一致的？这涉及到指令重排的抑制。

```cpp
#include <atomic>
#include <thread>

// 原子操作基础
std::atomic<int> order_count{0};

void process_order() {
    order_count.fetch_add(1, std::memory_order_relaxed);  // 最快，不保证顺序
}

// 内存序详解
class SpinLock {
    std::atomic<bool> flag{false};
public:
    void lock() {
        // acquire: 确保后续读写不会被重排到这之前
        while (flag.exchange(true, std::memory_order_acquire)) {
            // 自旋等待
            while (flag.load(std::memory_order_relaxed)) {
                _mm_pause();  // CPU 提示：减少功耗
            }
        }
    }

    void unlock() {
        // release: 确保之前的写操作对其他线程可见
        flag.store(false, std::memory_order_release);
    }
};

// 生产者-消费者模式（无锁）
class LockFreeFlag {
    std::atomic<bool> data_ready{false};
    int data;

public:
    void produce(int value) {
        data = value;  // 普通写
        // release: 确保 data 的写入对消费者可见
        data_ready.store(true, std::memory_order_release);
    }

    bool consume(int& out) {
        // acquire: 确保能看到 produce 中 data 的写入
        if (data_ready.load(std::memory_order_acquire)) {
            out = data;
            return true;
        }
        return false;
    }
};

// Seq_cst - 最强保证（全局顺序一致）
std::atomic<int> x{0}, y{0};
std::atomic<int> r1{0}, r2{0};

void thread1() {
    x.store(1, std::memory_order_seq_cst);
    r1 = y.load(std::memory_order_seq_cst);
}

void thread2() {
    y.store(1, std::memory_order_seq_cst);
    r2 = x.load(std::memory_order_seq_cst);
}
// seq_cst 保证不可能同时 r1 == 0 && r2 == 0
```

### 2. 无锁编程 (Lock-free)

* **无锁队列 (SPSC/MPSC)**: 利用 CAS (Compare and Swap) 操作实现并行。在 HFT 架构中，数据在风控、交易、行情模块间传递，通常使用无锁环形缓冲区（Ring Buffer）。

```cpp
// SPSC 无锁队列 (Single Producer Single Consumer)
template<typename T, size_t SIZE>
class SPSCQueue {
    static_assert((SIZE & (SIZE - 1)) == 0, "Size must be power of 2");

    T buffer[SIZE];
    alignas(64) std::atomic<size_t> write_pos{0};
    alignas(64) std::atomic<size_t> read_pos{0};

public:
    bool try_push(const T& item) {
        size_t w = write_pos.load(std::memory_order_relaxed);
        size_t next_w = (w + 1) & (SIZE - 1);  // 环形

        if (next_w == read_pos.load(std::memory_order_acquire)) {
            return false;  // 队列满
        }

        buffer[w] = item;
        write_pos.store(next_w, std::memory_order_release);
        return true;
    }

    bool try_pop(T& item) {
        size_t r = read_pos.load(std::memory_order_relaxed);

        if (r == write_pos.load(std::memory_order_acquire)) {
            return false;  // 队列空
        }

        item = buffer[r];
        read_pos.store((r + 1) & (SIZE - 1), std::memory_order_release);
        return true;
    }
};

// 使用示例
SPSCQueue<Order, 1024> order_queue;

// 生产者线程
void market_data_thread() {
    Order order;
    while (running) {
        // 接收市场数据
        if (order_queue.try_push(order)) {
            // 成功
        } else {
            // 队列满，处理背压
        }
    }
}

// 消费者线程
void strategy_thread() {
    Order order;
    while (running) {
        if (order_queue.try_pop(order)) {
            // 处理订单
            process_order(order);
        }
    }
}

// 无锁栈 (Treiber Stack)
template<typename T>
class LockFreeStack {
    struct Node {
        T data;
        Node* next;
    };

    std::atomic<Node*> head{nullptr};

public:
    void push(const T& value) {
        Node* new_node = new Node{value, nullptr};
        Node* old_head = head.load(std::memory_order_relaxed);

        do {
            new_node->next = old_head;
        } while (!head.compare_exchange_weak(
            old_head, new_node,
            std::memory_order_release,
            std::memory_order_relaxed
        ));
    }

    bool pop(T& result) {
        Node* old_head = head.load(std::memory_order_relaxed);

        while (old_head) {
            if (head.compare_exchange_weak(
                old_head, old_head->next,
                std::memory_order_acquire,
                std::memory_order_relaxed
            )) {
                result = old_head->data;
                delete old_head;  // 注意：实际应用需要内存回收策略
                return true;
            }
        }
        return false;
    }
};
```

### 3. 系统级优化与 Kernel Bypass

* **上下文切换**: 进程/线程切换会导致寄存器保存、TLB 失效，延迟达到微秒级。
* **CPU 亲和性 (CPU Affinity)**: 将线程绑定到特定核心，防止迁移带来的 Cache 失效。使用 `pthread_setaffinity_np` 或 `taskset`。
* **Kernel Bypass**: 跳过内核协议栈：
  * **DPDK**: Intel 的用户态网络框架
  * **Solarflare Onload**: 加速 TCP/UDP
  * **Mellanox VMA**: RDMA 与传统 socket 结合
* **Huge Pages**: 使用 2MB/1GB 页面替代 4KB 页面，减少页表项，提高 TLB 命中率（减少 TLB Miss）。
* **Isolcpus**: 预留 CPU 核心，操作系统调度器不会往上放其他进程，避免干扰交易线程。
* **中断亲和性 (IRQ Affinity)**: 让网卡中断固定在某个核心，避免干扰关键交易线程。
* **NUMA 感知**: 内存访问延迟，跨 NUMA 节点访问慢 2-3 倍。使用 `numactl` 绑定内存分配。
* **关闭节能特性**: 禁用 C-States、Turbo Boost，保持 CPU 频率恒定，避免延迟抖动。

```cpp
#include <pthread.h>
#include <sched.h>
#include <numa.h>
#include <sys/mman.h>

// CPU 亲和性设置
void set_cpu_affinity(int cpu_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);

    pthread_t thread = pthread_self();
    int result = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (result != 0) {
        // 错误处理
    }
}

// 线程优先级设置（实时调度）
void set_realtime_priority() {
    struct sched_param param;
    param.sched_priority = 99;  // 最高优先级

    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) != 0) {
        // 需要 root 权限或 CAP_SYS_NICE
    }
}

// Huge Pages 内存分配
void* allocate_huge_pages(size_t size) {
    void* ptr = mmap(nullptr, size,
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
                     -1, 0);

    if (ptr == MAP_FAILED) {
        // 回退到普通页面
        ptr = mmap(nullptr, size,
                   PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS,
                   -1, 0);
    }

    // 锁定内存，防止被换出
    mlock(ptr, size);

    return ptr;
}

// NUMA 感知分配
void* numa_aware_alloc(size_t size, int node) {
    if (numa_available() < 0) {
        return malloc(size);
    }

    // 在指定 NUMA 节点分配内存
    return numa_alloc_onnode(size, node);
}

// 完整的低延迟线程初始化
void init_low_latency_thread(int cpu_id, int numa_node) {
    // 1. 设置 CPU 亲和性
    set_cpu_affinity(cpu_id);

    // 2. 设置实时优先级
    set_realtime_priority();

    // 3. 预分配内存（避免运行时分配）
    void* buffer = numa_aware_alloc(1024*1024, numa_node);
    mlock(buffer, 1024*1024);

    // 4. 预热缓存
    memset(buffer, 0, 1024*1024);
}

// Shell 命令示例（系统配置）
/*
# 设置 Huge Pages
echo 1024 > /proc/sys/vm/nr_hugepages

# 隔离 CPU 核心（启动参数）
# /etc/default/grub:
# GRUB_CMDLINE_LINUX="isolcpus=2,3,4,5"

# 关闭 CPU 节能
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    echo performance > $cpu
done

# 设置网卡中断亲和性（IRQ 绑定到 CPU 0）
echo 1 > /proc/irq/<IRQ_NUMBER>/smp_affinity

# NUMA 绑定运行程序
numactl --cpunodebind=0 --membind=0 ./trading_engine
*/
```

---

## 三、 数据结构与算法（量化偏好）

量化不只考 LeetCode 原题，更看重**对数据流的实时处理**。

* **滑动窗口**: 实时计算过去 5 分钟的均价。
* **堆 (Priority Queue)**: 订单簿（Order Book）的撮合逻辑，永远在处理最高买价和最低卖价。
* **实时中位数**: 经典的“双堆法”——一个大顶堆维护前半部分，一个小顶堆维护后半部分。
* **Order Book 数据结构**: 需要支持 O(1) 插入、删除、查询最优价格。通常用哈希表 + 双向链表 + 红黑树的组合实现。
```cpp
#include <queue>
#include <deque>
#include <map>
#include <unordered_map>

// 1. 滑动窗口 - 计算移动平均
class MovingAverage {
    std::deque<double> window;
    double sum = 0.0;
    size_t max_size;

public:
    MovingAverage(size_t window_size) : max_size(window_size) {}

    double add(double price) {
        window.push_back(price);
        sum += price;

        if (window.size() > max_size) {
            sum -= window.front();
            window.pop_front();
        }

        return sum / window.size();
    }
};

// 2. 实时中位数（双堆法）
class MedianFinder {
    std::priority_queue<int> max_heap;  // 存较小的一半
    std::priority_queue<int, std::vector<int>, std::greater<int>> min_heap;  // 存较大的一半

public:
    void addNum(int num) {
        // 保持 max_heap.size() >= min_heap.size()
        if (max_heap.empty() || num <= max_heap.top()) {
            max_heap.push(num);
        } else {
            min_heap.push(num);
        }

        // 平衡两个堆
        if (max_heap.size() > min_heap.size() + 1) {
            min_heap.push(max_heap.top());
            max_heap.pop();
        } else if (min_heap.size() > max_heap.size()) {
            max_heap.push(min_heap.top());
            min_heap.pop();
        }
    }

    double findMedian() {
        if (max_heap.size() == min_heap.size()) {
            return (max_heap.top() + min_heap.top()) / 2.0;
        }
        return max_heap.top();
    }
};

// 3. 简化的 Order Book 实现
class OrderBook {
    struct PriceLevel {
        double price;
        int total_quantity;
        std::list<Order> orders;
    };

    // 买单：价格从高到低
    std::map<double, PriceLevel, std::greater<double>> bids;
    // 卖单：价格从低到高
    std::map<double, PriceLevel, std::less<double>> asks;
    // 订单 ID 到迭代器的映射（快速删除）
    std::unordered_map<uint64_t, std::pair<
        std::map<double, PriceLevel>::iterator,
        std::list<Order>::iterator
    >> order_map;

public:
    // O(log N) 插入
    void add_order(const Order& order) {
        auto& book = order.is_buy ? bids : asks;
        auto& level = book[order.price];

        level.price = order.price;
        level.total_quantity += order.quantity;
        level.orders.push_back(order);

        // 保存迭代器，支持 O(1) 删除
        auto list_it = std::prev(level.orders.end());
        order_map[order.id] = {book.find(order.price), list_it};
    }

    // O(1) 删除
    void cancel_order(uint64_t order_id) {
        auto it = order_map.find(order_id);
        if (it == order_map.end()) return;

        auto [price_it, list_it] = it->second;
        auto& level = price_it->second;

        level.total_quantity -= list_it->quantity;
        level.orders.erase(list_it);

        if (level.orders.empty()) {
            // 这里需要判断是 bids 还是 asks
            // 简化处理
        }

        order_map.erase(it);
    }

    // O(1) 查询最优价格
    double best_bid() const {
        return bids.empty() ? 0.0 : bids.begin()->first;
    }

    double best_ask() const {
        return asks.empty() ? 0.0 : asks.begin()->first;
    }

    double mid_price() const {
        return (best_bid() + best_ask()) / 2.0;
    }
};

// 4. 单调队列 - 滑动窗口最大值
class MonotonicQueue {
    std::deque<int> dq;  // 存储索引，值单调递减

public:
    void push(int idx, int value, const std::vector<int>& prices) {
        // 移除队尾比当前值小的元素
        while (!dq.empty() && prices[dq.back()] <= value) {
            dq.pop_back();
        }
        dq.push_back(idx);
    }

    void pop(int idx) {
        if (!dq.empty() && dq.front() == idx) {
            dq.pop_front();
        }
    }

    int max(const std::vector<int>& prices) const {
        return prices[dq.front()];
    }
};
```
---

## 四、 内存管理与高性能分配

* **自定义 Allocator**: 订单对象频繁创建销毁，使用 **Object Pool** 避免 `malloc`/`new` 的系统调用开销和内存碎片。
* **Stack-based Allocator**: 短生命周期对象（如临时计算结果）直接在栈上分配，函数返回时自动释放。
* **Arena Allocator**: 区域式分配，批量分配大块内存，统一释放，适合请求处理场景。
* **jemalloc/tcmalloc**: 替换系统默认分配器 (glibc malloc)，提供更好的多线程性能和更低的内存碎片。
* **内存池预热**: 系统启动时预分配所有可能用到的内存，避免运行时分配。

```cpp
#include <memory>
#include <vector>
#include <cstdlib>

// 1. 对象池 (Object Pool)
template<typename T, size_t POOL_SIZE = 1024>
class ObjectPool {
    union Node {
        T object;
        Node* next;
    };

    Node* free_list = nullptr;
    std::vector<Node*> blocks;

    void allocate_block() {
        Node* block = static_cast<Node*>(::operator new(sizeof(Node) * POOL_SIZE));
        blocks.push_back(block);

        // 将新块链入 free list
        for (size_t i = 0; i < POOL_SIZE - 1; ++i) {
            block[i].next = &block[i + 1];
        }
        block[POOL_SIZE - 1].next = free_list;
        free_list = &block[0];
    }

public:
    ObjectPool() {
        allocate_block();
    }

    ~ObjectPool() {
        for (auto block : blocks) {
            ::operator delete(block);
        }
    }

    template<typename... Args>
    T* construct(Args&&... args) {
        if (!free_list) {
            allocate_block();
        }

        Node* node = free_list;
        free_list = node->next;

        // 在已分配的内存上构造对象
        return new(&node->object) T(std::forward<Args>(args)...);
    }

    void destroy(T* ptr) {
        ptr->~T();  // 显式调用析构函数

        Node* node = reinterpret_cast<Node*>(ptr);
        node->next = free_list;
        free_list = node;
    }
};

// 使用示例
ObjectPool<Order> order_pool;

void process_market_data() {
    Order* order = order_pool.construct(/* 构造参数 */);
    // 使用 order
    order_pool.destroy(order);  // 回收，而非真正 delete
}

// 2. 栈分配器 (Stack Allocator) - 极快
template<size_t SIZE>
class StackAllocator {
    char buffer[SIZE];
    char* top = buffer;

public:
    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) {
        // 对齐
        size_t space = SIZE - (top - buffer);
        void* ptr = top;
        if (!std::align(alignment, size, ptr, space)) {
            throw std::bad_alloc();
        }

        top = static_cast<char*>(ptr) + size;
        return ptr;
    }

    void reset() {
        top = buffer;  // 一次性清空
    }

    // 注意：不支持单独 deallocate
};

// 3. Arena Allocator（区域分配器）
class ArenaAllocator {
    struct Block {
        Block* next;
        size_t used;
        size_t size;
        alignas(16) char data[];
    };

    Block* current_block = nullptr;
    static constexpr size_t DEFAULT_BLOCK_SIZE = 64 * 1024;  // 64KB

    Block* allocate_block(size_t min_size) {
        size_t block_size = std::max(DEFAULT_BLOCK_SIZE, min_size + sizeof(Block));
        Block* block = static_cast<Block*>(malloc(block_size));
        block->next = current_block;
        block->used = 0;
        block->size = block_size - sizeof(Block);
        return block;
    }

public:
    ~ArenaAllocator() {
        while (current_block) {
            Block* next = current_block->next;
            free(current_block);
            current_block = next;
        }
    }

    void* allocate(size_t size, size_t alignment = 8) {
        if (!current_block) {
            current_block = allocate_block(size);
        }

        // 计算对齐后的位置
        size_t aligned_used = (current_block->used + alignment - 1) & ~(alignment - 1);

        if (aligned_used + size > current_block->size) {
            // 当前块不够，分配新块
            current_block = allocate_block(size);
            aligned_used = 0;
        }

        void* ptr = current_block->data + aligned_used;
        current_block->used = aligned_used + size;
        return ptr;
    }

    // 区域分配器特点：不支持单独释放，一次性清空所有
    void reset() {
        // 保留第一个块，释放其他
        if (current_block && current_block->next) {
            Block* next = current_block->next;
            while (next) {
                Block* temp = next->next;
                free(next);
                next = temp;
            }
            current_block->next = nullptr;
        }
        if (current_block) {
            current_block->used = 0;
        }
    }
};

// 4. 自定义 STL Allocator
template<typename T>
class PoolAllocator {
    ObjectPool<T>* pool;

public:
    using value_type = T;

    PoolAllocator(ObjectPool<T>* p) : pool(p) {}

    T* allocate(size_t n) {
        if (n != 1) throw std::bad_alloc();
        return pool->construct();
    }

    void deallocate(T* ptr, size_t) {
        pool->destroy(ptr);
    }
};

// 使用自定义分配器的容器
ObjectPool<Order> global_pool;
std::vector<Order, PoolAllocator<Order>> orders(PoolAllocator<Order>(&global_pool));
```

---

## 五、 网络 I/O 与数据传输

* **Zero-copy**: 使用 `splice`、`sendfile`、`mmap` 避免用户态与内核态之间的数据拷贝。
* **UDP Multicast**: 交易所行情数据通常通过 UDP 组播广播，需要处理乱序、丢包、重传。
* **Kernel Bypass 网络栈**: 用户态实现 TCP/IP 协议栈，绕过内核，延迟可降低到亚微秒级。
* **RDMA (Remote Direct Memory Access)**: 网卡直接读写远程内存，绕过 CPU，用于高频交易系统间的通信。
* **消息序列化**: Protocol Buffers 太慢，通常使用固定格式的二进制协议或 SBE (Simple Binary Encoding)。

```cpp
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <fcntl.h>

// 1. UDP Multicast 接收（市场行情）
class MulticastReceiver {
    int sockfd;

public:
    bool init(const char* multicast_ip, int port) {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0) return false;

        // 设置 socket 选项：允许重用地址
        int reuse = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        // 绑定端口
        struct sockaddr_in local_addr{};
        local_addr.sin_family = AF_INET;
        local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        local_addr.sin_port = htons(port);

        if (bind(sockfd, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
            return false;
        }

        // 加入组播组
        struct ip_mreq mreq{};
        mreq.imr_multiaddr.s_addr = inet_addr(multicast_ip);
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);

        if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
            return false;
        }

        // 设置接收缓冲区（防止丢包）
        int bufsize = 16 * 1024 * 1024;  // 16MB
        setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));

        return true;
    }

    ssize_t receive(void* buffer, size_t size) {
        return recvfrom(sockfd, buffer, size, 0, nullptr, nullptr);
    }
};

// 2. Zero-copy 文件发送
ssize_t sendfile_zerocopy(int out_fd, int in_fd, off_t offset, size_t count) {
    #ifdef __linux__
    return sendfile(out_fd, in_fd, &offset, count);
    #else
    // macOS/BSD 使用不同的 API
    off_t len = count;
    return sendfile(in_fd, out_fd, offset, &len, nullptr, 0);
    #endif
}

// 3. 内存映射文件（mmap）- 避免拷贝
class MappedFile {
    void* mapped_data = nullptr;
    size_t file_size = 0;

public:
    bool open(const char* filename) {
        int fd = ::open(filename, O_RDONLY);
        if (fd < 0) return false;

        struct stat st;
        if (fstat(fd, &st) < 0) {
            ::close(fd);
            return false;
        }

        file_size = st.st_size;
        mapped_data = mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
        ::close(fd);  // 可以立即关闭 fd

        if (mapped_data == MAP_FAILED) {
            return false;
        }

        // 建议内核预读（顺序访问）
        madvise(mapped_data, file_size, MADV_SEQUENTIAL);

        return true;
    }

    ~MappedFile() {
        if (mapped_data) {
            munmap(mapped_data, file_size);
        }
    }

    const void* data() const { return mapped_data; }
    size_t size() const { return file_size; }
};

// 4. 简单二进制序列化（固定格式，零拷贝）
#pragma pack(push, 1)  // 禁用填充
struct MarketDataMessage {
    uint64_t timestamp;
    uint32_t symbol_id;
    double price;
    int32_t quantity;
    uint8_t side;  // 0=buy, 1=sell

    // 直接从网络字节流解析（需要处理字节序）
    static MarketDataMessage from_bytes(const void* data) {
        MarketDataMessage msg;
        memcpy(&msg, data, sizeof(msg));
        // 如果需要，转换字节序
        // msg.timestamp = be64toh(msg.timestamp);
        return msg;
    }

    void to_bytes(void* buffer) const {
        memcpy(buffer, this, sizeof(*this));
    }
};
#pragma pack(pop)

// 5. 高性能 TCP 设置
void configure_low_latency_tcp(int sockfd) {
    // 禁用 Nagle 算法（立即发送小包）
    int flag = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    // 减小发送缓冲区（降低延迟）
    int sndbuf = 8192;
    setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

    // 设置 TCP_QUICKACK（快速 ACK）
    #ifdef TCP_QUICKACK
    int quickack = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_QUICKACK, &quickack, sizeof(quickack));
    #endif
}
```

---

## 六、 时间与时钟精度

* **RDTSC 指令**: 直接读 CPU 时间戳计数器（Time Stamp Counter），纳秒级精度，用于性能测量。
* **std::chrono 的代价**: `system_clock::now()` 会进入内核（vDSO 调用），`steady_clock` 更快但不保证单调递增。
* **PTP/NTP**: Precision Time Protocol 与交易所时钟同步，时间戳精度直接影响策略的准确性和监管合规。
* **硬件时间戳**: 高端网卡支持在数据包到达时打上硬件时间戳，消除软件延迟。
* **延迟测量最佳实践**: 不能用 `gettimeofday`（太慢），要用 TSC 或硬件时间戳，并且要考虑 CPU 频率缩放的影响。

```cpp
#include <chrono>
#include <x86intrin.h>

// 1. RDTSC - CPU 时间戳计数器（最快）
class TSCClock {
    static double cycles_per_ns;

public:
    // 初始化：校准 TSC 频率
    static void calibrate() {
        auto start_time = std::chrono::steady_clock::now();
        uint64_t start_tsc = __rdtsc();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        uint64_t end_tsc = __rdtsc();
        auto end_time = std::chrono::steady_clock::now();

        auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            end_time - start_time).count();

        cycles_per_ns = double(end_tsc - start_tsc) / duration_ns;
    }

    // 读取当前时间戳（CPU 周期）
    static inline uint64_t now_cycles() {
        // 使用 LFENCE 防止指令重排
        _mm_lfence();
        uint64_t tsc = __rdtsc();
        _mm_lfence();
        return tsc;
    }

    // 转换为纳秒
    static inline uint64_t now_ns() {
        return now_cycles() / cycles_per_ns;
    }

    // 计算两个时间戳的差值（纳秒）
    static inline uint64_t elapsed_ns(uint64_t start_cycles) {
        return (now_cycles() - start_cycles) / cycles_per_ns;
    }
};

double TSCClock::cycles_per_ns = 2.5;  // 假设 2.5 GHz CPU

// 2. 高精度延迟测量
class LatencyMeasurement {
    uint64_t start_cycle;

public:
    void start() {
        start_cycle = TSCClock::now_cycles();
    }

    uint64_t stop_ns() {
        return TSCClock::elapsed_ns(start_cycle);
    }
};

// 使用示例
void measure_function_latency() {
    LatencyMeasurement lat;
    lat.start();

    // 执行被测函数
    process_order();

    uint64_t latency_ns = lat.stop_ns();
    // latency_ns 就是纳秒级延迟
}

// 3. std::chrono 使用（较慢但更安全）
uint64_t get_timestamp_ns() {
    using namespace std::chrono;
    auto now = steady_clock::now();
    return duration_cast<nanoseconds>(now.time_since_epoch()).count();
}

// 4. 延迟统计（P50, P99, P999）
class LatencyStats {
    std::vector<uint64_t> samples;

public:
    void record(uint64_t latency_ns) {
        samples.push_back(latency_ns);
    }

    void report() {
        if (samples.empty()) return;

        std::sort(samples.begin(), samples.end());
        size_t n = samples.size();

        uint64_t p50 = samples[n * 50 / 100];
        uint64_t p99 = samples[n * 99 / 100];
        uint64_t p999 = samples[n * 999 / 1000];
        uint64_t max = samples.back();

        printf("Latency: P50=%lu ns, P99=%lu ns, P999=%lu ns, Max=%lu ns\\n",
               p50, p99, p999, max);
    }
};

// 5. 时间戳对齐（确保时钟序）
class MonotonicTimestamp {
    static std::atomic<uint64_t> last_timestamp;

public:
    static uint64_t now() {
        uint64_t ts = TSCClock::now_ns();
        uint64_t last = last_timestamp.load(std::memory_order_relaxed);

        // 确保时间戳单调递增（即使 TSC 跳变）
        while (ts <= last) {
            ts = last + 1;
            if (last_timestamp.compare_exchange_weak(
                last, ts, std::memory_order_release, std::memory_order_relaxed)) {
                return ts;
            }
        }

        last_timestamp.store(ts, std::memory_order_release);
        return ts;
    }
};

std::atomic<uint64_t> MonotonicTimestamp::last_timestamp{0};

// 6. 时钟同步检查
void verify_clock_sync() {
    // 检查系统时钟与硬件时钟的偏差
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t sys_ns = ts.tv_sec * 1000000000ULL + ts.tv_nsec;

    uint64_t tsc_ns = TSCClock::now_ns();

    // 如果偏差过大，可能需要重新校准
    int64_t drift = std::abs(int64_t(sys_ns - tsc_ns));
    if (drift > 1000000) {  // 1ms
        // 警告：时钟漂移过大
    }
}
```

---

## 七、 性能测量与调优工具

* **延迟分布**: 不只看平均延迟，更要看 P99、P99.9 延迟（尾延迟），这才是真正的性能瓶颈。
* **性能分析工具**:
  * **perf**: Linux 性能分析工具，CPU 采样、Cache Miss 分析
  * **Intel VTune**: 微架构级性能分析
  * **cachegrind/callgrind**: Valgrind 工具套件，Cache 模拟
  * **火焰图 (Flame Graph)**: 快速定位热点函数
* **延迟注入测试**: 模拟网络抖动、CPU 负载，测试系统在极端情况下的表现。
* **硬件计数器**: 通过 PMU (Performance Monitoring Unit) 读取 Cache Miss、分支预测失败、TLB Miss 等硬件事件。

```bash
# 性能分析工具使用示例

# 1. perf 命令行工具
# CPU 采样分析（找热点函数）
perf record -g ./trading_engine
perf report

# Cache Miss 分析
perf stat -e cache-references,cache-misses,branches,branch-misses ./trading_engine

# 详细硬件计数器
perf stat -e L1-dcache-loads,L1-dcache-load-misses,LLC-loads,LLC-load-misses ./program

# 2. 生成火焰图
perf record -F 99 -g ./trading_engine
perf script > out.perf
./FlameGraph/stackcollapse-perf.pl out.perf > out.folded
./FlameGraph/flamegraph.pl out.folded > flamegraph.svg

# 3. Valgrind Cache 分析
valgrind --tool=cachegrind --cache-sim=yes ./program
cg_annotate cachegrind.out.<pid>

# 4. Intel VTune (命令行)
vtune -collect hotspots -result-dir vtune_results ./program
vtune -report hotspots -result-dir vtune_results
```

```cpp
// C++ 代码中的性能计数器（使用 perf_event）
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <unistd.h>

class PerfCounter {
    int fd;

public:
    bool init(uint32_t type, uint64_t config) {
        struct perf_event_attr pe{};
        pe.type = type;
        pe.size = sizeof(pe);
        pe.config = config;
        pe.disabled = 1;
        pe.exclude_kernel = 1;
        pe.exclude_hv = 1;

        fd = perf_event_open(&pe, 0, -1, -1, 0);
        return fd >= 0;
    }

    void start() {
        ioctl(fd, PERF_EVENT_IOC_RESET, 0);
        ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
    }

    uint64_t stop() {
        ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
        uint64_t count;
        read(fd, &count, sizeof(count));
        return count;
    }

    ~PerfCounter() { if (fd >= 0) close(fd); }
};

// 测量 Cache Miss
void measure_cache_misses() {
    PerfCounter counter;
    counter.init(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES);

    counter.start();
    // 执行被测代码
    process_orders();
    uint64_t misses = counter.stop();

    printf("Cache misses: %lu\\n", misses);
}
```

---

## 八、 C++20/23 新特性（前沿公司已采用）

* **Coroutines**: 异步交易流程的状态管理，比回调和状态机更清晰。
* **Concepts**: 模板约束，编译错误更清晰，替代 SFINAE。
* **std::span**: 无开销的数组视图，替代指针+长度的组合。
* **Ranges**: 延迟求值的迭代器组合，更少的临时对象分配。
* **std::format**: 性能优于 `sprintf`，类型安全优于 `iostream`。
* **Modules**: 替代头文件，加快编译速度，减少符号污染。

```cpp
#include <concepts>
#include <span>
#include <ranges>
#include <format>
#include <coroutine>

// 1. Concepts - 约束模板参数
template<typename T>
concept Numeric = std::is_arithmetic_v<T>;

template<Numeric T>
T calculate_average(std::span<const T> data) {
    T sum = 0;
    for (T val : data) {
        sum += val;
    }
    return sum / data.size();
}

// 更复杂的 Concept
template<typename T>
concept OrderType = requires(T order) {
    { order.price } -> std::convertible_to<double>;
    { order.quantity } -> std::convertible_to<int>;
    { order.is_valid() } -> std::same_as<bool>;
};

template<OrderType T>
void process_order(const T& order) {
    if (order.is_valid()) {
        // 处理订单
    }
}

// 2. std::span - 零开销数组视图
void calculate_vwap(std::span<const double> prices,
                   std::span<const double> volumes) {
    // 不需要传递 size，自动携带长度信息
    assert(prices.size() == volumes.size());

    double sum_pv = 0, sum_v = 0;
    for (size_t i = 0; i < prices.size(); ++i) {
        sum_pv += prices[i] * volumes[i];
        sum_v += volumes[i];
    }
    double vwap = sum_pv / sum_v;
}

// 调用
std::vector<double> prices = {100.1, 100.2, 100.3};
std::vector<double> volumes = {1000, 2000, 3000};
calculate_vwap(prices, volumes);  // 自动转换为 span

// 3. Ranges - 惰性求值
void process_top_orders() {
    std::vector<Order> orders = get_all_orders();

    // 传统方式需要多次遍历和临时容器
    // 现代方式：组合多个操作，延迟执行
    auto valid_sorted_top10 = orders
        | std::views::filter([](const Order& o) { return o.is_valid(); })
        | std::views::transform([](const Order& o) { return o.price; })
        | std::views::take(10);

    // 只在实际访问时才计算
    for (double price : valid_sorted_top10) {
        process_price(price);
    }
}

// 4. std::format - 高性能格式化
std::string format_order_log(const Order& order) {
    // 比 sprintf 安全，比 iostream 快
    return std::format("Order[id={}, price={:.2f}, qty={}]",
                      order.id, order.price, order.quantity);
}

// 5. Coroutines - 异步状态机
#include <coroutine>

struct Task {
    struct promise_type {
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {}
    };

    std::coroutine_handle<promise_type> handle;

    ~Task() { if (handle) handle.destroy(); }
};

// 异步订单处理流程
Task process_order_async(Order order) {
    // 验证订单
    bool valid = co_await validate_order_async(order);
    if (!valid) co_return;

    // 风控检查
    bool risk_ok = co_await risk_check_async(order);
    if (!risk_ok) co_return;

    // 发送到交易所
    co_await send_to_exchange_async(order);
}

// 6. std::jthread - 自动 join 的线程
void start_market_data_thread() {
    std::jthread thread([](std::stop_token stop) {
        while (!stop.stop_requested()) {
            // 处理市场数据
            process_market_data();
        }
    });
    // 析构时自动 request_stop() 和 join()
}

// 7. Three-way comparison (C++20)
struct Order {
    uint64_t timestamp;
    double price;

    // 自动生成 ==, !=, <, <=, >, >=
    auto operator<=>(const Order& other) const = default;
};

// 8. Designated initializers
Order create_order() {
    return Order{
        .timestamp = get_timestamp(),
        .price = 100.5,
        .quantity = 1000
    };
}
```

---

## 九、 实战中的权衡与陷阱

* **静态库 vs 动态库**: 静态链接启动更快，但内存占用更大；动态链接便于更新，但有加载开销。
* **何时使用虚函数**: 策略框架、插件系统中不可避免，但要把虚函数调用移出热路径。
* **过度优化的风险**: 微优化要基于 Profile 数据，不要猜测瓶颈在哪。
* **可维护性 vs 性能**: 极致性能代码往往可读性差，需要详细注释和文档。
* **第三方库的选择**: 避免使用重量级库（如 Boost.Asio），许多 HFT 公司自研轻量级框架。

---

## 十、 面试常考场景题

1. **如何测量一次函数调用的延迟（要求纳秒级精度）？**
   * 提示：使用 RDTSC，注意 CPU 乱序执行，需要加内存屏障 (CPUID/LFENCE)

2. **Order Book 的数据结构设计**
   * 要求：插入订单 O(1)、删除订单 O(1)、查询最优价格 O(1)
   * 提示：哈希表存订单，每个价格档位维护双向链表，价格档位用红黑树组织

3. **如何在多线程环境下无锁地传递行情数据？**
   * 提示：SPSC 无锁队列，使用 `std::atomic` 和内存序，避免 ABA 问题

4. **为什么 HFT 系统要在用户态实现 TCP/IP 协议栈？**
   * 提示：内核协议栈有上下文切换、系统调用、拷贝开销，Kernel Bypass 可降低延迟到亚微秒级

5. **解释 Cache Line 和 False Sharing，如何避免？**
   * 提示：64 字节对齐，使用 `alignas(64)` 或手动填充

6. **实现一个基于 `std::atomic` 的简单无锁环形队列（SPSC）**
   * 考察：原子操作、内存序、环形缓冲区索引计算

7. **如何保证策略系统的确定性（Determinism）？**
   * 提示：固定时间戳、禁用多线程、避免浮点数舍入误差、replay 测试

```cpp
// ========== 面试题完整实现 ==========

// 1. 纳秒级延迟测量（带内存屏障）
inline uint64_t rdtsc_with_fence() {
    uint32_t lo, hi;
    __asm__ __volatile__ (
        "lfence\\n"          // 内存屏障
        "rdtsc\\n"
        "lfence"
        : "=a"(lo), "=d"(hi)
        :
        : "memory"
    );
    return ((uint64_t)hi << 32) | lo;
}

void measure_function_call() {
    uint64_t start = rdtsc_with_fence();

    // 被测函数
    some_function();

    uint64_t end = rdtsc_with_fence();
    uint64_t cycles = end - start;

    // 转换为纳秒（假设 2.5 GHz CPU）
    double ns = cycles / 2.5;
}

// 2. 完整的 Order Book 实现
class HighPerformanceOrderBook {
public:
    struct Order {
        uint64_t id;
        double price;
        int quantity;
        bool is_buy;
        uint64_t timestamp;
    };

private:
    struct PriceLevel {
        double price;
        int total_quantity = 0;
        std::list<Order> orders;
    };

    // 买单：价格从高到低（红黑树）
    std::map<double, PriceLevel, std::greater<double>> bids;
    // 卖单：价格从低到高
    std::map<double, PriceLevel> asks;

    // O(1) 查找和删除：order_id -> (price_level_iterator, order_iterator)
    std::unordered_map<uint64_t, std::tuple<
        bool,  // is_buy
        double,  // price
        std::list<Order>::iterator
    >> order_index;

public:
    // O(log P) 插入，P 是价格档位数量（通常很小）
    void add_order(const Order& order) {
        auto& book = order.is_buy ? bids : asks;
        auto& level = book[order.price];

        if (level.orders.empty()) {
            level.price = order.price;
        }

        level.total_quantity += order.quantity;
        level.orders.push_back(order);

        auto order_it = std::prev(level.orders.end());
        order_index[order.id] = {order.is_buy, order.price, order_it};
    }

    // O(1) 删除（通过 order_id）
    bool cancel_order(uint64_t order_id) {
        auto it = order_index.find(order_id);
        if (it == order_index.end()) {
            return false;
        }

        auto [is_buy, price, order_it] = it->second;
        auto& book = is_buy ? bids : asks;
        auto level_it = book.find(price);

        if (level_it == book.end()) {
            return false;
        }

        auto& level = level_it->second;
        level.total_quantity -= order_it->quantity;
        level.orders.erase(order_it);

        // 如果价格档位空了，删除它
        if (level.orders.empty()) {
            book.erase(level_it);
        }

        order_index.erase(it);
        return true;
    }

    // O(1) 查询最优价格
    std::optional<double> best_bid() const {
        return bids.empty() ? std::nullopt : std::optional{bids.begin()->first};
    }

    std::optional<double> best_ask() const {
        return asks.empty() ? std::nullopt : std::optional{asks.begin()->first};
    }

    double spread() const {
        auto bid = best_bid();
        auto ask = best_ask();
        if (!bid || !ask) return 0.0;
        return *ask - *bid;
    }

    // 撮合订单（简化版）
    std::vector<std::pair<Order, Order>> match() {
        std::vector<std::pair<Order, Order>> matches;

        while (!bids.empty() && !asks.empty()) {
            auto& best_bid_level = bids.begin()->second;
            auto& best_ask_level = asks.begin()->second;

            if (best_bid_level.price < best_ask_level.price) {
                break;  // 无法撮合
            }

            auto& buy_order = best_bid_level.orders.front();
            auto& sell_order = best_ask_level.orders.front();

            int match_qty = std::min(buy_order.quantity, sell_order.quantity);

            // 记录成交
            matches.push_back({buy_order, sell_order});

            // 更新数量
            buy_order.quantity -= match_qty;
            sell_order.quantity -= match_qty;

            // 移除完全成交的订单
            if (buy_order.quantity == 0) {
                order_index.erase(buy_order.id);
                best_bid_level.orders.pop_front();
            }
            if (sell_order.quantity == 0) {
                order_index.erase(sell_order.id);
                best_ask_level.orders.pop_front();
            }

            // 移除空的价格档位
            if (best_bid_level.orders.empty()) {
                bids.erase(bids.begin());
            }
            if (best_ask_level.orders.empty()) {
                asks.erase(asks.begin());
            }
        }

        return matches;
    }
};

// 3. 完整的 SPSC 无锁队列（处理 ABA 问题）
template<typename T, size_t SIZE>
class SPSCQueue {
    static_assert((SIZE & (SIZE - 1)) == 0, "SIZE must be power of 2");

    struct alignas(64) Slot {
        T data;
        std::atomic<uint64_t> sequence{0};
    };

    Slot buffer[SIZE];
    static constexpr size_t MASK = SIZE - 1;

    alignas(64) std::atomic<uint64_t> enqueue_pos{0};
    alignas(64) std::atomic<uint64_t> dequeue_pos{0};

public:
    bool try_enqueue(const T& item) {
        uint64_t pos = enqueue_pos.load(std::memory_order_relaxed);
        Slot* slot = &buffer[pos & MASK];
        uint64_t seq = slot->sequence.load(std::memory_order_acquire);

        // 检查是否可以写入
        if (seq != pos) {
            return false;  // 队列满或消费者还未读取
        }

        slot->data = item;
        slot->sequence.store(pos + 1, std::memory_order_release);
        enqueue_pos.store(pos + 1, std::memory_order_relaxed);
        return true;
    }

    bool try_dequeue(T& item) {
        uint64_t pos = dequeue_pos.load(std::memory_order_relaxed);
        Slot* slot = &buffer[pos & MASK];
        uint64_t seq = slot->sequence.load(std::memory_order_acquire);

        // 检查是否有数据可读
        if (seq != pos + 1) {
            return false;  // 队列空
        }

        item = slot->data;
        slot->sequence.store(pos + SIZE, std::memory_order_release);
        dequeue_pos.store(pos + 1, std::memory_order_relaxed);
        return true;
    }
};

// 4. 确定性回放系统
class DeterministicEngine {
    // 固定的时间源
    uint64_t virtual_time = 0;

    // 录制的事件日志
    struct Event {
        uint64_t timestamp;
        std::string type;
        std::vector<uint8_t> data;
    };
    std::vector<Event> event_log;

    bool is_recording = true;
    size_t replay_index = 0;

public:
    // 获取确定性时间戳
    uint64_t get_time() {
        if (is_recording) {
            return virtual_time++;
        } else {
            // 回放模式：从日志读取
            return event_log[replay_index].timestamp;
        }
    }

    // 记录事件
    void record_event(const std::string& type, const void* data, size_t size) {
        if (!is_recording) return;

        Event event;
        event.timestamp = virtual_time;
        event.type = type;
        event.data.assign(static_cast<const uint8_t*>(data),
                         static_cast<const uint8_t*>(data) + size);
        event_log.push_back(event);
    }

    // 回放事件
    bool replay_next_event(std::string& type, std::vector<uint8_t>& data) {
        if (replay_index >= event_log.size()) {
            return false;
        }

        const auto& event = event_log[replay_index++];
        type = event.type;
        data = event.data;
        virtual_time = event.timestamp;
        return true;
    }

    void start_replay() {
        is_recording = false;
        replay_index = 0;
        virtual_time = 0;
    }
};

// 5. Cache Line 和 False Sharing 演示
struct BadCounters {
    std::atomic<int> counter1;  // 假设在同一 Cache Line
    std::atomic<int> counter2;
    // False sharing: 两个线程分别修改 counter1 和 counter2
};

struct GoodCounters {
    alignas(64) std::atomic<int> counter1;
    alignas(64) std::atomic<int> counter2;
    // 每个 counter 独占一个 Cache Line
};

void benchmark_false_sharing() {
    BadCounters bad;
    GoodCounters good;

    // 测试 bad（有 false sharing）
    auto start = std::chrono::high_resolution_clock::now();
    std::thread t1([&]{ for(int i=0; i<10000000; i++) bad.counter1.fetch_add(1); });
    std::thread t2([&]{ for(int i=0; i<10000000; i++) bad.counter2.fetch_add(1); });
    t1.join(); t2.join();
    auto bad_time = std::chrono::high_resolution_clock::now() - start;

    // 测试 good（无 false sharing）
    start = std::chrono::high_resolution_clock::now();
    std::thread t3([&]{ for(int i=0; i<10000000; i++) good.counter1.fetch_add(1); });
    std::thread t4([&]{ for(int i=0; i<10000000; i++) good.counter2.fetch_add(1); });
    t3.join(); t4.join();
    auto good_time = std::chrono::high_resolution_clock::now() - start;

    // good_time 应该明显小于 bad_time
}
```

---

## 总结

量化交易 C++ 的核心是**对计算机体系结构的深刻理解**：从 CPU 流水线、缓存层次、内存模型，到操作系统调度、网络协议栈。每一纳秒的优化都可能带来收益上的巨大差异。

**学习路径建议**：
1. 深入理解 CPU 缓存（Cache）和内存层次结构
2. 掌握无锁编程和内存模型（C++11 Memory Model）
3. 学习 SIMD 编程（至少会用 AVX2）
4. 熟悉 Linux 性能分析工具（perf, strace, tcpdump）
5. 阅读高性能开源项目代码（如 Folly, abseil, DPDK）
6. 动手实现：Order Book、无锁队列、内存池、行情解析器
