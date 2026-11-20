# HANDOFF — 給接手的開發者／AI

> 最後更新：2026-11-20 · 狀態：PLAN.md 第 9 節規劃表全部完成 · 線上 Demo：<https://zhao-926.github.io/bidding-system/> · Repo：<https://github.com/zhao-926/bidding-system>

## 1. 專案目的與課程限制

資料結構（電機四技二甲，許協誌老師）期末專題 #11「**二手交易競標平台**」，單人完成。課程規範的**硬性限制**：

1. **`core/`（C++）與 `web/js/core/`（JavaScript）內不准使用內建容器或內建排序。**
   - C++ 禁止：`vector list forward_list deque array map set unordered_map unordered_set multimap stack queue priority_queue`、`<algorithm>` 的 `sort`。允許：`<string> <cstdint> <cstring> <iostream> <fstream> <sstream> <chrono> <cstdlib>`、`new/delete`。
   - JS 禁止：`Map Set WeakMap WeakSet`、`Array.from/of`、`Object.keys/values/entries`、陣列方法 `sort splice slice concat reverse shift unshift indexOf includes filter map reduce …`；陣列只能當「固定大小的底層記憶體」（`new Array(n)` + 索引存取）。
   - **`make check`（`scripts/check_no_stl.sh`）會自動檢查，CI 也會跑；改 core 後務必執行。**
   - STL 只允許出現在 `bench/`（效能對照組）；`tests/`、`web/app.js`、`cli/` 是介面與測試程式，不在限制內（但 `cli/` 也盡量不用 STL 容器）。
2. **每個公開操作都要在函式上方註解標明時間複雜度**（例如 `// O(1) average, O(n) worst`）。
3. **程式碼要讓學生本人能看懂並口頭說明**：命名清楚、關鍵步驟有中文註解、不使用過度炫技的語法。取捨時以「可說明」優先於「炫技或最佳化」。
4. 每個 Phase 結束 commit（訊息格式 `phaseN: 說明`）並 push 到 `main`。
5. 繁體中文（台灣用語）為文件與介面語言；程式碼、指令保持英文。

## 2. 重要決定（與原計畫不同之處）

| 決定 | 說明 |
|---|---|
| **網頁版用 JS 移植，不用 Emscripten／WebAssembly** | 使用者在 Phase 2 後決定。`web/js/core/` 是 `core/` 的一比一移植（檔名、類別、方法名、註解、複雜度標示都對應）。**備案「Emscripten 失敗才走 JS」並未啟用，因為本專案從未使用 Emscripten**；PLAN.md 已同步更新。`wasm/` 資料夾不存在、`make web` 不會編譯任何東西。 |
| 兩版本以「存檔位元組」驗證一致 | `tests/js/test_core.mjs` 把 JS 載入範例資料後的 `saveToString()` 與 **C++ 產生的 `data/sample.txt`** 逐位元組比對，並讓 JS 讀 C++ 的存檔再存出。這是兩版行為一致的主要保證。 |
| minIncrement 允許為 0 | 為了能測「同價先到者勝」；起標價必須 ≥ 1，加價 ≥ 0，時長 ≥ 1 分鐘。 |
| 有效出價數 = Heap 大小 | 排序「出價次數」與卡片的「有效出價 N 筆」都排除已撤標的。 |
| BST 與所有走訪都用迴圈 | 避免依序插入造成的退化樹遞迴過深 stack overflow（benchmark 會插入 100,000 筆依序資料）。 |
| `User` 多了 `wonItems` | PLAN 的模型沒有，為了 F13「我得標的」。 |

## 3. 目錄與檔案職責

```
core/                       C++ 核心（無 UI、無 STL 容器）
  DynArray.h                動態陣列（容量翻倍）；越界 → 印訊息並 abort()
  LinkedList.h              單向鏈結串列（pushFront/popFront/find/forEach）
  HashTable.h               separate chaining；hashKey() 針對 std::string（djb2）與 int（乘法雜湊）
  MaxHeap.h                 最大堆積 + HashTable<id,位置>，支援 removeById；元素需有 getId()、hasHigherPriority()
  BST.h                     二元搜尋樹（key 只需 operator<）；全迴圈實作；不可複製
  MergeSort.h               穩定合併排序（DynArray + 比較函式）
  Models.h                  Bid / ItemKey / Item / User
  AuctionSystem.h/.cpp      所有業務邏輯 F1–F16、存讀檔、範例資料
cli/main.cpp                終端機數字選單（只做輸入輸出）
web/
  index.html style.css      頁面與樣式（深淺色自動、手機可用）
  app.js                    介面邏輯：呼叫 js/core、畫面繪製、localStorage、資料結構檢視
  js/core/*.js              core/ 的 JS 一比一移植（ES Module）
tests/                      C++ 測試：test_framework.h（ASSERT_TRUE/FALSE/EQ）、test_main.cpp、各結構與 test_auction.cpp
tests/js/test_core.mjs      JS 測試（Node，自帶極簡框架）
bench/bench.cpp             自刻 vs STL；輸出 docs/benchmark.md（唯一可用 STL 的地方）
data/sample.txt             範例資料存檔（由 make sample 產生，勿手改）
scripts/check_no_stl.sh     檢查 core/ 與 web/js/core/ 沒有使用被禁止的內建容器
scripts/build_web.sh        網頁部署前檢查（檔案齊全、相對路徑、JS 測試）
.github/workflows/deploy.yml  push 到 main → 測試 → 部署 web/ 到 GitHub Pages
docs/                       architecture.md complexity.md benchmark.md demo-script.md contribution.md images/
PLAN.md                     實作計畫書與完成檢查表（第 9 節全部 [x]）
package.json                只有 "type": "module"（讓 Node 把 .js 當 ES Module）與 npm test 捷徑
```

## 4. 建置環境與指令

| 項目 | 版本（開發時） |
|---|---|
| g++ | 14.2.0（MSYS2 UCRT64，Windows 11），`-std=c++17 -Wall -Wextra -O2`，目前零警告 |
| make | GNU Make 4.4.1。**這台 Windows 機器沒有 `make` 指令，要用 `mingw32-make`**（用法相同）；CI 在 ubuntu-latest 用 `make` |
| Node.js | 26（CI 使用 22）；只用於 JS 測試，網頁本身不需要建置 |
| Emscripten | **未使用** |

```bash
make test       # C++ 測試：107 個案例／556 個斷言
make check      # 禁用內建容器檢查（C++ 與 JS）
make test-js    # JS 測試：37 個案例（含與 data/sample.txt 逐位元組比對）
make cli        # build/auction_cli（Windows 為 .exe）
make bench      # 約 15 秒；覆寫 docs/benchmark.md（數字隨機器而異）
make sample     # 重新產生 data/sample.txt（改了範例資料後必須執行，否則 test_sample_file_matches_builtin_sample 會失敗）
make web        # 部署前檢查
python -m http.server -d web 8000   # 本機預覽網頁（需透過 http，不能 file://）
```

## 5. 資料流與主要 API

### 資料結構的分工（AuctionSystem 持有）

- `HashTable<int, Item> items_`：商品 ID → 商品（節點配置，位址穩定，所以可以保存 `Item*`）。
- `HashTable<string, User> users_`：使用者名稱 → 使用者。
- `BST<ItemKey, int> endTimeTree_`：**只存進行中商品**，key = (endTime, itemId)；結標時 `popMin` 取出。
- `DynArray<int> itemOrder_`、`DynArray<string> userNames_`：刊登／註冊順序。
- 每個 `Item`：`MaxHeap<Bid> bidHeap`（**只含有效出價**）＋ `LinkedList<Bid> bidHistory`（全部出價，新的在頭，含 `active=false` 的已撤標）。
- `MaxHeap` 內部另有 `HashTable<int,int> indexOfId_`（bidId → 陣列位置），每次交換都要同步更新。

### AuctionSystem 公開 API（C++；JS 版方法名相同，差異見下）

所有可能失敗的操作回傳 `Result {ok, message(中文), id}`，不丟例外。

| 功能 | 方法 |
|---|---|
| F1 | `registerUser(name)`、`userExists`、`findUser`、`userNames()` |
| F2/F3/F4 | `listItem(seller,title,desc,startPrice,minIncrement,durationMinutes)`、`findItem(id)`、`searchItems(keyword, openOnly)`、`allItemIds()`、`currentPrice(item)`、`minNextBid(item)` |
| F5–F8 | `placeBid(bidder,itemId,amount)`、`cancelBid(bidder,itemId)`、`highestBid(itemId)`、`bidHistory(itemId)` |
| F9/F10 | `upcomingItems(maxCount)`、`itemsEndingBetween(t1,t2)` |
| F11/F14 | `now()`、`advanceTime(minutes)`、`advanceToNextEnd()`、`lastClosedItems()` |
| F12 | `sortedItems(SortField, ascending, openOnly)`（`SORT_BY_PRICE/BID_COUNT/END_TIME`；穩定） |
| F15/F16 | `saveToString()`、`loadFromString(text)`（失敗不破壞現有資料）、`saveToFile/loadFromFile`（僅 C++）、`reset()`、`loadSampleData()` |
| 檢視 | C++：`itemTableBucketCount/LoadFactor/MaxChain`、`endTimeTreeHeight/Size`；JS：`itemTable()`、`userTable()`、`endTimeTree()` 直接回傳結構供畫圖 |

**C++ vs JS 的刻意差異**：C++ 回傳指標（找不到 `nullptr`）、JS 回傳物件或 `null`；C++ 用輸出參數、JS 回傳 `{key,value}`；BST/HashTable 在 JS 需傳入比較／雜湊函式；JS 多了 `BST.layout()`、`HashTable.forEachInBucket()` 給檢視面板用；JS 沒有檔案讀寫（網頁用 localStorage）；JS 的字串搜尋用手寫迴圈（不用 `includes`）。

### 存檔格式（兩版共用，見 docs/architecture.md 第 5 節）

文字、一行一筆、`|` 分隔；`%`、`|`、換行跳脫為 `%25 %7C %0A`；出價依 bidId 排序以便「重播」還原 Heap 與歷史。**修改格式時兩邊的 `saveToString/_loadInto` 與 `data/sample.txt` 必須一起改。**

### 關鍵業務規則

- 出價：商品須 OPEN 且 `now < endTime`；賣家不能出價；無人出價時 ≥ 起標價，否則 ≥ 最高價 + 最小加價。
- Heap 比較：金額高者優先；同價**時間早者優先**（`Bid::hasHigherPriority`）。
- 撤標：只能撤自己「最新一筆有效出價」（歷史由新到舊第一個 `active && bidder==me`），商品須仍 OPEN；`removeById` + 標記 `active=false`。
- 結標：`advanceTime` 後反覆 `peekMin/popMin`，`endTime <= now` 的商品結標；堆頂＝得標者，無有效出價＝流標（winner 空、finalPrice 0）。

## 6. 已知限制與未完成項目

- **無未完成項目**（PLAN.md 第 9 節 31 項全部勾選）。
- **BST 未自平衡**：依序插入會退化成 O(n)（benchmark 有量化）；改進方向 AVL／紅黑樹（刻意不做，報告中討論）。
- **關鍵字搜尋是 O(n·m) 線性掃描**（刻意取捨）。
- 撤標找自己最新出價是 O(m) 掃描歷史。
- 網頁使用單一 `localStorage` 存檔（key `bidding-system:save:v1`、`bidding-system:user:v1`），沒有後端、沒有多人同步；清除瀏覽器資料會回到範例資料。
- 沒有帳號驗證：「使用者」只是名稱，任何人可以切換成任何人（課程 Demo 範圍）。
- JS 版數字解析限 15 位數（JS 安全整數範圍）；C++ 為 18 位數。
- DynArray 越界在 C++ 是 `abort()`、在 JS 是丟出 Error（屬於程式錯誤而非使用者錯誤；使用者輸入錯誤一律走 `Result`）。
- `bench` 的數字隨機器與負載而異；`docs/benchmark.md` 是在開發機上的一次量測。
- 本機用 Windows 開發，`.gitattributes` 設 `* text=auto`、`*.sh`/`Makefile` 強制 LF；存檔解析會忽略 `\r`。

## 7. 如何新增一個功能（範例：「依賣家篩選商品」）

1. **C++ 核心**：在 `core/AuctionSystem.h` 宣告 `DynArray<int> itemsBySeller(const std::string& seller) const;`（上方註解複雜度），在 `AuctionSystem.cpp` 實作——可直接走訪 `users_.find(seller)->sellingItems`（O(k)）。維持「不用 STL 容器」。
2. **C++ 測試**：在 `tests/test_auction.cpp` 新增 `test_items_by_seller()`，並在檔尾 `run_auction_tests()` 加 `RUN_TEST(...)`。執行 `make test && make check`。
3. **JS 移植**：在 `web/js/core/AuctionSystem.js` 新增同名方法（同樣的註解與複雜度）；不要用 `filter/map` 等陣列方法（`make check` 會擋）。
4. **JS 測試**：在 `tests/js/test_core.mjs` 新增對應 `test(...)`。執行 `make test-js`。
5. **介面**：
   - CLI：在 `cli/main.cpp` 新增 `actXxx()` 與選單項目、`main` 的 `else if`。
   - 網頁：在 `web/app.js` 的 `OPS` 加一筆（名稱、資料結構、複雜度，會顯示在底部操作列）、在對應的 render 函式加畫面、用 `perform(OPS.xxx, () => ..., mutates)` 包住操作。
6. 若有**改動存檔格式或範例資料**：同步改兩邊的存讀檔，執行 `make sample` 重新產生 `data/sample.txt`，再跑 `make test` 與 `make test-js`（逐位元組比對會失敗提醒你）。
7. 更新 `docs/complexity.md`、README 功能表；commit（`phaseN: …` 或描述性訊息）並 push — Actions 會自動測試並部署。

## 8. 測試策略（改動時請維持）

- **隨機測試對暴力解**：Heap（push/pop/removeById 1000 步 × 5 組種子）、BST（insert/remove/range 1000 步 × 5）、AuctionSystem（隨機刊登/出價/撤標/快轉 1000 步 × 3，檢查「堆頂 = 歷史有效出價最優者」「進行中商品數 = BST 節點數」「存讀檔往返相同」）。C++ 與 JS 各一套。
- **突變檢查**：開發時曾刻意破壞程式（反轉同價規則、撤標不更新 Heap、結標用 `<` 取代 `<=`、漏掉最小加價、BST 兩子節點刪除不搬 value、MergeSort 不穩定），確認測試會失敗；新增測試時建議同樣驗證「測試真的抓得到錯」。
- **跨語言一致**：見第 2 節。

## 9. 部署

- Pages 來源為 **GitHub Actions**（Repo Settings → Pages，已設定，不要改成從分支部署）。
- `deploy.yml`：job `test`（`make test`、`make check`、`make test-js`、`bash scripts/build_web.sh`）通過後，job `deploy` 上傳 `web/` 並用 `actions/deploy-pages` 發布。測試失敗就不會部署。
- 網站路徑有 `/bidding-system/` 子路徑，所以頁面內資源一律用**相對路徑**（`build_web.sh` 會檢查 `index.html` 沒有以 `/` 開頭的路徑）。
