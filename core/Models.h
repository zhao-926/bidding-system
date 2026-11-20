// Models.h — 資料模型：Bid（出價）、Item（商品）、User（使用者）
//
// 金額一律用「元」為單位的整數（long long），避免浮點誤差；
// 時間用「模擬時鐘的分鐘數」（long long），由 AuctionSystem 的 now 控制。

#ifndef MODELS_H
#define MODELS_H

#include <string>

#include "DynArray.h"
#include "LinkedList.h"
#include "MaxHeap.h"

// 一筆出價
struct Bid {
    int bidId;
    int itemId;
    std::string bidder;
    long long amount;
    long long time;      // 出價當下的模擬時鐘
    bool active;         // 撤標後設為 false（歷史保留，但從 Heap 移除）

    Bid() : bidId(0), itemId(0), amount(0), time(0), active(true) {}

    // ---- 以下兩個函式是 MaxHeap<Bid> 需要的 ----

    // O(1)
    int getId() const { return bidId; }

    // 比較規則：金額高者優先；金額相同時，時間早者優先（先出價者勝）
    // O(1)
    bool hasHigherPriority(const Bid& other) const {
        if (amount != other.amount) {
            return amount > other.amount;
        }
        return time < other.time;
    }
};

// 商品在 BST 裡的排序鍵：(結標時間, 商品ID)
// 只用結標時間會有「兩件商品同時結標」而 key 重複的問題，所以加上商品 ID 當第二層比較。
struct ItemKey {
    long long endTime;
    int itemId;

    ItemKey() : endTime(0), itemId(0) {}
    ItemKey(long long time, int id) : endTime(time), itemId(id) {}

    // O(1)
    bool operator<(const ItemKey& other) const {
        if (endTime != other.endTime) {
            return endTime < other.endTime;
        }
        return itemId < other.itemId;
    }
};

// 一件商品
struct Item {
    enum Status { OPEN, CLOSED };

    int itemId;
    std::string title;
    std::string seller;
    std::string description;
    long long startPrice;
    long long minIncrement;     // 最小加價幅度（可為 0）
    long long endTime;
    Status status;
    std::string winner;         // 結標後填入；流標（無人出價）則為空字串
    long long finalPrice;       // 得標價；流標為 0
    MaxHeap<Bid> bidHeap;       // 「有效」出價，堆頂 = 目前最高價
    LinkedList<Bid> bidHistory; // 全部出價紀錄（含已撤標），新的插在最前面

    Item() : itemId(0), startPrice(0), minIncrement(0), endTime(0), status(OPEN), finalPrice(0) {}
};

// 一位使用者
struct User {
    std::string username;
    DynArray<int> sellingItems;   // 我刊登的商品 ID
    DynArray<int> biddingItems;   // 我出價過的商品 ID（不重複）
    DynArray<int> wonItems;       // 我得標的商品 ID
};

#endif  // MODELS_H
