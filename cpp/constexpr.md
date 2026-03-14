这是一份关于 `constexpr` 的技术笔记总结，你可以直接保存到你的 Notion 或 Obsidian 中。

---

# C++ 核心知识点：`constexpr` 编译时计算

## 1. 核心定义

`constexpr` (Constant Expression) 是 C++11 引入的关键字，其核心思想是**将计算提前到编译阶段**。

* **目标**：提升程序运行性能（零开时损耗），增强代码安全性。
* **原则**：如果输入是常量，则结果在编译时计算；如果输入是变量，则退化为普通函数。

---

## 2. 关键用法对比

| 特性 | `const` | `constexpr` | `consteval` (C++20) |
| --- | --- | --- | --- |
| **语义** | 只读 (Read-only) | 常量表达式 (Compile-time constant) | 立即函数 (Immediate function) |
| **计算时机** | 运行时或编译时 | **优先编译时** | **必须编译时** |
| **强制性** | 弱 | 中（修饰变量时强，修饰函数时弱） | 强（不满足则编译报错） |

---

## 3. 函数规则演进

随着 C++ 标准的迭代，`constexpr` 函数的限制不断放宽：

* **C++11**：函数体只能包含一条 `return` 语句。
* **C++14**：允许使用 `if`、`switch`、循环（`for`/`while`）以及局部变量修改。
* **C++17**：引入 `if constexpr`，允许根据模板类型在编译时“剪枝”代码。
* **C++20**：支持动态内存分配（`std::vector`/`std::string`）在编译时临时使用。

---

## 4. 代码示例

### A. 基础阶梯税率计算

```cpp
constexpr double calculate_fee(double price) {
    if (price > 1000.0) return price * 0.0005;
    return price * 0.001;
}

// 情况 1: 强制编译时计算
constexpr double fee1 = calculate_fee(2000.0); // 结果 1.0 直接硬编码进二进制

// 情况 2: 自动退化为运行时计算
double input;
std::cin >> input;
double fee2 = calculate_fee(input); // 像普通函数一样运行

```

### B. 编译时代码剪枝 (`if constexpr`)

```cpp
template <typename T>
void print_value(T v) {
    if constexpr (std::is_pointer_v<T>) {
        std::cout << *v; // 只有当 T 是指针时，这段代码才会被编译
    } else {
        std::cout << v;  // 否则只编译这一行
    }
}

```

---

## 5. 为什么使用 `constexpr`？

1. **极致性能**：运行时 CPU 压力为零，直接读取预计算结果。
2. **更小的二进制体积**：编译器会剔除永远不会执行的 `if constexpr` 分支。
3. **常量上下文需求**：定义数组长度 `int arr[N]` 或模板参数时，必须使用编译时常量。

---

**下一步建议：**
你想了解如何定义一个 `constexpr` 的类（Literal Types），让对象也能在编译时被创建出来吗？
