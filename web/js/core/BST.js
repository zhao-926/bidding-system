// BST.js — 自刻二元搜尋樹（對應 core/BST.h）
//
// 每個節點的 key 都「大於左子樹所有 key、小於右子樹所有 key」，中序走訪剛好由小到大。
// 最壞情況：key 依序插入時樹退化成一條鏈（高度 = n），操作變 O(n)；改善方向是 AVL / 紅黑樹。
// 所有走訪都用「迴圈 + 自己的堆疊（DynArray）」，所以退化的樹也不會遞迴過深。
//
// 與 C++ 版的差異：
//   - C++ 用 operator< 比較 key；這裡在建構時傳入 less(a, b) 函式
//   - C++ 用「輸出參數」回傳 key / value；這裡回傳 { key, value } 物件（空樹回傳 null）
//   - 多了 layout()：給「資料結構檢視」面板畫樹狀圖用

import { DynArray } from './DynArray.js';

class Node {
    constructor(key, value) {
        this.key = key;
        this.value = value;
        this.left = null;
        this.right = null;
    }
}

export class BST {
    // less(a, b)：a 的 key 是否小於 b 的 key
    // O(1)
    constructor(less) {
        this._less = less;
        this._root = null;
        this._size = 0;
    }

    // 插入 (key, value)。key 已存在則更新 value 並回傳 false；新增回傳 true
    // O(h)：平均 O(log n)，最差 O(n)（依序插入時退化成鏈）
    insert(key, value) {
        if (this._root === null) {
            this._root = new Node(key, value);
            this._size++;
            return true;
        }
        let current = this._root;
        while (true) {
            if (this._less(key, current.key)) {            // 往左走
                if (current.left === null) {
                    current.left = new Node(key, value);
                    this._size++;
                    return true;
                }
                current = current.left;
            } else if (this._less(current.key, key)) {     // 往右走
                if (current.right === null) {
                    current.right = new Node(key, value);
                    this._size++;
                    return true;
                }
                current = current.right;
            } else {                                       // key 相同：更新
                current.value = value;
                return false;
            }
        }
    }

    // 找 key，回傳 value；找不到回傳 null
    // O(h)：平均 O(log n)，最差 O(n)
    find(key) {
        let current = this._root;
        while (current !== null) {
            if (this._less(key, current.key)) {
                current = current.left;
            } else if (this._less(current.key, key)) {
                current = current.right;
            } else {
                return current.value;
            }
        }
        return null;
    }

    // O(h)
    contains(key) { return this.find(key) !== null; }

    // 刪除 key；不存在回傳 false。分三種情況：
    //   1. 葉節點：直接拿掉
    //   2. 只有 1 個子節點：用那個子節點頂替
    //   3. 有 2 個子節點：找「後繼者」（右子樹最小的節點），把它的內容複製過來，再刪掉後繼者
    // O(h)：平均 O(log n)，最差 O(n)
    remove(key) {
        // 先找到要刪的節點 current，並記住它的父節點 parent
        let parent = null;
        let current = this._root;
        while (current !== null && (this._less(key, current.key) || this._less(current.key, key))) {
            parent = current;
            current = this._less(key, current.key) ? current.left : current.right;
        }
        if (current === null) {
            return false;                                  // 找不到
        }

        if (current.left !== null && current.right !== null) {
            // 情況 3：兩個子節點。找後繼者（右子樹一路往左走到底）
            let successorParent = current;
            let successor = current.right;
            while (successor.left !== null) {
                successorParent = successor;
                successor = successor.left;
            }
            current.key = successor.key;                   // 把後繼者的內容搬到 current
            current.value = successor.value;
            parent = successorParent;                      // 接下來改成刪掉 successor
            current = successor;                           // （它最多只有一個右子節點）
        }

        // 情況 1、2（情況 3 轉換後也走到這裡）：current 最多一個子節點
        const child = (current.left !== null) ? current.left : current.right;
        this._replaceChild(parent, current, child);
        this._size--;
        return true;
    }

    // 看最小的 key（不刪除）；空樹回傳 null，否則回傳 { key, value }
    // O(h)
    peekMin() {
        if (this._root === null) {
            return null;
        }
        let current = this._root;
        while (current.left !== null) {
            current = current.left;
        }
        return { key: current.key, value: current.value };
    }

    // 取出並刪除最小的 key（結標時反覆呼叫）；空樹回傳 null，否則回傳 { key, value }
    // O(h)：一路往左走到底，最小的節點沒有左子節點，用它的右子節點頂替即可
    popMin() {
        if (this._root === null) {
            return null;
        }
        let parent = null;
        let current = this._root;
        while (current.left !== null) {
            parent = current;
            current = current.left;
        }
        this._replaceChild(parent, current, current.right);
        this._size--;
        return { key: current.key, value: current.value };
    }

    // 中序走訪（由小到大），對每個節點呼叫 callback(key, value)
    // O(n)
    inorder(callback) {
        const stack = new DynArray();                      // 自己的堆疊，取代遞迴
        let current = this._root;
        while (current !== null || !stack.empty()) {
            while (current !== null) {                     // 一路往左，沿途把節點存起來
                stack.pushBack(current);
                current = current.left;
            }
            current = stack.back();                        // 拿回最近一個還沒處理的節點
            stack.popBack();
            callback(current.key, current.value);          // 處理自己
            current = current.right;                       // 再換右子樹
        }
    }

    // 範圍查詢：由小到大列出所有 low <= key <= high 的節點
    // 剪枝：key < low 的節點連同它的左子樹整個跳過；遇到 key > high 就整個結束
    // O(h + k)，k 為結果筆數（平均 O(log n + k)）
    rangeQuery(low, high, callback) {
        const stack = new DynArray();
        let current = this._root;
        while (current !== null || !stack.empty()) {
            while (current !== null) {
                if (this._less(current.key, low)) {
                    current = current.right;               // 太小：自己和左子樹都不用看
                } else {
                    stack.pushBack(current);
                    current = current.left;
                }
            }
            if (stack.empty()) {
                break;
            }
            current = stack.back();
            stack.popBack();
            if (this._less(high, current.key)) {
                break;                                     // 超過上界：後面只會更大，結束
            }
            callback(current.key, current.value);
            current = current.right;
        }
    }

    // 樹高（空樹 0、只有根 1）
    // O(n)
    height() {
        if (this._root === null) {
            return 0;
        }
        const nodes = new DynArray();                      // 兩個陣列一一對應：節點 與 它的深度
        const depths = new DynArray();
        nodes.pushBack(this._root);
        depths.pushBack(1);
        let maxDepth = 0;
        while (!nodes.empty()) {
            const node = nodes.back();
            const depth = depths.back();
            nodes.popBack();
            depths.popBack();
            if (depth > maxDepth) {
                maxDepth = depth;
            }
            if (node.left !== null) {
                nodes.pushBack(node.left);
                depths.pushBack(depth + 1);
            }
            if (node.right !== null) {
                nodes.pushBack(node.right);
                depths.pushBack(depth + 1);
            }
        }
        return maxDepth;
    }

    // 給「資料結構檢視」畫樹用：中序走訪，回傳 DynArray，每個元素是
    //   { key, value, depth, rank, parentRank }
    //   depth：深度（根 = 1）；rank：中序名次（0 起算，可當 x 座標）；parentRank：父節點的 rank（根 = -1）
    // O(n)
    layout() {
        const entries = new DynArray();      // 依中序順序的 { node, depth, parent }
        const stack = new DynArray();        // 堆疊裡放 { node, depth, parent }
        let current = this._root;
        let currentDepth = 1;
        let currentParent = null;
        while (current !== null || !stack.empty()) {
            while (current !== null) {
                stack.pushBack({ node: current, depth: currentDepth, parent: currentParent });
                currentParent = current;
                current = current.left;
                currentDepth++;
            }
            const entry = stack.back();
            stack.popBack();
            entry.node._rank = entries.size();             // 中序名次
            entries.pushBack(entry);
            current = entry.node.right;
            currentDepth = entry.depth + 1;
            currentParent = entry.node;
        }
        const result = new DynArray();
        for (let i = 0; i < entries.size(); i++) {
            const e = entries.get(i);
            result.pushBack({
                key: e.node.key,
                value: e.node.value,
                depth: e.depth,
                rank: i,
                parentRank: (e.parent === null) ? -1 : e.parent._rank,
            });
        }
        return result;
    }

    // 刪除所有節點
    // O(1)（JS 有垃圾回收）
    clear() {
        this._root = null;
        this._size = 0;
    }

    // O(1)
    size() { return this._size; }

    // O(1)
    empty() { return this._size === 0; }

    // 把 parent 底下的 oldChild 換成 newChild（可以是 null）；parent 為 null 代表 oldChild 是根
    // O(1)
    _replaceChild(parent, oldChild, newChild) {
        if (parent === null) {
            this._root = newChild;
        } else if (parent.left === oldChild) {
            parent.left = newChild;
        } else {
            parent.right = newChild;
        }
    }
}
