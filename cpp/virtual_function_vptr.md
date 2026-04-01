带路党”——**虚函数表指针（vptr）**。


---

### vtable, vptr

当你给类加上第一个 `virtual` 函数时，编译器为了实现多态，必须在运行时知道该调用哪个版本的函数。为此，它会做两件事：

1. **生成虚函数表（vtable）**：每个包含虚函数的**类**，在内存中都有一张只读的表，记录了该类所有虚函数的地址。
       - Each entry is a function pointer pointing to the most derived version of the virtual functions accessible by that class.
2. **插入虚指针（vptr）**：每个该类的**对象**，都会被编译器自动添加一个隐藏的指针成员。这个指针指向该类对应的虚函数表。

### How it Works 
#### At Compile Time: 
The compiler sees the virtual keyword and builds a **vtable** for the class. It also adds code to the constructor to initialize the **vptr** to point to this vtable.

#### At Runtime: 
When you call a virtual function through a base class pointer, the program follows these steps:

- Follow the vptr from the object to its vtable.
- Look up the correct function address in the vtable.
- Jump to that address and execute the code.

在 C++ 中，**RTTI** 的全称是 **Run-Time Type Identification**（运行时类型识别）。

简单来说，它是 C++ 提供的一种机制，让程序在**运行时**（而不是编译时）能够确定一个对象的实际类型。

---

## 1. 为什么需要 RTTI？

在多态编程中，我们经常使用父类指针指向子类对象：

```cpp
Shape* s = new Circle();

```

在编译时，编译器只知道 `s` 是 `Shape*` 类型。但有些时候，你可能需要知道 `s` 指向的到底是 `Circle` 还是 `Rectangle`，以便调用某些子类特有的方法。RTTI 就是为了解决这种“身份识别”问题而设计的。

---

## 2. RTTI 的两大核心工具

RTTI 主要通过两个关键字来实现：

### ① `dynamic_cast` 运算符

用于将基类指针（或引用）安全地转换为派生类指针（或引用）。

* **工作原理**：它会在运行时检查转换是否有效。
* **结果**：如果转换失败，对于指针会返回 `nullptr`；对于引用会抛出 `std::bad_cast` 异常。

```cpp
if (Circle* c = dynamic_cast<Circle*>(s)) {
    c->drawRadius(); // 只有确认是圆，才调用圆特有的方法
}

```

### ② `typeid` 运算符

用于获取表达式的类型信息。

* 它返回一个 `std::type_info` 对象的引用。
* 可以通过 `.name()` 获取类型名称，或者直接用 `==` 比较两个对象是否属于同一类型。

```cpp
if (typeid(*s) == typeid(Circle)) {
    std::cout << "This is a Circle!";
}

```

---

## 3. RTTI 是如何实现的？（底层原理）

RTTI 并不是免费的午餐，它的实现高度依赖于 **虚函数表（vtable）**。

* **vptr 与 vtable**：当类中包含虚函数时，编译器会为该类生成一张虚函数表。
* **类型信息存储**：在虚函数表的偏移量为 -1 或某个固定位置，通常会存储一个指向 **`type_info`** 数据的指针。
* **运行时查询**：当你调用 `dynamic_cast` 或 `typeid` 时，程序会通过对象的虚函数表指针（vptr）找到这张表，进而提取出实际的类型信息。

> **注意**：如果一个类没有虚函数，它就没有虚函数表，也就无法使用 RTTI（`dynamic_cast` 会报错）。这种情况下，`typeid` 只能识别出编译时的静态类型。

---

## 4. 优缺点与最佳实践

### 优点

* 提供了类型安全性，避免了 C 风格强转（static_cast）可能导致的崩溃。
* 方便了某些复杂的设计模式实现。

### 缺点

* **性能开销**：查虚表、查类型树、字符串比较等操作比普通转换慢。
* **二进制膨胀**：存储类型信息会增加可执行文件的体积。
* **破坏抽象**：过度依赖 RTTI 通常意味着类层次设计存在问题（违反了开闭原则）。

---

## 总结

RTTI 是 C++ 用来在运行时“认亲”的手段。虽然功能强大，但在高性能开发中（如游戏引擎或嵌入式），程序员有时会通过编译器选项（如 `-fno-rtti`）彻底禁用它，转而使用自定义的枚举或 ID 来手动实现类型识别，以换取极致的性能。

