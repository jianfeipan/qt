/*
implement in C++:
Multi-Exchange Order Book: Design an order book that can ingest "Limit" and "Market" orders from multiple exchanges. 
You must be able to return the NBBO (National Best Bid and Offer) efficiently.
Tip: Use a TreeMap or Max-Heap/Min-Heap and explain the $O(\log N)$ trade-offs.

Design

We maintain, for each side: bid, ask

a price level map:
bids: highest price is best
asks: lowest price is best
at each price, we track aggregate quantity per exchange

That gives:

insert/update/delete at a price: O(log P)
best bid / best ask lookup: O(1) from begin()
market order matching: repeatedly consume best levels

*/

#include <iostream>
#include <map>
#include <unordered_map>
#include <string>
#include <optional>
#include <algorithm>

class MultiExchangeOrderBook {
public:
    enum class Side {
        Buy,
        Sell
    };

    struct Quote {
        double price;
        int quantity;
        std::string exchange;
    };

    struct NBBO {
        std::optional<Quote> bestBid;
        std::optional<Quote> bestAsk;
    };

private:
    // For each price level, store aggregate quantity by exchange.
    using ExchangeQtyMap = std::unordered_map<std::string, int>;

    // Descending prices for bids.
    std::map<double, ExchangeQtyMap, std::greater<double>> bids_;

    // Ascending prices for asks.
    std::map<double, ExchangeQtyMap> asks_;

private:
    static int totalQuantity(const ExchangeQtyMap& exQty) {
        int total = 0;
        for (const auto& [ex, qty] : exQty) {
            total += qty;
        }
        return total;
    }

    static void addToBook(std::map<double, ExchangeQtyMap, std::greater<double>>& book,
                          double price,
                          const std::string& exchange,
                          int qty) {
        if (qty <= 0) return;
        book[price][exchange] += qty;
    }

    static void addToBook(std::map<double, ExchangeQtyMap>& book,
                          double price,
                          const std::string& exchange,
                          int qty) {
        if (qty <= 0) return;
        book[price][exchange] += qty;
    }

    static void cleanupPriceLevel(std::map<double, ExchangeQtyMap, std::greater<double>>& book,
                                  typename std::map<double, ExchangeQtyMap, std::greater<double>>::iterator it) {
        for (auto exIt = it->second.begin(); exIt != it->second.end(); ) {
            if (exIt->second <= 0) {
                exIt = it->second.erase(exIt);
            } else {
                ++exIt;
            }
        }
        if (it->second.empty()) {
            book.erase(it);
        }
    }

    static void cleanupPriceLevel(std::map<double, ExchangeQtyMap>& book,
                                  typename std::map<double, ExchangeQtyMap>::iterator it) {
        for (auto exIt = it->second.begin(); exIt != it->second.end(); ) {
            if (exIt->second <= 0) {
                exIt = it->second.erase(exIt);
            } else {
                ++exIt;
            }
        }
        if (it->second.empty()) {
            book.erase(it);
        }
    }

    static std::optional<Quote> bestFromBidBook(
        const std::map<double, ExchangeQtyMap, std::greater<double>>& book) {

        if (book.empty()) return std::nullopt;

        const auto& [price, exMap] = *book.begin();

        // Pick one exchange at that best price. Here we choose the exchange
        // with the largest displayed quantity at that price.
        std::string bestExchange;
        int bestQty = 0;
        for (const auto& [ex, qty] : exMap) {
            if (qty > bestQty) {
                bestQty = qty;
                bestExchange = ex;
            }
        }

        if (bestQty <= 0) return std::nullopt;
        return Quote{price, bestQty, bestExchange};
    }

    static std::optional<Quote> bestFromAskBook(
        const std::map<double, ExchangeQtyMap>& book) {

        if (book.empty()) return std::nullopt;

        const auto& [price, exMap] = *book.begin();

        std::string bestExchange;
        int bestQty = 0;
        for (const auto& [ex, qty] : exMap) {
            if (qty > bestQty) {
                bestQty = qty;
                bestExchange = ex;
            }
        }

        if (bestQty <= 0) return std::nullopt;
        return Quote{price, bestQty, bestExchange};
    }

    void matchBuy(int& qty) {
        // Buy market/marketable limit order consumes asks from lowest upward.
        while (qty > 0 && !asks_.empty()) {
            auto bestAskIt = asks_.begin();

            for (auto exIt = bestAskIt->second.begin();
                 exIt != bestAskIt->second.end() && qty > 0; ) {
                int traded = std::min(qty, exIt->second);
                qty -= traded;
                exIt->second -= traded;

                if (exIt->second == 0) {
                    exIt = bestAskIt->second.erase(exIt);
                } else {
                    ++exIt;
                }
            }

            if (bestAskIt->second.empty()) {
                asks_.erase(bestAskIt);
            }
        }
    }

    void matchSell(int& qty) {
        // Sell market/marketable limit order consumes bids from highest downward.
        while (qty > 0 && !bids_.empty()) {
            auto bestBidIt = bids_.begin();

            for (auto exIt = bestBidIt->second.begin();
                 exIt != bestBidIt->second.end() && qty > 0; ) {
                int traded = std::min(qty, exIt->second);
                qty -= traded;
                exIt->second -= traded;

                if (exIt->second == 0) {
                    exIt = bestBidIt->second.erase(exIt);
                } else {
                    ++exIt;
                }
            }

            if (bestBidIt->second.empty()) {
                bids_.erase(bestBidIt);
            }
        }
    }

public:
    // Add a limit order.
    // If crossing the spread, it trades immediately; remaining qty rests on book.
    void addLimitOrder(const std::string& exchange, Side side, double price, int qty) {
        if (qty <= 0) return;

        if (side == Side::Buy) {
            // Match against asks while marketable.
            while (qty > 0 && !asks_.empty() && asks_.begin()->first <= price) {
                auto bestAskIt = asks_.begin();

                for (auto exIt = bestAskIt->second.begin();
                     exIt != bestAskIt->second.end() && qty > 0; ) {
                    int traded = std::min(qty, exIt->second);
                    qty -= traded;
                    exIt->second -= traded;

                    if (exIt->second == 0) {
                        exIt = bestAskIt->second.erase(exIt);
                    } else {
                        ++exIt;
                    }
                }

                if (bestAskIt->second.empty()) {
                    asks_.erase(bestAskIt);
                }
            }

            if (qty > 0) {
                addToBook(bids_, price, exchange, qty);
            }
        } else {
            // Sell limit order
            while (qty > 0 && !bids_.empty() && bids_.begin()->first >= price) {
                auto bestBidIt = bids_.begin();

                for (auto exIt = bestBidIt->second.begin();
                     exIt != bestBidIt->second.end() && qty > 0; ) {
                    int traded = std::min(qty, exIt->second);
                    qty -= traded;
                    exIt->second -= traded;

                    if (exIt->second == 0) {
                        exIt = bestBidIt->second.erase(exIt);
                    } else {
                        ++exIt;
                    }
                }

                if (bestBidIt->second.empty()) {
                    bids_.erase(bestBidIt);
                }
            }

            if (qty > 0) {
                addToBook(asks_, price, exchange, qty);
            }
        }
    }

    // Add a market order.
    void addMarketOrder(Side side, int qty) {
        if (qty <= 0) return;

        if (side == Side::Buy) {
            matchBuy(qty);
        } else {
            matchSell(qty);
        }
    }

    // Cancel quantity from a resting limit order at an exchange+price.
    void cancelOrder(const std::string& exchange, Side side, double price, int qty) {
        if (qty <= 0) return;

        if (side == Side::Buy) {
            auto it = bids_.find(price);
            if (it == bids_.end()) return;

            auto exIt = it->second.find(exchange);
            if (exIt == it->second.end()) return;

            exIt->second -= qty;
            cleanupPriceLevel(bids_, it);
        } else {
            auto it = asks_.find(price);
            if (it == asks_.end()) return;

            auto exIt = it->second.find(exchange);
            if (exIt == it->second.end()) return;

            exIt->second -= qty;
            cleanupPriceLevel(asks_, it);
        }
    }

    NBBO getNBBO() const {
        return NBBO{
            bestFromBidBook(bids_),
            bestFromAskBook(asks_)
        };
    }

    void printBook() const {
        std::cout << "----- BIDS -----\n";
        for (const auto& [price, exMap] : bids_) {
            std::cout << price << " : total=" << totalQuantity(exMap) << " [ ";
            for (const auto& [ex, qty] : exMap) {
                std::cout << ex << ":" << qty << " ";
            }
            std::cout << "]\n";
        }

        std::cout << "----- ASKS -----\n";
        for (const auto& [price, exMap] : asks_) {
            std::cout << price << " : total=" << totalQuantity(exMap) << " [ ";
            for (const auto& [ex, qty] : exMap) {
                std::cout << ex << ":" << qty << " ";
            }
            std::cout << "]\n";
        }
    }
};

static void printNBBO(const MultiExchangeOrderBook::NBBO& nbbo) {
    std::cout << "NBBO:\n";

    if (nbbo.bestBid) {
        std::cout << "  Best Bid: "
                  << nbbo.bestBid->price
                  << " x " << nbbo.bestBid->quantity
                  << " @ " << nbbo.bestBid->exchange << "\n";
    } else {
        std::cout << "  Best Bid: NONE\n";
    }

    if (nbbo.bestAsk) {
        std::cout << "  Best Ask: "
                  << nbbo.bestAsk->price
                  << " x " << nbbo.bestAsk->quantity
                  << " @ " << nbbo.bestAsk->exchange << "\n";
    } else {
        std::cout << "  Best Ask: NONE\n";
    }
}

int main() {
    MultiExchangeOrderBook ob;

    ob.addLimitOrder("NYSE",   MultiExchangeOrderBook::Side::Buy,  100.00, 200);
    ob.addLimitOrder("NASDAQ", MultiExchangeOrderBook::Side::Buy,  101.00, 150);
    ob.addLimitOrder("IEX",    MultiExchangeOrderBook::Side::Sell, 103.00, 120);
    ob.addLimitOrder("BATS",   MultiExchangeOrderBook::Side::Sell, 102.00, 180);

    ob.printBook();
    printNBBO(ob.getNBBO());

    std::cout << "\nAdd market buy 100\n";
    ob.addMarketOrder(MultiExchangeOrderBook::Side::Buy, 100);

    ob.printBook();
    printNBBO(ob.getNBBO());

    std::cout << "\nAdd sell limit 101 for 100 on NYSE (crosses best bid)\n";
    ob.addLimitOrder("NYSE", MultiExchangeOrderBook::Side::Sell, 101.00, 100);

    ob.printBook();
    printNBBO(ob.getNBBO());

    return 0;
}
