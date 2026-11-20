// AuctionSystem.js — 競標系統業務邏輯（對應 core/AuctionSystem.h / .cpp，F1 ~ F16）
//
// 使用到的資料結構：
//   HashTable(商品 ID -> Item)、HashTable(使用者名稱 -> User)、
//   Item.bidHeap（MaxHeap，最高出價）、Item.bidHistory（LinkedList，出價歷史）、
//   BST（進行中商品依 (結標時間, ID) 排序）、DynArray（清單）、mergeSort（排序）。
//
// 所有可能失敗的操作都回傳 Result = { ok, message, id }，錯誤訊息為中文，不丟例外。
// 存檔格式與 C++ 版完全相同（見 saveToString）。

import { BST } from './BST.js';
import { DynArray } from './DynArray.js';
import { HashTable, hashInt, hashString } from './HashTable.js';
import { mergeSort } from './MergeSort.js';
import { Bid, Item, ItemKey, itemKeyLess, ITEM_CLOSED, ITEM_OPEN, User } from './Models.js';

// ======================================================================
// Result 與排序欄位
// ======================================================================

function success(message, id = 0) {
    return { ok: true, message: message, id: id };
}

function fail(message) {
    return { ok: false, message: message, id: 0 };
}

export const SORT_BY_PRICE = 0;        // 目前價格（無人出價則為起標價）
export const SORT_BY_BID_COUNT = 1;    // 有效出價數
export const SORT_BY_END_TIME = 2;     // 結標時間

const MAX_ITEM_ID = 2147483647;        // 範圍查詢的上界用

// ======================================================================
// 小工具（不使用 JS 內建的陣列方法與字串搜尋，邏輯與 C++ 版相同）
// ======================================================================

// 英文字母轉小寫（只處理 A-Z，中文等字元原樣保留）
function toLowerAscii(text) {
    let result = '';
    for (let i = 0; i < text.length; i++) {
        const code = text.charCodeAt(i);
        if (code >= 65 && code <= 90) {
            result += String.fromCharCode(code + 32);
        } else {
            result += text.charAt(i);
        }
    }
    return result;
}

// text 是否包含 keyword（樸素的雙層迴圈比對，O(n·m)）
function containsText(text, keyword) {
    if (keyword.length === 0) {
        return true;
    }
    for (let start = 0; start + keyword.length <= text.length; start++) {
        let matched = 0;
        while (matched < keyword.length && text.charCodeAt(start + matched) === keyword.charCodeAt(matched)) {
            matched++;
        }
        if (matched === keyword.length) {
            return true;
        }
    }
    return false;
}

// 文字轉整數；格式不合法回傳 null（最多 15 位數，確保在 JS 安全整數範圍內）
function parseNumber(text) {
    if (text.length === 0 || text.length > 15) {
        return null;
    }
    const negative = (text.charAt(0) === '-');
    const start = negative ? 1 : 0;
    if (start === text.length) {
        return null;
    }
    let value = 0;
    for (let i = start; i < text.length; i++) {
        const code = text.charCodeAt(i);
        if (code < 48 || code > 57) {
            return null;
        }
        value = value * 10 + (code - 48);
    }
    return negative ? -value : value;
}

// 存檔用：欄位以 | 分隔、一筆資料一行，所以文字內的 % | 換行都要先「跳脫」成 %XX
function escapeField(text) {
    let result = '';
    for (let i = 0; i < text.length; i++) {
        const c = text.charAt(i);
        if (c === '%') {
            result += '%25';
        } else if (c === '|') {
            result += '%7C';
        } else if (c === '\n') {
            result += '%0A';
        } else if (c === '\r') {
            result += '%0D';
        } else {
            result += c;
        }
    }
    return result;
}

function hexValue(code) {
    if (code >= 48 && code <= 57) return code - 48;          // 0-9
    if (code >= 65 && code <= 70) return code - 65 + 10;     // A-F
    if (code >= 97 && code <= 102) return code - 97 + 10;    // a-f
    return -1;
}

function unescapeField(text) {
    let result = '';
    for (let i = 0; i < text.length; i++) {
        if (text.charAt(i) === '%' && i + 2 < text.length &&
            hexValue(text.charCodeAt(i + 1)) >= 0 && hexValue(text.charCodeAt(i + 2)) >= 0) {
            result += String.fromCharCode(hexValue(text.charCodeAt(i + 1)) * 16 + hexValue(text.charCodeAt(i + 2)));
            i += 2;
        } else {
            result += text.charAt(i);
        }
    }
    return result;
}

// 把一行依 | 切成多個欄位，回傳 DynArray
function splitFields(line) {
    const fields = new DynArray();
    let current = '';
    for (let i = 0; i < line.length; i++) {
        if (line.charAt(i) === '|') {
            fields.pushBack(current);
            current = '';
        } else {
            current += line.charAt(i);
        }
    }
    fields.pushBack(current);
    return fields;
}

// 排序用：取出商品在指定欄位的數值
function sortValue(item, field) {
    if (field === SORT_BY_PRICE) {
        return AuctionSystem.currentPrice(item);
    }
    if (field === SORT_BY_BID_COUNT) {
        return item.bidHeap.size();
    }
    return item.endTime;
}

// ======================================================================
// 範例資料（與 C++ 版完全相同）
// ======================================================================

// [賣家, 標題, 描述, 起標價, 最小加價, 拍賣時長（分鐘）]
const SAMPLE_ITEMS = [
    ['alice', '資料結構與演算法 教科書（第四版）', '九成新，內有少量螢光筆劃記。', 300, 20, 45],
    ['eve', '折疊雨傘（黑色）', '使用約半年，傘骨完好。', 50, 10, 50],
    ['bob', '藍牙降噪耳機', '原價 3,500 元，附原廠收納盒，功能正常。', 800, 50, 180],
    ['carol', '機械鍵盤（青軸）', '87 鍵，附鍵帽拔取器，無按鍵失靈。', 1500, 100, 240],
    ['dave', '二手公路腳踏車', '鋁合金車架，騎乘約一年，附車鎖。', 3000, 200, 1440],
    ['eve', '27 吋顯示器', '2K 解析度，無亮點，附電源線與 HDMI 線。', 2500, 100, 360],
    ['alice', '電路學課本', '附歷屆考古題影本。', 200, 10, 600],
    ['bob', '無線滑鼠', '電池式，附接收器。', 150, 10, 120],
    ['carol', '桌上型電風扇', '三段風速，運轉安靜。', 300, 20, 720],
    ['dave', 'LED 檯燈', '可調亮度，無閃爍。', 120, 10, 300],
    ['eve', '二手筆電（i5／16GB）', '使用兩年，電池健康度約 80%，附充電器。', 5000, 300, 2880],
    ['alice', '工程計算機', '可程式型，附保護套。', 250, 10, 90],
    ['bob', '螢幕掛燈', '護眼無頻閃，夾式安裝。', 400, 20, 480],
    ['carol', '不鏽鋼保溫瓶', '500ml，保溫效果良好。', 100, 10, 1000],
    ['dave', '民謠吉他', '附琴袋與備用琴弦，琴頸筆直。', 2000, 100, 4320],
    ['eve', '慢跑鞋（US 9）', '穿過約十次，鞋底磨損很少。', 600, 50, 150],
    ['alice', '小型冰箱', '90 公升，宿舍適用，冷藏正常。', 1200, 100, 960],
    ['bob', '行動電源', '20000mAh，支援快充。', 350, 25, 200],
    ['carol', '微波爐', '700W，附玻璃轉盤。', 900, 50, 540],
    ['dave', '全新筆記本一批', 'A5 方格，共 10 本。', 50, 5, 100],
];

// 事件：[第幾分鐘, 商品編號（1~20）, 使用者, 金額, 是否為撤標]。依時間順序排列
const SAMPLE_STEPS = [
    [5, 1, 'bob', 300, false], [6, 4, 'dave', 1500, false], [7, 6, 'alice', 2500, false],
    [8, 3, 'carol', 800, false], [9, 8, 'carol', 150, false], [10, 1, 'carol', 340, false],
    [10, 5, 'eve', 3000, false], [11, 9, 'dave', 300, false], [12, 3, 'dave', 850, false],
    [13, 12, 'bob', 250, false], [14, 11, 'alice', 5000, false], [15, 4, 'alice', 1600, false],
    [16, 13, 'eve', 400, false], [17, 16, 'bob', 600, false], [18, 6, 'bob', 2600, false],
    [19, 8, 'eve', 160, false], [20, 1, 'bob', 360, false], [20, 5, 'bob', 3200, false],
    [21, 15, 'alice', 2000, false], [22, 17, 'carol', 1200, false], [23, 12, 'dave', 260, false],
    [24, 11, 'bob', 5300, false], [25, 4, 'dave', 1700, false], [26, 18, 'eve', 350, false],
    [27, 16, 'dave', 650, false], [29, 8, 'carol', 170, false], [30, 3, 'carol', 900, false],
    [31, 15, 'eve', 2100, false], [33, 6, 'alice', 2700, false], [34, 11, 'alice', 5600, false],
    [35, 4, 'alice', 1800, false], [36, 19, 'dave', 900, false], [37, 17, 'dave', 1300, false],
    [39, 8, 'eve', 180, false], [40, 3, 'eve', 1000, false], [42, 6, 'carol', 2800, false],
    [43, 12, 'bob', 270, false], [44, 11, 'bob', 5900, false], [45, 3, 'eve', 0, true],
    [46, 19, 'bob', 950, false], [50, 4, 'dave', 1900, false], [51, 15, 'alice', 2200, false],
    [52, 6, 'bob', 2900, false], [54, 11, 'carol', 6200, false], [55, 6, 'alice', 3000, false],
    [56, 19, 'dave', 1000, false],
];

const SAMPLE_FINAL_TIME = 60;   // 範例資料建立完成時的模擬時間

// ======================================================================
// 競標系統
// ======================================================================

export class AuctionSystem {
    // O(1)
    constructor() {
        this._now = 0;
        this._nextItemId = 1;
        this._nextBidId = 1;
        this._items = new HashTable(hashInt);          // 商品 ID -> Item
        this._users = new HashTable(hashString);       // 使用者名稱 -> User
        this._itemOrder = new DynArray();              // 依刊登順序的商品 ID
        this._userNames = new DynArray();              // 依註冊順序的使用者名稱
        this._endTimeTree = new BST(itemKeyLess);      // 進行中商品：ItemKey -> 商品 ID
        this._lastClosed = new DynArray();             // 最近一次結標的商品 ID
    }

    // 清空所有資料，回到初始狀態
    // O(n)
    reset() {
        this._items.clear();
        this._users.clear();
        this._itemOrder.clear();
        this._userNames.clear();
        this._endTimeTree.clear();
        this._lastClosed.clear();
        this._now = 0;
        this._nextItemId = 1;
        this._nextBidId = 1;
    }

    // ---------------- F1 使用者 ----------------

    // 註冊新使用者；名稱不可空白、不可重複
    // O(1) average
    registerUser(username) {
        if (username.length === 0) {
            return fail('使用者名稱不可空白');
        }
        if (containsText(username, '|') || containsText(username, '\n')) {
            return fail('使用者名稱不可包含「|」或換行');
        }
        if (this._users.contains(username)) {
            return fail('使用者「' + username + '」已經存在');
        }
        this._users.insert(username, new User(username));
        this._userNames.pushBack(username);
        return success('已註冊使用者「' + username + '」');
    }

    // O(1) average
    userExists(username) { return this._users.contains(username); }

    // 查使用者（含我賣的／我出價的／我得標的清單，即 F13「我的頁面」）；找不到回傳 null
    // O(1) average
    findUser(username) { return this._users.find(username); }

    // 依註冊順序的使用者名稱（DynArray）
    // O(1)
    userNames() { return this._userNames; }

    // ---------------- F2、F3、F4 商品 ----------------

    // 刊登商品；durationMinutes 為從「現在」起算的拍賣時長（分鐘）。成功時 result.id = 新商品 ID
    // O(log n) average（HashTable 插入 O(1) + BST 插入 O(log n)）
    listItem(seller, title, description, startPrice, minIncrement, durationMinutes) {
        const sellerUser = this._users.find(seller);
        if (sellerUser === null) {
            return fail('找不到使用者「' + seller + '」，請先註冊');
        }
        if (title.length === 0) {
            return fail('商品標題不可空白');
        }
        if (!Number.isInteger(startPrice) || startPrice < 1) {
            return fail('起標價必須是大於等於 1 的整數');
        }
        if (!Number.isInteger(minIncrement) || minIncrement < 0) {
            return fail('最小加價幅度必須是不小於 0 的整數');
        }
        if (!Number.isInteger(durationMinutes) || durationMinutes < 1) {
            return fail('拍賣時長必須是大於 0 的整數（分鐘）');
        }

        const item = new Item();
        item.itemId = this._nextItemId++;
        item.title = title;
        item.seller = seller;
        item.description = description;
        item.startPrice = startPrice;
        item.minIncrement = minIncrement;
        item.endTime = this._now + durationMinutes;
        item.status = ITEM_OPEN;

        this._items.insert(item.itemId, item);                                        // Hash Table：ID -> 商品
        this._itemOrder.pushBack(item.itemId);                                        // 刊登順序
        this._endTimeTree.insert(new ItemKey(item.endTime, item.itemId), item.itemId);   // BST：依結標時間
        sellerUser.sellingItems.pushBack(item.itemId);
        return success('商品刊登成功，編號 #' + item.itemId, item.itemId);
    }

    // F3：依 ID 查商品；找不到回傳 null
    // O(1) average
    findItem(itemId) { return this._items.find(itemId); }

    // F4：關鍵字搜尋（標題包含關鍵字，英文不分大小寫；關鍵字空白則全部列出）
    // 回傳符合的商品 ID（DynArray，依刊登順序）。線性掃描 O(n·m)，報告中作為「取捨」案例討論
    searchItems(keyword, openOnly) {
        const found = new DynArray();
        const lowerKeyword = toLowerAscii(keyword);
        for (let i = 0; i < this._itemOrder.size(); i++) {      // 線性掃描每件商品
            const item = this._items.find(this._itemOrder.get(i));
            if (openOnly && item.status !== ITEM_OPEN) {
                continue;
            }
            if (containsText(toLowerAscii(item.title), lowerKeyword)) {
                found.pushBack(item.itemId);
            }
        }
        return found;
    }

    // 所有商品 ID（DynArray，依刊登順序）
    // O(1)
    allItemIds() { return this._itemOrder; }

    // 商品目前價格：有人出價 = 最高出價，否則 = 起標價
    // O(1)
    static currentPrice(item) {
        const top = item.bidHeap.top();
        return (top !== null) ? top.amount : item.startPrice;
    }

    // 下一筆出價的最低金額：有人出價 = 最高價 + 最小加價，否則 = 起標價
    // O(1)
    static minNextBid(item) {
        const top = item.bidHeap.top();
        return (top !== null) ? top.amount + item.minIncrement : item.startPrice;
    }

    // ---------------- F5、F6、F7、F8 出價 ----------------

    // F5：出價。規則：商品須為 OPEN 且未過結標時間；賣家不能出價；
    //     第一筆 >= 起標價；之後 >= 目前最高價 + 最小加價。成功時 result.id = bidId
    // O(log n)（Heap 插入 O(log n)、歷史 pushFront O(1)）
    placeBid(bidder, itemId, amount) {
        const user = this._users.find(bidder);
        if (user === null) {
            return fail('找不到使用者「' + bidder + '」');
        }
        const item = this._items.find(itemId);
        if (item === null) {
            return fail('找不到商品 #' + itemId);
        }
        if (item.status !== ITEM_OPEN) {
            return fail('此商品已結標，無法出價');
        }
        if (this._now >= item.endTime) {
            return fail('已超過結標時間，無法出價');
        }
        if (item.seller === bidder) {
            return fail('賣家不能對自己的商品出價');
        }
        if (!Number.isInteger(amount)) {
            return fail('出價金額必須是整數');
        }
        const minimum = AuctionSystem.minNextBid(item);
        if (amount < minimum) {
            if (item.bidHeap.empty()) {
                return fail('出價太低：第一筆出價不得低於起標價 ' + minimum + ' 元');
            }
            return fail('出價太低：最低可出價 ' + minimum + ' 元（目前最高價 ' +
                AuctionSystem.currentPrice(item) + ' 元＋最小加價 ' + item.minIncrement + ' 元）');
        }

        const bid = new Bid();
        bid.bidId = this._nextBidId++;
        bid.itemId = itemId;
        bid.bidder = bidder;
        bid.amount = amount;
        bid.time = this._now;
        bid.active = true;

        item.bidHeap.push(bid);           // Max-Heap：維護最高價
        item.bidHistory.pushFront(bid);   // Linked List：歷史（新的放最前面）
        if (!user.biddingItems.contains(itemId)) {
            user.biddingItems.pushBack(itemId);
        }
        return success('出價成功：' + amount + ' 元', bid.bidId);
    }

    // F8：撤回自己在該商品「最新一筆有效出價」；商品須仍為 OPEN
    // O(m + log n)（m = 歷史筆數；Heap 任意刪除 O(log n)）
    cancelBid(bidder, itemId) {
        if (!this._users.contains(bidder)) {
            return fail('找不到使用者「' + bidder + '」');
        }
        const item = this._items.find(itemId);
        if (item === null) {
            return fail('找不到商品 #' + itemId);
        }
        if (item.status !== ITEM_OPEN || this._now >= item.endTime) {
            return fail('此商品已結標，無法撤回出價');
        }
        // 歷史是「由新到舊」，所以第一個符合的就是這位使用者最新的有效出價
        const myLatest = item.bidHistory.find((b) => b.active && b.bidder === bidder);
        if (myLatest === null) {
            return fail('您在此商品上沒有可以撤回的有效出價');
        }
        const cancelledAmount = myLatest.amount;
        item.bidHeap.removeById(myLatest.bidId);    // Max-Heap：任意刪除
        myLatest.active = false;                    // 歷史保留，只是標記為已撤標

        let message = '已撤回出價 ' + cancelledAmount + ' 元';
        const top = item.bidHeap.top();
        if (top !== null) {
            message += '；目前最高價為 ' + top.amount + ' 元（' + top.bidder + '）';
        } else {
            message += '；目前沒有任何有效出價';
        }
        return success(message);
    }

    // F6：目前最高出價（看堆頂）；沒有出價或找不到商品回傳 null
    // O(1) average
    highestBid(itemId) {
        const item = this._items.find(itemId);
        return (item === null) ? null : item.bidHeap.top();
    }

    // F7：出價歷史（DynArray，由新到舊，含已撤標）；找不到商品回傳空陣列
    // O(m)
    bidHistory(itemId) {
        const history = new DynArray();
        const item = this._items.find(itemId);
        if (item !== null) {
            item.bidHistory.forEach((bid) => history.pushBack(bid));
        }
        return history;
    }

    // ---------------- F9、F10 BST 查詢 ----------------

    // F9：即將結標列表（進行中商品，依結標時間由近到遠），最多 maxCount 件（<=0 代表全部）
    // O(n)（BST 中序走訪）
    upcomingItems(maxCount) {
        const result = new DynArray();
        this._endTimeTree.inorder((key, itemId) => {
            if (maxCount <= 0 || result.size() < maxCount) {
                result.pushBack(itemId);
            }
        });
        return result;
    }

    // F10：結標時間落在 [t1, t2] 的進行中商品（依結標時間由近到遠）
    // O(log n + k)
    itemsEndingBetween(t1, t2) {
        const result = new DynArray();
        this._endTimeTree.rangeQuery(new ItemKey(t1, 0), new ItemKey(t2, MAX_ITEM_ID),
            (key, itemId) => result.pushBack(itemId));
        return result;
    }

    // ---------------- F11、F14 模擬時鐘與結標 ----------------

    // 目前的模擬時間（分鐘）
    // O(1)
    now() { return this._now; }

    // 結標單一商品：堆頂為得標者；沒有有效出價則流標（winner 為空）
    // O(1)
    _closeItem(itemId) {
        const item = this._items.find(itemId);
        item.status = ITEM_CLOSED;
        const top = item.bidHeap.top();
        if (top !== null) {
            item.winner = top.bidder;
            item.finalPrice = top.amount;
            const winnerUser = this._users.find(top.bidder);
            if (winnerUser !== null) {
                winnerUser.wonItems.pushBack(itemId);
            }
        } else {
            item.winner = '';
            item.finalPrice = 0;
        }
    }

    // 把 BST 中所有 endTime <= now 的商品依序取出（最早的先）並結標
    // O(k log n)
    _closeExpiredItems() {
        while (true) {
            const next = this._endTimeTree.peekMin();
            if (next === null || next.key.endTime > this._now) {
                break;
            }
            this._endTimeTree.popMin();
            this._closeItem(next.value);
            this._lastClosed.pushBack(next.value);
        }
    }

    // F14：快轉 minutes 分鐘（必須是 > 0 的整數），並自動結標所有 endTime <= now 的商品（F11）
    // O(k log n)（k = 本次結標的商品數；每件 popMin O(log n)）
    advanceTime(minutes) {
        if (!Number.isInteger(minutes) || minutes < 1) {
            return fail('快轉時間必須是大於 0 的整數（分鐘）');
        }
        this._lastClosed.clear();
        this._now += minutes;
        this._closeExpiredItems();
        const closedCount = this._lastClosed.size();
        return success('時鐘快轉 ' + minutes + ' 分鐘（現在是第 ' + this._now + ' 分鐘），結標 ' +
            closedCount + ' 件商品', closedCount);
    }

    // F14：直接跳到「下一個結標時間」並結標；沒有進行中商品則回傳錯誤
    // O(k log n)
    advanceToNextEnd() {
        const next = this._endTimeTree.peekMin();
        if (next === null) {
            return fail('目前沒有進行中的商品');
        }
        let minutes = next.key.endTime - this._now;
        if (minutes < 1) {
            minutes = 1;
        }
        return this.advanceTime(minutes);
    }

    // 最近一次 advanceTime / advanceToNextEnd 結標的商品 ID（DynArray）
    // O(1)
    lastClosedItems() { return this._lastClosed; }

    // ---------------- F12 排序 ----------------

    // 商品列表排序（合併排序，穩定：欄位值相同者維持刊登順序）。回傳商品 ID 的 DynArray
    // O(n log n)
    sortedItems(field, ascending, openOnly) {
        const ids = new DynArray();
        for (let i = 0; i < this._itemOrder.size(); i++) {
            const item = this._items.find(this._itemOrder.get(i));
            if (!openOnly || item.status === ITEM_OPEN) {
                ids.pushBack(item.itemId);
            }
        }
        // 比較函式：a 應排在 b 前面嗎？（相等時回傳 false，穩定排序會保持原本順序）
        mergeSort(ids, (a, b) => {
            const valueA = sortValue(this._items.find(a), field);
            const valueB = sortValue(this._items.find(b), field);
            return ascending ? (valueA < valueB) : (valueA > valueB);
        });
        return ids;
    }

    // ---------------- F15 存檔／讀檔 ----------------
    //
    // 檔案格式（文字、一行一筆，欄位以 | 分隔；與 C++ 版相同）：
    //   AUCTION_V1
    //   now|<分鐘>
    //   nextItemId|<n>
    //   nextBidId|<n>
    //   user|<名稱>
    //   item|<id>|<賣家>|<標題>|<描述>|<起標價>|<最小加價>|<結標時間>|<狀態 0=進行中 1=已結標>|<得標者>|<得標價>
    //   bid|<bidId>|<itemId>|<出價者>|<金額>|<時間>|<有效 1/0>
    // 出價依 bidId 由小到大（時間順序）存放，讀檔時依序重播就能還原 Heap 與歷史。

    // O(n log n)（出價依 bidId 做一次合併排序）
    saveToString() {
        let out = 'AUCTION_V1\n';
        out += 'now|' + this._now + '\n';
        out += 'nextItemId|' + this._nextItemId + '\n';
        out += 'nextBidId|' + this._nextBidId + '\n';
        for (let i = 0; i < this._userNames.size(); i++) {
            out += 'user|' + escapeField(this._userNames.get(i)) + '\n';
        }

        const allBids = new DynArray();
        for (let i = 0; i < this._itemOrder.size(); i++) {
            const item = this._items.find(this._itemOrder.get(i));
            out += 'item|' + item.itemId + '|' + escapeField(item.seller) + '|' + escapeField(item.title) + '|' +
                escapeField(item.description) + '|' + item.startPrice + '|' + item.minIncrement + '|' +
                item.endTime + '|' + (item.status === ITEM_OPEN ? '0' : '1') + '|' +
                escapeField(item.winner) + '|' + item.finalPrice + '\n';
            item.bidHistory.forEach((bid) => allBids.pushBack(bid));
        }
        mergeSort(allBids, (a, b) => a.bidId < b.bidId);
        for (let i = 0; i < allBids.size(); i++) {
            const bid = allBids.get(i);
            out += 'bid|' + bid.bidId + '|' + bid.itemId + '|' + escapeField(bid.bidder) + '|' + bid.amount + '|' +
                bid.time + '|' + (bid.active ? '1' : '0') + '\n';
        }
        return out;
    }

    // 解析文字並建立資料（呼叫前自己必須是空的）。任何格式錯誤都回傳帶行號的錯誤訊息
    // O(n log n)
    _loadInto(text) {
        // 先把文字切成一行一行
        const lines = new DynArray();
        let current = '';
        for (let i = 0; i < text.length; i++) {
            const c = text.charAt(i);
            if (c === '\n') {
                lines.pushBack(current);
                current = '';
            } else if (c !== '\r') {
                current += c;
            }
        }
        if (current.length > 0) {
            lines.pushBack(current);
        }
        if (lines.empty() || lines.get(0) !== 'AUCTION_V1') {
            return fail('讀檔失敗：不是有效的存檔格式（缺少 AUCTION_V1 標頭）');
        }

        for (let lineNo = 1; lineNo < lines.size(); lineNo++) {
            const line = lines.get(lineNo);
            if (line.length === 0) {
                continue;
            }
            const where = '讀檔失敗：第 ' + (lineNo + 1) + ' 行';
            const f = splitFields(line);
            const kind = f.get(0);

            if (kind === 'now' && f.size() === 2) {
                const value = parseNumber(f.get(1));
                if (value === null) return fail(where + ' now 格式錯誤');
                this._now = value;
            } else if (kind === 'nextItemId' && f.size() === 2) {
                const value = parseNumber(f.get(1));
                if (value === null) return fail(where + ' nextItemId 格式錯誤');
                this._nextItemId = value;
            } else if (kind === 'nextBidId' && f.size() === 2) {
                const value = parseNumber(f.get(1));
                if (value === null) return fail(where + ' nextBidId 格式錯誤');
                this._nextBidId = value;
            } else if (kind === 'user' && f.size() === 2) {
                const r = this.registerUser(unescapeField(f.get(1)));
                if (!r.ok) return fail(where + ' ' + r.message);
            } else if (kind === 'item' && f.size() === 11) {
                const item = new Item();
                item.itemId = parseNumber(f.get(1));
                item.startPrice = parseNumber(f.get(5));
                item.minIncrement = parseNumber(f.get(6));
                item.endTime = parseNumber(f.get(7));
                const status = parseNumber(f.get(8));
                item.finalPrice = parseNumber(f.get(10));
                if (item.itemId === null || item.startPrice === null || item.minIncrement === null ||
                    item.endTime === null || status === null || item.finalPrice === null) {
                    return fail(where + ' 商品欄位格式錯誤');
                }
                item.seller = unescapeField(f.get(2));
                item.title = unescapeField(f.get(3));
                item.description = unescapeField(f.get(4));
                item.status = (status === 0) ? ITEM_OPEN : ITEM_CLOSED;
                item.winner = unescapeField(f.get(9));
                const seller = this._users.find(item.seller);
                if (seller === null) return fail(where + ' 賣家「' + item.seller + '」不存在');
                if (this._items.contains(item.itemId)) return fail(where + ' 商品編號重複');
                this._items.insert(item.itemId, item);
                this._itemOrder.pushBack(item.itemId);
                seller.sellingItems.pushBack(item.itemId);
                if (item.status === ITEM_OPEN) {
                    this._endTimeTree.insert(new ItemKey(item.endTime, item.itemId), item.itemId);
                } else if (item.winner.length > 0) {
                    const winner = this._users.find(item.winner);
                    if (winner === null) return fail(where + ' 得標者不存在');
                    winner.wonItems.pushBack(item.itemId);
                }
            } else if (kind === 'bid' && f.size() === 7) {
                const bid = new Bid();
                bid.bidId = parseNumber(f.get(1));
                bid.itemId = parseNumber(f.get(2));
                bid.amount = parseNumber(f.get(4));
                bid.time = parseNumber(f.get(5));
                const active = parseNumber(f.get(6));
                if (bid.bidId === null || bid.itemId === null || bid.amount === null ||
                    bid.time === null || active === null) {
                    return fail(where + ' 出價欄位格式錯誤');
                }
                bid.bidder = unescapeField(f.get(3));
                bid.active = (active !== 0);
                const item = this._items.find(bid.itemId);
                const user = this._users.find(bid.bidder);
                if (item === null) return fail(where + ' 出價的商品不存在');
                if (user === null) return fail(where + ' 出價者不存在');
                item.bidHistory.pushFront(bid);
                if (bid.active && !item.bidHeap.push(bid)) {
                    return fail(where + ' 出價編號重複');
                }
                if (!user.biddingItems.contains(bid.itemId)) {
                    user.biddingItems.pushBack(bid.itemId);
                }
            } else {
                return fail(where + ' 無法辨識：' + line);
            }
        }
        return success('讀檔成功');
    }

    // 從文字還原；格式錯誤時回傳錯誤，且「不會」破壞目前的資料
    // O(n log n)
    loadFromString(text) {
        // 先在一個暫時的系統裡試著解析：失敗就直接回報，目前的資料完全不動
        const staging = new AuctionSystem();
        const check = staging._loadInto(text);
        if (!check.ok) {
            return check;
        }
        this.reset();
        return this._loadInto(text);
    }

    // ---------------- F16 範例資料 ----------------

    // 重設並載入範例資料（5 位使用者、20 件商品、數十筆出價、1 筆撤標）
    // O(n log n)
    loadSampleData() {
        this.reset();
        const names = ['alice', 'bob', 'carol', 'dave', 'eve'];
        for (let i = 0; i < names.length; i++) {
            this.registerUser(names[i]);
        }
        // 第 0 分鐘：5 位使用者刊登全部 20 件商品
        for (let i = 0; i < SAMPLE_ITEMS.length; i++) {
            const s = SAMPLE_ITEMS[i];
            const r = this.listItem(s[0], s[1], s[2], s[3], s[4], s[5]);
            if (!r.ok) {
                return fail('範例資料建立失敗（刊登）：' + r.message);
            }
        }
        // 依時間順序重播出價與撤標事件；需要時先把時鐘往前撥
        for (let i = 0; i < SAMPLE_STEPS.length; i++) {
            const step = SAMPLE_STEPS[i];
            if (step[0] > this._now) {
                this.advanceTime(step[0] - this._now);
            }
            const r = step[4] ? this.cancelBid(step[2], step[1]) : this.placeBid(step[2], step[1], step[3]);
            if (!r.ok) {
                return fail('範例資料建立失敗（第 ' + (i + 1) + ' 個事件）：' + r.message);
            }
        }
        if (SAMPLE_FINAL_TIME > this._now) {
            this.advanceTime(SAMPLE_FINAL_TIME - this._now);
        }
        this._lastClosed.clear();
        return success('已載入範例資料（5 位使用者、' + this._itemOrder.size() + ' 件商品）');
    }

    // ---------------- 資料結構檢視（給網頁 Demo 面板用）----------------

    // 商品 Hash Table 的內部狀態
    // O(1)
    itemTable() { return this._items; }

    // 使用者 Hash Table 的內部狀態
    // O(1)
    userTable() { return this._users; }

    // 進行中商品的 BST
    // O(1)
    endTimeTree() { return this._endTimeTree; }
}
