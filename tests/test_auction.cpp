// test_auction.cpp — AuctionSystem 業務邏輯測試（F1 ~ F16）
#include <cstdio>
#include <string>

#include "test_framework.h"
#include "test_random.h"
#include "../core/AuctionSystem.h"

// ---- 測試輔助：建立一個有 alice/bob/carol/dave 四位使用者的系統 ----
static void addUsers(AuctionSystem& sys) {
    sys.registerUser("alice");
    sys.registerUser("bob");
    sys.registerUser("carol");
    sys.registerUser("dave");
}

// 刊登一件商品並回傳 ID（失敗回傳 -1）
static int listSimple(AuctionSystem& sys, const std::string& seller, const std::string& title,
                      long long start, long long increment, long long duration) {
    Result r = sys.listItem(seller, title, "desc", start, increment, duration);
    return r.ok ? r.id : -1;
}

static bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

// ===================== F1 使用者 =====================

static void test_register_user() {
    AuctionSystem sys;
    ASSERT_TRUE(sys.registerUser("alice").ok);
    ASSERT_TRUE(sys.userExists("alice"));
    ASSERT_FALSE(sys.userExists("bob"));
    ASSERT_TRUE(sys.registerUser("bob").ok);
    ASSERT_EQ(2, sys.userNames().size());
    ASSERT_EQ(std::string("alice"), sys.userNames().get(0));
    ASSERT_TRUE(sys.findUser("bob") != nullptr);
    ASSERT_TRUE(sys.findUser("nobody") == nullptr);
}

static void test_register_user_errors() {
    AuctionSystem sys;
    sys.registerUser("alice");
    Result duplicate = sys.registerUser("alice");
    ASSERT_FALSE(duplicate.ok);
    ASSERT_TRUE(contains(duplicate.message, "已經存在"));
    ASSERT_FALSE(sys.registerUser("").ok);
    ASSERT_FALSE(sys.registerUser("a|b").ok);
    ASSERT_EQ(1, sys.userNames().size());
}

// ===================== F2、F3 刊登與查詢 =====================

static void test_list_item_success() {
    AuctionSystem sys;
    addUsers(sys);
    sys.advanceTime(100);
    Result r = sys.listItem("alice", "自行車", "九成新", 1000, 50, 60);
    ASSERT_TRUE(r.ok);
    ASSERT_EQ(1, r.id);
    const Item* item = sys.findItem(1);
    ASSERT_TRUE(item != nullptr);
    ASSERT_EQ(std::string("自行車"), item->title);
    ASSERT_EQ(160, item->endTime);                 // 現在 100 + 時長 60
    ASSERT_TRUE(item->status == Item::OPEN);
    int second = listSimple(sys, "bob", "桌子", 100, 10, 30);
    ASSERT_EQ(2, second);                          // ID 自動遞增
    ASSERT_EQ(2, sys.endTimeTreeSize());
    ASSERT_EQ(1, sys.findUser("alice")->sellingItems.size());
}

static void test_list_item_validation_errors() {
    AuctionSystem sys;
    addUsers(sys);
    ASSERT_FALSE(sys.listItem("ghost", "x", "", 100, 10, 60).ok);     // 賣家不存在
    ASSERT_FALSE(sys.listItem("alice", "", "", 100, 10, 60).ok);      // 空標題
    ASSERT_FALSE(sys.listItem("alice", "x", "", 0, 10, 60).ok);       // 起標價 0
    ASSERT_FALSE(sys.listItem("alice", "x", "", 100, -1, 60).ok);     // 負的加價
    ASSERT_FALSE(sys.listItem("alice", "x", "", 100, 10, 0).ok);      // 時長 0
    ASSERT_EQ(0, sys.allItemIds().size());
    ASSERT_TRUE(sys.listItem("alice", "x", "", 100, 0, 1).ok);        // 加價 0、時長 1 是合法的
}

static void test_find_item_not_found() {
    AuctionSystem sys;
    ASSERT_TRUE(sys.findItem(1) == nullptr);
    ASSERT_TRUE(sys.findItem(-5) == nullptr);
    ASSERT_TRUE(sys.highestBid(1) == nullptr);
    ASSERT_EQ(0, sys.bidHistory(1).size());
}

// ===================== F4 關鍵字搜尋 =====================

static void test_search_items() {
    AuctionSystem sys;
    addUsers(sys);
    listSimple(sys, "alice", "Mechanical Keyboard", 100, 10, 60);   // 1
    listSimple(sys, "bob", "藍牙耳機", 100, 10, 60);                // 2
    listSimple(sys, "carol", "keyboard cover", 100, 10, 5);         // 3（很快結標）
    listSimple(sys, "dave", "機械鍵盤", 100, 10, 60);               // 4

    DynArray<int> r1 = sys.searchItems("KEYBOARD", false);          // 不分大小寫
    ASSERT_EQ(2, r1.size());
    ASSERT_EQ(1, r1.get(0));
    ASSERT_EQ(3, r1.get(1));
    DynArray<int> r2 = sys.searchItems("耳機", false);              // 中文
    ASSERT_EQ(1, r2.size());
    ASSERT_EQ(2, r2.get(0));
    ASSERT_EQ(4, sys.searchItems("", false).size());                // 空關鍵字 = 全部
    ASSERT_EQ(0, sys.searchItems("不存在", false).size());
    sys.advanceTime(10);                                            // 3 號結標
    ASSERT_EQ(1, sys.searchItems("keyboard", true).size());         // 只找進行中
    ASSERT_EQ(2, sys.searchItems("keyboard", false).size());
}

// ===================== F5 出價 =====================

static void test_place_bid_normal() {
    AuctionSystem sys;
    addUsers(sys);
    int id = listSimple(sys, "alice", "item", 100, 10, 60);
    Result first = sys.placeBid("bob", id, 100);                    // 第一筆 = 起標價
    ASSERT_TRUE(first.ok);
    ASSERT_EQ(1, first.id);
    ASSERT_EQ(100, sys.highestBid(id)->amount);
    ASSERT_TRUE(sys.placeBid("carol", id, 110).ok);                 // 剛好 = 最高價 + 加價
    ASSERT_EQ(std::string("carol"), sys.highestBid(id)->bidder);
    ASSERT_TRUE(sys.placeBid("bob", id, 500).ok);
    ASSERT_EQ(500, sys.highestBid(id)->amount);
    ASSERT_EQ(500, AuctionSystem::currentPrice(*sys.findItem(id)));
    ASSERT_EQ(510, AuctionSystem::minNextBid(*sys.findItem(id)));
}

static void test_place_bid_too_low() {
    AuctionSystem sys;
    addUsers(sys);
    int id = listSimple(sys, "alice", "item", 100, 10, 60);
    Result low = sys.placeBid("bob", id, 99);                       // 低於起標價
    ASSERT_FALSE(low.ok);
    ASSERT_TRUE(contains(low.message, "出價太低"));
    ASSERT_TRUE(contains(low.message, "100"));
    ASSERT_TRUE(sys.highestBid(id) == nullptr);                     // 失敗不會留下紀錄
    sys.placeBid("bob", id, 200);
    ASSERT_FALSE(sys.placeBid("carol", id, 200).ok);                // 等於最高價但沒加價
    ASSERT_FALSE(sys.placeBid("carol", id, 209).ok);                // 差 1 元
    Result msg = sys.placeBid("carol", id, 205);
    ASSERT_TRUE(contains(msg.message, "210"));                      // 訊息告知最低可出價
    ASSERT_EQ(1, sys.bidHistory(id).size());                        // 只有成功那一筆
}

static void test_place_bid_seller_and_invalid() {
    AuctionSystem sys;
    addUsers(sys);
    int id = listSimple(sys, "alice", "item", 100, 10, 60);
    Result self = sys.placeBid("alice", id, 500);                   // 賣家自己出價
    ASSERT_FALSE(self.ok);
    ASSERT_TRUE(contains(self.message, "賣家"));
    ASSERT_FALSE(sys.placeBid("ghost", id, 500).ok);                // 使用者不存在
    ASSERT_FALSE(sys.placeBid("bob", 999, 500).ok);                 // 商品不存在
    ASSERT_FALSE(sys.placeBid("bob", id, -50).ok);                  // 負數
}

static void test_place_bid_after_close() {
    AuctionSystem sys;
    addUsers(sys);
    int id = listSimple(sys, "alice", "item", 100, 10, 60);
    sys.placeBid("bob", id, 100);
    sys.advanceTime(60);                                            // 剛好到結標時間
    Result late = sys.placeBid("carol", id, 500);
    ASSERT_FALSE(late.ok);
    ASSERT_TRUE(contains(late.message, "已結標"));
    ASSERT_EQ(100, sys.highestBid(id)->amount);                     // 最高價不變
}

// 同價時，先出價者勝（minIncrement = 0 才會出現同價）
static void test_tie_first_bidder_wins() {
    AuctionSystem sys;
    addUsers(sys);
    int id = listSimple(sys, "alice", "item", 100, 0, 60);
    ASSERT_TRUE(sys.placeBid("bob", id, 200).ok);                   // t = 0
    sys.advanceTime(5);
    ASSERT_TRUE(sys.placeBid("carol", id, 200).ok);                 // t = 5，同價
    sys.advanceTime(5);
    ASSERT_TRUE(sys.placeBid("dave", id, 200).ok);                  // t = 10，同價
    ASSERT_EQ(std::string("bob"), sys.highestBid(id)->bidder);
    sys.advanceTime(100);
    ASSERT_EQ(std::string("bob"), sys.findItem(id)->winner);
    ASSERT_EQ(200, sys.findItem(id)->finalPrice);
}

// ===================== F6、F7 最高出價與歷史 =====================

static void test_bid_history_order_and_flags() {
    AuctionSystem sys;
    addUsers(sys);
    int id = listSimple(sys, "alice", "item", 100, 10, 60);
    sys.placeBid("bob", id, 100);
    sys.advanceTime(1);
    sys.placeBid("carol", id, 150);
    sys.advanceTime(1);
    sys.placeBid("dave", id, 200);
    sys.cancelBid("dave", id);
    DynArray<Bid> history = sys.bidHistory(id);
    ASSERT_EQ(3, history.size());                                   // 含已撤標
    ASSERT_EQ(std::string("dave"), history.get(0).bidder);          // 由新到舊
    ASSERT_FALSE(history.get(0).active);                            // 已撤標標記
    ASSERT_EQ(std::string("carol"), history.get(1).bidder);
    ASSERT_TRUE(history.get(1).active);
    ASSERT_EQ(std::string("bob"), history.get(2).bidder);
    ASSERT_EQ(2, history.get(0).time);                              // 時間為模擬時鐘
    ASSERT_EQ(2, sys.findItem(id)->bidHeap.size());                 // 有效出價數
}

// ===================== F8 撤標 =====================

static void test_cancel_restores_previous_highest() {
    AuctionSystem sys;
    addUsers(sys);
    int id = listSimple(sys, "alice", "item", 100, 10, 60);
    sys.placeBid("bob", id, 100);
    sys.placeBid("carol", id, 200);
    sys.placeBid("dave", id, 300);
    Result cancel = sys.cancelBid("dave", id);
    ASSERT_TRUE(cancel.ok);
    ASSERT_TRUE(contains(cancel.message, "200"));
    ASSERT_EQ(200, sys.highestBid(id)->amount);                     // 回到前一位
    ASSERT_EQ(std::string("carol"), sys.highestBid(id)->bidder);
    ASSERT_FALSE(sys.cancelBid("dave", id).ok);                     // 已沒有有效出價可撤
    ASSERT_TRUE(sys.cancelBid("carol", id).ok);
    ASSERT_EQ(std::string("bob"), sys.highestBid(id)->bidder);
    ASSERT_TRUE(sys.cancelBid("bob", id).ok);
    ASSERT_TRUE(sys.highestBid(id) == nullptr);                     // 全撤光
    ASSERT_TRUE(sys.placeBid("dave", id, 100).ok);                  // 撤光後可重新出價（起標價）
}

// 只能撤「自己最新的有效出價」，且一次只撤一筆
static void test_cancel_only_own_latest_bid() {
    AuctionSystem sys;
    addUsers(sys);
    int id = listSimple(sys, "alice", "item", 100, 10, 60);
    sys.placeBid("bob", id, 100);          // bob #1
    sys.placeBid("carol", id, 200);
    sys.placeBid("bob", id, 300);          // bob #2（最新）
    ASSERT_TRUE(sys.cancelBid("bob", id).ok);
    ASSERT_EQ(200, sys.highestBid(id)->amount);                     // 撤掉的是 300，不是 100
    DynArray<Bid> history = sys.bidHistory(id);
    ASSERT_FALSE(history.get(0).active);                            // bob 的 300 已撤
    ASSERT_TRUE(history.get(2).active);                             // bob 的 100 仍有效
    ASSERT_TRUE(sys.cancelBid("bob", id).ok);                       // 再撤一次才撤掉 100
    ASSERT_FALSE(sys.bidHistory(id).get(2).active);
}

static void test_cancel_errors() {
    AuctionSystem sys;
    addUsers(sys);
    int id = listSimple(sys, "alice", "item", 100, 10, 60);
    ASSERT_FALSE(sys.cancelBid("bob", id).ok);                      // 沒出價過
    ASSERT_FALSE(sys.cancelBid("ghost", id).ok);                    // 使用者不存在
    ASSERT_FALSE(sys.cancelBid("bob", 999).ok);                     // 商品不存在
    sys.placeBid("bob", id, 100);
    sys.advanceTime(60);
    Result closed = sys.cancelBid("bob", id);                       // 已結標
    ASSERT_FALSE(closed.ok);
    ASSERT_TRUE(contains(closed.message, "已結標"));
    ASSERT_EQ(std::string("bob"), sys.findItem(id)->winner);        // 得標結果不受影響
}

// ===================== F9、F10 BST =====================

static void test_upcoming_items_order() {
    AuctionSystem sys;
    addUsers(sys);
    listSimple(sys, "alice", "a", 100, 10, 300);    // id1 結標 300
    listSimple(sys, "bob", "b", 100, 10, 100);      // id2 結標 100
    listSimple(sys, "carol", "c", 100, 10, 200);    // id3 結標 200
    listSimple(sys, "dave", "d", 100, 10, 100);     // id4 結標 100（與 id2 同時）
    DynArray<int> all = sys.upcomingItems(0);
    ASSERT_EQ(4, all.size());
    ASSERT_EQ(2, all.get(0));                       // 同時間以 ID 小的優先
    ASSERT_EQ(4, all.get(1));
    ASSERT_EQ(3, all.get(2));
    ASSERT_EQ(1, all.get(3));
    ASSERT_EQ(2, sys.upcomingItems(2).size());      // 限制筆數
    sys.advanceTime(150);                           // id2、id4 結標
    DynArray<int> remaining = sys.upcomingItems(0);
    ASSERT_EQ(2, remaining.size());
    ASSERT_EQ(3, remaining.get(0));
    ASSERT_EQ(0, AuctionSystem().upcomingItems(5).size());          // 空系統
}

static void test_range_query_items() {
    AuctionSystem sys;
    addUsers(sys);
    for (int i = 1; i <= 10; i++) {
        listSimple(sys, "alice", "item", 100, 10, i * 10);          // 結標時間 10,20,...,100
    }
    DynArray<int> r = sys.itemsEndingBetween(30, 50);               // 含兩端
    ASSERT_EQ(3, r.size());
    ASSERT_EQ(3, r.get(0));
    ASSERT_EQ(5, r.get(2));
    ASSERT_EQ(0, sys.itemsEndingBetween(31, 39).size());            // 範圍內沒有
    ASSERT_EQ(10, sys.itemsEndingBetween(0, 1000).size());
    ASSERT_EQ(1, sys.itemsEndingBetween(100, 100).size());          // 單點
    ASSERT_EQ(0, sys.itemsEndingBetween(50, 30).size());            // 起迄顛倒
    sys.advanceTime(35);                                            // 前三件結標
    ASSERT_EQ(2, sys.itemsEndingBetween(30, 50).size());            // 30 已結標，只剩 40、50 進行中
}

// ===================== F11 結標 =====================

static void test_close_with_winner() {
    AuctionSystem sys;
    addUsers(sys);
    int id = listSimple(sys, "alice", "item", 100, 10, 60);
    sys.placeBid("bob", id, 100);
    sys.placeBid("carol", id, 150);
    sys.placeBid("bob", id, 300);
    sys.placeBid("dave", id, 400);
    sys.cancelBid("dave", id);                                      // 400 被撤 -> 最高為 bob 300
    Result r = sys.advanceTime(60);
    ASSERT_TRUE(r.ok);
    ASSERT_EQ(1, r.id);                                             // 結標 1 件
    const Item* item = sys.findItem(id);
    ASSERT_TRUE(item->status == Item::CLOSED);
    ASSERT_EQ(std::string("bob"), item->winner);
    ASSERT_EQ(300, item->finalPrice);
    ASSERT_EQ(1, sys.findUser("bob")->wonItems.size());
    ASSERT_EQ(0, sys.findUser("carol")->wonItems.size());
    ASSERT_EQ(0, sys.endTimeTreeSize());                            // 已從 BST 移除
}

static void test_close_without_bids_is_unsold() {
    AuctionSystem sys;
    addUsers(sys);
    int id = listSimple(sys, "alice", "nobody wants this", 100, 10, 30);
    sys.advanceTime(30);
    const Item* item = sys.findItem(id);
    ASSERT_TRUE(item->status == Item::CLOSED);
    ASSERT_EQ(std::string(""), item->winner);                       // 流標
    ASSERT_EQ(0, item->finalPrice);
    ASSERT_EQ(0, sys.findUser("alice")->wonItems.size());
}

// 出價全被撤光後結標，同樣是流標
static void test_close_after_all_bids_cancelled() {
    AuctionSystem sys;
    addUsers(sys);
    int id = listSimple(sys, "alice", "item", 100, 10, 30);
    sys.placeBid("bob", id, 100);
    sys.cancelBid("bob", id);
    sys.advanceTime(30);
    ASSERT_EQ(std::string(""), sys.findItem(id)->winner);
}

// 一次快轉結標多件商品：依結標時間由早到晚，lastClosedItems 記錄順序
static void test_close_multiple_items_in_order() {
    AuctionSystem sys;
    addUsers(sys);
    int a = listSimple(sys, "alice", "a", 100, 10, 50);
    int b = listSimple(sys, "alice", "b", 100, 10, 20);
    int c = listSimple(sys, "alice", "c", 100, 10, 80);
    int d = listSimple(sys, "alice", "d", 100, 10, 500);
    Result r = sys.advanceTime(100);
    ASSERT_EQ(3, r.id);
    const DynArray<int>& closed = sys.lastClosedItems();
    ASSERT_EQ(3, closed.size());
    ASSERT_EQ(b, closed.get(0));
    ASSERT_EQ(a, closed.get(1));
    ASSERT_EQ(c, closed.get(2));
    ASSERT_TRUE(sys.findItem(d)->status == Item::OPEN);
    sys.advanceTime(1);
    ASSERT_EQ(0, sys.lastClosedItems().size());                     // 這次沒有結標
}

// ===================== F14 模擬時鐘 =====================

static void test_advance_time_validation() {
    AuctionSystem sys;
    ASSERT_EQ(0, sys.now());
    ASSERT_FALSE(sys.advanceTime(0).ok);
    ASSERT_FALSE(sys.advanceTime(-5).ok);
    ASSERT_EQ(0, sys.now());
    ASSERT_TRUE(sys.advanceTime(60).ok);
    ASSERT_EQ(60, sys.now());
}

static void test_advance_to_next_end() {
    AuctionSystem sys;
    addUsers(sys);
    ASSERT_FALSE(sys.advanceToNextEnd().ok);                        // 沒有進行中商品
    int a = listSimple(sys, "alice", "a", 100, 10, 90);
    int b = listSimple(sys, "alice", "b", 100, 10, 40);
    ASSERT_TRUE(sys.advanceToNextEnd().ok);
    ASSERT_EQ(40, sys.now());
    ASSERT_TRUE(sys.findItem(b)->status == Item::CLOSED);
    ASSERT_TRUE(sys.findItem(a)->status == Item::OPEN);
    ASSERT_TRUE(sys.advanceToNextEnd().ok);
    ASSERT_EQ(90, sys.now());
    ASSERT_TRUE(sys.findItem(a)->status == Item::CLOSED);
    ASSERT_FALSE(sys.advanceToNextEnd().ok);
}

// ===================== F12 排序 =====================

static void test_sorted_items() {
    AuctionSystem sys;
    addUsers(sys);
    int i1 = listSimple(sys, "alice", "a", 500, 10, 300);   // 無人出價，價格 500
    int i2 = listSimple(sys, "alice", "b", 100, 10, 100);   // 兩筆出價，價格 150
    int i3 = listSimple(sys, "alice", "c", 300, 10, 200);   // 一筆出價，價格 300
    int i4 = listSimple(sys, "alice", "d", 300, 10, 400);   // 無人出價，價格 300（與 i3 同價）
    sys.placeBid("bob", i2, 100);
    sys.placeBid("carol", i2, 150);
    sys.placeBid("bob", i3, 300);

    DynArray<int> byPriceAsc = sys.sortedItems(SORT_BY_PRICE, true, false);
    ASSERT_EQ(i2, byPriceAsc.get(0));
    ASSERT_EQ(i3, byPriceAsc.get(1));                       // 同價 300：維持刊登順序（i3 在 i4 前）
    ASSERT_EQ(i4, byPriceAsc.get(2));
    ASSERT_EQ(i1, byPriceAsc.get(3));
    DynArray<int> byPriceDesc = sys.sortedItems(SORT_BY_PRICE, false, false);
    ASSERT_EQ(i1, byPriceDesc.get(0));
    ASSERT_EQ(i3, byPriceDesc.get(1));                      // 降冪時同價仍維持刊登順序
    ASSERT_EQ(i4, byPriceDesc.get(2));
    ASSERT_EQ(i2, byPriceDesc.get(3));

    DynArray<int> byCount = sys.sortedItems(SORT_BY_BID_COUNT, false, false);
    ASSERT_EQ(i2, byCount.get(0));                          // 2 筆
    ASSERT_EQ(i3, byCount.get(1));                          // 1 筆
    DynArray<int> byEnd = sys.sortedItems(SORT_BY_END_TIME, true, false);
    ASSERT_EQ(i2, byEnd.get(0));
    ASSERT_EQ(i3, byEnd.get(1));
    ASSERT_EQ(i1, byEnd.get(2));
    ASSERT_EQ(i4, byEnd.get(3));

    sys.advanceTime(150);                                   // i2 結標
    ASSERT_EQ(3, sys.sortedItems(SORT_BY_PRICE, true, true).size());   // 只列進行中
    ASSERT_EQ(4, sys.sortedItems(SORT_BY_PRICE, true, false).size());
}

// ===================== F13 我的頁面 =====================

static void test_my_page() {
    AuctionSystem sys;
    addUsers(sys);
    int i1 = listSimple(sys, "alice", "a", 100, 10, 50);
    int i2 = listSimple(sys, "alice", "b", 100, 10, 90);
    int i3 = listSimple(sys, "bob", "c", 100, 10, 50);
    sys.placeBid("carol", i1, 100);
    sys.placeBid("carol", i1, 200);                         // 同一商品多次出價，只記一次
    sys.placeBid("carol", i2, 100);
    sys.placeBid("dave", i3, 100);
    const User* alice = sys.findUser("alice");
    const User* carol = sys.findUser("carol");
    ASSERT_EQ(2, alice->sellingItems.size());
    ASSERT_EQ(2, carol->biddingItems.size());
    ASSERT_EQ(0, carol->wonItems.size());
    sys.advanceTime(60);                                    // i1、i3 結標
    ASSERT_EQ(1, sys.findUser("carol")->wonItems.size());
    ASSERT_EQ(i1, sys.findUser("carol")->wonItems.get(0));
    ASSERT_EQ(1, sys.findUser("dave")->wonItems.size());
    ASSERT_EQ(i2, sys.findUser("carol")->biddingItems.get(1));
}

// ===================== F15 存檔／讀檔 =====================

static void test_save_load_round_trip() {
    AuctionSystem a;
    a.loadSampleData();
    std::string saved = a.saveToString();
    AuctionSystem b;
    Result r = b.loadFromString(saved);
    ASSERT_TRUE(r.ok);
    ASSERT_EQ(saved, b.saveToString());                     // 存 -> 讀 -> 再存，內容完全相同
    ASSERT_EQ(a.now(), b.now());
    ASSERT_EQ(a.allItemIds().size(), b.allItemIds().size());
    // 還原後系統仍可正常運作
    ASSERT_EQ(a.highestBid(6)->amount, b.highestBid(6)->amount);
    ASSERT_EQ(a.upcomingItems(0).size(), b.upcomingItems(0).size());
    ASSERT_TRUE(b.placeBid("dave", 6, 5000).ok);
    ASSERT_TRUE(b.advanceToNextEnd().ok);
}

static void test_save_load_special_characters() {
    AuctionSystem a;
    a.registerUser("alice");
    a.registerUser("bob");
    a.listItem("alice", "標題含|直線與%百分比", "第一行\n第二行|含分隔線 100%", 100, 10, 60);
    a.placeBid("bob", 1, 100);
    AuctionSystem b;
    ASSERT_TRUE(b.loadFromString(a.saveToString()).ok);
    ASSERT_EQ(std::string("標題含|直線與%百分比"), b.findItem(1)->title);
    ASSERT_EQ(std::string("第一行\n第二行|含分隔線 100%"), b.findItem(1)->description);
}

// 壞掉的存檔：回報錯誤，而且「不會」破壞目前的資料
static void test_load_invalid_keeps_current_data() {
    AuctionSystem sys;
    sys.loadSampleData();
    std::string before = sys.saveToString();
    ASSERT_FALSE(sys.loadFromString("").ok);
    ASSERT_FALSE(sys.loadFromString("hello world").ok);
    ASSERT_FALSE(sys.loadFromString("AUCTION_V1\nbid|1|99|nobody|100|0|1\n").ok);       // 商品不存在
    ASSERT_FALSE(sys.loadFromString("AUCTION_V1\nuser|a\nitem|1|ghost|t|d|1|1|1|0||0\n").ok);  // 賣家不存在
    ASSERT_FALSE(sys.loadFromString("AUCTION_V1\nnow|abc\n").ok);
    Result garbage = sys.loadFromString("AUCTION_V1\nunknown line\n");
    ASSERT_FALSE(garbage.ok);
    ASSERT_TRUE(contains(garbage.message, "第 2 行"));
    ASSERT_EQ(before, sys.saveToString());                  // 原本的資料完全沒變
}

static void test_save_and_load_file() {
    const char* path = "test_save_tmp.txt";
    AuctionSystem a;
    a.loadSampleData();
    ASSERT_TRUE(a.saveToFile(path).ok);
    AuctionSystem b;
    ASSERT_TRUE(b.loadFromFile(path).ok);
    ASSERT_EQ(a.saveToString(), b.saveToString());
    std::remove(path);
    ASSERT_FALSE(b.loadFromFile("this_file_does_not_exist.txt").ok);
    ASSERT_FALSE(b.saveToFile("no_such_directory/x.txt").ok);
}

// ===================== F16 範例資料 =====================

static void test_sample_data() {
    AuctionSystem sys;
    Result r = sys.loadSampleData();
    ASSERT_TRUE(r.ok);
    ASSERT_EQ(5, sys.userNames().size());
    ASSERT_EQ(20, sys.allItemIds().size());

    int totalBids = 0;
    int cancelledBids = 0;
    int openWithoutBids = 0;
    int closedWithWinner = 0;
    int unsold = 0;
    for (int i = 0; i < sys.allItemIds().size(); i++) {
        const Item* item = sys.findItem(sys.allItemIds().get(i));
        totalBids += item->bidHistory.size();
        DynArray<Bid> history = sys.bidHistory(item->itemId);
        for (int j = 0; j < history.size(); j++) {
            if (!history.get(j).active) cancelledBids++;
        }
        if (item->status == Item::OPEN && item->bidHistory.empty()) openWithoutBids++;
        if (item->status == Item::CLOSED && !item->winner.empty()) closedWithWinner++;
        if (item->status == Item::CLOSED && item->winner.empty()) unsold++;
        ASSERT_TRUE(item->startPrice >= 50 && item->startPrice <= 5000);
    }
    ASSERT_TRUE(totalBids >= 30);                           // 數十筆出價
    ASSERT_TRUE(cancelledBids >= 1);                        // 含撤標紀錄
    ASSERT_TRUE(openWithoutBids >= 1);                      // 有進行中且無人出價（Demo 流標用）
    ASSERT_TRUE(closedWithWinner >= 1);                     // 已有成交案例
    ASSERT_TRUE(unsold >= 1);                               // 已有流標案例
    ASSERT_TRUE(sys.endTimeTreeSize() > 0);
    // 重複載入結果相同、不會累加
    std::string first = sys.saveToString();
    sys.loadSampleData();
    ASSERT_EQ(first, sys.saveToString());
}

// data/sample.txt（CLI 啟動時讀的檔案）必須與內建範例資料完全一致；
// 內建範例改動後請執行 make sample 重新產生
static void test_sample_file_matches_builtin_sample() {
    AuctionSystem builtin;
    builtin.loadSampleData();
    AuctionSystem fromFile;
    Result r = fromFile.loadFromFile("data/sample.txt");   // 測試從專案根目錄執行
    ASSERT_TRUE(r.ok);
    ASSERT_EQ(builtin.saveToString(), fromFile.saveToString());
}

// 重設後回到空白狀態
static void test_reset() {
    AuctionSystem sys;
    sys.loadSampleData();
    sys.reset();
    ASSERT_EQ(0, sys.now());
    ASSERT_EQ(0, sys.userNames().size());
    ASSERT_EQ(0, sys.allItemIds().size());
    ASSERT_EQ(0, sys.endTimeTreeSize());
    sys.registerUser("alice");
    ASSERT_EQ(1, sys.listItem("alice", "x", "", 10, 1, 5).id);   // ID 從 1 重新開始
}

// 資料結構檢視用的統計
static void test_data_structure_stats() {
    AuctionSystem sys;
    sys.loadSampleData();
    ASSERT_TRUE(sys.itemTableBucketCount() >= 20);
    ASSERT_TRUE(sys.itemTableLoadFactor() <= 0.75);
    ASSERT_TRUE(sys.itemTableMaxChain() >= 1);
    ASSERT_TRUE(sys.endTimeTreeHeight() >= 1);
    ASSERT_EQ(sys.endTimeTreeSize(), sys.upcomingItems(0).size());
}

// ===================== 隨機一致性測試 =====================

// 檢查：每件商品的「堆頂」必須等於「歷史中所有有效出價」裡優先權最高者（暴力解）；
//       進行中商品數 = BST 節點數；已結標商品的得標者 = 結標時的最高出價者
static bool systemIsConsistent(const AuctionSystem& sys) {
    int openCount = 0;
    for (int i = 0; i < sys.allItemIds().size(); i++) {
        const Item* item = sys.findItem(sys.allItemIds().get(i));
        DynArray<Bid> history = sys.bidHistory(item->itemId);
        int bestIndex = -1;
        int activeCount = 0;
        for (int j = 0; j < history.size(); j++) {
            if (!history.get(j).active) continue;
            activeCount++;
            if (bestIndex < 0 || history.get(j).hasHigherPriority(history.get(bestIndex))) {
                bestIndex = j;
            }
        }
        const Bid* top = sys.highestBid(item->itemId);
        if (bestIndex < 0) {
            if (top != nullptr) return false;
        } else {
            if (top == nullptr || top->bidId != history.get(bestIndex).bidId) return false;
        }
        if (activeCount != item->bidHeap.size() || !item->bidHeap.isValid()) return false;
        if (item->status == Item::OPEN) {
            openCount++;
        } else if (bestIndex >= 0) {
            if (item->winner != history.get(bestIndex).bidder) return false;
            if (item->finalPrice != history.get(bestIndex).amount) return false;
        } else if (!item->winner.empty() || item->finalPrice != 0) {
            return false;
        }
    }
    return openCount == sys.endTimeTreeSize();
}

static void test_random_operations_stay_consistent() {
    bool consistent = true;
    for (int seed = 1; seed <= 3; seed++) {
        SimpleRandom random(seed * 4241);
        AuctionSystem sys;
        addUsers(sys);
        const char* names[] = {"alice", "bob", "carol", "dave"};
        for (int step = 0; step < 1000; step++) {
            int action = random.next(10);
            if (action < 2) {
                listSimple(sys, names[random.next(4)], "random item", 10 + random.next(50),
                           random.next(3), 1 + random.next(120));
            } else if (action < 7) {
                int count = sys.allItemIds().size();
                if (count > 0) {
                    int id = sys.allItemIds().get(random.next(count));
                    sys.placeBid(names[random.next(4)], id, 10 + random.next(400));
                }
            } else if (action < 9) {
                int count = sys.allItemIds().size();
                if (count > 0) {
                    int id = sys.allItemIds().get(random.next(count));
                    sys.cancelBid(names[random.next(4)], id);
                }
            } else {
                sys.advanceTime(1 + random.next(30));
            }
            if (step % 25 == 0 && !systemIsConsistent(sys)) consistent = false;
        }
        if (!systemIsConsistent(sys)) consistent = false;
        // 存檔再讀檔，內容必須完全相同
        AuctionSystem copy;
        if (!copy.loadFromString(sys.saveToString()).ok || copy.saveToString() != sys.saveToString()) {
            consistent = false;
        }
        if (!systemIsConsistent(copy)) consistent = false;
    }
    ASSERT_TRUE(consistent);
}

void run_auction_tests() {
    RUN_TEST(test_register_user);
    RUN_TEST(test_register_user_errors);
    RUN_TEST(test_list_item_success);
    RUN_TEST(test_list_item_validation_errors);
    RUN_TEST(test_find_item_not_found);
    RUN_TEST(test_search_items);
    RUN_TEST(test_place_bid_normal);
    RUN_TEST(test_place_bid_too_low);
    RUN_TEST(test_place_bid_seller_and_invalid);
    RUN_TEST(test_place_bid_after_close);
    RUN_TEST(test_tie_first_bidder_wins);
    RUN_TEST(test_bid_history_order_and_flags);
    RUN_TEST(test_cancel_restores_previous_highest);
    RUN_TEST(test_cancel_only_own_latest_bid);
    RUN_TEST(test_cancel_errors);
    RUN_TEST(test_upcoming_items_order);
    RUN_TEST(test_range_query_items);
    RUN_TEST(test_close_with_winner);
    RUN_TEST(test_close_without_bids_is_unsold);
    RUN_TEST(test_close_after_all_bids_cancelled);
    RUN_TEST(test_close_multiple_items_in_order);
    RUN_TEST(test_advance_time_validation);
    RUN_TEST(test_advance_to_next_end);
    RUN_TEST(test_sorted_items);
    RUN_TEST(test_my_page);
    RUN_TEST(test_save_load_round_trip);
    RUN_TEST(test_save_load_special_characters);
    RUN_TEST(test_load_invalid_keeps_current_data);
    RUN_TEST(test_save_and_load_file);
    RUN_TEST(test_sample_data);
    RUN_TEST(test_sample_file_matches_builtin_sample);
    RUN_TEST(test_reset);
    RUN_TEST(test_data_structure_stats);
    RUN_TEST(test_random_operations_stay_consistent);
}
