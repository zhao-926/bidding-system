// LinkedList.h — 自刻單向鏈結串列（資料結構 #2）
//
// 概念：每個節點存「資料 + 指向下一個節點的指標」，只記錄頭節點 head_。
//
//   head_ -> [最新] -> [次新] -> ... -> [最舊] -> nullptr
//
// 為什麼出價歷史用它？出價永遠是「新的放最前面」，pushFront 只要 O(1)，
// 而且不需要像陣列那樣預先配置或搬動資料。查看歷史時由新到舊走訪即可。

#ifndef LINKEDLIST_H
#define LINKEDLIST_H

template <typename T>
class LinkedList {
private:
    struct Node {
        T value;
        Node* next;
        Node(const T& v, Node* n) : value(v), next(n) {}
    };

public:
    // O(1)
    LinkedList() : head_(nullptr), size_(0) {}

    // 複製建構：逐一複製節點並保持原本順序
    // O(n)
    LinkedList(const LinkedList& other) : head_(nullptr), size_(0) {
        copyFrom(other);
    }

    // O(n)
    LinkedList& operator=(const LinkedList& other) {
        if (this != &other) {
            clear();
            copyFrom(other);
        }
        return *this;
    }

    // O(n)
    ~LinkedList() { clear(); }

    // 在最前面插入一個元素
    // O(1)
    void pushFront(const T& value) {
        head_ = new Node(value, head_);   // 新節點的 next 指向舊的頭
        size_++;
    }

    // 移除最前面的元素；空串列回傳 false
    // O(1)
    bool popFront() {
        if (head_ == nullptr) {
            return false;
        }
        Node* oldHead = head_;
        head_ = head_->next;              // 頭往後移一格
        delete oldHead;
        size_--;
        return true;
    }

    // 回傳最前面元素的指標；空串列回傳 nullptr
    // O(1)
    T* front() { return (head_ == nullptr) ? nullptr : &head_->value; }

    // O(1)
    const T* front() const { return (head_ == nullptr) ? nullptr : &head_->value; }

    // 從頭到尾走訪，對每個元素呼叫 callback(const T&)
    // O(n)
    template <typename Func>
    void forEach(Func callback) const {
        for (Node* cur = head_; cur != nullptr; cur = cur->next) {
            callback(cur->value);
        }
    }

    // 找第一個讓 predicate(元素) 為 true 的元素，回傳其指標；找不到回傳 nullptr
    // 回傳的是非 const 指標，呼叫端可以直接修改該元素（例如把出價標記為已撤標）
    // O(n)
    template <typename Pred>
    T* find(Pred predicate) {
        for (Node* cur = head_; cur != nullptr; cur = cur->next) {
            if (predicate(cur->value)) {
                return &cur->value;
            }
        }
        return nullptr;
    }

    // 唯讀版本
    // O(n)
    template <typename Pred>
    const T* find(Pred predicate) const {
        for (Node* cur = head_; cur != nullptr; cur = cur->next) {
            if (predicate(cur->value)) {
                return &cur->value;
            }
        }
        return nullptr;
    }

    // 刪除全部節點
    // O(n)
    void clear() {
        while (head_ != nullptr) {
            Node* next = head_->next;     // 先記住下一個，再刪目前這個
            delete head_;
            head_ = next;
        }
        size_ = 0;
    }

    // O(1)（插入時維護計數，不用重新數）
    int size() const { return size_; }

    // O(1)
    bool empty() const { return size_ == 0; }

private:
    Node* head_;
    int size_;

    // 把 other 的元素依相同順序複製過來（呼叫前自己必須是空的）
    // 用 tail 指標記住「目前最後一個節點」，才能一路接在後面
    // O(n)
    void copyFrom(const LinkedList& other) {
        Node* tail = nullptr;
        for (Node* cur = other.head_; cur != nullptr; cur = cur->next) {
            Node* node = new Node(cur->value, nullptr);
            if (tail == nullptr) {
                head_ = node;             // 第一個節點
            } else {
                tail->next = node;
            }
            tail = node;
            size_++;
        }
    }
};

#endif  // LINKEDLIST_H
