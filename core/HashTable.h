// HashTable.h — 自刻雜湊表（資料結構 #3），使用 separate chaining（鏈結法）
//
// 概念：
//   buckets_ 是一個「指標陣列」，每格（bucket）指向一條鏈結串列。
//   key 經過雜湊函式變成整數，再對容量取餘數，決定要放進哪一格。
//   不同 key 若落在同一格（碰撞），就串在同一條鏈上。
//
//   buckets_[0] -> nullptr
//   buckets_[1] -> [keyA|valueA] -> [keyB|valueB] -> nullptr   <- A、B 碰撞
//   buckets_[2] -> [keyC|valueC] -> nullptr
//
// 負載因子（load factor）= 元素數 / bucket 數。超過 0.75 就把 bucket 數翻倍並
// 重新分配所有元素（rehash），讓每條鏈保持很短，所以平均仍是 O(1)。
//
// 用途：商品 ID -> 商品、使用者名稱 -> 使用者、bidId -> 出價在 Heap 內的位置。

#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <string>

// ---------- 雜湊函式（依 key 型別各一個，HashTable 內以 hashKey(key) 呼叫）----------

// 字串：djb2 演算法。h 從 5381 開始，每讀一個字元就 h = h * 33 + 字元。
// O(L)，L 為字串長度
inline unsigned int hashKey(const std::string& text) {
    unsigned int h = 5381;
    for (std::size_t i = 0; i < text.size(); i++) {
        h = h * 33 + static_cast<unsigned char>(text[i]);
    }
    return h;
}

// 整數：乘法雜湊。乘上一個大質數 2654435769（黃金比例的整數近似值）把數字打散，
// 再把高 16 位元折回低位元，讓取餘數時也能用到高位元的資訊。
// O(1)
inline unsigned int hashKey(int key) {
    unsigned int h = static_cast<unsigned int>(key) * 2654435769u;
    return h ^ (h >> 16);
}

// ---------- 雜湊表本體 ----------

template <typename K, typename V>
class HashTable {
private:
    struct Node {
        K key;
        V value;
        Node* next;
        Node(const K& k, const V& v, Node* n) : key(k), value(v), next(n) {}
    };

public:
    // 一開始有 8 個 bucket
    // O(1)
    HashTable() : buckets_(nullptr), bucketCount_(0), size_(0) {
        allocateBuckets(8);
    }

    // 複製建構：把 other 的每一筆資料重新插入
    // O(n)
    HashTable(const HashTable& other) : buckets_(nullptr), bucketCount_(0), size_(0) {
        allocateBuckets(other.bucketCount_);
        copyFrom(other);
    }

    // O(n)
    HashTable& operator=(const HashTable& other) {
        if (this != &other) {
            clear();
            delete[] buckets_;
            buckets_ = nullptr;
            allocateBuckets(other.bucketCount_);
            copyFrom(other);
        }
        return *this;
    }

    // O(n)
    ~HashTable() {
        clear();
        delete[] buckets_;
    }

    // 插入 (key, value)。key 已存在就覆蓋 value。
    // 回傳 true = 新增了一筆；false = key 已存在，只是更新。
    // O(1) average, O(n) worst（所有 key 碰撞在同一格；rehash 時單次 O(n) 但攤銷 O(1)）
    bool insert(const K& key, const V& value) {
        int index = bucketIndex(key);
        for (Node* cur = buckets_[index]; cur != nullptr; cur = cur->next) {
            if (cur->key == key) {
                cur->value = value;       // key 已存在：更新
                return false;
            }
        }
        // 新 key：插在這條鏈的最前面（O(1)）
        buckets_[index] = new Node(key, value, buckets_[index]);
        size_++;

        // 負載因子 > 0.75 就擴容（用整數運算：size/count > 3/4 等價於 size*4 > count*3）
        if (size_ * 4 > bucketCount_ * 3) {
            rehash(bucketCount_ * 2);
        }
        return true;
    }

    // 找 key 對應的 value，回傳指標；找不到回傳 nullptr
    // 回傳指標而非複製，呼叫端可直接修改 value
    // O(1) average, O(n) worst
    V* find(const K& key) {
        int index = bucketIndex(key);
        for (Node* cur = buckets_[index]; cur != nullptr; cur = cur->next) {
            if (cur->key == key) {
                return &cur->value;
            }
        }
        return nullptr;
    }

    // 唯讀版本
    // O(1) average, O(n) worst
    const V* find(const K& key) const {
        int index = bucketIndex(key);
        for (Node* cur = buckets_[index]; cur != nullptr; cur = cur->next) {
            if (cur->key == key) {
                return &cur->value;
            }
        }
        return nullptr;
    }

    // O(1) average, O(n) worst
    bool contains(const K& key) const { return find(key) != nullptr; }

    // 刪除 key；成功回傳 true，key 不存在回傳 false
    // O(1) average, O(n) worst
    bool remove(const K& key) {
        int index = bucketIndex(key);
        Node* prev = nullptr;             // 記住前一個節點，才能把鏈接起來
        for (Node* cur = buckets_[index]; cur != nullptr; prev = cur, cur = cur->next) {
            if (cur->key == key) {
                if (prev == nullptr) {
                    buckets_[index] = cur->next;   // 刪的是鏈頭
                } else {
                    prev->next = cur->next;        // 刪的是鏈中間或尾端
                }
                delete cur;
                size_--;
                return true;
            }
        }
        return false;
    }

    // 走訪所有元素，對每一筆呼叫 callback(key, value)（順序不固定）
    // O(n + bucketCount)
    template <typename Func>
    void forEach(Func callback) const {
        for (int i = 0; i < bucketCount_; i++) {
            for (Node* cur = buckets_[i]; cur != nullptr; cur = cur->next) {
                callback(cur->key, cur->value);
            }
        }
    }

    // 清空所有元素（bucket 數量維持不變）
    // O(n + bucketCount)
    void clear() {
        for (int i = 0; i < bucketCount_; i++) {
            Node* cur = buckets_[i];
            while (cur != nullptr) {
                Node* next = cur->next;
                delete cur;
                cur = next;
            }
            buckets_[i] = nullptr;
        }
        size_ = 0;
    }

    // O(1)
    int size() const { return size_; }

    // O(1)
    bool empty() const { return size_ == 0; }

    // O(1)
    int bucketCount() const { return bucketCount_; }

    // 負載因子 = 元素數 / bucket 數
    // O(1)
    double loadFactor() const {
        return static_cast<double>(size_) / bucketCount_;
    }

    // 第 index 個 bucket 的鏈長度（給「資料結構檢視」面板用）；index 不合法回傳 0
    // O(該鏈長度)
    int bucketLength(int index) const {
        if (index < 0 || index >= bucketCount_) {
            return 0;
        }
        int length = 0;
        for (Node* cur = buckets_[index]; cur != nullptr; cur = cur->next) {
            length++;
        }
        return length;
    }

    // 最長的鏈有多長
    // O(n + bucketCount)
    int maxChainLength() const {
        int longest = 0;
        for (int i = 0; i < bucketCount_; i++) {
            int length = bucketLength(i);
            if (length > longest) {
                longest = length;
            }
        }
        return longest;
    }

private:
    Node** buckets_;     // 指標陣列：buckets_[i] 是第 i 條鏈的頭
    int bucketCount_;    // bucket 數量
    int size_;           // 元素總數

    // 配置 count 個空 bucket（new Node*[count]() 的 () 代表全部初始化為 nullptr）
    void allocateBuckets(int count) {
        buckets_ = new Node*[count]();
        bucketCount_ = count;
    }

    // key 要放進哪一格
    // O(1)（字串 key 為 O(L)）
    int bucketIndex(const K& key) const {
        return static_cast<int>(hashKey(key) % static_cast<unsigned int>(bucketCount_));
    }

    // 擴容並重新分配：配置新的 bucket 陣列，把舊節點「搬」過去（直接改指標，不重新 new）
    // O(n + 新 bucket 數)
    void rehash(int newBucketCount) {
        Node** oldBuckets = buckets_;
        int oldBucketCount = bucketCount_;

        allocateBuckets(newBucketCount);  // buckets_ / bucketCount_ 換成新的

        for (int i = 0; i < oldBucketCount; i++) {
            Node* cur = oldBuckets[i];
            while (cur != nullptr) {
                Node* next = cur->next;                 // 先記住下一個
                int index = bucketIndex(cur->key);      // 用「新容量」重新算位置
                cur->next = buckets_[index];            // 接到新鏈的最前面
                buckets_[index] = cur;
                cur = next;
            }
        }
        delete[] oldBuckets;
    }

    // 把 other 的所有資料插入自己（呼叫前自己必須是空的、bucket 已配置）
    // O(n)
    void copyFrom(const HashTable& other) {
        for (int i = 0; i < other.bucketCount_; i++) {
            for (Node* cur = other.buckets_[i]; cur != nullptr; cur = cur->next) {
                insert(cur->key, cur->value);
            }
        }
    }
};

#endif  // HASHTABLE_H
