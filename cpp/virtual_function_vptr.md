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

