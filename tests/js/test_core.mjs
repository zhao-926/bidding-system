// tests/js/test_core.mjs — 網頁用 JS 核心（web/js/core）的測試
//
// 執行：node tests/js/test_core.mjs（或 make test-js）。請從專案根目錄執行。
// 最重要的一項：JS 載入範例資料後存出的文字，必須和 C++ 產生的 data/sample.txt「逐位元組相同」，
// 以此證明 JS 移植版和 C++ 版行為一致。
// （測試檔本身可以自由使用 JS 內建陣列；限制只針對 web/js/core/ 內的核心程式。）

import { readFileSync } from 'node:fs';
import { BST } from '../../web/js/core/BST.js';
import { DynArray } from '../../web/js/core/DynArray.js';
import { HashTable, hashInt, hashString } from '../../web/js/core/HashTable.js';
import { LinkedList } from '../../web/js/core/LinkedList.js';
import { MaxHeap } from '../../web/js/core/MaxHeap.js';
import { mergeSort } from '../../web/js/core/MergeSort.js';
import {
    AuctionSystem, SORT_BY_BID_COUNT, SORT_BY_END_TIME, SORT_BY_PRICE,
} from '../../web/js/core/AuctionSystem.js';
import { ITEM_CLOSED, ITEM_OPEN } from '../../web/js/core/Models.js';

// ---------------------------------------------------------------- 極簡測試框架

let passedTests = 0;
let failedTests = 0;
let currentFailures = 0;

function check(condition, label) {
    if (!condition) {
        currentFailures++;
        console.log('    FAIL: ' + label);
    }
}
function eq(expected, actual, label) {
    check(expected === actual, label + '（期望 ' + expected + '，實際 ' + actual + '）');
}
function test(name, fn) {
    currentFailures = 0;
    try {
        fn();
    } catch (error) {
        currentFailures++;
        console.log('    EXCEPTION: ' + error.stack);
    }
    if (currentFailures === 0) {
        passedTests++;
        console.log('  [ OK ] ' + name);
    } else {
        failedTests++;
        console.log('  [FAIL] ' + name);
    }
}

// 固定種子的亂數（與 C++ 測試的概念相同）
class SimpleRandom {
    constructor(seed) { this.state = BigInt(seed); }
    next(bound) {
        this.state = (this.state * 6364136223846793005n + 1442695040888963407n) & 0xffffffffffffffffn;
        return Number((this.state >> 33n) % BigInt(bound));
    }
}

function toJsArray(dyn) {
    const out = [];
    for (let i = 0; i < dyn.size(); i++) out.push(dyn.get(i));
    return out;
}

// ---------------------------------------------------------------- DynArray

console.log('== DynArray ==');
test('push / get / set / back', () => {
    const a = new DynArray();
    eq(true, a.empty(), '一開始為空');
    a.pushBack(10); a.pushBack(20); a.pushBack(30);
    eq(3, a.size(), 'size');
    eq(20, a.get(1), 'get');
    a.set(1, 99);
    eq(99, a.get(1), 'set');
    eq(30, a.back(), 'back');
});
test('容量翻倍後資料正確', () => {
    const a = new DynArray();
    for (let i = 0; i < 1000; i++) a.pushBack(i * 2);
    eq(1000, a.size(), 'size');
    check(a.capacity() >= 1000, 'capacity');
    let ok = true;
    for (let i = 0; i < 1000; i++) if (a.get(i) !== i * 2) ok = false;
    check(ok, '全部資料正確');
});
test('removeAt / popBack / contains / clear', () => {
    const a = new DynArray();
    for (let i = 1; i <= 5; i++) a.pushBack(i);
    eq(true, a.removeAt(2), '刪中間');
    eq('1,2,4,5', toJsArray(a).join(','), '順序');
    eq(false, a.removeAt(9), '越界回傳 false');
    eq(false, a.removeAt(-1), '負索引回傳 false');
    a.popBack();
    eq(3, a.size(), 'popBack');
    eq(true, a.contains(4), 'contains');
    eq(false, a.contains(5), '不含');
    a.clear();
    eq(true, a.empty(), 'clear');
});
test('越界與空陣列會丟出錯誤', () => {
    const a = new DynArray();
    let threw = 0;
    try { a.get(0); } catch (e) { threw++; }
    try { a.popBack(); } catch (e) { threw++; }
    try { a.back(); } catch (e) { threw++; }
    eq(3, threw, '三個例外');
});
test('clone 是獨立的副本', () => {
    const a = new DynArray();
    a.pushBack(1);
    const b = a.clone();
    b.pushBack(2);
    eq(1, a.size(), '原本不變');
    eq(2, b.size(), '副本變動');
});

// ---------------------------------------------------------------- LinkedList

console.log('== LinkedList ==');
test('pushFront 順序（由新到舊）、size、front', () => {
    const l = new LinkedList();
    eq(null, l.front(), '空串列 front 為 null');
    l.pushFront(1); l.pushFront(2); l.pushFront(3);
    let order = '';
    l.forEach((v) => { order += v; });
    eq('321', order, '走訪順序');
    eq(3, l.size(), 'size');
    eq(3, l.front(), 'front');
});
test('popFront / find / clear', () => {
    const l = new LinkedList();
    eq(false, l.popFront(), '空串列 popFront');
    l.pushFront({ id: 1 }); l.pushFront({ id: 2 });
    eq(2, l.find((x) => x.id === 2).id, 'find 命中');
    eq(null, l.find((x) => x.id === 9), 'find 找不到');
    l.find((x) => x.id === 1).id = 100;
    eq(100, l.find((x) => x.id === 100).id, 'find 回傳同一個物件，可直接修改');
    eq(true, l.popFront(), 'popFront');
    eq(1, l.size(), 'size');
    l.clear();
    eq(true, l.empty(), 'clear');
});

// ---------------------------------------------------------------- HashTable

console.log('== HashTable ==');
test('整數 key：insert / find / 更新 / remove', () => {
    const h = new HashTable(hashInt);
    eq(null, h.find(1), '空表');
    eq(true, h.insert(1, 'one'), '新增');
    eq(false, h.insert(1, 'uno'), '重複 key 只更新');
    eq('uno', h.find(1), '值被更新');
    eq(1, h.size(), 'size');
    eq(false, h.remove(99), '刪不存在的 key');
    eq(true, h.remove(1), '刪存在的 key');
    eq(0, h.size(), 'size');
});
test('rehash：1000 筆資料全部正確、load factor <= 0.75', () => {
    const h = new HashTable(hashInt);
    const initial = h.bucketCount();
    for (let i = 0; i < 1000; i++) h.insert(i, i * 3);
    check(h.bucketCount() > initial, 'bucket 數增加');
    check(h.loadFactor() <= 0.75, 'load factor');
    let ok = true;
    for (let i = 0; i < 1000; i++) if (h.find(i) !== i * 3) ok = false;
    check(ok, '資料全部還在');
    let total = 0;
    for (let b = 0; b < h.bucketCount(); b++) total += h.bucketLength(b);
    eq(1000, total, '各 bucket 鏈長度總和 = size');
    eq(0, h.bucketLength(-1), '不合法的 bucket');
});
test('字串 key、負數 key、forEach、clear', () => {
    const h = new HashTable(hashString);
    h.insert('alice', 1); h.insert('bob', 2);
    eq(2, h.find('bob'), 'bob');
    let count = 0;
    h.forEach(() => { count++; });
    eq(2, count, 'forEach');
    h.clear();
    eq(0, h.size(), 'clear');
    const n = new HashTable(hashInt);
    n.insert(-1, 'a'); n.insert(-100, 'b');
    eq('b', n.find(-100), '負數 key');
});
test('hash 函式與 C++ 版相同（固定數值）', () => {
    // C++：djb2("alice")、乘法雜湊（key=1）。這些數值由 C++ 公式手算並以相同公式驗證
    eq(5381, hashString(''), 'djb2 空字串 = 5381');
    eq((5381 * 33 + 97) >>> 0, hashString('a'), 'djb2("a")');
    eq(((Math.imul(1, 0x9e3779b1) >>> 0) ^ ((Math.imul(1, 0x9e3779b1) >>> 0) >>> 16)) >>> 0, hashInt(1), 'hashInt(1)');
});

// ---------------------------------------------------------------- MaxHeap

console.log('== MaxHeap ==');
class TestBid {
    constructor(id, amount, time) { this.id = id; this.amount = amount; this.time = time; }
    getId() { return this.id; }
    hasHigherPriority(o) { return this.amount !== o.amount ? this.amount > o.amount : this.time < o.time; }
}
test('top / pop 順序、同價時間早者優先', () => {
    const h = new MaxHeap();
    eq(null, h.top(), '空堆積');
    eq(false, h.pop(), '空堆積 pop');
    h.push(new TestBid(1, 100, 1)); h.push(new TestBid(2, 300, 2)); h.push(new TestBid(3, 200, 3));
    eq(2, h.top().id, '最高');
    h.push(new TestBid(4, 300, 0));                      // 同價但時間更早
    eq(4, h.top().id, '同價時間早者優先');
    let order = '';
    while (!h.empty()) { order += h.top().id; h.pop(); }
    eq('4231', order, 'pop 順序');
});
test('removeById：刪頂、刪中間、不存在、重複 id', () => {
    const h = new MaxHeap();
    for (let i = 1; i <= 15; i++) h.push(new TestBid(i, i * 10, i));
    eq(false, h.push(new TestBid(3, 999, 99)), '重複 id');
    eq(true, h.removeById(15), '刪頂');
    eq(14, h.top().id, '前一位上來');
    eq(true, h.removeById(8), '刪中間');
    eq(false, h.removeById(99), '不存在');
    check(h.isValid(), '堆積仍然合法');
    eq(false, h.contains(8), 'contains');
});
test('隨機測試：與暴力解比對（1000 步 × 5 組）', () => {
    let allMatched = true;
    for (let seed = 1; seed <= 5; seed++) {
        const rnd = new SimpleRandom(seed * 7919);
        const heap = new MaxHeap();
        const brute = [];
        let nextId = 1;
        const bestIndex = () => {
            let best = 0;
            for (let i = 1; i < brute.length; i++) if (brute[i].hasHigherPriority(brute[best])) best = i;
            return best;
        };
        for (let step = 0; step < 1000; step++) {
            const action = rnd.next(10);
            if (action < 5 || brute.length === 0) {
                const bid = new TestBid(nextId++, rnd.next(50), step);
                heap.push(bid); brute.push(bid);
            } else if (action < 7) {
                const b = bestIndex(); heap.pop(); brute.splice(b, 1);
            } else {
                const v = rnd.next(brute.length);
                if (!heap.removeById(brute[v].id)) allMatched = false;
                brute.splice(v, 1);
            }
            if (heap.size() !== brute.length || !heap.isValid()) allMatched = false;
            if (brute.length > 0 && heap.top().id !== brute[bestIndex()].id) allMatched = false;
        }
    }
    check(allMatched, '每一步都與暴力解一致');
});

// ---------------------------------------------------------------- BST

console.log('== BST ==');
const numLess = (a, b) => a < b;
function inorderKeys(tree) { let s = ''; tree.inorder((k) => { s += k + ','; }); return s; }
function rangeKeys(tree, lo, hi) { let s = ''; tree.rangeQuery(lo, hi, (k) => { s += k + ','; }); return s; }
test('空樹與基本操作', () => {
    const t = new BST(numLess);
    eq(true, t.empty(), '空');
    eq(0, t.height(), '高度');
    eq(null, t.find(1), 'find');
    eq(false, t.remove(1), 'remove');
    eq(null, t.popMin(), 'popMin');
    eq(null, t.peekMin(), 'peekMin');
    eq(true, t.insert(5, 'a'), 'insert');
    eq(false, t.insert(5, 'b'), '重複 key 更新');
    eq('b', t.find(5), '更新後的值');
});
test('remove 三種情況（葉、單子、雙子）', () => {
    const t = new BST(numLess);
    const keys = [50, 30, 70, 20, 40, 60, 80, 65];
    for (let i = 0; i < keys.length; i++) t.insert(keys[i], i * 10);
    eq(true, t.remove(20), '葉');
    eq('30,40,50,60,65,70,80,', inorderKeys(t), '刪葉後');
    eq(true, t.remove(50), '根（雙子）');
    eq('30,40,60,65,70,80,', inorderKeys(t), '刪根後');
    eq(50, t.find(60), '後繼者的 value 跟著 key 搬動');
    eq(true, t.remove(70), '70 有兩個子節點');
    eq('30,40,60,65,80,', inorderKeys(t), '刪 70 後');
    eq(true, t.remove(30), '單一子節點');
    eq(false, t.remove(999), '不存在');
});
test('popMin / peekMin / rangeQuery / height', () => {
    const t = new BST(numLess);
    for (let k = 10; k <= 100; k += 10) t.insert(k, k * 2);
    eq('30,40,50,', rangeKeys(t, 30, 50), '範圍兩端都含');
    eq('', rangeKeys(t, 31, 39), '範圍內沒有');
    eq('70,', rangeKeys(t, 70, 70), '單點');
    eq('', rangeKeys(t, 50, 40), '起迄顛倒');
    eq(10, t.peekMin().key, 'peekMin');
    eq(10, t.size(), 'peek 不刪除');
    const m = t.popMin();
    eq(20, m.value, 'popMin value');
    eq(9, t.size(), 'popMin 刪除');
    const seq = new BST(numLess);
    for (let k = 1; k <= 1000; k++) seq.insert(k, 0);
    eq(1000, seq.height(), '依序插入退化成鏈');
});
test('退化的深樹不會 stack overflow（迴圈實作）', () => {
    const t = new BST(numLess);
    for (let k = 0; k < 20000; k++) t.insert(k, k);
    let count = 0;
    t.inorder(() => { count++; });
    eq(20000, count, 'inorder 走完 20000 層');
    eq(20000, t.layout().size(), 'layout 走完');
});
test('layout：rank / depth / parentRank', () => {
    const t = new BST(numLess);
    [20, 10, 30, 5].forEach((k) => t.insert(k, k));
    const rows = toJsArray(t.layout());
    eq('5,10,20,30', rows.map((r) => r.key).join(','), '中序');
    eq('3,2,1,2', rows.map((r) => r.depth).join(','), '深度');
    eq('1,2,-1,2', rows.map((r) => r.parentRank).join(','), '父節點名次');
});
test('隨機測試：insert / remove 與暴力解比對', () => {
    let ok = true;
    for (let seed = 1; seed <= 5; seed++) {
        const rnd = new SimpleRandom(seed * 104729);
        const t = new BST(numLess);
        const present = [];
        for (let step = 0; step < 1000; step++) {
            const key = rnd.next(200);
            if (rnd.next(2) === 0) {
                const isNew = t.insert(key, key * 3);
                const bruteNew = present.indexOf(key) < 0;
                if (isNew !== bruteNew) ok = false;
                if (bruteNew) present.push(key);
            } else {
                const removed = t.remove(key);
                const idx = present.indexOf(key);
                if (removed !== (idx >= 0)) ok = false;
                if (idx >= 0) present.splice(idx, 1);
            }
            if (t.size() !== present.length) ok = false;
        }
        let previous = -1;
        let visited = 0;
        t.inorder((k) => { if (k <= previous) ok = false; previous = k; visited++; });
        if (visited !== present.length) ok = false;
        for (let trial = 0; trial < 20; trial++) {
            const a = rnd.next(200), b = rnd.next(200);
            const lo = Math.min(a, b), hi = Math.max(a, b);
            let treeCount = 0;
            t.rangeQuery(lo, hi, () => { treeCount++; });
            let bruteCount = 0;
            for (const k of present) if (k >= lo && k <= hi) bruteCount++;
            if (treeCount !== bruteCount) ok = false;
        }
    }
    check(ok, '與暴力解一致');
});

// ---------------------------------------------------------------- mergeSort

console.log('== mergeSort ==');
test('各種長度排序正確、不使用內建 sort', () => {
    const rnd = new SimpleRandom(99);
    let ok = true;
    const sizes = [0, 1, 2, 3, 4, 5, 7, 8, 9, 16, 17, 100, 101, 1000];
    for (const n of sizes) {
        const a = new DynArray();
        const counts = new Array(50).fill(0);
        for (let i = 0; i < n; i++) { const v = rnd.next(50); a.pushBack(v); counts[v]++; }
        mergeSort(a, (x, y) => x < y);
        for (let i = 1; i < a.size(); i++) if (a.get(i - 1) > a.get(i)) ok = false;
        for (let i = 0; i < a.size(); i++) counts[a.get(i)]--;
        for (const c of counts) if (c !== 0) ok = false;
        if (a.size() !== n) ok = false;
    }
    check(ok, '有序且元素不變');
});
test('穩定性：相同 key 保持原本順序', () => {
    const a = new DynArray();
    const rnd = new SimpleRandom(2024);
    for (let i = 0; i < 500; i++) a.pushBack({ key: rnd.next(10), original: i });
    mergeSort(a, (x, y) => x.key < y.key);
    let stable = true, sorted = true;
    for (let i = 1; i < a.size(); i++) {
        const p = a.get(i - 1), c = a.get(i);
        if (p.key > c.key) sorted = false;
        if (p.key === c.key && p.original > c.original) stable = false;
    }
    check(sorted, '有序');
    check(stable, '穩定');
});

// ---------------------------------------------------------------- AuctionSystem 業務邏輯

console.log('== AuctionSystem ==');
function newSystem() {
    const s = new AuctionSystem();
    ['alice', 'bob', 'carol', 'dave'].forEach((n) => s.registerUser(n));
    return s;
}
function list(s, seller, title, start, inc, duration) {
    const r = s.listItem(seller, title, 'desc', start, inc, duration);
    return r.ok ? r.id : -1;
}

test('F1 使用者：註冊、重複、空白', () => {
    const s = new AuctionSystem();
    eq(true, s.registerUser('alice').ok, '註冊');
    eq(false, s.registerUser('alice').ok, '重複');
    eq(false, s.registerUser('').ok, '空白');
    eq(false, s.registerUser('a|b').ok, '含 |');
    eq(true, s.userExists('alice'), 'userExists');
    eq(null, s.findUser('nobody'), '找不到');
});
test('F2/F3 刊登與查詢、驗證錯誤', () => {
    const s = newSystem();
    s.advanceTime(100);
    const r = s.listItem('alice', '自行車', '九成新', 1000, 50, 60);
    eq(true, r.ok, '刊登');
    eq(160, s.findItem(r.id).endTime, 'endTime = now + 時長');
    eq(false, s.listItem('ghost', 'x', '', 100, 10, 60).ok, '賣家不存在');
    eq(false, s.listItem('alice', '', '', 100, 10, 60).ok, '空標題');
    eq(false, s.listItem('alice', 'x', '', 0, 10, 60).ok, '起標價 0');
    eq(false, s.listItem('alice', 'x', '', 100, -1, 60).ok, '負加價');
    eq(false, s.listItem('alice', 'x', '', 100, 10, 0).ok, '時長 0');
    eq(null, s.findItem(999), '找不到商品');
});
test('F4 關鍵字搜尋', () => {
    const s = newSystem();
    list(s, 'alice', 'Mechanical Keyboard', 100, 10, 60);
    list(s, 'bob', '藍牙耳機', 100, 10, 60);
    list(s, 'carol', 'keyboard cover', 100, 10, 5);
    eq(2, s.searchItems('KEYBOARD', false).size(), '不分大小寫');
    eq(1, s.searchItems('耳機', false).size(), '中文');
    eq(3, s.searchItems('', false).size(), '空關鍵字 = 全部');
    s.advanceTime(10);
    eq(1, s.searchItems('keyboard', true).size(), '只找進行中');
});
test('F5 出價規則：起標價、加價、賣家、不存在、已結標', () => {
    const s = newSystem();
    const id = list(s, 'alice', 'item', 100, 10, 60);
    eq(false, s.placeBid('bob', id, 99).ok, '低於起標價');
    eq(true, s.placeBid('bob', id, 100).ok, '等於起標價');
    eq(false, s.placeBid('carol', id, 109).ok, '少 1 元');
    const low = s.placeBid('carol', id, 105);
    eq(true, low.message.indexOf('110') >= 0, '訊息告知最低可出價');
    eq(true, s.placeBid('carol', id, 110).ok, '剛好最低');
    eq(false, s.placeBid('alice', id, 500).ok, '賣家不能出價');
    eq(false, s.placeBid('ghost', id, 500).ok, '使用者不存在');
    eq(false, s.placeBid('bob', 999, 500).ok, '商品不存在');
    s.advanceTime(60);
    const late = s.placeBid('dave', id, 500);
    eq(false, late.ok, '結標後出價');
    eq(true, late.message.indexOf('已結標') >= 0, '訊息');
});
test('同價先到者勝（minIncrement = 0）', () => {
    const s = newSystem();
    const id = list(s, 'alice', 'item', 100, 0, 60);
    s.placeBid('bob', id, 200); s.advanceTime(5);
    s.placeBid('carol', id, 200); s.advanceTime(5);
    s.placeBid('dave', id, 200);
    eq('bob', s.highestBid(id).bidder, '最早的 bob');
    s.advanceTime(100);
    eq('bob', s.findItem(id).winner, '得標者');
});
test('F6/F7/F8 歷史順序、撤標回到前一位、只撤最新一筆', () => {
    const s = newSystem();
    const id = list(s, 'alice', 'item', 100, 10, 60);
    s.placeBid('bob', id, 100);
    s.placeBid('carol', id, 200);
    s.placeBid('bob', id, 300);
    const h = s.bidHistory(id);
    eq(3, h.size(), '3 筆歷史');
    eq(300, h.get(0).amount, '由新到舊');
    eq(true, s.cancelBid('bob', id).ok, '撤標');
    eq(200, s.highestBid(id).amount, '撤的是 300，回到 200');
    eq(false, s.bidHistory(id).get(0).active, '歷史標記已撤標');
    eq(true, s.bidHistory(id).get(2).active, 'bob 的 100 仍有效');
    eq(true, s.cancelBid('bob', id).ok, '再撤一次撤掉 100');
    eq(false, s.cancelBid('bob', id).ok, '已沒有有效出價');
    eq(true, s.cancelBid('carol', id).ok, 'carol 撤標');
    eq(null, s.highestBid(id), '全撤光');
    eq(false, s.cancelBid('dave', id).ok, '沒出價過');
    s.placeBid('dave', id, 100);
    s.advanceTime(60);
    eq(false, s.cancelBid('dave', id).ok, '已結標不能撤');
});
test('F9/F10 即將結標順序與範圍查詢', () => {
    const s = newSystem();
    list(s, 'alice', 'a', 100, 10, 300);
    list(s, 'bob', 'b', 100, 10, 100);
    list(s, 'carol', 'c', 100, 10, 200);
    list(s, 'dave', 'd', 100, 10, 100);
    eq('2,4,3,1', toJsArray(s.upcomingItems(0)).join(','), '同時間以 ID 小者優先');
    eq(2, s.upcomingItems(2).size(), '限制筆數');
    eq('2,4', toJsArray(s.itemsEndingBetween(100, 100)).join(','), '單一時間點');
    eq('2,4,3', toJsArray(s.itemsEndingBetween(0, 250)).join(','), '範圍');
    eq(0, s.itemsEndingBetween(50, 30).size(), '起迄顛倒');
    s.advanceTime(150);
    eq('3,1', toJsArray(s.upcomingItems(0)).join(','), '已結標的不再出現');
});
test('F11 結標：得標、流標、一次結標多件', () => {
    const s = newSystem();
    const a = list(s, 'alice', 'a', 100, 10, 50);
    const b = list(s, 'alice', 'b', 100, 10, 20);
    const c = list(s, 'alice', 'c', 100, 10, 80);
    const d = list(s, 'alice', 'd', 100, 10, 500);
    s.placeBid('bob', a, 100);
    s.placeBid('carol', a, 150);
    s.placeBid('dave', a, 400);
    s.cancelBid('dave', a);
    const r = s.advanceTime(100);
    eq(3, r.id, '結標 3 件');
    eq(`${b},${a},${c}`, toJsArray(s.lastClosedItems()).join(','), '依結標時間排序');
    eq('carol', s.findItem(a).winner, '得標者（撤掉的最高價不算）');
    eq(150, s.findItem(a).finalPrice, '得標價');
    eq(1, s.findUser('carol').wonItems.size(), 'wonItems');
    eq('', s.findItem(b).winner, '流標');
    eq(ITEM_CLOSED, s.findItem(b).status, '已結標');
    eq(ITEM_OPEN, s.findItem(d).status, '還沒到時間');
    s.advanceTime(1);
    eq(0, s.lastClosedItems().size(), '這次沒有結標');
});
test('F14 時鐘：驗證與跳到下一個結標', () => {
    const s = newSystem();
    eq(false, s.advanceToNextEnd().ok, '沒有進行中商品');
    eq(false, s.advanceTime(0).ok, '0');
    eq(false, s.advanceTime(-5).ok, '負數');
    eq(false, s.advanceTime(1.5).ok, '非整數');
    const a = list(s, 'alice', 'a', 100, 10, 90);
    const b = list(s, 'alice', 'b', 100, 10, 40);
    s.advanceToNextEnd();
    eq(40, s.now(), '跳到 40');
    eq(ITEM_CLOSED, s.findItem(b).status, 'b 結標');
    s.advanceToNextEnd();
    eq(90, s.now(), '跳到 90');
    eq(ITEM_CLOSED, s.findItem(a).status, 'a 結標');
});
test('F12 排序（穩定、升降冪、openOnly）', () => {
    const s = newSystem();
    const i1 = list(s, 'alice', 'a', 500, 10, 300);
    const i2 = list(s, 'alice', 'b', 100, 10, 100);
    const i3 = list(s, 'alice', 'c', 300, 10, 200);
    const i4 = list(s, 'alice', 'd', 300, 10, 400);
    s.placeBid('bob', i2, 100); s.placeBid('carol', i2, 150); s.placeBid('bob', i3, 300);
    eq(`${i2},${i3},${i4},${i1}`, toJsArray(s.sortedItems(SORT_BY_PRICE, true, false)).join(','), '價格升冪（同價維持刊登順序）');
    eq(`${i1},${i3},${i4},${i2}`, toJsArray(s.sortedItems(SORT_BY_PRICE, false, false)).join(','), '價格降冪（同價仍維持刊登順序）');
    eq(i2, s.sortedItems(SORT_BY_BID_COUNT, false, false).get(0), '出價數最多');
    eq(`${i2},${i3},${i1},${i4}`, toJsArray(s.sortedItems(SORT_BY_END_TIME, true, false)).join(','), '結標時間');
    s.advanceTime(150);
    eq(3, s.sortedItems(SORT_BY_PRICE, true, true).size(), '只列進行中');
});
test('F13 我的頁面', () => {
    const s = newSystem();
    const i1 = list(s, 'alice', 'a', 100, 10, 50);
    const i2 = list(s, 'alice', 'b', 100, 10, 90);
    s.placeBid('carol', i1, 100); s.placeBid('carol', i1, 200); s.placeBid('carol', i2, 100);
    eq(2, s.findUser('alice').sellingItems.size(), '我賣的');
    eq(2, s.findUser('carol').biddingItems.size(), '我出價的（不重複）');
    s.advanceTime(60);
    eq(i1, s.findUser('carol').wonItems.get(0), '我得標的');
});
test('F15 存檔／讀檔：往返、特殊字元、壞檔不破壞現有資料', () => {
    const a = new AuctionSystem();
    a.loadSampleData();
    const saved = a.saveToString();
    const b = new AuctionSystem();
    eq(true, b.loadFromString(saved).ok, '讀檔');
    eq(saved, b.saveToString(), '存→讀→存 內容相同');
    check(b.placeBid('dave', 6, 5000).ok, '還原後仍可出價');

    const c = new AuctionSystem();
    c.registerUser('alice'); c.registerUser('bob');
    c.listItem('alice', '標題含|直線與%百分比', '第一行\n第二行|含分隔線 100%', 100, 10, 60);
    const d = new AuctionSystem();
    d.loadFromString(c.saveToString());
    eq('標題含|直線與%百分比', d.findItem(1).title, '標題特殊字元');
    eq('第一行\n第二行|含分隔線 100%', d.findItem(1).description, '描述特殊字元');

    const sys = new AuctionSystem();
    sys.loadSampleData();
    const before = sys.saveToString();
    eq(false, sys.loadFromString('').ok, '空字串');
    eq(false, sys.loadFromString('hello').ok, '非存檔格式');
    eq(false, sys.loadFromString('AUCTION_V1\nbid|1|99|nobody|100|0|1\n').ok, '商品不存在');
    const garbage = sys.loadFromString('AUCTION_V1\nunknown line\n');
    eq(false, garbage.ok, '無法辨識的行');
    eq(true, garbage.message.indexOf('第 2 行') >= 0, '錯誤訊息含行號');
    eq(before, sys.saveToString(), '現有資料完全沒變');
});
test('F16 範例資料：數量與內容', () => {
    const s = new AuctionSystem();
    eq(true, s.loadSampleData().ok, '載入');
    eq(5, s.userNames().size(), '5 位使用者');
    eq(20, s.allItemIds().size(), '20 件商品');
    let totalBids = 0, cancelled = 0, openNoBids = 0, sold = 0, unsold = 0;
    for (let i = 0; i < s.allItemIds().size(); i++) {
        const item = s.findItem(s.allItemIds().get(i));
        totalBids += item.bidHistory.size();
        s.bidHistory(item.itemId).size();
        for (let j = 0; j < s.bidHistory(item.itemId).size(); j++) if (!s.bidHistory(item.itemId).get(j).active) cancelled++;
        if (item.status === ITEM_OPEN && item.bidHistory.empty()) openNoBids++;
        if (item.status === ITEM_CLOSED && item.winner !== '') sold++;
        if (item.status === ITEM_CLOSED && item.winner === '') unsold++;
    }
    check(totalBids >= 30, '數十筆出價');
    check(cancelled >= 1, '含撤標');
    check(openNoBids >= 1, '有進行中且無人出價');
    check(sold >= 1 && unsold >= 1, '有成交與流標案例');
    eq(60, s.now(), '範例資料建立完成時間');
});
test('★ 與 C++ 版逐位元組一致：JS 範例資料存檔 == data/sample.txt', () => {
    const cppText = readFileSync('data/sample.txt', 'utf8').replace(/\r\n/g, '\n');
    const s = new AuctionSystem();
    s.loadSampleData();
    eq(true, s.saveToString() === cppText, 'JS saveToString() 與 C++ 產生的 data/sample.txt 完全相同');
    // 反向：JS 讀 C++ 的檔案，再存出來也要一樣
    const t = new AuctionSystem();
    eq(true, t.loadFromString(cppText).ok, 'JS 能讀 C++ 的存檔');
    eq(true, t.saveToString() === cppText, '讀進來再存出去仍相同');
});

// 隨機一致性：堆頂 = 歷史中有效出價的最優者；進行中商品數 = BST 節點數；存讀檔往返相同
function consistent(sys) {
    let openCount = 0;
    for (let i = 0; i < sys.allItemIds().size(); i++) {
        const item = sys.findItem(sys.allItemIds().get(i));
        const history = toJsArray(sys.bidHistory(item.itemId));
        let best = null, active = 0;
        for (const b of history) {
            if (!b.active) continue;
            active++;
            if (best === null || b.hasHigherPriority(best)) best = b;
        }
        const top = sys.highestBid(item.itemId);
        if (best === null ? top !== null : (top === null || top.bidId !== best.bidId)) return false;
        if (active !== item.bidHeap.size() || !item.bidHeap.isValid()) return false;
        if (item.status === ITEM_OPEN) openCount++;
        else if (best !== null && (item.winner !== best.bidder || item.finalPrice !== best.amount)) return false;
        else if (best === null && (item.winner !== '' || item.finalPrice !== 0)) return false;
    }
    return openCount === sys.endTimeTree().size();
}
test('隨機操作 1000 步 × 3 組後系統仍一致', () => {
    let ok = true;
    const names = ['alice', 'bob', 'carol', 'dave'];
    for (let seed = 1; seed <= 3; seed++) {
        const rnd = new SimpleRandom(seed * 4241);
        const sys = newSystem();
        for (let step = 0; step < 1000; step++) {
            const action = rnd.next(10);
            const count = sys.allItemIds().size();
            if (action < 2) {
                list(sys, names[rnd.next(4)], 'random item', 10 + rnd.next(50), rnd.next(3), 1 + rnd.next(120));
            } else if (action < 7 && count > 0) {
                sys.placeBid(names[rnd.next(4)], sys.allItemIds().get(rnd.next(count)), 10 + rnd.next(400));
            } else if (action < 9 && count > 0) {
                sys.cancelBid(names[rnd.next(4)], sys.allItemIds().get(rnd.next(count)));
            } else {
                sys.advanceTime(1 + rnd.next(30));
            }
            if (step % 25 === 0 && !consistent(sys)) ok = false;
        }
        if (!consistent(sys)) ok = false;
        const copy = new AuctionSystem();
        if (!copy.loadFromString(sys.saveToString()).ok || copy.saveToString() !== sys.saveToString()) ok = false;
        if (!consistent(copy)) ok = false;
    }
    check(ok, '一致性檢查全部通過');
});

// ---------------------------------------------------------------- 結果

console.log('');
console.log('JS tests: ' + passedTests + '/' + (passedTests + failedTests) + ' passed');
process.exit(failedTests === 0 ? 0 : 1);
