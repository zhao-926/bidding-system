// MaxHeap.h — 自刻最大堆積 / Priority Queue（資料結構 #4）
//
// 概念：用「陣列」存放一棵完全二元樹，堆頂（index 0）永遠是優先權最高的元素。
//   第 i 個節點：父節點 = (i-1)/2，左子 = 2i+1，右子 = 2i+2
//   堆積性質：每個父節點的優先權都 >= 它的兩個子節點。
//
// 為什麼最高出價用 Heap？
//   每次出價只要 push（O(log n)），查最高價只要看堆頂（O(1)）；
//   如果改用「每次查詢都排序」要 O(n log n)。
//
// 撤標需要「刪除任意一筆出價」，所以另外用 HashTable 記錄「id -> 目前在陣列的位置」，
// 這樣不用掃描整個陣列就能找到要刪的元素（O(1)），刪除總共是 O(log n)。
//
// 對元素型別 T 的要求（Bid 會實作這兩個函式）：
//   int  getId() const;                              // 唯一編號（例如 bidId）
//   bool hasHigherPriority(const T& other) const;    // 自己的優先權是否「嚴格高於」other

#ifndef MAXHEAP_H
#define MAXHEAP_H

#include "DynArray.h"
#include "HashTable.h"

template <typename T>
class MaxHeap {
public:
    // O(1)
    MaxHeap() {}

    // 加入一個元素；若相同 id 已存在則不加入並回傳 false
    // O(log n)（放到陣列尾端，再往上調整 sift-up）
    bool push(const T& item) {
        int id = item.getId();
        if (indexOfId_.contains(id)) {
            return false;
        }
        data_.pushBack(item);
        int index = data_.size() - 1;
        indexOfId_.insert(id, index);
        siftUp(index);
        return true;
    }

    // 取得優先權最高的元素（不移除）；堆積為空回傳 nullptr
    // O(1)
    const T* top() const {
        if (data_.empty()) {
            return nullptr;
        }
        return &data_.get(0);
    }

    // 移除堆頂；空堆積回傳 false
    // O(log n)（最後一個元素補到堆頂，再往下調整 sift-down）
    bool pop() {
        if (data_.empty()) {
            return false;
        }
        removeAtIndex(0);
        return true;
    }

    // 依 id 刪除任意元素（撤標用）；找不到回傳 false
    // O(log n)（用 HashTable 以 O(1) 找到位置，再 O(log n) 調整）
    bool removeById(int id) {
        int* indexPointer = indexOfId_.find(id);
        if (indexPointer == nullptr) {
            return false;
        }
        removeAtIndex(*indexPointer);
        return true;
    }

    // 是否含有此 id
    // O(1) average
    bool contains(int id) const { return indexOfId_.contains(id); }

    // 取得陣列第 index 格的元素（給「資料結構檢視」畫樹狀圖用；index 必須合法）
    // O(1)
    const T& at(int index) const { return data_.get(index); }

    // O(1)
    int size() const { return data_.size(); }

    // O(1)
    bool empty() const { return data_.empty(); }

    // 清空
    // O(n)
    void clear() {
        data_.clear();
        indexOfId_.clear();
    }

    // 檢查堆積性質與「id -> 位置」索引是否都正確（測試用）
    // O(n)
    bool isValid() const {
        if (indexOfId_.size() != data_.size()) {
            return false;
        }
        for (int i = 0; i < data_.size(); i++) {
            const int* indexPointer = indexOfId_.find(data_.get(i).getId());
            if (indexPointer == nullptr || *indexPointer != i) {
                return false;                         // 索引與實際位置不符
            }
            if (i > 0) {
                int parent = (i - 1) / 2;
                if (data_.get(i).hasHigherPriority(data_.get(parent))) {
                    return false;                     // 子節點比父節點大，違反堆積性質
                }
            }
        }
        return true;
    }

private:
    DynArray<T> data_;               // 堆積本體（完全二元樹存成陣列）
    HashTable<int, int> indexOfId_;  // id -> 該元素目前在 data_ 的位置

    // 交換兩個位置的元素，並同步更新索引表
    // O(1) average
    void swapNodes(int i, int j) {
        T temp = data_.get(i);
        data_.set(i, data_.get(j));
        data_.set(j, temp);
        indexOfId_.insert(data_.get(i).getId(), i);
        indexOfId_.insert(data_.get(j).getId(), j);
    }

    // 往上調整：只要比父節點「優先權高」就跟父節點交換；回傳最後停下的位置
    // O(log n)（樹高）
    int siftUp(int index) {
        while (index > 0) {
            int parent = (index - 1) / 2;
            if (data_.get(index).hasHigherPriority(data_.get(parent))) {
                swapNodes(index, parent);
                index = parent;
            } else {
                break;
            }
        }
        return index;
    }

    // 往下調整：和「優先權較高的那個子節點」比，子節點更高就交換
    // O(log n)（樹高）
    void siftDown(int index) {
        int count = data_.size();
        while (true) {
            int left = 2 * index + 1;
            int right = 2 * index + 2;
            int best = index;                 // 目前 index、left、right 三者中最高的
            if (left < count && data_.get(left).hasHigherPriority(data_.get(best))) {
                best = left;
            }
            if (right < count && data_.get(right).hasHigherPriority(data_.get(best))) {
                best = right;
            }
            if (best == index) {
                break;                        // 已經比兩個子節點都高，結束
            }
            swapNodes(index, best);
            index = best;
        }
    }

    // 刪除第 index 格的元素：
    //   1. 跟最後一個元素交換  2. 移除最後一格  3. 被換上來的元素可能太大或太小，上下各調整一次
    // O(log n)
    void removeAtIndex(int index) {
        int removedId = data_.get(index).getId();
        int lastIndex = data_.size() - 1;
        if (index != lastIndex) {
            swapNodes(index, lastIndex);
        }
        data_.popBack();
        indexOfId_.remove(removedId);

        if (index < data_.size()) {           // 刪的不是最後一格，才需要調整
            int newIndex = siftUp(index);
            if (newIndex == index) {          // 沒有往上動，就可能需要往下
                siftDown(index);
            }
        }
    }
};

#endif  // MAXHEAP_H
