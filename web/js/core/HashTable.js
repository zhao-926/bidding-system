// HashTable.js — 自刻雜湊表，separate chaining（對應 core/HashTable.h）
//
//   buckets[0] -> null
//   buckets[1] -> [keyA|valueA] -> [keyB|valueB] -> null   <- A、B 碰撞
//   buckets[2] -> [keyC|valueC] -> null
//
// 負載因子 = 元素數 / bucket 數；超過 0.75 就把 bucket 數翻倍並重新分配（rehash）。
// 注意：不使用 JS 內建的 Map / Set / 物件當字典，bucket 只是一塊固定大小的陣列。
//
// 與 C++ 版的差異：
//   - C++ 用多載函式依型別選雜湊；這裡在建構時傳入雜湊函式（hashInt 或 hashString）
//   - find 找不到回傳 null（所以 value 不要存 null）

// 字串：djb2（h 從 5381 開始，每個字元 h = h * 33 + 字元）。>>> 0 讓結果保持為 32 位元無號整數
// O(L)，L 為字串長度
export function hashString(text) {
    let h = 5381;
    for (let i = 0; i < text.length; i++) {
        h = (Math.imul(h, 33) + text.charCodeAt(i)) >>> 0;
    }
    return h;
}

// 整數：乘法雜湊（乘上 2654435769，再把高 16 位元折回低位元）
// O(1)
export function hashInt(key) {
    let h = Math.imul(key | 0, 0x9e3779b1) >>> 0;   // 0x9e3779b1 = 2654435769
    h = (h ^ (h >>> 16)) >>> 0;
    return h;
}

class Node {
    constructor(key, value, next) {
        this.key = key;
        this.value = value;
        this.next = next;
    }
}

export class HashTable {
    // hashFunction：hashInt 或 hashString。一開始有 8 個 bucket
    // O(1)
    constructor(hashFunction) {
        this._hash = hashFunction;
        this._buckets = null;
        this._bucketCount = 0;
        this._size = 0;
        this._allocateBuckets(8);
    }

    // 插入 (key, value)。key 已存在就覆蓋 value。回傳 true = 新增；false = 只是更新
    // O(1) average, O(n) worst（所有 key 碰撞在同一格；rehash 單次 O(n) 但攤銷 O(1)）
    insert(key, value) {
        const index = this._bucketIndex(key);
        for (let cur = this._buckets[index]; cur !== null; cur = cur.next) {
            if (cur.key === key) {
                cur.value = value;               // key 已存在：更新
                return false;
            }
        }
        this._buckets[index] = new Node(key, value, this._buckets[index]);   // 插在鏈的最前面
        this._size++;
        // size/count > 3/4  等價於  size*4 > count*3
        if (this._size * 4 > this._bucketCount * 3) {
            this._rehash(this._bucketCount * 2);
        }
        return true;
    }

    // 找 key 對應的 value；找不到回傳 null
    // O(1) average, O(n) worst
    find(key) {
        const index = this._bucketIndex(key);
        for (let cur = this._buckets[index]; cur !== null; cur = cur.next) {
            if (cur.key === key) {
                return cur.value;
            }
        }
        return null;
    }

    // O(1) average, O(n) worst
    contains(key) { return this.find(key) !== null; }

    // 刪除 key；成功回傳 true，不存在回傳 false
    // O(1) average, O(n) worst
    remove(key) {
        const index = this._bucketIndex(key);
        let prev = null;                         // 記住前一個節點，才能把鏈接起來
        for (let cur = this._buckets[index]; cur !== null; prev = cur, cur = cur.next) {
            if (cur.key === key) {
                if (prev === null) {
                    this._buckets[index] = cur.next;     // 刪的是鏈頭
                } else {
                    prev.next = cur.next;                // 刪的是鏈中間或尾端
                }
                this._size--;
                return true;
            }
        }
        return false;
    }

    // 走訪所有元素，對每一筆呼叫 callback(key, value)（順序不固定）
    // O(n + bucketCount)
    forEach(callback) {
        for (let i = 0; i < this._bucketCount; i++) {
            for (let cur = this._buckets[i]; cur !== null; cur = cur.next) {
                callback(cur.key, cur.value);
            }
        }
    }

    // 清空所有元素（bucket 數量維持不變）
    // O(bucketCount)
    clear() {
        for (let i = 0; i < this._bucketCount; i++) {
            this._buckets[i] = null;
        }
        this._size = 0;
    }

    // O(1)
    size() { return this._size; }

    // O(1)
    empty() { return this._size === 0; }

    // O(1)
    bucketCount() { return this._bucketCount; }

    // 負載因子 = 元素數 / bucket 數
    // O(1)
    loadFactor() { return this._size / this._bucketCount; }

    // 第 index 個 bucket 的鏈長度；index 不合法回傳 0（給「資料結構檢視」面板用）
    // O(該鏈長度)
    bucketLength(index) {
        if (!Number.isInteger(index) || index < 0 || index >= this._bucketCount) {
            return 0;
        }
        let length = 0;
        for (let cur = this._buckets[index]; cur !== null; cur = cur.next) {
            length++;
        }
        return length;
    }

    // 走訪第 index 個 bucket 的碰撞鏈（由鏈頭到鏈尾），對每一筆呼叫 callback(key, value)
    // 只有 JS 版有：給「資料結構檢視」面板顯示碰撞鏈用
    // O(該鏈長度)
    forEachInBucket(index, callback) {
        if (!Number.isInteger(index) || index < 0 || index >= this._bucketCount) {
            return;
        }
        for (let cur = this._buckets[index]; cur !== null; cur = cur.next) {
            callback(cur.key, cur.value);
        }
    }

    // 最長的鏈有多長
    // O(n + bucketCount)
    maxChainLength() {
        let longest = 0;
        for (let i = 0; i < this._bucketCount; i++) {
            const length = this.bucketLength(i);
            if (length > longest) {
                longest = length;
            }
        }
        return longest;
    }

    // 配置 count 個空 bucket
    _allocateBuckets(count) {
        this._buckets = new Array(count);
        for (let i = 0; i < count; i++) {
            this._buckets[i] = null;
        }
        this._bucketCount = count;
    }

    // key 要放進哪一格
    // O(1)（字串 key 為 O(L)）
    _bucketIndex(key) {
        return this._hash(key) % this._bucketCount;
    }

    // 擴容並重新分配：把舊節點「搬」到新的 bucket 陣列（直接改指標，不重新建立節點）
    // O(n + 新 bucket 數)
    _rehash(newBucketCount) {
        const oldBuckets = this._buckets;
        const oldBucketCount = this._bucketCount;
        this._allocateBuckets(newBucketCount);
        for (let i = 0; i < oldBucketCount; i++) {
            let cur = oldBuckets[i];
            while (cur !== null) {
                const next = cur.next;                   // 先記住下一個
                const index = this._bucketIndex(cur.key);    // 用「新容量」重新算位置
                cur.next = this._buckets[index];         // 接到新鏈的最前面
                this._buckets[index] = cur;
                cur = next;
            }
        }
    }
}
