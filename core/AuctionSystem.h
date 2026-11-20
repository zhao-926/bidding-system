// AuctionSystem.h — 競標系統的業務邏輯（整合所有自刻資料結構，不含任何 UI）
//
// 使用到的資料結構一覽：
//   HashTable<int, Item>          商品 ID -> 商品              （F2、F3、F5…）
//   HashTable<string, User>       使用者名稱 -> 使用者          （F1、F13）
//   Item::bidHeap  (MaxHeap)      每件商品的有效出價，堆頂=最高價 （F5、F6、F8、F11）
//   Item::bidHistory (LinkedList) 每件商品的出價歷史             （F7）
//   BST<ItemKey, int>             進行中商品，依 (結標時間, ID) 排序（F9、F10、F11）
//   DynArray                      商品清單、使用者清單、我的頁面   （F4、F13）
//   mergeSort                     商品列表排序                   （F12）
//
// 所有可能失敗的操作都回傳 Result，錯誤訊息為中文，呼叫端（CLI / 網頁）直接顯示即可，不會 crash。

#ifndef AUCTIONSYSTEM_H
#define AUCTIONSYSTEM_H

#include <string>

#include "BST.h"
#include "DynArray.h"
#include "HashTable.h"
#include "MergeSort.h"
#include "Models.h"

// 操作結果：ok 表示成功；message 是給使用者看的中文訊息；id 是新建立物件的編號（沒有則為 0）
struct Result {
    bool ok;
    std::string message;
    int id;

    static Result success(const std::string& message, int id = 0) {
        Result r;
        r.ok = true;
        r.message = message;
        r.id = id;
        return r;
    }
    static Result fail(const std::string& message) {
        Result r;
        r.ok = false;
        r.message = message;
        r.id = 0;
        return r;
    }

private:
    Result() : ok(false), id(0) {}
};

// 商品列表可依哪個欄位排序（F12）
enum SortField {
    SORT_BY_PRICE,       // 目前價格（無人出價則為起標價）
    SORT_BY_BID_COUNT,   // 有效出價數
    SORT_BY_END_TIME     // 結標時間
};

class AuctionSystem {
public:
    AuctionSystem();

    // 不允許複製（內含 BST）
    AuctionSystem(const AuctionSystem&) = delete;
    AuctionSystem& operator=(const AuctionSystem&) = delete;

    // ---------------- F1 使用者 ----------------

    // 註冊新使用者；名稱不可空白、不可重複
    // O(1) average（HashTable 插入）
    Result registerUser(const std::string& username);

    // O(1) average
    bool userExists(const std::string& username) const;

    // 查使用者（含我賣的／我出價的／我得標的清單，即 F13「我的頁面」）；找不到回傳 nullptr
    // O(1) average
    const User* findUser(const std::string& username) const;

    // 依註冊順序的使用者名稱
    // O(1)
    const DynArray<std::string>& userNames() const { return userNames_; }

    // ---------------- F2、F3、F4 商品 ----------------

    // 刊登商品；durationMinutes 為從「現在」起算的拍賣時長（分鐘）。成功時 Result.id = 新商品 ID
    // O(log n) average（HashTable 插入 O(1) + BST 插入 O(log n)）
    Result listItem(const std::string& seller, const std::string& title,
                    const std::string& description, long long startPrice,
                    long long minIncrement, long long durationMinutes);

    // F3：依 ID 查商品；找不到回傳 nullptr
    // O(1) average
    const Item* findItem(int itemId) const;

    // F4：關鍵字搜尋（標題包含關鍵字，英文不分大小寫；關鍵字空白則全部列出）
    // 回傳符合的商品 ID（依刊登順序）。這是線性掃描，O(n·m)，報告中作為「取捨」案例討論
    DynArray<int> searchItems(const std::string& keyword, bool openOnly) const;

    // 所有商品 ID（依刊登順序）
    // O(1)
    const DynArray<int>& allItemIds() const { return itemOrder_; }

    // 商品目前價格：有人出價 = 最高出價，否則 = 起標價
    // O(1)
    static long long currentPrice(const Item& item);

    // 下一筆出價的最低金額：有人出價 = 最高價 + 最小加價，否則 = 起標價
    // O(1)
    static long long minNextBid(const Item& item);

    // ---------------- F5、F6、F7、F8 出價 ----------------

    // F5：出價。規則：商品須為 OPEN 且未過結標時間；賣家不能出價；
    //     第一筆 >= 起標價；之後 >= 目前最高價 + 最小加價。成功時 Result.id = bidId
    // O(log n)（n = 該商品有效出價數；Heap 插入 O(log n)、歷史 pushFront O(1)）
    Result placeBid(const std::string& bidder, int itemId, long long amount);

    // F8：撤回自己在該商品「最新一筆有效出價」；商品須仍為 OPEN
    // O(m + log n)（m = 歷史筆數，需由新到舊找到該使用者最新有效出價；Heap 任意刪除 O(log n)）
    Result cancelBid(const std::string& bidder, int itemId);

    // F6：目前最高出價（看堆頂）；沒有出價或找不到商品回傳 nullptr
    // O(1) average
    const Bid* highestBid(int itemId) const;

    // F7：出價歷史（由新到舊，含已撤標）；找不到商品回傳空陣列
    // O(m)
    DynArray<Bid> bidHistory(int itemId) const;

    // ---------------- F9、F10 BST 查詢 ----------------

    // F9：即將結標列表（進行中商品，依結標時間由近到遠），最多回傳 maxCount 件（<=0 代表全部）
    // O(n)（BST 中序走訪）
    DynArray<int> upcomingItems(int maxCount) const;

    // F10：結標時間落在 [t1, t2] 的進行中商品（依結標時間由近到遠）
    // O(log n + k)
    DynArray<int> itemsEndingBetween(long long t1, long long t2) const;

    // ---------------- F11、F14 模擬時鐘與結標 ----------------

    // 目前的模擬時間（分鐘）
    // O(1)
    long long now() const { return now_; }

    // F14：快轉 minutes 分鐘（必須 > 0），並自動結標所有 endTime <= now 的商品（F11）
    // O(k log n)（k = 本次結標的商品數；每件 popMin O(log n)）
    Result advanceTime(long long minutes);

    // F14：直接跳到「下一個結標時間」並結標；沒有進行中商品則回傳錯誤
    // O(k log n)
    Result advanceToNextEnd();

    // 最近一次 advanceTime / advanceToNextEnd 結標的商品 ID
    // O(1)
    const DynArray<int>& lastClosedItems() const { return lastClosed_; }

    // ---------------- F12 排序 ----------------

    // 商品列表排序（合併排序，穩定：欄位值相同者維持刊登順序）
    // O(n log n)
    DynArray<int> sortedItems(SortField field, bool ascending, bool openOnly) const;

    // ---------------- F15、F16 存檔、讀檔、範例資料 ----------------

    // 清空所有資料，回到初始狀態（時間 0、沒有使用者與商品）
    // O(n)
    void reset();

    // F16：重設並載入範例資料（5 位使用者、20 件商品、數十筆出價、1 筆撤標）
    // O(n log n)
    Result loadSampleData();

    // F15：序列化成文字（網頁存進 localStorage、CLI 存成檔案都用這個格式）
    // O(n log n)
    std::string saveToString() const;

    // F15：從文字還原；格式錯誤時回傳錯誤，且「不會」破壞目前的資料
    // O(n log n)
    Result loadFromString(const std::string& text);

    // O(n log n)
    Result saveToFile(const std::string& path) const;

    // O(n log n)
    Result loadFromFile(const std::string& path);

    // ---------------- 資料結構檢視（給 CLI / 網頁 Demo 面板用）----------------

    // O(1)
    int itemTableBucketCount() const { return items_.bucketCount(); }
    // O(1)
    double itemTableLoadFactor() const { return items_.loadFactor(); }
    // O(n + bucket 數)
    int itemTableMaxChain() const { return items_.maxChainLength(); }
    // O(n)
    int endTimeTreeHeight() const { return endTimeTree_.height(); }
    // O(1)
    int endTimeTreeSize() const { return endTimeTree_.size(); }

private:
    long long now_;
    int nextItemId_;
    int nextBidId_;
    HashTable<int, Item> items_;                // 商品 ID -> 商品
    HashTable<std::string, User> users_;        // 使用者名稱 -> 使用者
    DynArray<int> itemOrder_;                   // 依刊登順序的商品 ID
    DynArray<std::string> userNames_;           // 依註冊順序的使用者名稱
    BST<ItemKey, int> endTimeTree_;             // 進行中商品：(結標時間, ID) -> 商品 ID
    DynArray<int> lastClosed_;                  // 最近一次結標的商品 ID

    void closeItem(int itemId);
    void closeExpiredItems();
    Result loadInto(const std::string& text);   // 實際解析；呼叫前自己必須是空的
};

#endif  // AUCTIONSYSTEM_H
