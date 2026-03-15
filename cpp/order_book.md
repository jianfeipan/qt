这是一个非常经典的低延迟金融系统面试题。在设计订单簿（Order Book）时，核心矛盾在于 **插入性能 (O(log N) vs O(1))** 与 **查询性能 (O(1))** 之间的平衡，以及如何高效地管理多交易所的数据聚合。

## 1. 数据结构选择

要实现 BBO (Best Bid and Offer) 和 NBBO (National Best Bid and Offer)，我们需要维护每个交易所的买卖盘价格分布。

* **交易所内部 (Per Exchange):** * 使用两个 **TreeMap**（Java）或 **std::map**（C++）：`bids` (降序) 和 `asks` (升序)。
* Key 是 `price`，Value 是该价格下的总 `quantity`。
* `firstKey()` 即可在 $O(\log N)$ 或 $O(1)$（视具体实现）内获取最优价。


* **全市场聚合 (Global NBBO):**
* 我们需要快速知道“谁是所有交易所中最牛的价”。
* 可以使用两个 **PriorityQueue** (堆) 或 **TreeMap** 来存储各个交易所当前的 Best Price。



---

## 2. 核心类设计 (Python/Pseudo-code 示例)

为了应对“数据量翻 100 倍”的挑战，我们不仅要存价格，还要处理订单的更新与删除。

```python
from collections import defaultdict
import sortedcontainers # 高性能有序集合库

class OrderBook:
    def __init__(self):
        # 存储每个交易所的订单分布: {exchange_id: {"bids": SortedDict, "asks": SortedDict}}
        self.exchanges = defaultdict(lambda: {
            "bids": sortedcontainers.SortedDict(), # Key取负值实现降序
            "asks": sortedcontainers.SortedDict()
        })
        
        # 全局最优价追踪: 存储各交易所的当前最佳价格，用于计算 NBBO
        # {price: count_of_exchanges_at_this_price}
        self.global_bids = sortedcontainers.SortedDict() 
        self.global_asks = sortedcontainers.SortedDict()

    def update_order(self, exchange_id, price, quantity, order_type):
        exch = self.exchanges[exchange_id]
        side = "bids" if order_type == "bid" else "asks"
        book = exch[side]
        
        # 1. 记录更新前的旧 Best Price
        old_best = book.peekitem(0)[0] if book else None
        
        # 2. 更新该交易所的价格点
        if quantity == 0:
            if price in book: del book[price]
        else:
            book[price] = quantity
            
        # 3. 如果 Best Price 变了，同步更新全局 NBBO 索引
        new_best = book.peekitem(0)[0] if book else None
        if old_best != new_best:
            self._update_global_index(side, old_best, new_best)

    def get_exchange_bbo(self, exchange_id):
        exch = self.exchanges[exchange_id]
        best_bid = -exch["bids"].peekitem(0)[0] if exch["bids"] else None
        best_ask = exch["asks"].peekitem(0)[0] if exch["asks"] else None
        return (best_bid, best_ask)

    def get_nbbo(self):
        best_bid = -self.global_bids.peekitem(0)[0] if self.global_bids else None
        best_ask = self.global_asks.peekitem(0)[0] if self.global_asks else None
        return (best_bid, best_ask)

```

---

## 3. 面试官的连环追问 (Senior Level)

### Q1: 如果数据量翻 100 倍，内存受限怎么办？

* **答案：** 放弃 `TreeMap` 这种对象开销巨大的结构。改用 **原生数组 (Primitive Arrays)** 或 **内存池 (Memory Pool)**。
* **价格离散化：** 如果价格步长（Tick Size）是固定的（如 0.01），可以使用 `LongArray` 或固定长度的 `Bucket` 来代替 Map，将价格映射为索引。这样查询和插入都能达到 $O(1)$。

### Q2: 如何处理 L3 行情（逐笔委托）？

* **答案：** 上述设计是 L2（聚合报价）。如果是 L3，你需要为每个价格点维护一个 **FIFO 队列**（通常是双向链表），存储具体的 `order_id`。因为在同一个价格上，先到的订单优先成交。

### Q3: 延迟太高了，如何优化 `get_nbbo`？

* **答案：** 现在的实现是在每次插入时更新全局索引。如果查询频率远低于更新频率，可以采用 **Lazy Evaluation**（懒加载）。
* **多线程并发：** 如果多个交易所的数据由不同线程处理，`global_bids` 会成为竞争热点。可以使用 **Read-Write Lock** 或者将 NBBO 的更新改为 **无锁（Lock-free）的原子引用更新**。

### Q4: TCP vs UDP 在接收这些数据时的取舍？

* **答案：**
* **UDP (Multicast):** 适合行情广播。极低延迟，但可能丢包。需要实现序列号校验和重传机制（Gap Fill）。
* **TCP:** 适合订单执行（Order Entry）。保证不丢包、不乱序，但三次握手和拥塞控制会带来延迟抖动（Jitter）。



---

## 4. 进一步优化：Hardware/System Level

在 Senior 面试中，如果你提到以下两点，评分会直接拉满：

1. **CPU Affinity (亲和性):** 将处理特定交易所数据的线程绑定到特定的 CPU 核心，减少 Cache Miss。
2. **Zero-Copy:** 使用 Direct Buffer 接收网络包，直接在内存中解析字段，避免数据从内核空间拷贝到用户空间。
