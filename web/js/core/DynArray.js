// DynArray.js — 自刻動態陣列（對應 core/DynArray.h）
//
// 概念與 C++ 版完全相同：底層是一塊「固定大小」的記憶體（new Array(capacity)），
// 放滿時配置兩倍大的新記憶體並搬資料，所以 pushBack 攤銷 O(1)。
// 注意：這裡「不」使用 Array 的 push / pop / splice / slice 等方法，只用「索引存取」。
//
// 用途：MaxHeap 的底層儲存、商品清單、使用者的「我賣的／我出價的」清單。

export class DynArray {
    // O(1)
    constructor() {
        this._data = null;     // 底層固定大小的陣列
        this._size = 0;        // 已使用數量
        this._capacity = 0;    // 總容量
    }

    // 在尾端加入一個元素
    // O(1) amortized
    pushBack(value) {
        if (this._size === this._capacity) {
            this._grow();                       // 滿了，容量翻倍
        }
        this._data[this._size] = value;
        this._size++;
    }

    // 移除最後一個元素（陣列不可為空）
    // O(1)
    popBack() {
        if (this._size === 0) {
            throw new Error('DynArray.popBack：陣列是空的');
        }
        this._size--;
        this._data[this._size] = undefined;     // 放掉參照，讓垃圾回收可以回收
    }

    // 取得第 index 個元素；越界會丟出錯誤
    // O(1)
    get(index) {
        if (!this.isValidIndex(index)) {
            throw new Error('DynArray.get：索引超出範圍 ' + index);
        }
        return this._data[index];
    }

    // 設定第 index 個元素；越界會丟出錯誤
    // O(1)
    set(index, value) {
        if (!this.isValidIndex(index)) {
            throw new Error('DynArray.set：索引超出範圍 ' + index);
        }
        this._data[index] = value;
    }

    // 最後一個元素（陣列不可為空）
    // O(1)
    back() {
        if (this._size === 0) {
            throw new Error('DynArray.back：陣列是空的');
        }
        return this._data[this._size - 1];
    }

    // 刪除第 index 個元素，後面的元素整體往前補位；索引不合法回傳 false
    // O(n)
    removeAt(index) {
        if (!this.isValidIndex(index)) {
            return false;
        }
        for (let i = index; i < this._size - 1; i++) {
            this._data[i] = this._data[i + 1];  // 往前補位
        }
        this._size--;
        this._data[this._size] = undefined;
        return true;
    }

    // 線性搜尋：是否有等於 value 的元素（用 === 比較，適用數字與字串）
    // O(n)
    contains(value) {
        for (let i = 0; i < this._size; i++) {
            if (this._data[i] === value) {
                return true;
            }
        }
        return false;
    }

    // 清空（保留已配置的記憶體）
    // O(1)
    clear() {
        this._size = 0;
    }

    // O(1)
    isValidIndex(index) { return Number.isInteger(index) && index >= 0 && index < this._size; }

    // O(1)
    size() { return this._size; }

    // O(1)
    capacity() { return this._capacity; }

    // O(1)
    empty() { return this._size === 0; }

    // 淺複製成一個新的 DynArray（對應 C++ 的複製建構）
    // O(n)
    clone() {
        const copy = new DynArray();
        for (let i = 0; i < this._size; i++) {
            copy.pushBack(this._data[i]);
        }
        return copy;
    }

    // 容量翻倍（第一次從 0 變成 4）
    // O(n)
    _grow() {
        const newCapacity = (this._capacity === 0) ? 4 : this._capacity * 2;
        const newData = new Array(newCapacity);
        for (let i = 0; i < this._size; i++) {
            newData[i] = this._data[i];         // 把舊資料搬到新記憶體
        }
        this._data = newData;
        this._capacity = newCapacity;
    }
}
