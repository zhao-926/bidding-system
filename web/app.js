// app.js — 網頁介面邏輯
//
// 這個檔案只負責「畫面」：讀使用者操作 → 呼叫 js/core 裡自刻的 AuctionSystem → 把結果畫出來。
// 所有業務規則與資料結構都在 js/core/（與 C++ 版 core/ 一比一對應）。
// 注意：資料結構的限制（不准用 Map/Set/sort 等）只針對 js/core/；這裡是介面程式，可以自由使用 JS。

import {
    AuctionSystem, SORT_BY_BID_COUNT, SORT_BY_END_TIME, SORT_BY_PRICE,
} from './js/core/AuctionSystem.js';
import { ITEM_OPEN } from './js/core/Models.js';

// ---------------------------------------------------------------- 狀態

const STORAGE_KEY = 'bidding-system:save:v1';
const USER_KEY = 'bidding-system:user:v1';

const system = new AuctionSystem();
const state = {
    user: '',            // 目前使用者
    view: 'list',        // list | sell | mine | ds
    search: '',
    sort: 'listing',     // listing | price | bids | end
    order: 'asc',
    openOnly: false,
    detailId: null,      // 詳情視窗顯示的商品 ID
    heapItemId: null,    // 資料結構檢視：要看哪件商品的 Heap
    rangeResult: null,   // 側欄範圍查詢的結果 { t1, t2, ids }
};

// 本次操作的說明（顯示在底部操作列）
const OPS = {
    register: ['註冊使用者', 'HashTable.insert', 'O(1) 平均'],
    switchUser: ['切換使用者', 'HashTable.find', 'O(1) 平均'],
    list: ['刊登商品', 'HashTable.insert + BST.insert', 'O(1) 平均 + O(log n) 平均'],
    bid: ['出價', 'MaxHeap.push + LinkedList.pushFront', 'O(log n) + O(1)'],
    cancel: ['撤回出價', 'MaxHeap.removeById（HashTable 以 O(1) 找到堆積位置）', 'O(log n)'],
    advance: ['時鐘快轉／自動結標', 'BST.popMin（每件）＋ 堆頂決定得標者', 'O(k log n)'],
    search: ['關鍵字搜尋', 'DynArray 線性掃描', 'O(n·m)'],
    sort: ['商品排序', 'mergeSort（穩定）', 'O(n log n)'],
    open: ['查看商品', 'HashTable.find＋Heap 堆頂＋LinkedList 走訪', 'O(1) 平均 + O(m)'],
    upcoming: ['即將結標列表', 'BST.inorder', 'O(n)'],
    range: ['時段範圍查詢', 'BST.rangeQuery', 'O(log n + k)'],
    reset: ['重設為範例資料', '批次刊登與出價（HashTable、BST、MaxHeap、LinkedList）', 'O(n log n)'],
    mine: ['我的頁面', 'HashTable.find ＋ DynArray 走訪', 'O(1) 平均 + O(k)'],
};

// ---------------------------------------------------------------- 小工具

function $(selector) { return document.querySelector(selector); }

// 把使用者輸入的文字轉成安全的 HTML（避免 XSS）
function esc(text) {
    return String(text)
        .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;').replace(/'/g, '&#39;');
}

function money(n) { return n.toLocaleString('en-US'); }

// 分鐘 -> 「1 天 2 小時 5 分」
function fmtDuration(minutes) {
    if (minutes <= 0) return '0 分';
    const days = Math.floor(minutes / 1440);
    const hours = Math.floor((minutes % 1440) / 60);
    const mins = minutes % 60;
    let text = '';
    if (days > 0) text += days + ' 天 ';
    if (hours > 0) text += hours + ' 小時 ';
    if (mins > 0 || text === '') text += mins + ' 分';
    return text.trim();
}

function storageGet(key) {
    try { return window.localStorage.getItem(key); } catch (e) { return null; }
}
function storageSet(key, value) {
    try { window.localStorage.setItem(key, value); } catch (e) { /* 隱私模式等情況下無法存檔，略過 */ }
}

function save() {
    storageSet(STORAGE_KEY, system.saveToString());
    storageSet(USER_KEY, state.user);
}

let toastTimer = null;
function toast(message, ok) {
    const el = $('#toast');
    el.textContent = message;
    el.className = 'toast ' + (ok ? 'ok' : 'error');
    el.hidden = false;
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => { el.hidden = true; }, ok ? 2600 : 4500);
}

function showOp(op, elapsedMs) {
    $('#opText').innerHTML =
        esc(op[0]) + ' — ' + esc(op[1]) + '，複雜度 <span class="cx">' + esc(op[2]) + '</span>' +
        ' <span class="ms">（實測 ' + elapsedMs.toFixed(3) + ' ms）</span>';
}

// 統一的操作流程：執行 → 顯示本次操作與複雜度 → 成功／失敗提示 → 需要時存檔 → 重畫
function perform(op, fn, mutates) {
    const t0 = performance.now();
    const result = fn();
    const elapsed = performance.now() - t0;
    showOp(op, elapsed);
    if (result && result.ok === false) {
        toast(result.message, false);
    } else if (result && result.ok === true && result.message) {
        toast(result.message, true);
    }
    if (mutates && (!result || result.ok)) save();
    renderAll();
    return result;
}

function toArray(dynArray) {
    const out = [];
    for (let i = 0; i < dynArray.size(); i++) out.push(dynArray.get(i));
    return out;
}

// ---------------------------------------------------------------- 商品顯示

function statusBadge(item) {
    if (item.status === ITEM_OPEN) return '<span class="badge badge-open">進行中</span>';
    if (item.winner === '') return '<span class="badge badge-unsold">流標</span>';
    return '<span class="badge badge-sold">已成交</span>';
}

function remainingText(item) {
    const left = item.endTime - system.now();
    const urgent = left <= 60 ? ' urgent' : '';
    return '<span class="' + urgent.trim() + '">剩餘 ' + esc(fmtDuration(left)) + '</span>';
}

function itemCard(item) {
    const price = AuctionSystem.currentPrice(item);
    const top = item.bidHeap.top();
    const label = item.status === ITEM_OPEN
        ? (top ? '目前最高價' : '起標價')
        : (item.winner === '' ? '起標價' : '得標價');
    const shown = item.status === ITEM_OPEN || item.winner === '' ? price : item.finalPrice;
    const foot = item.status === ITEM_OPEN
        ? remainingText(item)
        : (item.winner === '' ? '無人出價' : '得標：' + esc(item.winner));
    return '<button type="button" class="card" data-action="open-item" data-id="' + item.itemId + '">' +
        '<div class="card-head"><span class="card-title">' + esc(item.title) + '</span>' + statusBadge(item) + '</div>' +
        '<div class="card-id">#' + item.itemId + ' · 賣家 ' + esc(item.seller) + '</div>' +
        '<div class="price">' + money(shown) + '<small>元 · ' + label + '</small></div>' +
        '<div class="card-meta"><span>有效出價 ' + item.bidHeap.size() + ' 筆</span>' + foot + '</div>' +
        '</button>';
}

function currentItemIds() {
    const ids = [];
    const matches = system.searchItems(state.search, state.openOnly);
    if (state.sort === 'listing') {
        const all = toArray(matches);
        if (state.order === 'desc') all.reverse();
        return all;
    }
    const field = state.sort === 'price' ? SORT_BY_PRICE : (state.sort === 'bids' ? SORT_BY_BID_COUNT : SORT_BY_END_TIME);
    const sorted = system.sortedItems(field, state.order === 'asc', state.openOnly);
    const allowed = {};
    for (let i = 0; i < matches.size(); i++) allowed[matches.get(i)] = true;
    for (let i = 0; i < sorted.size(); i++) {
        if (allowed[sorted.get(i)]) ids.push(sorted.get(i));
    }
    return ids;
}

// ---------------------------------------------------------------- 各區塊繪製

function renderTop() {
    const select = $('#userSelect');
    let html = '<option value="">（尚未選擇）</option>';
    const names = system.userNames();
    for (let i = 0; i < names.size(); i++) {
        const name = names.get(i);
        html += '<option value="' + esc(name) + '"' + (name === state.user ? ' selected' : '') + '>' + esc(name) + '</option>';
    }
    select.innerHTML = html;
    $('#clockText').textContent = '第 ' + system.now() + ' 分鐘（' + fmtDuration(system.now()) + '）';
    $('#sellerName').textContent = state.user === '' ? '（請先選擇使用者）' : state.user;
}

function renderList() {
    const ids = currentItemIds();
    if (ids.length === 0) {
        $('#itemGrid').innerHTML = '<div class="empty">沒有符合條件的商品</div>';
        return;
    }
    let html = '';
    for (let i = 0; i < ids.length; i++) {
        html += itemCard(system.findItem(ids[i]));
    }
    $('#itemGrid').innerHTML = html;
}

function miniRow(item, whenText) {
    return '<li data-action="open-item" data-id="' + item.itemId + '"><span class="name">#' + item.itemId + ' ' +
        esc(item.title) + '</span><span class="when">' + whenText + '</span></li>';
}

function renderSide() {
    // 即將結標：BST 中序走訪（最近的 8 件）
    const upcoming = system.upcomingItems(8);
    let html = '';
    for (let i = 0; i < upcoming.size(); i++) {
        const item = system.findItem(upcoming.get(i));
        html += miniRow(item, '剩 ' + esc(fmtDuration(item.endTime - system.now())));
    }
    $('#upcomingList').innerHTML = html || '<li class="none">目前沒有進行中的商品</li>';

    // 範圍查詢結果
    if (state.rangeResult === null) {
        $('#rangeResult').innerHTML = '<li class="none">輸入時間範圍後按「查詢」</li>';
    } else {
        const ids = state.rangeResult.ids;
        let rows = '';
        for (let i = 0; i < ids.length; i++) {
            const item = system.findItem(ids[i]);
            rows += miniRow(item, '第 ' + item.endTime + ' 分');
        }
        $('#rangeResult').innerHTML = rows ||
            '<li class="none">第 ' + state.rangeResult.t1 + '～' + state.rangeResult.t2 + ' 分沒有結標的商品</li>';
    }
}

function itemRow(item) {
    const foot = item.status === ITEM_OPEN
        ? '目前 ' + money(AuctionSystem.currentPrice(item)) + ' 元 · ' + remainingText(item)
        : (item.winner === '' ? '流標' : '得標 ' + esc(item.winner) + ' ' + money(item.finalPrice) + ' 元');
    return '<div class="row" data-action="open-item" data-id="' + item.itemId + '">' +
        '<span class="grow">#' + item.itemId + ' ' + esc(item.title) + '</span>' + statusBadge(item) +
        '<span class="note">' + foot + '</span></div>';
}

function mineBlock(title, tag, ids) {
    let rows = '';
    for (let i = 0; i < ids.size(); i++) rows += itemRow(system.findItem(ids.get(i)));
    return '<section class="panel mine-block"><h2>' + title + ' <span class="tag">' + tag + '</span> <span class="muted">（' +
        ids.size() + '）</span></h2><div class="row-list">' + (rows || '<div class="empty">沒有資料</div>') + '</div></section>';
}

function renderMine() {
    const user = system.findUser(state.user);
    if (user === null) {
        $('#mineContent').innerHTML = '<div class="empty">請先在上方選擇（或註冊）使用者，才能看到「我的頁面」。</div>';
        return;
    }
    $('#mineContent').innerHTML =
        '<p class="muted">使用者 <strong>' + esc(user.username) + '</strong> 的頁面</p>' +
        mineBlock('我賣的', 'DynArray', user.sellingItems) +
        mineBlock('我出價過的', 'DynArray', user.biddingItems) +
        mineBlock('我得標的', 'DynArray', user.wonItems);
}

// ---------------------------------------------------------------- 商品詳情

function renderDetail() {
    const overlay = $('#detailOverlay');
    if (state.detailId === null) {
        overlay.hidden = true;
        return;
    }
    const item = system.findItem(state.detailId);
    if (item === null) {
        state.detailId = null;
        overlay.hidden = true;
        return;
    }
    const top = system.highestBid(item.itemId);
    const history = system.bidHistory(item.itemId);
    const isOpen = item.status === ITEM_OPEN;
    const user = system.findUser(state.user);

    let topLine;
    if (top) {
        topLine = money(top.amount) + ' 元 <span class="note">（' + esc(top.bidder) + '，第 ' + top.time + ' 分）</span>';
    } else {
        topLine = '尚無有效出價';
    }

    // 出價區
    let bidArea = '';
    if (isOpen) {
        if (user === null) {
            bidArea = '<div class="bid-form"><span class="note">請先在上方選擇使用者，才能出價。</span></div>';
        } else if (item.seller === user.username) {
            bidArea = '<div class="bid-form"><span class="note">您是這件商品的賣家，不能出價。</span></div>';
        } else {
            const min = AuctionSystem.minNextBid(item);
            let hasActive = false;
            for (let i = 0; i < history.size(); i++) {
                if (history.get(i).active && history.get(i).bidder === user.username) hasActive = true;
            }
            bidArea = '<form class="bid-form" data-form="bid" data-id="' + item.itemId + '">' +
                '<label for="bidAmount">出價金額（元）</label>' +
                '<input id="bidAmount" name="amount" type="number" step="1" value="' + min + '" required>' +
                '<button type="submit" class="btn btn-primary">出價</button>' +
                (hasActive ? '<button type="button" class="btn btn-danger" data-action="cancel-bid" data-id="' + item.itemId + '">撤回我的最新出價</button>' : '') +
                '<span class="note">最低可出價 <strong>' + money(min) + '</strong> 元' +
                (top ? '（最高價 ' + money(top.amount) + ' ＋ 最小加價 ' + money(item.minIncrement) + '）' : '（起標價）') + '</span>' +
                '</form>';
        }
    } else {
        bidArea = '<div class="bid-form"><span class="note">此商品已結標，無法出價。</span></div>';
    }

    let historyRows = '';
    for (let i = 0; i < history.size(); i++) {
        const b = history.get(i);
        const isTop = top !== null && top.bidId === b.bidId;
        historyRows += '<li class="' + (b.active ? '' : 'cancelled') + (isTop ? ' top' : '') + '">' +
            '<span class="amount">' + money(b.amount) + ' 元</span><span>' + esc(b.bidder) + '</span>' +
            (b.active ? '' : '<span class="badge badge-cancel">已撤標</span>') +
            (isTop ? '<span class="badge badge-open">目前最高</span>' : '') +
            '<span class="time">第 ' + b.time + ' 分</span></li>';
    }

    let resultLine = '';
    if (!isOpen) {
        resultLine = item.winner === ''
            ? '<div><div class="k">結標結果</div><div class="v">流標（無人出價）</div></div>'
            : '<div><div class="k">得標者</div><div class="v">' + esc(item.winner) + '</div></div>' +
              '<div><div class="k">得標價</div><div class="v">' + money(item.finalPrice) + ' 元</div></div>';
    }

    $('#detailBody').innerHTML =
        '<div class="detail-head"><h2 id="detailTitle">' + esc(item.title) + '</h2>' + statusBadge(item) +
        '<span class="card-id">#' + item.itemId + '</span></div>' +
        '<p class="muted">賣家 <strong>' + esc(item.seller) + '</strong>　' + esc(item.description) + '</p>' +
        '<div class="kv">' +
        '<div><div class="k">目前最高出價</div><div class="v">' + topLine + '</div></div>' +
        '<div><div class="k">起標價／最小加價</div><div class="v">' + money(item.startPrice) + ' / ' + money(item.minIncrement) + '</div></div>' +
        '<div><div class="k">結標時間</div><div class="v">第 ' + item.endTime + ' 分' +
        (isOpen ? '<div class="note">剩餘 ' + esc(fmtDuration(item.endTime - system.now())) + '</div>' : '') + '</div></div>' +
        resultLine + '</div>' +
        bidArea +
        '<h3>出價歷史 <span class="tag">LinkedList 由新到舊</span> <span class="muted">共 ' + history.size() + ' 筆</span></h3>' +
        (historyRows ? '<ul class="history">' + historyRows + '</ul>' : '<p class="note">還沒有人出價。</p>') +
        '<p style="margin-top:14px"><button type="button" class="btn" data-action="view-heap" data-id="' + item.itemId +
        '">在「資料結構檢視」看這件商品的 Max-Heap</button></p>';
    overlay.hidden = false;
}

// ---------------------------------------------------------------- 資料結構檢視

function heapSvg(item) {
    const heap = item.bidHeap;
    const shown = Math.min(heap.size(), 31);          // 最多畫 5 層
    if (shown === 0) {
        return '<div class="empty">這件商品目前沒有有效出價，Heap 是空的。</div>';
    }
    const width = 640;
    const rowHeight = 66;
    const levels = Math.floor(Math.log2(shown)) + 1;
    const height = levels * rowHeight + 20;
    const xs = [];
    const ys = [];
    for (let i = 0; i < shown; i++) {
        const level = Math.floor(Math.log2(i + 1));
        const indexInLevel = i - (Math.pow(2, level) - 1);
        xs.push(((indexInLevel + 0.5) / Math.pow(2, level)) * width);
        ys.push(level * rowHeight + 32);
    }
    let svg = '<svg class="fluid" viewBox="0 0 ' + width + ' ' + height + '" role="img" aria-label="Max-Heap 樹狀圖">';
    for (let i = 1; i < shown; i++) {
        const p = Math.floor((i - 1) / 2);
        svg += '<line class="edge" x1="' + xs[p] + '" y1="' + ys[p] + '" x2="' + xs[i] + '" y2="' + ys[i] + '"/>';
    }
    for (let i = 0; i < shown; i++) {
        const bid = heap.at(i);
        svg += '<circle class="' + (i === 0 ? 'node-top' : 'node') + '" cx="' + xs[i] + '" cy="' + ys[i] + '" r="24"/>' +
            '<text x="' + xs[i] + '" y="' + (ys[i] + 4) + '" text-anchor="middle" font-size="12" font-weight="700">' + money(bid.amount) + '</text>' +
            '<text class="svg-sub" x="' + xs[i] + '" y="' + (ys[i] + 38) + '" text-anchor="middle">' + esc(bid.bidder) + '</text>';
    }
    svg += '</svg>';
    let arrayLine = '';
    for (let i = 0; i < heap.size(); i++) {
        arrayLine += '[' + i + ']' + heap.at(i).amount + ' ';
    }
    return '<div class="svg-wrap">' + svg + '</div>' +
        '<p class="array-line">底層陣列（層序）：' + esc(arrayLine.trim()) + '</p>' +
        (heap.size() > shown ? '<p class="note">只畫前 ' + shown + ' 個節點（共 ' + heap.size() + ' 筆）。</p>' : '');
}

function hashView() {
    const table = system.itemTable();
    const count = table.bucketCount();
    let cells = '';
    let collisions = 0;
    for (let i = 0; i < count; i++) {
        const length = table.bucketLength(i);
        if (length > 1) collisions += length - 1;
        let chain = '';
        table.forEachInBucket(i, (key) => { chain += '<span class="link">#' + key + '</span>'; });
        cells += '<div class="bucket l' + Math.min(length, 3) + '" title="bucket ' + i + '：鏈長度 ' + length + '">' +
            '<span class="idx">[' + i + ']</span> ' + chain + '</div>';
    }
    return '<div class="stat-row">' +
        '<span>元素數 <b>' + table.size() + '</b></span>' +
        '<span>bucket 數 <b>' + count + '</b></span>' +
        '<span>負載因子 <b>' + table.loadFactor().toFixed(3) + '</b>（超過 0.75 就 rehash，容量翻倍）</span>' +
        '<span>最長鏈 <b>' + table.maxChainLength() + '</b></span>' +
        '<span>發生碰撞的元素 <b>' + collisions + '</b></span></div>' +
        '<div class="bucket-grid">' + cells + '</div>' +
        '<p class="note">每一格是一個 bucket，藍色標籤是掛在這一格鏈上的商品 ID；黃底代表這條鏈有 2 筆以上（發生碰撞）。使用者表（HashTable&lt;string, User&gt;）：' +
        system.userTable().size() + ' 位、' + system.userTable().bucketCount() + ' 個 bucket。</p>';
}

function bstView() {
    const tree = system.endTimeTree();
    if (tree.size() === 0) {
        return '<div class="empty">目前沒有進行中的商品，BST 是空的。</div>';
    }
    const rows = toArray(tree.layout());
    const gap = rows.length > 40 ? 34 : 50;
    const levelHeight = 62;
    let maxDepth = 0;
    for (let i = 0; i < rows.length; i++) if (rows[i].depth > maxDepth) maxDepth = rows[i].depth;
    const width = Math.max(300, rows.length * gap + 30);
    const height = maxDepth * levelHeight + 20;
    const x = (r) => 24 + r.rank * gap;
    const y = (r) => 28 + (r.depth - 1) * levelHeight;
    let svg = '<svg viewBox="0 0 ' + width + ' ' + height + '" width="' + width + '" height="' + height + '" role="img" aria-label="BST 樹狀圖">';
    for (let i = 0; i < rows.length; i++) {
        if (rows[i].parentRank >= 0) {
            const p = rows[rows[i].parentRank];
            svg += '<line class="edge" x1="' + x(p) + '" y1="' + y(p) + '" x2="' + x(rows[i]) + '" y2="' + y(rows[i]) + '"/>';
        }
    }
    for (let i = 0; i < rows.length; i++) {
        const r = rows[i];
        svg += '<circle class="' + (r.depth === 1 ? 'node-top' : 'node') + '" cx="' + x(r) + '" cy="' + y(r) + '" r="19"/>' +
            '<text x="' + x(r) + '" y="' + (y(r) + 4) + '" text-anchor="middle" font-size="11" font-weight="700">#' + r.value + '</text>' +
            '<text class="svg-sub" x="' + x(r) + '" y="' + (y(r) + 33) + '" text-anchor="middle">@' + r.key.endTime + '</text>';
    }
    svg += '</svg>';
    return '<div class="stat-row"><span>節點數（進行中商品）<b>' + tree.size() + '</b></span><span>樹高 <b>' + tree.height() +
        '</b>（理想 ' + Math.ceil(Math.log2(tree.size() + 1)) + '）</span></div>' +
        '<div class="svg-wrap">' + svg + '</div>' +
        '<p class="note">節點內是商品 ID，下方 @ 是結標時間（key = (結標時間, 商品ID)）。由左到右就是中序走訪＝依結標時間排序；根節點以綠色粗框標示。' +
        '依序插入時會退化成一條鏈（樹高 = 節點數），這是未平衡 BST 的最壞情況。</p>';
}

function complexityTable() {
    return '<table class="table"><thead><tr><th>功能</th><th>資料結構與操作</th><th>複雜度</th></tr></thead><tbody>' +
        '<tr><td>依 ID 查商品／使用者</td><td>HashTable.find</td><td>O(1) 平均，最差 O(n)</td></tr>' +
        '<tr><td>出價</td><td>MaxHeap.push ＋ LinkedList.pushFront</td><td>O(log n) ＋ O(1)</td></tr>' +
        '<tr><td>查目前最高價</td><td>MaxHeap.top（堆頂）</td><td>O(1)</td></tr>' +
        '<tr><td>撤回出價</td><td>MaxHeap.removeById（HashTable 記位置）</td><td>O(log n)</td></tr>' +
        '<tr><td>出價歷史</td><td>LinkedList 走訪</td><td>O(m)</td></tr>' +
        '<tr><td>即將結標列表</td><td>BST.inorder</td><td>O(n)</td></tr>' +
        '<tr><td>時段範圍查詢</td><td>BST.rangeQuery</td><td>O(log n + k)</td></tr>' +
        '<tr><td>結標</td><td>BST.popMin ＋ MaxHeap.top</td><td>O(log n) 每件</td></tr>' +
        '<tr><td>商品排序</td><td>mergeSort（穩定）</td><td>O(n log n)</td></tr>' +
        '<tr><td>關鍵字搜尋</td><td>DynArray 線性掃描</td><td>O(n·m)</td></tr>' +
        '</tbody></table>';
}

function renderDS() {
    const ids = system.allItemIds();
    if (state.heapItemId === null || system.findItem(state.heapItemId) === null) {
        // 預設挑出價最多的進行中商品（Demo 比較有看頭）
        let best = null;
        for (let i = 0; i < ids.size(); i++) {
            const item = system.findItem(ids.get(i));
            if (best === null || item.bidHeap.size() > best.bidHeap.size()) best = item;
        }
        state.heapItemId = best === null ? null : best.itemId;
    }
    let options = '';
    for (let i = 0; i < ids.size(); i++) {
        const item = system.findItem(ids.get(i));
        options += '<option value="' + item.itemId + '"' + (item.itemId === state.heapItemId ? ' selected' : '') + '>#' +
            item.itemId + ' ' + esc(item.title) + '（有效出價 ' + item.bidHeap.size() + ' 筆）</option>';
    }
    const heapItem = state.heapItemId === null ? null : system.findItem(state.heapItemId);
    $('#dsContent').innerHTML = '<div class="ds-grid">' +
        '<section class="panel"><h2>Max-Heap：每件商品的有效出價 <span class="tag">堆頂 = 目前最高價</span></h2>' +
        '<p><label class="field-inline" for="heapSelect">選擇商品　</label><select id="heapSelect">' + options + '</select></p>' +
        (heapItem ? heapSvg(heapItem) : '<div class="empty">沒有商品</div>') +
        '<p class="note">金額高者在上；同價時「時間早者優先」。撤標時用 HashTable 記錄的「出價 → 陣列位置」直接找到並刪除。</p></section>' +
        '<section class="panel"><h2>Hash Table：商品 ID → 商品 <span class="tag">separate chaining</span></h2>' + hashView() + '</section>' +
        '<section class="panel"><h2>BST：進行中商品（依結標時間） <span class="tag">未平衡</span></h2>' + bstView() + '</section>' +
        '<section class="panel"><h2>各功能的資料結構與時間複雜度</h2>' + complexityTable() + '</section>' +
        '</div>';
}

// ---------------------------------------------------------------- 總繪製

function renderTabs() {
    const tabs = document.querySelectorAll('.tab');
    for (let i = 0; i < tabs.length; i++) {
        tabs[i].setAttribute('aria-selected', tabs[i].dataset.view === state.view ? 'true' : 'false');
    }
    const views = ['list', 'sell', 'mine', 'ds'];
    for (let i = 0; i < views.length; i++) {
        $('#view-' + views[i]).hidden = (views[i] !== state.view);
    }
}

function renderAll() {
    renderTop();
    renderTabs();
    if (state.view === 'list') renderList();
    if (state.view === 'mine') renderMine();
    if (state.view === 'ds') renderDS();
    renderSide();
    renderDetail();
}

// ---------------------------------------------------------------- 事件

function setView(view) {
    state.view = view;
    if (view === 'mine') {
        const t0 = performance.now();
        renderAll();
        showOp(OPS.mine, performance.now() - t0);
    } else {
        renderAll();
    }
}

function advance(minutes) {
    perform(OPS.advance, () => {
        const result = system.advanceTime(minutes);
        return result.ok ? describeClosed(result) : result;
    }, true);
}

// 把本次結標的商品列在提示訊息裡
function describeClosed(result) {
    const closed = system.lastClosedItems();
    let text = result.message;
    for (let i = 0; i < closed.size(); i++) {
        const item = system.findItem(closed.get(i));
        text += '\n#' + item.itemId + ' ' + item.title + '：' +
            (item.winner === '' ? '流標' : item.winner + ' 以 ' + money(item.finalPrice) + ' 元得標');
    }
    return { ok: true, message: text, id: result.id };
}

document.addEventListener('click', (event) => {
    const target = event.target.closest('[data-action], [data-view]');
    if (target === null) {
        if (event.target.id === 'detailOverlay') closeDetail();
        return;
    }
    if (target.dataset.view) {
        setView(target.dataset.view);
        return;
    }
    const action = target.dataset.action;
    const id = target.dataset.id === undefined ? null : Number(target.dataset.id);
    if (action === 'open-item') {
        const t0 = performance.now();
        state.detailId = id;
        renderAll();
        showOp(OPS.open, performance.now() - t0);
    } else if (action === 'close-detail') {
        closeDetail();
    } else if (action === 'cancel-bid') {
        perform(OPS.cancel, () => system.cancelBid(state.user, id), true);
    } else if (action === 'advance') {
        advance(Number(target.dataset.minutes));
    } else if (action === 'advance-next') {
        perform(OPS.advance, () => {
            const result = system.advanceToNextEnd();
            return result.ok ? describeClosed(result) : result;
        }, true);
    } else if (action === 'advance-custom') {
        const value = Number($('#customMinutes').value);
        advance(value);
        $('#customMinutes').value = '';
    } else if (action === 'reset') {
        if (window.confirm('這會清除目前所有資料並載入範例資料，確定嗎？')) {
            state.user = '';
            state.detailId = null;
            state.heapItemId = null;
            state.rangeResult = null;
            perform(OPS.reset, () => system.loadSampleData(), true);
        }
    } else if (action === 'duration') {
        document.querySelector('#sellForm [name="duration"]').value = target.dataset.minutes;
    } else if (action === 'view-heap') {
        state.heapItemId = id;
        state.detailId = null;
        setView('ds');
    }
});

function closeDetail() {
    state.detailId = null;
    renderDetail();
}

document.addEventListener('keydown', (event) => {
    if (event.key === 'Escape' && state.detailId !== null) closeDetail();
});

document.addEventListener('submit', (event) => {
    const form = event.target;
    event.preventDefault();
    if (form.id === 'sellForm') {
        const data = new FormData(form);
        const result = perform(OPS.list, () => {
            if (state.user === '') return { ok: false, message: '請先在上方選擇（或註冊）使用者，才能刊登商品', id: 0 };
            return system.listItem(state.user, String(data.get('title')).trim(), String(data.get('description')).trim(),
                Number(data.get('startPrice')), Number(data.get('minIncrement')), Number(data.get('duration')));
        }, true);
        if (result.ok) {
            form.reset();
            form.querySelector('[name="startPrice"]').value = 100;
            form.querySelector('[name="minIncrement"]').value = 10;
            form.querySelector('[name="duration"]').value = 60;
            state.view = 'list';
            state.detailId = result.id;
            renderAll();
        }
    } else if (form.dataset.form === 'bid') {
        const id = Number(form.dataset.id);
        const amount = Number(new FormData(form).get('amount'));
        perform(OPS.bid, () => {
            if (state.user === '') return { ok: false, message: '請先選擇使用者', id: 0 };
            return system.placeBid(state.user, id, amount);
        }, true);
    } else if (form.id === 'rangeForm') {
        const data = new FormData(form);
        const t1 = Number(data.get('t1'));
        const t2 = Number(data.get('t2'));
        perform(OPS.range, () => {
            state.rangeResult = { t1: t1, t2: t2, ids: toArray(system.itemsEndingBetween(t1, t2)) };
            return null;
        }, false);
    }
});

document.addEventListener('change', (event) => {
    const el = event.target;
    if (el.id === 'userSelect') {
        perform(OPS.switchUser, () => {
            state.user = el.value;
            if (state.user !== '' && system.findUser(state.user) === null) state.user = '';
            storageSet(USER_KEY, state.user);
            return null;
        }, false);
    } else if (el.id === 'sortSelect') {
        state.sort = el.value;
        perform(OPS.sort, () => null, false);
    } else if (el.id === 'orderSelect') {
        state.order = el.value;
        perform(OPS.sort, () => null, false);
    } else if (el.id === 'openOnlyCheck') {
        state.openOnly = el.checked;
        perform(OPS.search, () => null, false);
    } else if (el.id === 'heapSelect') {
        state.heapItemId = Number(el.value);
        renderAll();
    }
});

document.addEventListener('input', (event) => {
    if (event.target.id === 'searchInput') {
        state.search = event.target.value.trim();
        perform(OPS.search, () => null, false);
    }
});

function registerFromInput() {
    const input = $('#newUserInput');
    const name = input.value.trim();
    const result = perform(OPS.register, () => system.registerUser(name), true);
    if (result.ok) {
        state.user = name;                 // 註冊後直接切換成這位使用者
        input.value = '';
        save();
        renderAll();
    }
}
$('#addUserBtn').addEventListener('click', registerFromInput);
$('#newUserInput').addEventListener('keydown', (event) => {
    if (event.key === 'Enter') registerFromInput();
});

// ---------------------------------------------------------------- 啟動

// 網址 # 後面可以指定起始畫面（方便 Demo 與截圖）：#list #sell #mine #ds，或 #item=6 直接開啟商品詳情
function applyHash() {
    const hash = window.location.hash.replace('#', '');
    if (hash === 'list' || hash === 'sell' || hash === 'mine' || hash === 'ds') {
        state.view = hash;
    } else if (/^item=\d+$/.test(hash) && system.findItem(Number(hash.slice(5))) !== null) {
        state.detailId = Number(hash.slice(5));
    }
}

function boot() {
    const saved = storageGet(STORAGE_KEY);
    let message = null;
    if (saved !== null) {
        const result = system.loadFromString(saved);
        if (!result.ok) {
            message = '瀏覽器裡的存檔無法讀取（' + result.message + '），已重設為範例資料';
            system.loadSampleData();
        }
    } else {
        system.loadSampleData();           // 第一次開啟：載入範例資料
    }
    const savedUser = storageGet(USER_KEY);
    state.user = (savedUser !== null && system.findUser(savedUser) !== null) ? savedUser : '';
    save();
    applyHash();
    renderAll();
    $('#opText').textContent = '尚未操作 — 操作後這裡會顯示用到的資料結構與時間複雜度';
    if (message !== null) toast(message, false);
}

boot();
