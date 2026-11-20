// LinkedList.js — 自刻單向鏈結串列（對應 core/LinkedList.h）
//
//   head -> [最新] -> [次新] -> ... -> [最舊] -> null
//
// 出價歷史永遠是「新的放最前面」，pushFront 只要 O(1)；查看歷史時由新到舊走訪。
// 與 C++ 版的差異：C++ 回傳指標（找不到為 nullptr），這裡回傳元素本身（找不到為 null）。

class Node {
    constructor(value, next) {
        this.value = value;
        this.next = next;
    }
}

export class LinkedList {
    // O(1)
    constructor() {
        this._head = null;
        this._size = 0;
    }

    // 在最前面插入一個元素
    // O(1)
    pushFront(value) {
        this._head = new Node(value, this._head);   // 新節點的 next 指向舊的頭
        this._size++;
    }

    // 移除最前面的元素；空串列回傳 false
    // O(1)
    popFront() {
        if (this._head === null) {
            return false;
        }
        this._head = this._head.next;               // 頭往後移一格
        this._size--;
        return true;
    }

    // 最前面的元素；空串列回傳 null
    // O(1)
    front() {
        return (this._head === null) ? null : this._head.value;
    }

    // 從頭到尾走訪，對每個元素呼叫 callback(value)
    // O(n)
    forEach(callback) {
        for (let cur = this._head; cur !== null; cur = cur.next) {
            callback(cur.value);
        }
    }

    // 找第一個讓 predicate(元素) 為 true 的元素；找不到回傳 null
    // 回傳的是同一個物件，呼叫端可以直接修改（例如把出價標記為已撤標）
    // O(n)
    find(predicate) {
        for (let cur = this._head; cur !== null; cur = cur.next) {
            if (predicate(cur.value)) {
                return cur.value;
            }
        }
        return null;
    }

    // 刪除全部節點
    // O(1)（JS 有垃圾回收，直接放掉頭節點即可；C++ 版需要逐一 delete，是 O(n)）
    clear() {
        this._head = null;
        this._size = 0;
    }

    // O(1)
    size() { return this._size; }

    // O(1)
    empty() { return this._size === 0; }
}
