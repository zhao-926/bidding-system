// BST.h — 自刻二元搜尋樹（資料結構 #5）
//
// 概念：每個節點的 key 都「大於左子樹所有 key、小於右子樹所有 key」。
//   所以找 key 時每次比較就能丟掉一半的樹（樹平衡時），平均 O(log n)。
//   中序走訪（左 -> 自己 -> 右）會剛好由小到大。
//
// 用途：以 (結標時間, 商品ID) 當 key 管理所有「進行中」商品，
//   - 即將結標列表 = 中序走訪
//   - 時段範圍查詢 = rangeQuery
//   - 結標 = 反覆 popMin（取出結標時間最早的）
//
// 最壞情況：key 依序插入（1,2,3,...）時樹會退化成一條鏈，高度 = n，操作變 O(n)。
// 改善方向是 AVL / 紅黑樹這類「自平衡」的樹（本專題不實作，報告中討論）。
//
// 為了避免退化的樹造成遞迴太深（stack overflow），所有走訪都用迴圈 + 自己的堆疊（DynArray）。
//
// 對 key 型別 K 的要求：只需要支援 operator<（相等 = 兩邊都不小於對方）。

#ifndef BST_H
#define BST_H

#include "DynArray.h"

template <typename K, typename V>
class BST {
private:
    struct Node {
        K key;
        V value;
        Node* left;
        Node* right;
        Node(const K& k, const V& v) : key(k), value(v), left(nullptr), right(nullptr) {}
    };

public:
    // O(1)
    BST() : root_(nullptr), size_(0) {}

    // 不允許複製（避免兩棵樹共用節點）
    BST(const BST&) = delete;
    BST& operator=(const BST&) = delete;

    // O(n)
    ~BST() { clear(); }

    // 插入 (key, value)。key 已存在則更新 value 並回傳 false；新增回傳 true。
    // O(h)：平均 O(log n)，最差 O(n)（依序插入時退化成鏈）
    bool insert(const K& key, const V& value) {
        if (root_ == nullptr) {
            root_ = new Node(key, value);
            size_++;
            return true;
        }
        Node* current = root_;
        while (true) {
            if (key < current->key) {                  // 往左走
                if (current->left == nullptr) {
                    current->left = new Node(key, value);
                    size_++;
                    return true;
                }
                current = current->left;
            } else if (current->key < key) {           // 往右走
                if (current->right == nullptr) {
                    current->right = new Node(key, value);
                    size_++;
                    return true;
                }
                current = current->right;
            } else {                                   // key 相同：更新
                current->value = value;
                return false;
            }
        }
    }

    // 找 key，回傳 value 的指標；找不到回傳 nullptr
    // O(h)：平均 O(log n)，最差 O(n)
    V* find(const K& key) {
        Node* current = root_;
        while (current != nullptr) {
            if (key < current->key) {
                current = current->left;
            } else if (current->key < key) {
                current = current->right;
            } else {
                return &current->value;
            }
        }
        return nullptr;
    }

    // O(h)
    bool contains(const K& key) { return find(key) != nullptr; }

    // 刪除 key；不存在回傳 false。分三種情況：
    //   1. 葉節點（0 個子節點）：直接拿掉
    //   2. 只有 1 個子節點：用那個子節點頂替
    //   3. 有 2 個子節點：找「後繼者」（右子樹最小的節點），把它的內容複製過來，再刪掉後繼者
    // O(h)：平均 O(log n)，最差 O(n)
    bool remove(const K& key) {
        // 先找到要刪的節點 current，並記住它的父節點 parent
        Node* parent = nullptr;
        Node* current = root_;
        while (current != nullptr && (key < current->key || current->key < key)) {
            parent = current;
            current = (key < current->key) ? current->left : current->right;
        }
        if (current == nullptr) {
            return false;                              // 找不到
        }

        if (current->left != nullptr && current->right != nullptr) {
            // 情況 3：兩個子節點。找後繼者 successor（右子樹一路往左走到底）
            Node* successorParent = current;
            Node* successor = current->right;
            while (successor->left != nullptr) {
                successorParent = successor;
                successor = successor->left;
            }
            current->key = successor->key;             // 把後繼者的內容搬到 current
            current->value = successor->value;
            parent = successorParent;                  // 接下來改成刪掉 successor
            current = successor;                       // （它最多只有一個右子節點）
        }

        // 情況 1、2（情況 3 轉換後也走到這裡）：current 最多一個子節點
        Node* child = (current->left != nullptr) ? current->left : current->right;
        replaceChild(parent, current, child);
        delete current;
        size_--;
        return true;
    }

    // 看最小的 key（不刪除）；空樹回傳 false
    // O(h)
    bool peekMin(K& keyOut, V& valueOut) const {
        if (root_ == nullptr) {
            return false;
        }
        Node* current = root_;
        while (current->left != nullptr) {
            current = current->left;
        }
        keyOut = current->key;
        valueOut = current->value;
        return true;
    }

    // 取出並刪除最小的 key（結標時反覆呼叫）；空樹回傳 false
    // O(h)：一路往左走到底，最小的節點沒有左子節點，用它的右子節點頂替即可
    bool popMin(K& keyOut, V& valueOut) {
        if (root_ == nullptr) {
            return false;
        }
        Node* parent = nullptr;
        Node* current = root_;
        while (current->left != nullptr) {
            parent = current;
            current = current->left;
        }
        keyOut = current->key;
        valueOut = current->value;
        replaceChild(parent, current, current->right);
        delete current;
        size_--;
        return true;
    }

    // 中序走訪（由小到大），對每個節點呼叫 callback(key, value)
    // O(n)
    template <typename Func>
    void inorder(Func callback) const {
        DynArray<Node*> stack;                         // 自己的堆疊，取代遞迴
        Node* current = root_;
        while (current != nullptr || !stack.empty()) {
            while (current != nullptr) {               // 一路往左，沿途把節點存起來
                stack.pushBack(current);
                current = current->left;
            }
            current = stack.back();                    // 拿回最近一個還沒處理的節點
            stack.popBack();
            callback(current->key, current->value);    // 處理自己
            current = current->right;                  // 再換右子樹
        }
    }

    // 範圍查詢：由小到大列出所有 low <= key <= high 的節點
    // 剪枝：key < low 的節點連同它的左子樹整個跳過；遇到 key > high 就整個結束。
    // O(h + k)，k 為結果筆數（平均 O(log n + k)）
    template <typename Func>
    void rangeQuery(const K& low, const K& high, Func callback) const {
        DynArray<Node*> stack;
        Node* current = root_;
        while (current != nullptr || !stack.empty()) {
            while (current != nullptr) {
                if (current->key < low) {
                    current = current->right;          // 太小：自己和左子樹都不用看
                } else {
                    stack.pushBack(current);
                    current = current->left;
                }
            }
            if (stack.empty()) {
                break;
            }
            current = stack.back();
            stack.popBack();
            if (high < current->key) {
                break;                                 // 超過上界：後面只會更大，結束
            }
            callback(current->key, current->value);
            current = current->right;
        }
    }

    // 樹高（空樹 0、只有根 1）。給「資料結構檢視」顯示退化程度用
    // O(n)
    int height() const {
        if (root_ == nullptr) {
            return 0;
        }
        DynArray<Node*> nodes;                         // 兩個陣列一一對應：節點 與 它的深度
        DynArray<int> depths;
        nodes.pushBack(root_);
        depths.pushBack(1);
        int maxDepth = 0;
        while (!nodes.empty()) {
            Node* node = nodes.back();
            int depth = depths.back();
            nodes.popBack();
            depths.popBack();
            if (depth > maxDepth) {
                maxDepth = depth;
            }
            if (node->left != nullptr) {
                nodes.pushBack(node->left);
                depths.pushBack(depth + 1);
            }
            if (node->right != nullptr) {
                nodes.pushBack(node->right);
                depths.pushBack(depth + 1);
            }
        }
        return maxDepth;
    }

    // 刪除所有節點
    // O(n)
    void clear() {
        DynArray<Node*> pending;                       // 還沒刪除的節點
        if (root_ != nullptr) {
            pending.pushBack(root_);
        }
        while (!pending.empty()) {
            Node* node = pending.back();
            pending.popBack();
            if (node->left != nullptr) {
                pending.pushBack(node->left);
            }
            if (node->right != nullptr) {
                pending.pushBack(node->right);
            }
            delete node;
        }
        root_ = nullptr;
        size_ = 0;
    }

    // O(1)
    int size() const { return size_; }

    // O(1)
    bool empty() const { return size_ == 0; }

private:
    Node* root_;
    int size_;

    // 把 parent 底下的 oldChild 換成 newChild（可以是 nullptr）；parent 為 nullptr 代表 oldChild 是根
    // O(1)
    void replaceChild(Node* parent, Node* oldChild, Node* newChild) {
        if (parent == nullptr) {
            root_ = newChild;
        } else if (parent->left == oldChild) {
            parent->left = newChild;
        } else {
            parent->right = newChild;
        }
    }
};

#endif  // BST_H
