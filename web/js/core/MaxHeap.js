// MaxHeap.js — 自刻最大堆積 / Priority Queue（對應 core/MaxHeap.h）
//
// 用「陣列」存放完全二元樹：第 i 個節點的父節點 = (i-1)/2，左子 = 2i+1，右子 = 2i+2。
// 堆積性質：每個父節點的優先權都 >= 它的子節點，所以堆頂（index 0）永遠是最高價。
//
// 撤標需要「刪除任意一筆出價」，所以另外用 HashTable 記錄「id -> 目前在陣列的位置」，
// 不用掃描整個陣列就能找到要刪的元素，刪除總共 O(log n)。
//
// 對元素 T 的要求（Bid 會實作）：
//   getId()                    唯一編號（例如 bidId）
//   hasHigherPriority(other)   自己的優先權是否「嚴格高於」other

import { DynArray } from './DynArray.js';
import { HashTable, hashInt } from './HashTable.js';

export class MaxHeap {
    // O(1)
    constructor() {
        this._data = new DynArray();               // 堆積本體
        this._indexOfId = new HashTable(hashInt);  // id -> 該元素目前在 _data 的位置
    }

    // 加入一個元素；若相同 id 已存在則不加入並回傳 false
    // O(log n)（放到陣列尾端，再往上調整 sift-up）
    push(item) {
        const id = item.getId();
        if (this._indexOfId.contains(id)) {
            return false;
        }
        this._data.pushBack(item);
        const index = this._data.size() - 1;
        this._indexOfId.insert(id, index);
        this._siftUp(index);
        return true;
    }

    // 取得優先權最高的元素（不移除）；堆積為空回傳 null
    // O(1)
    top() {
        if (this._data.empty()) {
            return null;
        }
        return this._data.get(0);
    }

    // 移除堆頂；空堆積回傳 false
    // O(log n)（最後一個元素補到堆頂，再往下調整 sift-down）
    pop() {
        if (this._data.empty()) {
            return false;
        }
        this._removeAtIndex(0);
        return true;
    }

    // 依 id 刪除任意元素（撤標用）；找不到回傳 false
    // O(log n)（用 HashTable 以 O(1) 找到位置，再 O(log n) 調整）
    removeById(id) {
        const index = this._indexOfId.find(id);
        if (index === null) {
            return false;
        }
        this._removeAtIndex(index);
        return true;
    }

    // O(1) average
    contains(id) { return this._indexOfId.contains(id); }

    // 取得陣列第 index 格的元素（給「資料結構檢視」畫樹狀圖用）
    // O(1)
    at(index) { return this._data.get(index); }

    // O(1)
    size() { return this._data.size(); }

    // O(1)
    empty() { return this._data.empty(); }

    // 清空
    // O(n)
    clear() {
        this._data.clear();
        this._indexOfId.clear();
    }

    // 檢查堆積性質與「id -> 位置」索引是否都正確（測試用）
    // O(n)
    isValid() {
        if (this._indexOfId.size() !== this._data.size()) {
            return false;
        }
        for (let i = 0; i < this._data.size(); i++) {
            const index = this._indexOfId.find(this._data.get(i).getId());
            if (index === null || index !== i) {
                return false;                       // 索引與實際位置不符
            }
            if (i > 0) {
                const parent = Math.floor((i - 1) / 2);
                if (this._data.get(i).hasHigherPriority(this._data.get(parent))) {
                    return false;                   // 子節點比父節點大，違反堆積性質
                }
            }
        }
        return true;
    }

    // 交換兩個位置的元素，並同步更新索引表
    // O(1) average
    _swapNodes(i, j) {
        const temp = this._data.get(i);
        this._data.set(i, this._data.get(j));
        this._data.set(j, temp);
        this._indexOfId.insert(this._data.get(i).getId(), i);
        this._indexOfId.insert(this._data.get(j).getId(), j);
    }

    // 往上調整：只要比父節點「優先權高」就跟父節點交換；回傳最後停下的位置
    // O(log n)
    _siftUp(index) {
        while (index > 0) {
            const parent = Math.floor((index - 1) / 2);
            if (this._data.get(index).hasHigherPriority(this._data.get(parent))) {
                this._swapNodes(index, parent);
                index = parent;
            } else {
                break;
            }
        }
        return index;
    }

    // 往下調整：和「優先權較高的那個子節點」比，子節點更高就交換
    // O(log n)
    _siftDown(index) {
        const count = this._data.size();
        while (true) {
            const left = 2 * index + 1;
            const right = 2 * index + 2;
            let best = index;                       // index、left、right 三者中最高的
            if (left < count && this._data.get(left).hasHigherPriority(this._data.get(best))) {
                best = left;
            }
            if (right < count && this._data.get(right).hasHigherPriority(this._data.get(best))) {
                best = right;
            }
            if (best === index) {
                break;                              // 已經比兩個子節點都高，結束
            }
            this._swapNodes(index, best);
            index = best;
        }
    }

    // 刪除第 index 格的元素：
    //   1. 跟最後一個元素交換  2. 移除最後一格  3. 被換上來的元素可能太大或太小，上下各調整一次
    // O(log n)
    _removeAtIndex(index) {
        const removedId = this._data.get(index).getId();
        const lastIndex = this._data.size() - 1;
        if (index !== lastIndex) {
            this._swapNodes(index, lastIndex);
        }
        this._data.popBack();
        this._indexOfId.remove(removedId);

        if (index < this._data.size()) {            // 刪的不是最後一格，才需要調整
            const newIndex = this._siftUp(index);
            if (newIndex === index) {               // 沒有往上動，就可能需要往下
                this._siftDown(index);
            }
        }
    }
}
