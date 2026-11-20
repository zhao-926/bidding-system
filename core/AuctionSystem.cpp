// AuctionSystem.cpp — 競標系統業務邏輯的實作（F1 ~ F16）
#include "AuctionSystem.h"

#include <fstream>
#include <sstream>

// ======================================================================
// 小工具（只在這個檔案內使用）
// ======================================================================

static const int MAX_ITEM_ID = 2147483647;   // 範圍查詢的上界用（int 最大值）

// 英文字母轉小寫（只處理 A-Z，中文等多位元組字元原樣保留）
static std::string toLowerAscii(const std::string& text) {
    std::string result = text;
    for (std::size_t i = 0; i < result.size(); i++) {
        if (result[i] >= 'A' && result[i] <= 'Z') {
            result[i] = static_cast<char>(result[i] - 'A' + 'a');
        }
    }
    return result;
}

// 文字轉整數；格式不合法回傳 false（避免使用會丟例外的 stoll）
static bool parseLongLong(const std::string& text, long long& out) {
    if (text.empty() || text.size() > 18) {
        return false;
    }
    std::size_t start = 0;
    bool negative = false;
    if (text[0] == '-') {
        negative = true;
        start = 1;
        if (text.size() == 1) {
            return false;
        }
    }
    long long value = 0;
    for (std::size_t i = start; i < text.size(); i++) {
        if (text[i] < '0' || text[i] > '9') {
            return false;
        }
        value = value * 10 + (text[i] - '0');
    }
    out = negative ? -value : value;
    return true;
}

static bool parseInt(const std::string& text, int& out) {
    long long value = 0;
    if (!parseLongLong(text, value) || value > 2147483647LL || value < -2147483647LL) {
        return false;
    }
    out = static_cast<int>(value);
    return true;
}

// 存檔用：欄位以 '|' 分隔、一筆資料一行，所以文字內的 % | 換行都要先「跳脫」成 %XX
static std::string escapeField(const std::string& text) {
    std::string result;
    for (std::size_t i = 0; i < text.size(); i++) {
        char c = text[i];
        if (c == '%') {
            result += "%25";
        } else if (c == '|') {
            result += "%7C";
        } else if (c == '\n') {
            result += "%0A";
        } else if (c == '\r') {
            result += "%0D";
        } else {
            result += c;
        }
    }
    return result;
}

static int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static std::string unescapeField(const std::string& text) {
    std::string result;
    for (std::size_t i = 0; i < text.size(); i++) {
        if (text[i] == '%' && i + 2 < text.size() && hexValue(text[i + 1]) >= 0 &&
            hexValue(text[i + 2]) >= 0) {
            result += static_cast<char>(hexValue(text[i + 1]) * 16 + hexValue(text[i + 2]));
            i += 2;
        } else {
            result += text[i];
        }
    }
    return result;
}

// 把一行依 '|' 切成多個欄位
static void splitFields(const std::string& line, DynArray<std::string>& fields) {
    std::string current;
    for (std::size_t i = 0; i < line.size(); i++) {
        if (line[i] == '|') {
            fields.pushBack(current);
            current.clear();
        } else {
            current += line[i];
        }
    }
    fields.pushBack(current);
}

// 排序用：取出商品在指定欄位的數值
static long long sortValue(const Item& item, SortField field) {
    if (field == SORT_BY_PRICE) {
        return AuctionSystem::currentPrice(item);
    }
    if (field == SORT_BY_BID_COUNT) {
        return item.bidHeap.size();
    }
    return item.endTime;
}

// ======================================================================
// 建構、重設
// ======================================================================

AuctionSystem::AuctionSystem() : now_(0), nextItemId_(1), nextBidId_(1) {}

// O(n)
void AuctionSystem::reset() {
    items_.clear();
    users_.clear();
    itemOrder_.clear();
    userNames_.clear();
    endTimeTree_.clear();
    lastClosed_.clear();
    now_ = 0;
    nextItemId_ = 1;
    nextBidId_ = 1;
}

// ======================================================================
// F1 使用者
// ======================================================================

// O(1) average
Result AuctionSystem::registerUser(const std::string& username) {
    if (username.empty()) {
        return Result::fail("使用者名稱不可空白");
    }
    if (username.find('|') != std::string::npos || username.find('\n') != std::string::npos) {
        return Result::fail("使用者名稱不可包含「|」或換行");
    }
    if (users_.contains(username)) {
        return Result::fail("使用者「" + username + "」已經存在");
    }
    User user;
    user.username = username;
    users_.insert(username, user);
    userNames_.pushBack(username);
    return Result::success("已註冊使用者「" + username + "」");
}

// O(1) average
bool AuctionSystem::userExists(const std::string& username) const {
    return users_.contains(username);
}

// O(1) average
const User* AuctionSystem::findUser(const std::string& username) const {
    return users_.find(username);
}

// ======================================================================
// F2、F3、F4 商品
// ======================================================================

// O(log n) average
Result AuctionSystem::listItem(const std::string& seller, const std::string& title,
                               const std::string& description, long long startPrice,
                               long long minIncrement, long long durationMinutes) {
    User* sellerUser = users_.find(seller);
    if (sellerUser == nullptr) {
        return Result::fail("找不到使用者「" + seller + "」，請先註冊");
    }
    if (title.empty()) {
        return Result::fail("商品標題不可空白");
    }
    if (startPrice < 1) {
        return Result::fail("起標價必須大於等於 1 元");
    }
    if (minIncrement < 0) {
        return Result::fail("最小加價幅度不可為負數");
    }
    if (durationMinutes < 1) {
        return Result::fail("拍賣時長必須大於 0 分鐘");
    }

    Item item;
    item.itemId = nextItemId_++;
    item.title = title;
    item.seller = seller;
    item.description = description;
    item.startPrice = startPrice;
    item.minIncrement = minIncrement;
    item.endTime = now_ + durationMinutes;
    item.status = Item::OPEN;

    items_.insert(item.itemId, item);                                 // Hash Table：ID -> 商品
    itemOrder_.pushBack(item.itemId);                                 // 刊登順序
    endTimeTree_.insert(ItemKey(item.endTime, item.itemId), item.itemId);  // BST：依結標時間
    sellerUser->sellingItems.pushBack(item.itemId);
    return Result::success("商品刊登成功，編號 #" + std::to_string(item.itemId), item.itemId);
}

// O(1) average
const Item* AuctionSystem::findItem(int itemId) const {
    return items_.find(itemId);
}

// O(n·m)
DynArray<int> AuctionSystem::searchItems(const std::string& keyword, bool openOnly) const {
    DynArray<int> found;
    std::string lowerKeyword = toLowerAscii(keyword);
    for (int i = 0; i < itemOrder_.size(); i++) {                     // 線性掃描每件商品
        const Item* item = items_.find(itemOrder_.get(i));
        if (openOnly && item->status != Item::OPEN) {
            continue;
        }
        if (lowerKeyword.empty() ||
            toLowerAscii(item->title).find(lowerKeyword) != std::string::npos) {
            found.pushBack(item->itemId);
        }
    }
    return found;
}

// O(1)
long long AuctionSystem::currentPrice(const Item& item) {
    const Bid* top = item.bidHeap.top();
    return (top != nullptr) ? top->amount : item.startPrice;
}

// O(1)
long long AuctionSystem::minNextBid(const Item& item) {
    const Bid* top = item.bidHeap.top();
    return (top != nullptr) ? top->amount + item.minIncrement : item.startPrice;
}

// ======================================================================
// F5、F6、F7、F8 出價
// ======================================================================

// O(log n)
Result AuctionSystem::placeBid(const std::string& bidder, int itemId, long long amount) {
    User* user = users_.find(bidder);
    if (user == nullptr) {
        return Result::fail("找不到使用者「" + bidder + "」");
    }
    Item* item = items_.find(itemId);
    if (item == nullptr) {
        return Result::fail("找不到商品 #" + std::to_string(itemId));
    }
    if (item->status != Item::OPEN) {
        return Result::fail("此商品已結標，無法出價");
    }
    if (now_ >= item->endTime) {
        return Result::fail("已超過結標時間，無法出價");
    }
    if (item->seller == bidder) {
        return Result::fail("賣家不能對自己的商品出價");
    }
    long long minimum = minNextBid(*item);
    if (amount < minimum) {
        if (item->bidHeap.empty()) {
            return Result::fail("出價太低：第一筆出價不得低於起標價 " + std::to_string(minimum) + " 元");
        }
        return Result::fail("出價太低：最低可出價 " + std::to_string(minimum) + " 元（目前最高價 " +
                            std::to_string(currentPrice(*item)) + " 元＋最小加價 " +
                            std::to_string(item->minIncrement) + " 元）");
    }

    Bid bid;
    bid.bidId = nextBidId_++;
    bid.itemId = itemId;
    bid.bidder = bidder;
    bid.amount = amount;
    bid.time = now_;
    bid.active = true;

    item->bidHeap.push(bid);          // Max-Heap：維護最高價
    item->bidHistory.pushFront(bid);  // Linked List：歷史（新的放最前面）
    if (!user->biddingItems.contains(itemId)) {
        user->biddingItems.pushBack(itemId);
    }
    return Result::success("出價成功：" + std::to_string(amount) + " 元", bid.bidId);
}

// O(m + log n)
Result AuctionSystem::cancelBid(const std::string& bidder, int itemId) {
    if (!users_.contains(bidder)) {
        return Result::fail("找不到使用者「" + bidder + "」");
    }
    Item* item = items_.find(itemId);
    if (item == nullptr) {
        return Result::fail("找不到商品 #" + std::to_string(itemId));
    }
    if (item->status != Item::OPEN || now_ >= item->endTime) {
        return Result::fail("此商品已結標，無法撤回出價");
    }
    // 歷史是「由新到舊」，所以第一個符合的就是這位使用者最新的有效出價
    Bid* myLatest = item->bidHistory.find(
        [&bidder](const Bid& b) { return b.active && b.bidder == bidder; });
    if (myLatest == nullptr) {
        return Result::fail("您在此商品上沒有可以撤回的有效出價");
    }
    long long cancelledAmount = myLatest->amount;
    item->bidHeap.removeById(myLatest->bidId);   // Max-Heap：任意刪除
    myLatest->active = false;                    // 歷史保留，只是標記為已撤標

    std::string message = "已撤回出價 " + std::to_string(cancelledAmount) + " 元";
    const Bid* top = item->bidHeap.top();
    if (top != nullptr) {
        message += "；目前最高價為 " + std::to_string(top->amount) + " 元（" + top->bidder + "）";
    } else {
        message += "；目前沒有任何有效出價";
    }
    return Result::success(message);
}

// O(1) average
const Bid* AuctionSystem::highestBid(int itemId) const {
    const Item* item = items_.find(itemId);
    if (item == nullptr) {
        return nullptr;
    }
    return item->bidHeap.top();
}

// O(m)
DynArray<Bid> AuctionSystem::bidHistory(int itemId) const {
    DynArray<Bid> history;
    const Item* item = items_.find(itemId);
    if (item != nullptr) {
        item->bidHistory.forEach([&history](const Bid& bid) { history.pushBack(bid); });
    }
    return history;
}

// ======================================================================
// F9、F10 BST 查詢
// ======================================================================

// O(n)
DynArray<int> AuctionSystem::upcomingItems(int maxCount) const {
    DynArray<int> result;
    endTimeTree_.inorder([&result, maxCount](const ItemKey&, const int& itemId) {
        if (maxCount <= 0 || result.size() < maxCount) {
            result.pushBack(itemId);
        }
    });
    return result;
}

// O(log n + k)
DynArray<int> AuctionSystem::itemsEndingBetween(long long t1, long long t2) const {
    DynArray<int> result;
    endTimeTree_.rangeQuery(ItemKey(t1, 0), ItemKey(t2, MAX_ITEM_ID),
                            [&result](const ItemKey&, const int& itemId) { result.pushBack(itemId); });
    return result;
}

// ======================================================================
// F11、F14 模擬時鐘與結標
// ======================================================================

// 結標單一商品：堆頂為得標者；沒有有效出價則流標（winner 為空）
// O(1)
void AuctionSystem::closeItem(int itemId) {
    Item* item = items_.find(itemId);
    item->status = Item::CLOSED;
    const Bid* top = item->bidHeap.top();
    if (top != nullptr) {
        item->winner = top->bidder;
        item->finalPrice = top->amount;
        User* winnerUser = users_.find(top->bidder);
        if (winnerUser != nullptr) {
            winnerUser->wonItems.pushBack(itemId);
        }
    } else {
        item->winner = "";
        item->finalPrice = 0;
    }
}

// 把 BST 中所有 endTime <= now 的商品依序取出（最早的先）並結標
// O(k log n)
void AuctionSystem::closeExpiredItems() {
    ItemKey key;
    int itemId = 0;
    while (endTimeTree_.peekMin(key, itemId) && key.endTime <= now_) {
        endTimeTree_.popMin(key, itemId);
        closeItem(itemId);
        lastClosed_.pushBack(itemId);
    }
}

// O(k log n)
Result AuctionSystem::advanceTime(long long minutes) {
    if (minutes < 1) {
        return Result::fail("快轉時間必須大於 0 分鐘");
    }
    lastClosed_.clear();
    now_ += minutes;
    closeExpiredItems();
    int closedCount = lastClosed_.size();
    return Result::success("時鐘快轉 " + std::to_string(minutes) + " 分鐘（現在是第 " +
                               std::to_string(now_) + " 分鐘），結標 " + std::to_string(closedCount) +
                               " 件商品",
                           closedCount);
}

// O(k log n)
Result AuctionSystem::advanceToNextEnd() {
    ItemKey key;
    int itemId = 0;
    if (!endTimeTree_.peekMin(key, itemId)) {
        return Result::fail("目前沒有進行中的商品");
    }
    long long minutes = key.endTime - now_;
    if (minutes < 1) {
        minutes = 1;
    }
    return advanceTime(minutes);
}

// ======================================================================
// F12 排序
// ======================================================================

// O(n log n)
DynArray<int> AuctionSystem::sortedItems(SortField field, bool ascending, bool openOnly) const {
    DynArray<int> ids;
    for (int i = 0; i < itemOrder_.size(); i++) {
        const Item* item = items_.find(itemOrder_.get(i));
        if (!openOnly || item->status == Item::OPEN) {
            ids.pushBack(item->itemId);
        }
    }
    // 比較函式：a 應排在 b 前面嗎？（相等時回傳 false，穩定排序會保持原本順序）
    mergeSort(ids, [this, field, ascending](const int& a, const int& b) {
        long long valueA = sortValue(*items_.find(a), field);
        long long valueB = sortValue(*items_.find(b), field);
        return ascending ? (valueA < valueB) : (valueA > valueB);
    });
    return ids;
}

// ======================================================================
// F15 存檔／讀檔
// ======================================================================
//
// 檔案格式（文字、一行一筆，欄位以 | 分隔）：
//   AUCTION_V1
//   now|<分鐘>
//   nextItemId|<n>
//   nextBidId|<n>
//   user|<名稱>
//   item|<id>|<賣家>|<標題>|<描述>|<起標價>|<最小加價>|<結標時間>|<狀態 0=進行中 1=已結標>|<得標者>|<得標價>
//   bid|<bidId>|<itemId>|<出價者>|<金額>|<時間>|<有效 1/0>
// 出價依 bidId 由小到大（時間順序）存放，讀檔時依序重播就能還原 Heap 與歷史。

// O(n log n)（出價依 bidId 做一次合併排序）
std::string AuctionSystem::saveToString() const {
    std::string out;
    out += "AUCTION_V1\n";
    out += "now|" + std::to_string(now_) + "\n";
    out += "nextItemId|" + std::to_string(nextItemId_) + "\n";
    out += "nextBidId|" + std::to_string(nextBidId_) + "\n";
    for (int i = 0; i < userNames_.size(); i++) {
        out += "user|" + escapeField(userNames_.get(i)) + "\n";
    }

    DynArray<Bid> allBids;
    for (int i = 0; i < itemOrder_.size(); i++) {
        const Item* item = items_.find(itemOrder_.get(i));
        out += "item|" + std::to_string(item->itemId) + "|" + escapeField(item->seller) + "|" +
               escapeField(item->title) + "|" + escapeField(item->description) + "|" +
               std::to_string(item->startPrice) + "|" + std::to_string(item->minIncrement) + "|" +
               std::to_string(item->endTime) + "|" + (item->status == Item::OPEN ? "0" : "1") + "|" +
               escapeField(item->winner) + "|" + std::to_string(item->finalPrice) + "\n";
        item->bidHistory.forEach([&allBids](const Bid& bid) { allBids.pushBack(bid); });
    }
    mergeSort(allBids, [](const Bid& a, const Bid& b) { return a.bidId < b.bidId; });
    for (int i = 0; i < allBids.size(); i++) {
        const Bid& bid = allBids.get(i);
        out += "bid|" + std::to_string(bid.bidId) + "|" + std::to_string(bid.itemId) + "|" +
               escapeField(bid.bidder) + "|" + std::to_string(bid.amount) + "|" +
               std::to_string(bid.time) + "|" + (bid.active ? "1" : "0") + "\n";
    }
    return out;
}

// 解析文字並建立資料（呼叫前自己必須是空的）。任何格式錯誤都回傳帶行號的錯誤訊息
// O(n log n)
Result AuctionSystem::loadInto(const std::string& text) {
    // 先把文字切成一行一行
    DynArray<std::string> lines;
    std::string current;
    for (std::size_t i = 0; i < text.size(); i++) {
        if (text[i] == '\n') {
            lines.pushBack(current);
            current.clear();
        } else if (text[i] != '\r') {
            current += text[i];
        }
    }
    if (!current.empty()) {
        lines.pushBack(current);
    }
    if (lines.empty() || lines.get(0) != "AUCTION_V1") {
        return Result::fail("讀檔失敗：不是有效的存檔格式（缺少 AUCTION_V1 標頭）");
    }

    for (int lineNo = 1; lineNo < lines.size(); lineNo++) {
        const std::string& line = lines.get(lineNo);
        if (line.empty()) {
            continue;
        }
        std::string where = "讀檔失敗：第 " + std::to_string(lineNo + 1) + " 行";
        DynArray<std::string> f;
        splitFields(line, f);
        const std::string& kind = f.get(0);

        if (kind == "now" && f.size() == 2) {
            if (!parseLongLong(f.get(1), now_)) return Result::fail(where + " now 格式錯誤");
        } else if (kind == "nextItemId" && f.size() == 2) {
            if (!parseInt(f.get(1), nextItemId_)) return Result::fail(where + " nextItemId 格式錯誤");
        } else if (kind == "nextBidId" && f.size() == 2) {
            if (!parseInt(f.get(1), nextBidId_)) return Result::fail(where + " nextBidId 格式錯誤");
        } else if (kind == "user" && f.size() == 2) {
            Result r = registerUser(unescapeField(f.get(1)));
            if (!r.ok) return Result::fail(where + " " + r.message);
        } else if (kind == "item" && f.size() == 11) {
            Item item;
            long long status = 0;
            if (!parseInt(f.get(1), item.itemId) || !parseLongLong(f.get(5), item.startPrice) ||
                !parseLongLong(f.get(6), item.minIncrement) || !parseLongLong(f.get(7), item.endTime) ||
                !parseLongLong(f.get(8), status) || !parseLongLong(f.get(10), item.finalPrice)) {
                return Result::fail(where + " 商品欄位格式錯誤");
            }
            item.seller = unescapeField(f.get(2));
            item.title = unescapeField(f.get(3));
            item.description = unescapeField(f.get(4));
            item.status = (status == 0) ? Item::OPEN : Item::CLOSED;
            item.winner = unescapeField(f.get(9));
            User* seller = users_.find(item.seller);
            if (seller == nullptr) return Result::fail(where + " 賣家「" + item.seller + "」不存在");
            if (items_.contains(item.itemId)) return Result::fail(where + " 商品編號重複");
            items_.insert(item.itemId, item);
            itemOrder_.pushBack(item.itemId);
            seller->sellingItems.pushBack(item.itemId);
            if (item.status == Item::OPEN) {
                endTimeTree_.insert(ItemKey(item.endTime, item.itemId), item.itemId);
            } else if (!item.winner.empty()) {
                User* winner = users_.find(item.winner);
                if (winner == nullptr) return Result::fail(where + " 得標者不存在");
                winner->wonItems.pushBack(item.itemId);
            }
        } else if (kind == "bid" && f.size() == 7) {
            Bid bid;
            long long active = 0;
            if (!parseInt(f.get(1), bid.bidId) || !parseInt(f.get(2), bid.itemId) ||
                !parseLongLong(f.get(4), bid.amount) || !parseLongLong(f.get(5), bid.time) ||
                !parseLongLong(f.get(6), active)) {
                return Result::fail(where + " 出價欄位格式錯誤");
            }
            bid.bidder = unescapeField(f.get(3));
            bid.active = (active != 0);
            Item* item = items_.find(bid.itemId);
            User* user = users_.find(bid.bidder);
            if (item == nullptr) return Result::fail(where + " 出價的商品不存在");
            if (user == nullptr) return Result::fail(where + " 出價者不存在");
            item->bidHistory.pushFront(bid);
            if (bid.active && !item->bidHeap.push(bid)) {
                return Result::fail(where + " 出價編號重複");
            }
            if (!user->biddingItems.contains(bid.itemId)) {
                user->biddingItems.pushBack(bid.itemId);
            }
        } else {
            return Result::fail(where + " 無法辨識：" + line);
        }
    }
    return Result::success("讀檔成功");
}

// O(n log n)
Result AuctionSystem::loadFromString(const std::string& text) {
    // 先在一個暫時的系統裡試著解析：失敗就直接回報，目前的資料完全不動
    {
        AuctionSystem staging;
        Result check = staging.loadInto(text);
        if (!check.ok) {
            return check;
        }
    }
    reset();
    return loadInto(text);
}

// O(n log n)
Result AuctionSystem::saveToFile(const std::string& path) const {
    std::ofstream file(path.c_str(), std::ios::binary);
    if (!file) {
        return Result::fail("無法寫入檔案：" + path);
    }
    file << saveToString();
    file.close();
    if (!file) {
        return Result::fail("寫入檔案時發生錯誤：" + path);
    }
    return Result::success("已存檔到 " + path);
}

// O(n log n)
Result AuctionSystem::loadFromFile(const std::string& path) {
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file) {
        return Result::fail("找不到檔案或無法讀取：" + path);
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    Result r = loadFromString(buffer.str());
    if (r.ok) {
        return Result::success("已從 " + path + " 讀檔");
    }
    return r;
}

// ======================================================================
// F16 範例資料
// ======================================================================

namespace {

struct SampleItem {
    const char* seller;
    const char* title;
    const char* description;
    long long startPrice;
    long long minIncrement;
    long long durationMinutes;
};

// 一個「事件」：在第 time 分鐘，某人對某商品出價（或撤回出價）
struct SampleStep {
    long long time;
    int itemNumber;        // 商品編號（1~20，依刊登順序）
    const char* user;
    long long amount;      // 出價金額；撤回出價時不使用
    bool isCancel;
};

const SampleItem SAMPLE_ITEMS[] = {
    {"alice", "資料結構與演算法 教科書（第四版）", "九成新，內有少量螢光筆劃記。", 300, 20, 45},
    {"eve", "折疊雨傘（黑色）", "使用約半年，傘骨完好。", 50, 10, 50},
    {"bob", "藍牙降噪耳機", "原價 3,500 元，附原廠收納盒，功能正常。", 800, 50, 180},
    {"carol", "機械鍵盤（青軸）", "87 鍵，附鍵帽拔取器，無按鍵失靈。", 1500, 100, 240},
    {"dave", "二手公路腳踏車", "鋁合金車架，騎乘約一年，附車鎖。", 3000, 200, 1440},
    {"eve", "27 吋顯示器", "2K 解析度，無亮點，附電源線與 HDMI 線。", 2500, 100, 360},
    {"alice", "電路學課本", "附歷屆考古題影本。", 200, 10, 600},
    {"bob", "無線滑鼠", "電池式，附接收器。", 150, 10, 120},
    {"carol", "桌上型電風扇", "三段風速，運轉安靜。", 300, 20, 720},
    {"dave", "LED 檯燈", "可調亮度，無閃爍。", 120, 10, 300},
    {"eve", "二手筆電（i5／16GB）", "使用兩年，電池健康度約 80%，附充電器。", 5000, 300, 2880},
    {"alice", "工程計算機", "可程式型，附保護套。", 250, 10, 90},
    {"bob", "螢幕掛燈", "護眼無頻閃，夾式安裝。", 400, 20, 480},
    {"carol", "不鏽鋼保溫瓶", "500ml，保溫效果良好。", 100, 10, 1000},
    {"dave", "民謠吉他", "附琴袋與備用琴弦，琴頸筆直。", 2000, 100, 4320},
    {"eve", "慢跑鞋（US 9）", "穿過約十次，鞋底磨損很少。", 600, 50, 150},
    {"alice", "小型冰箱", "90 公升，宿舍適用，冷藏正常。", 1200, 100, 960},
    {"bob", "行動電源", "20000mAh，支援快充。", 350, 25, 200},
    {"carol", "微波爐", "700W，附玻璃轉盤。", 900, 50, 540},
    {"dave", "全新筆記本一批", "A5 方格，共 10 本。", 50, 5, 100},
};
const int SAMPLE_ITEM_COUNT = sizeof(SAMPLE_ITEMS) / sizeof(SAMPLE_ITEMS[0]);

// 依時間順序排列（時間相同者依列出順序）。時間 45 的撤標：eve 撤回她在藍牙耳機上的 1000 元出價
const SampleStep SAMPLE_STEPS[] = {
    {5, 1, "bob", 300, false},    {6, 4, "dave", 1500, false},  {7, 6, "alice", 2500, false},
    {8, 3, "carol", 800, false},  {9, 8, "carol", 150, false},  {10, 1, "carol", 340, false},
    {10, 5, "eve", 3000, false},  {11, 9, "dave", 300, false},  {12, 3, "dave", 850, false},
    {13, 12, "bob", 250, false},  {14, 11, "alice", 5000, false}, {15, 4, "alice", 1600, false},
    {16, 13, "eve", 400, false},  {17, 16, "bob", 600, false},  {18, 6, "bob", 2600, false},
    {19, 8, "eve", 160, false},   {20, 1, "bob", 360, false},   {20, 5, "bob", 3200, false},
    {21, 15, "alice", 2000, false}, {22, 17, "carol", 1200, false}, {23, 12, "dave", 260, false},
    {24, 11, "bob", 5300, false}, {25, 4, "dave", 1700, false}, {26, 18, "eve", 350, false},
    {27, 16, "dave", 650, false}, {29, 8, "carol", 170, false}, {30, 3, "carol", 900, false},
    {31, 15, "eve", 2100, false}, {33, 6, "alice", 2700, false}, {34, 11, "alice", 5600, false},
    {35, 4, "alice", 1800, false}, {36, 19, "dave", 900, false}, {37, 17, "dave", 1300, false},
    {39, 8, "eve", 180, false},   {40, 3, "eve", 1000, false},  {42, 6, "carol", 2800, false},
    {43, 12, "bob", 270, false},  {44, 11, "bob", 5900, false}, {45, 3, "eve", 0, true},
    {46, 19, "bob", 950, false},  {50, 4, "dave", 1900, false}, {51, 15, "alice", 2200, false},
    {52, 6, "bob", 2900, false},  {54, 11, "carol", 6200, false}, {55, 6, "alice", 3000, false},
    {56, 19, "dave", 1000, false},
};
const int SAMPLE_STEP_COUNT = sizeof(SAMPLE_STEPS) / sizeof(SAMPLE_STEPS[0]);

const long long SAMPLE_FINAL_TIME = 60;   // 範例資料建立完成時的模擬時間

}  // namespace

// O(n log n)
Result AuctionSystem::loadSampleData() {
    reset();
    const char* names[] = {"alice", "bob", "carol", "dave", "eve"};
    for (int i = 0; i < 5; i++) {
        registerUser(names[i]);
    }
    // 第 0 分鐘：5 位使用者刊登全部 20 件商品
    for (int i = 0; i < SAMPLE_ITEM_COUNT; i++) {
        const SampleItem& s = SAMPLE_ITEMS[i];
        Result r = listItem(s.seller, s.title, s.description, s.startPrice, s.minIncrement,
                            s.durationMinutes);
        if (!r.ok) {
            return Result::fail("範例資料建立失敗（刊登）：" + r.message);
        }
    }
    // 依時間順序重播出價與撤標事件；需要時先把時鐘往前撥
    for (int i = 0; i < SAMPLE_STEP_COUNT; i++) {
        const SampleStep& step = SAMPLE_STEPS[i];
        if (step.time > now_) {
            advanceTime(step.time - now_);
        }
        Result r = step.isCancel ? cancelBid(step.user, step.itemNumber)
                                 : placeBid(step.user, step.itemNumber, step.amount);
        if (!r.ok) {
            return Result::fail("範例資料建立失敗（第 " + std::to_string(i + 1) + " 個事件）：" + r.message);
        }
    }
    if (SAMPLE_FINAL_TIME > now_) {
        advanceTime(SAMPLE_FINAL_TIME - now_);
    }
    lastClosed_.clear();
    return Result::success("已載入範例資料（5 位使用者、" + std::to_string(itemOrder_.size()) + " 件商品）");
}
