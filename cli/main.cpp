// cli/main.cpp — 終端機數字選單介面
//
// 這個檔案只負責「輸入／輸出」：把使用者輸入的文字轉成呼叫 AuctionSystem，再把結果印出來。
// 所有業務規則與資料結構都在 core/ 裡，網頁版共用同一套邏輯。
//
// 用法：
//   auction_cli                      啟動時讀 data/sample.txt（找不到就用內建範例資料）
//   auction_cli <檔案>               啟動時讀指定的存檔
//   auction_cli --empty              從空白系統開始
//   auction_cli --write-sample <檔案> 把內建範例資料寫成存檔後結束（用來產生 data/sample.txt）
#include <iostream>
#include <string>

#include "AuctionSystem.h"

#ifdef _WIN32
#include <windows.h>
#endif

static AuctionSystem g_system;          // 整個程式只有一個競標系統
static std::string g_currentUser;       // 目前登入（切換）的使用者，空字串代表尚未選擇
static bool g_inputEnded = false;       // stdin 已經結束（例如管線輸入跑完）

// ======================================================================
// 輸入輸出小工具
// ======================================================================

// 顯示提示並讀一行；遇到輸入結束（EOF）時設定旗標並回傳空字串
static std::string ask(const std::string& prompt) {
    std::cout << prompt << std::flush;
    std::string line;
    if (!std::getline(std::cin, line)) {
        g_inputEnded = true;
        return "";
    }
    if (!line.empty() && line[line.size() - 1] == '\r') {
        line.erase(line.size() - 1);              // 去掉 Windows 的 \r
    }
    return line;
}

// 解析整數（可有負號）；不是整數回傳 false
static bool parseNumber(const std::string& text, long long& out) {
    if (text.empty() || text.size() > 18) {
        return false;
    }
    std::size_t start = (text[0] == '-') ? 1 : 0;
    if (start == text.size()) {
        return false;
    }
    long long value = 0;
    for (std::size_t i = start; i < text.size(); i++) {
        if (text[i] < '0' || text[i] > '9') {
            return false;
        }
        value = value * 10 + (text[i] - '0');
    }
    out = (start == 1) ? -value : value;
    return true;
}

// 反覆詢問直到輸入合法整數；輸入結束則回傳 false
static bool askNumber(const std::string& prompt, long long& out) {
    while (true) {
        std::string line = ask(prompt);
        if (g_inputEnded) {
            return false;
        }
        if (parseNumber(line, out)) {
            return true;
        }
        std::cout << "  ※ 請輸入整數數字" << std::endl;
    }
}

// 把「分鐘」格式化成易讀的時間長度，例如 1500 -> "1 天 1 小時"
static std::string formatDuration(long long minutes) {
    if (minutes <= 0) {
        return "0 分";
    }
    long long days = minutes / 1440;
    long long hours = (minutes % 1440) / 60;
    long long mins = minutes % 60;
    std::string text;
    if (days > 0) text += std::to_string(days) + " 天 ";
    if (hours > 0) text += std::to_string(hours) + " 小時 ";
    if (mins > 0 || text.empty()) text += std::to_string(mins) + " 分";
    while (!text.empty() && text[text.size() - 1] == ' ') {
        text.erase(text.size() - 1);
    }
    return text;
}

static std::string statusText(const Item& item) {
    if (item.status == Item::OPEN) {
        return "進行中";
    }
    if (item.winner.empty()) {
        return "已結標（流標）";
    }
    return "已結標（得標：" + item.winner + " " + std::to_string(item.finalPrice) + " 元）";
}

// 一件商品的一行摘要
static void printItemLine(const Item& item) {
    std::cout << "  #" << item.itemId << "  " << item.title << "  |  目前價 "
              << AuctionSystem::currentPrice(item) << " 元  |  有效出價 " << item.bidHeap.size()
              << " 筆  |  ";
    if (item.status == Item::OPEN) {
        std::cout << "剩餘 " << formatDuration(item.endTime - g_system.now());
    } else {
        std::cout << statusText(item);
    }
    std::cout << std::endl;
}

static void printItemList(const DynArray<int>& ids) {
    if (ids.empty()) {
        std::cout << "  （沒有符合的商品）" << std::endl;
        return;
    }
    for (int i = 0; i < ids.size(); i++) {
        const Item* item = g_system.findItem(ids.get(i));
        if (item != nullptr) {
            printItemLine(*item);
        }
    }
    std::cout << "  共 " << ids.size() << " 件" << std::endl;
}

static void printResult(const Result& result) {
    std::cout << (result.ok ? "  ✔ " : "  ✘ ") << result.message << std::endl;
}

static void printHint(const std::string& text) {
    std::cout << "  [資料結構] " << text << std::endl;
}

// 需要先選擇使用者的功能呼叫它；沒選回傳 false
static bool requireUser() {
    if (g_currentUser.empty()) {
        std::cout << "  ✘ 請先使用選單 1 選擇（或註冊）使用者" << std::endl;
        return false;
    }
    return true;
}

// 讀取商品 ID 並確認存在；不存在回傳 false
static bool askItemId(const std::string& prompt, int& itemId) {
    long long value = 0;
    if (!askNumber(prompt, value)) {
        return false;
    }
    if (g_system.findItem(static_cast<int>(value)) == nullptr) {
        std::cout << "  ✘ 找不到商品 #" << value << std::endl;
        return false;
    }
    itemId = static_cast<int>(value);
    return true;
}

// ======================================================================
// 各項功能
// ======================================================================

static void printHeader() {
    std::cout << std::endl
              << "================ 二手交易競標平台 ================" << std::endl
              << " 使用者：" << (g_currentUser.empty() ? "（尚未選擇）" : g_currentUser)
              << "    模擬時間：第 " << g_system.now() << " 分鐘（" << formatDuration(g_system.now())
              << "）" << std::endl
              << "==================================================" << std::endl;
}

static void printMenu() {
    std::cout << " 1. 切換／註冊使用者        8. 即將結標列表（BST）\n"
              << " 2. 刊登商品                9. 結標時段範圍查詢（BST）\n"
              << " 3. 商品列表（可排序）     10. 我的頁面\n"
              << " 4. 關鍵字搜尋商品         11. 時鐘快轉／結標\n"
              << " 5. 查看商品詳情與出價歷史 12. 存檔／讀檔\n"
              << " 6. 出價                   13. 重設為範例資料\n"
              << " 7. 撤回出價               14. 資料結構檢視\n"
              << " 0. 離開\n";
}

// 1. 切換／註冊使用者
static void actSwitchUser() {
    std::cout << "  目前的使用者：";
    for (int i = 0; i < g_system.userNames().size(); i++) {
        std::cout << g_system.userNames().get(i) << (i + 1 < g_system.userNames().size() ? "、" : "");
    }
    std::cout << std::endl;
    std::string name = ask("  輸入使用者名稱（不存在就會註冊新使用者）：");
    if (g_inputEnded || name.empty()) {
        return;
    }
    if (!g_system.userExists(name)) {
        Result r = g_system.registerUser(name);
        printResult(r);
        printHint("HashTable<string, User>::insert  O(1) 平均");
        if (!r.ok) {
            return;
        }
    }
    g_currentUser = name;
    std::cout << "  已切換為使用者「" << name << "」" << std::endl;
}

// 2. 刊登商品
static void actListItem() {
    if (!requireUser()) return;
    std::string title = ask("  商品標題：");
    if (g_inputEnded) return;
    std::string description = ask("  商品描述：");
    if (g_inputEnded) return;
    long long startPrice = 0, increment = 0, duration = 0;
    if (!askNumber("  起標價（元）：", startPrice)) return;
    if (!askNumber("  最小加價幅度（元，可為 0）：", increment)) return;
    if (!askNumber("  拍賣時長（分鐘，60=1 小時、1440=1 天）：", duration)) return;
    Result r = g_system.listItem(g_currentUser, title, description, startPrice, increment, duration);
    printResult(r);
    printHint("HashTable::insert O(1) 平均 + BST::insert O(log n) 平均");
}

// 3. 商品列表（可排序）
static void actShowItems() {
    std::cout << "  排序方式：1 刊登順序  2 目前價格  3 有效出價數  4 結標時間\n";
    long long sortChoice = 0;
    if (!askNumber("  選擇（1-4）：", sortChoice)) return;
    bool ascending = true;
    if (sortChoice >= 2 && sortChoice <= 4) {
        long long order = 0;
        if (!askNumber("  1 由小到大  2 由大到小：", order)) return;
        ascending = (order != 2);
    }
    std::string onlyOpenText = ask("  只顯示進行中的商品？(y/N)：");
    if (g_inputEnded) return;
    bool openOnly = (onlyOpenText == "y" || onlyOpenText == "Y");

    DynArray<int> ids;
    if (sortChoice == 2) {
        ids = g_system.sortedItems(SORT_BY_PRICE, ascending, openOnly);
    } else if (sortChoice == 3) {
        ids = g_system.sortedItems(SORT_BY_BID_COUNT, ascending, openOnly);
    } else if (sortChoice == 4) {
        ids = g_system.sortedItems(SORT_BY_END_TIME, ascending, openOnly);
    } else {
        ids = g_system.searchItems("", openOnly);
    }
    printItemList(ids);
    if (sortChoice >= 2 && sortChoice <= 4) {
        printHint("mergeSort（穩定）O(n log n)");
    }
}

// 4. 關鍵字搜尋
static void actSearch() {
    std::string keyword = ask("  關鍵字（標題包含）：");
    if (g_inputEnded) return;
    printItemList(g_system.searchItems(keyword, false));
    printHint("DynArray 線性掃描 O(n·m)（取捨：沒有為關鍵字另建索引）");
}

// 5. 商品詳情
static void printItemDetail(const Item& item) {
    std::cout << "  ----------------------------------------------" << std::endl
              << "  商品 #" << item.itemId << "：" << item.title << std::endl
              << "  賣家：" << item.seller << "    狀態：" << statusText(item) << std::endl
              << "  描述：" << item.description << std::endl
              << "  起標價 " << item.startPrice << " 元，最小加價 " << item.minIncrement << " 元，結標時間第 "
              << item.endTime << " 分鐘";
    if (item.status == Item::OPEN) {
        std::cout << "（剩餘 " << formatDuration(item.endTime - g_system.now()) << "）";
    }
    std::cout << std::endl;

    const Bid* top = g_system.highestBid(item.itemId);
    if (top != nullptr) {
        std::cout << "  目前最高出價：" << top->amount << " 元（" << top->bidder << "，第 " << top->time
                  << " 分鐘）" << std::endl;
    } else {
        std::cout << "  目前沒有有效出價" << std::endl;
    }
    if (item.status == Item::OPEN) {
        std::cout << "  下一筆最低可出價：" << AuctionSystem::minNextBid(item) << " 元" << std::endl;
    }

    DynArray<Bid> history = g_system.bidHistory(item.itemId);
    std::cout << "  出價歷史（由新到舊，共 " << history.size() << " 筆）：" << std::endl;
    for (int i = 0; i < history.size(); i++) {
        const Bid& bid = history.get(i);
        std::cout << "    第 " << bid.time << " 分鐘  " << bid.bidder << "  " << bid.amount << " 元"
                  << (bid.active ? "" : "  （已撤標）") << std::endl;
    }
    std::cout << "  ----------------------------------------------" << std::endl;
}

static void actItemDetail() {
    int itemId = 0;
    if (!askItemId("  商品編號：", itemId)) return;
    printItemDetail(*g_system.findItem(itemId));
    printHint("HashTable::find O(1)；最高出價 = Heap 堆頂 O(1)；歷史 = LinkedList 走訪 O(m)");
}

// 6. 出價
static void actPlaceBid() {
    if (!requireUser()) return;
    int itemId = 0;
    if (!askItemId("  商品編號：", itemId)) return;
    const Item* item = g_system.findItem(itemId);
    if (item->status == Item::OPEN) {
        std::cout << "  （目前價 " << AuctionSystem::currentPrice(*item) << " 元，最低可出價 "
                  << AuctionSystem::minNextBid(*item) << " 元）" << std::endl;
    }
    long long amount = 0;
    if (!askNumber("  出價金額（元）：", amount)) return;
    Result r = g_system.placeBid(g_currentUser, itemId, amount);
    printResult(r);
    if (r.ok) {
        printHint("MaxHeap::push O(log n) + LinkedList::pushFront O(1)");
    }
}

// 7. 撤回出價
static void actCancelBid() {
    if (!requireUser()) return;
    int itemId = 0;
    if (!askItemId("  商品編號：", itemId)) return;
    Result r = g_system.cancelBid(g_currentUser, itemId);
    printResult(r);
    if (r.ok) {
        printHint("MaxHeap::removeById O(log n)（HashTable 記錄位置）+ 歷史標記已撤標");
    }
}

// 8. 即將結標列表
static void actUpcoming() {
    long long count = 0;
    if (!askNumber("  列出最近幾件？（0 = 全部）：", count)) return;
    printItemList(g_system.upcomingItems(static_cast<int>(count)));
    printHint("BST 中序走訪 O(n)，由結標時間近到遠");
}

// 9. 範圍查詢
static void actRangeQuery() {
    std::cout << "  現在是第 " << g_system.now() << " 分鐘" << std::endl;
    long long t1 = 0, t2 = 0;
    if (!askNumber("  起（第幾分鐘）：", t1)) return;
    if (!askNumber("  迄（第幾分鐘）：", t2)) return;
    printItemList(g_system.itemsEndingBetween(t1, t2));
    printHint("BST::rangeQuery O(log n + k)（剪枝走訪）");
}

// 10. 我的頁面
static void printIdList(const char* label, const DynArray<int>& ids) {
    std::cout << " ◆ " << label << "（" << ids.size() << "）" << std::endl;
    printItemList(ids);
}

static void actMyPage() {
    if (!requireUser()) return;
    const User* user = g_system.findUser(g_currentUser);
    printIdList("我賣的", user->sellingItems);
    printIdList("我出價過的", user->biddingItems);
    printIdList("我得標的", user->wonItems);
}

// 11. 時鐘快轉
static void printClosedItems() {
    const DynArray<int>& closed = g_system.lastClosedItems();
    for (int i = 0; i < closed.size(); i++) {
        const Item* item = g_system.findItem(closed.get(i));
        std::cout << "    結標 #" << item->itemId << " " << item->title << " → " << statusText(*item)
                  << std::endl;
    }
}

static void actClock() {
    std::cout << "  1. +1 分鐘  2. +1 小時  3. +1 天  4. 跳到下一個結標時間  5. 自訂分鐘數\n";
    long long choice = 0;
    if (!askNumber("  選擇（1-5）：", choice)) return;
    Result r = Result::fail("無效的選擇");
    if (choice == 1) {
        r = g_system.advanceTime(1);
    } else if (choice == 2) {
        r = g_system.advanceTime(60);
    } else if (choice == 3) {
        r = g_system.advanceTime(1440);
    } else if (choice == 4) {
        r = g_system.advanceToNextEnd();
    } else if (choice == 5) {
        long long minutes = 0;
        if (!askNumber("  快轉幾分鐘：", minutes)) return;
        r = g_system.advanceTime(minutes);
    }
    printResult(r);
    if (r.ok) {
        printClosedItems();
        printHint("BST::peekMin/popMin O(log n) 取出到期商品，堆頂決定得標者");
    }
}

// 12. 存檔／讀檔
static void actSaveLoad() {
    std::string defaultPath = "data/save.txt";
    long long choice = 0;
    if (!askNumber("  1 存檔  2 讀檔：", choice)) return;
    std::string path = ask("  檔案路徑（直接 Enter = " + defaultPath + "）：");
    if (g_inputEnded) return;
    if (path.empty()) {
        path = defaultPath;
    }
    if (choice == 1) {
        printResult(g_system.saveToFile(path));
    } else if (choice == 2) {
        Result r = g_system.loadFromFile(path);
        printResult(r);
        if (r.ok && !g_system.userExists(g_currentUser)) {
            g_currentUser = "";                       // 讀進來的資料沒有這位使用者
        }
    } else {
        std::cout << "  ✘ 無效的選擇" << std::endl;
    }
}

// 13. 重設為範例資料
static void actResetSample() {
    std::string sure = ask("  這會清除目前所有資料，確定嗎？(y/N)：");
    if (sure != "y" && sure != "Y") {
        std::cout << "  已取消" << std::endl;
        return;
    }
    printResult(g_system.loadSampleData());
    g_currentUser = "";
}

// 14. 資料結構檢視
static void actInspect() {
    std::cout << "  ◆ Hash Table（商品 ID -> 商品）：bucket 數 " << g_system.itemTableBucketCount()
              << "，負載因子 " << g_system.itemTableLoadFactor() << "，最長鏈 "
              << g_system.itemTableMaxChain() << std::endl;
    std::cout << "  ◆ BST（進行中商品，依結標時間）：節點數 " << g_system.endTimeTreeSize() << "，樹高 "
              << g_system.endTimeTreeHeight() << std::endl;

    std::string text = ask("  要看哪件商品的出價 Heap？（輸入商品編號，Enter 略過）：");
    long long id = 0;
    if (g_inputEnded || !parseNumber(text, id)) {
        return;
    }
    const Item* item = g_system.findItem(static_cast<int>(id));
    if (item == nullptr) {
        std::cout << "  ✘ 找不到商品 #" << id << std::endl;
        return;
    }
    const MaxHeap<Bid>& heap = item->bidHeap;
    std::cout << "  ◆ 商品 #" << id << " 的 Max-Heap（陣列順序 = 層序走訪，共 " << heap.size() << " 筆）："
              << std::endl;
    int level = 0;
    int levelStart = 0;                                // 這一層第一個節點的索引
    for (int i = 0; i < heap.size(); i++) {
        if (i == levelStart * 2 + 1 && i > 0) {        // 進入下一層
            level++;
            levelStart = i;
        }
        std::cout << "    [" << i << "] " << std::string(level * 2, ' ') << heap.at(i).amount << " 元 ("
                  << heap.at(i).bidder << ")" << std::endl;
    }
}

// ======================================================================
// 主程式
// ======================================================================

static void loadInitialData(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[1]) == "--empty") {
        std::cout << "以空白系統啟動。" << std::endl;
        return;
    }
    if (argc >= 2) {
        Result r = g_system.loadFromFile(argv[1]);
        printResult(r);
        if (r.ok) {
            return;
        }
        std::cout << "改用內建範例資料。" << std::endl;
    } else {
        Result r = g_system.loadFromFile("data/sample.txt");
        if (r.ok) {
            std::cout << "已讀取 data/sample.txt（範例資料）" << std::endl;
            return;
        }
        std::cout << "找不到 data/sample.txt，改用內建範例資料。" << std::endl;
    }
    printResult(g_system.loadSampleData());
}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);                       // 讓 Windows 終端機正確顯示中文
    SetConsoleCP(CP_UTF8);
#endif

    if (argc >= 3 && std::string(argv[1]) == "--write-sample") {
        g_system.loadSampleData();
        Result r = g_system.saveToFile(argv[2]);
        printResult(r);
        return r.ok ? 0 : 1;
    }

    loadInitialData(argc, argv);

    while (!g_inputEnded) {
        printHeader();
        printMenu();
        std::string choice = ask("請選擇功能：");
        if (g_inputEnded) {
            break;
        }
        std::cout << std::endl;
        if (choice == "0") {
            break;
        } else if (choice == "1") {
            actSwitchUser();
        } else if (choice == "2") {
            actListItem();
        } else if (choice == "3") {
            actShowItems();
        } else if (choice == "4") {
            actSearch();
        } else if (choice == "5") {
            actItemDetail();
        } else if (choice == "6") {
            actPlaceBid();
        } else if (choice == "7") {
            actCancelBid();
        } else if (choice == "8") {
            actUpcoming();
        } else if (choice == "9") {
            actRangeQuery();
        } else if (choice == "10") {
            actMyPage();
        } else if (choice == "11") {
            actClock();
        } else if (choice == "12") {
            actSaveLoad();
        } else if (choice == "13") {
            actResetSample();
        } else if (choice == "14") {
            actInspect();
        } else {
            std::cout << "  ✘ 無效的選項，請輸入 0-14" << std::endl;
        }
    }
    std::cout << "再見！" << std::endl;
    return 0;
}
