// Models.js — 資料模型：Bid（出價）、ItemKey、Item（商品）、User（使用者）（對應 core/Models.h）
//
// 金額一律用「元」為單位的整數；時間用「模擬時鐘的分鐘數」。

import { DynArray } from './DynArray.js';
import { LinkedList } from './LinkedList.js';
import { MaxHeap } from './MaxHeap.js';

// 一筆出價
export class Bid {
    constructor() {
        this.bidId = 0;
        this.itemId = 0;
        this.bidder = '';
        this.amount = 0;
        this.time = 0;         // 出價當下的模擬時鐘
        this.active = true;    // 撤標後設為 false（歷史保留，但從 Heap 移除）
    }

    // ---- 以下兩個函式是 MaxHeap 需要的 ----

    // O(1)
    getId() { return this.bidId; }

    // 比較規則：金額高者優先；金額相同時，時間早者優先（先出價者勝）
    // O(1)
    hasHigherPriority(other) {
        if (this.amount !== other.amount) {
            return this.amount > other.amount;
        }
        return this.time < other.time;
    }
}

// 商品在 BST 裡的排序鍵：(結標時間, 商品ID)
// 只用結標時間會有「兩件商品同時結標」key 重複的問題，所以加上商品 ID 當第二層比較
export class ItemKey {
    constructor(endTime, itemId) {
        this.endTime = endTime;
        this.itemId = itemId;
    }
}

// BST 用的比較函式：a 是否小於 b
// O(1)
export function itemKeyLess(a, b) {
    if (a.endTime !== b.endTime) {
        return a.endTime < b.endTime;
    }
    return a.itemId < b.itemId;
}

// 商品狀態
export const ITEM_OPEN = 0;
export const ITEM_CLOSED = 1;

// 一件商品
export class Item {
    constructor() {
        this.itemId = 0;
        this.title = '';
        this.seller = '';
        this.description = '';
        this.startPrice = 0;
        this.minIncrement = 0;     // 最小加價幅度（可為 0）
        this.endTime = 0;
        this.status = ITEM_OPEN;
        this.winner = '';          // 結標後填入；流標（無人出價）則為空字串
        this.finalPrice = 0;       // 得標價；流標為 0
        this.bidHeap = new MaxHeap();        // 「有效」出價，堆頂 = 目前最高價
        this.bidHistory = new LinkedList();  // 全部出價紀錄（含已撤標），新的插在最前面
    }
}

// 一位使用者
export class User {
    constructor(username) {
        this.username = username;
        this.sellingItems = new DynArray();   // 我刊登的商品 ID
        this.biddingItems = new DynArray();   // 我出價過的商品 ID（不重複）
        this.wonItems = new DynArray();       // 我得標的商品 ID
    }
}
