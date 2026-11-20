# 二手交易競標平台 — 實作計畫書（給 Claude Code 執行）

> 課程：資料結構（電機四技二甲，許協誌老師）期末專題，題目 #11「二手交易競標平台」
> 開發者：趙加銘（單人完成）
> 目標：2026/11/22 前完成可運行系統；正式繳交為報告前一週

---

## 0. 給 Claude Code 的執行規則（請先讀）

**資料夾配置（已由使用者分配好，請遵守）**

```
D:\claude-practice\data-structures\
├── project\      ← 專案根目錄（本 PLAN.md 所在處），所有程式與 docs 都建在這裡
├── references\   ← 老師的課程簡報，只讀，不要修改
└── report\       ← 口頭報告簡報，使用者另行製作，Claude Code 不要動
```

**GitHub 部署資訊（使用者已設定完成）**

| 項目 | 值 |
|---|---|
| GitHub 帳號 | `zhao-926` |
| Repo | `https://github.com/zhao-926/bidding-system`（Public，目前為空 repo） |
| Git remote | `https://github.com/zhao-926/bidding-system.git`（HTTPS） |
| 預設分支 | `main` |
| GitHub Pages 來源 | **GitHub Actions**（已在 Settings → Pages 設定好，不要改成從分支部署） |
| 線上網址 | `https://zhao-926.github.io/bidding-system/` |

- Phase 1 開始時就 `git init -b main` 並設定 remote；每個 Phase commit 後 `git push origin main`。
- 部署 workflow 在 Phase 6 撰寫（見 Phase 6），在那之前 push 不會產生網站，屬正常現象。
- 第一次 push 若跳出 GitHub 登入，停下來請使用者完成登入，不要自行處理帳密或 token。

- 所有檔案直接建在 `project\` 底下，**不要**再多包一層子資料夾。
- **不需製作口頭報告簡報**；`docs/` 的文件會作為簡報素材，由使用者在 `report\` 另行處理。

1. **依 Phase 順序做**，每個 Phase 完成後跑該 Phase 的驗收指令，全部通過才進下一個 Phase，並在第 9 節的規劃表把對應項目打勾 `[x]`。
2. **核心資料結構必須自行實作**：`core/` 目錄內**禁止**使用 STL 容器
   （`vector`、`list`、`forward_list`、`deque`、`array`、`map`、`set`、`unordered_map`、`unordered_set`、`multimap`、`stack`、`queue`、`priority_queue`），也禁止 `<algorithm>` 的 `sort`。
   允許：`<string>`、`<cstdint>`、`<cstring>`、`<iostream>`、`<fstream>`、`<sstream>`、`<chrono>`、`new/delete`。
   STL **只能**出現在 `bench/`（當效能對照組）。
3. 每個資料結構的每個公開操作，都要在**函式上方註解標明時間複雜度**，例如 `// O(1) average, O(n) worst`。
4. 程式碼要寫得讓學生本人能看懂並口頭說明：變數命名清楚、關鍵步驟有中文註解，不要用過度炫技的模板技巧。
5. 每個 Phase 結束做一次 git commit（訊息格式：`phaseN: 說明`）。
6. 全部完成後必須產出 `HANDOFF.md`（見 Phase 7），讓其他 AI 能快速接手。

---

## 1. 專題規範摘要（來自老師簡報）

| 規範 | 本專題做法 |
|---|---|
| 運用學期所學資料結構設計可運行系統 | 競標平台：刊登、搜尋、出價、撤標、結標 |
| 至少 3–4 種核心資料結構自行實作，不可用內建容器 | 自行實作 **5 種**：Hash Table、Max-Heap（Priority Queue）、Linked List、BST、Merge Sort |
| 內建容器只能用於效能對照組 | `bench/` 用 STL 對照並輸出比較表 |
| 每個操作標明時間複雜度 | 程式註解＋`docs/complexity.md` |
| 繳交：原始碼＋README（含編譯/執行方式） | `README.md` |
| 系統架構圖（模組分工與資料流） | `docs/architecture.md`（Mermaid） |
| 資料結構選用與時間複雜度分析報告 | `docs/complexity.md`＋benchmark 結果 |
| 口頭報告／Demo（建議錄影） | GitHub Pages 網頁 Demo＋`docs/demo-script.md` 錄影腳本 |
| 分工說明表 | 單人完成，`docs/contribution.md` 一頁說明即可 |

老師指定的核心資料結構對應：

| 資料結構 | 用途 |
|---|---|
| Hash Table | 商品 ID → 商品、使用者名稱 → 使用者 |
| Priority Queue（Max-Heap） | 每件商品維護目前最高出價 |
| Linked List | 每件商品的出價歷史紀錄 |
| BST（加分） | 依結標時間排序商品，查「即將結標」、範圍查詢 |
| Merge Sort（加分，排序演算法） | 商品列表依價格／出價數排序 |

---

## 2. 技術架構

- **核心邏輯**：C++17，放在 `core/`，不依賴任何 I/O 以外的函式庫。
- **CLI 版本**：`g++` 編譯，用於測試、benchmark、終端機 Demo。
- **網頁版本（2026-11 決定：走 JavaScript 版，不使用 Emscripten）**：把 `core/` 用 JavaScript **一比一移植**到 `web/js/core/`，前端用純 HTML/CSS/JavaScript（ES Module，不需任何建置工具），部署到 **GitHub Pages**（純靜態、免費）。
  - JS 版同樣自刻所有資料結構，**不准用** `Map`/`Set`/`Array.prototype.sort` 等內建容器與陣列方法，陣列只能當「固定大小的底層記憶體」（`new Array(n)` + 索引存取）；`scripts/check_no_stl.sh` 會一併檢查。
  - 為確保兩個版本行為一致：JS 版的存檔格式與 C++ 版完全相同，且 JS 測試會驗證「JS 載入範例資料後的存檔」與 C++ 產生的 `data/sample.txt` **逐位元組相同**。
- **資料保存**：網頁版用 `localStorage` 存 JSON 快照（序列化／反序列化寫在 C++ 端或 JS 端皆可）；CLI 版讀 `data/sample.txt` 初始資料。
- **時間**：使用「模擬時鐘」（系統內部的 `now`，可快轉），Demo 時能立刻看到結標，不用真的等。

### 目錄結構

```
project/                   # = D:\claude-practice\data-structures\project（專案根目錄）
├── core/
│   ├── HashTable.h        # 自刻雜湊表（separate chaining）
│   ├── MaxHeap.h          # 自刻最大堆積（Priority Queue）
│   ├── LinkedList.h       # 自刻單向鏈結串列
│   ├── BST.h              # 自刻二元搜尋樹
│   ├── DynArray.h         # 自刻動態陣列（給 Heap 底層與一般列表用）
│   ├── MergeSort.h        # 自刻合併排序
│   ├── Models.h           # User / Item / Bid 結構
│   ├── AuctionSystem.h    # 系統邏輯（整合以上結構）
│   └── AuctionSystem.cpp
├── cli/
│   └── main.cpp           # 終端機選單介面
├── web/
│   ├── index.html
│   ├── style.css
│   ├── app.js             # 介面邏輯（呼叫 js/core）
│   └── js/core/           # core/ 的 JavaScript 一比一移植（DynArray、HashTable、…、AuctionSystem）
├── tests/
│   ├── test_main.cpp      # 自寫極簡測試框架（ASSERT 巨集）
│   └── ...
├── bench/
│   └── bench.cpp          # 自刻 vs STL 效能比較
├── data/
│   └── sample.txt
├── scripts/
│   ├── check_no_stl.sh    # 檢查 core/ 沒用到 STL 容器
│   └── build_web.sh
├── docs/
│   ├── architecture.md
│   ├── complexity.md
│   ├── demo-script.md
│   └── contribution.md
├── Makefile
├── README.md
└── HANDOFF.md
```

---

## 3. 資料模型

```cpp
struct Bid {
    int bidId;
    int itemId;
    std::string bidder;
    long long amount;      // 以「元」為單位的整數，避免浮點誤差
    long long time;        // 模擬時鐘時間
    bool active;           // 撤標後設為 false（歷史保留、heap 移除）
};

struct Item {
    int itemId;
    std::string title;
    std::string seller;
    std::string description;
    long long startPrice;
    long long minIncrement;  // 最小加價幅度
    long long endTime;
    enum Status { OPEN, CLOSED } status;
    std::string winner;      // 結標後填入，流標為空
    long long finalPrice;
    MaxHeap<Bid> bidHeap;          // 有效出價，堆頂 = 最高價
    LinkedList<Bid> bidHistory;    // 全部出價紀錄（含撤標），新的插在頭
};

struct User {
    std::string username;
    DynArray<int> sellingItems;
    DynArray<int> biddingItems;
};
```

**Heap 比較規則**：金額高者優先；金額相同時，**時間早者優先**（先出價者勝）。

---

## 4. 功能需求

| 編號 | 功能 | 使用的資料結構 | 規則 |
|---|---|---|---|
| F1 | 註冊／切換使用者 | Hash Table | 使用者名稱不可重複 |
| F2 | 刊登商品 | Hash Table、BST | 填標題、描述、起標價、最小加價、拍賣時長；自動配發 itemId |
| F3 | 依 ID 查商品 | Hash Table | O(1) 平均 |
| F4 | 關鍵字搜尋商品 | DynArray 線性掃描 | 標題包含關鍵字（說明：這是 O(n)，報告中當作取捨案例討論） |
| F5 | 出價 | Max-Heap、Linked List | 商品須為 OPEN、未過結標時間；賣家不能出價；第一筆 ≥ 起標價；之後 ≥ 目前最高價 + 最小加價 |
| F6 | 查目前最高出價 | Max-Heap | 看堆頂，O(1) |
| F7 | 查出價歷史 | Linked List | 由新到舊列出，含已撤標（標示） |
| F8 | 撤回出價 | Max-Heap（任意刪除）、Linked List | 只能撤自己的最新一筆有效出價、商品仍 OPEN；heap 需支援依 bidId 刪除（維護 bidId→heap 索引，用 Hash Table） |
| F9 | 即將結標列表 | BST | 中序走訪，依結標時間由近到遠 |
| F10 | 時段範圍查詢 | BST | 列出結標時間在 [t1, t2] 的商品 |
| F11 | 結標 | BST、Max-Heap | 推進模擬時鐘時，把 endTime ≤ now 的商品從 BST 取出並結標：堆頂為得標者，無出價則流標 |
| F12 | 商品列表排序 | Merge Sort | 依目前價格、出價次數、結標時間排序（升／降冪） |
| F13 | 我的頁面 | DynArray | 我賣的、我出價的、我得標的 |
| F14 | 模擬時鐘快轉 | — | +1 分鐘／+1 小時／直接跳到下一個結標時間 |
| F15 | 存檔／讀檔 | — | 網頁：localStorage；CLI：文字檔 |
| F16 | 重設為範例資料 | — | 載入約 20 件商品、5 位使用者、數十筆出價 |

所有錯誤（出價太低、已結標、找不到商品…）要回傳**明確的中文錯誤訊息**，網頁顯示在畫面上，不可 crash。

---

## 5. 資料結構實作規格與時間複雜度

| 結構 | 操作 | 複雜度 | 實作重點 |
|---|---|---|---|
| **HashTable<K,V>** | insert / find / remove | 平均 O(1)，最差 O(n) | separate chaining；字串用 djb2、整數用乘法雜湊；load factor > 0.75 時 rehash（容量翻倍，O(n) 但攤銷 O(1)） |
| **MaxHeap<T>** | push | O(log n) | 底層用 DynArray；sift-up |
| | top | O(1) | |
| | pop | O(log n) | sift-down |
| | removeById | O(log n) | 用 HashTable 記 bidId→索引；與最後一個交換後 sift-up/down，並同步更新索引 |
| **LinkedList<T>** | pushFront | O(1) | 單向串列，新出價插頭 |
| | traverse | O(n) | 提供 forEach(callback) |
| | find | O(n) | |
| | size | O(1) | 維護計數 |
| **BST<K,V>** | insert / remove / find | 平均 O(log n)，最差 O(n) | key = (endTime, itemId) 組合避免重複；remove 處理 0/1/2 子節點三種情況 |
| | inorder | O(n) | 即將結標列表 |
| | rangeQuery | O(log n + k) | 剪枝走訪 |
| | popMin | O(h) | 結標時反覆取最小 |
| **DynArray<T>** | pushBack | 攤銷 O(1) | 容量翻倍 |
| | get / set | O(1) | 含邊界檢查 |
| | removeAt | O(n) | |
| **MergeSort** | sort | O(n log n)，穩定 | 接受比較函式 |

`docs/complexity.md` 要包含：上表、每個結構「為什麼選它」的說明（例如：為何最高出價用 Heap 而不是每次排序、為何歷史用 Linked List、BST 最差情況與改善方向如 AVL），以及 benchmark 結果表。

---

## 6. 網頁介面規格

單頁應用，繁體中文介面，手機與電腦都能看。

- **頂列**：目前使用者（下拉切換／新增）、模擬時鐘顯示與快轉按鈕、重設範例資料
- **商品列表**：卡片顯示標題、目前最高價、出價數、剩餘時間、狀態；搜尋框、排序選單
- **即將結標**側欄（BST 中序結果）
- **商品詳情**：出價表單（顯示最低可出價金額）、最高出價者、出價歷史（Linked List）、撤標按鈕
- **刊登商品**表單
- **我的頁面**
- **「資料結構檢視」面板**（Demo 加分用）：
  - Heap：以樹狀圖顯示選定商品的出價堆積
  - Hash Table：顯示各 bucket 的鏈長度、load factor
  - BST：顯示樹狀結構與高度
  - 每次操作後顯示「本次操作：XXX，複雜度 O(...)」

---

## 7. 開發階段（Phase）

### Phase 1 — 專案骨架與基礎結構
- 建立目錄、Makefile（targets：`cli`、`test`、`bench`、`web`、`check`、`clean`）
- 實作 `DynArray`、`LinkedList`、`HashTable`
- 自寫測試框架（`ASSERT_EQ`、`ASSERT_TRUE`、統計通過／失敗數）
- 撰寫 `scripts/check_no_stl.sh`（grep `core/` 中被禁止的標頭與 `std::` 容器名稱，找到就 exit 1）
- **驗收**：`make test` 全過（每個結構至少 8 個測試案例，含邊界：空結構、刪除不存在 key、rehash 後資料仍正確）；`make check` 通過

### Phase 2 — MaxHeap 與 BST
- 實作 `MaxHeap`（含 removeById）、`BST`（含 remove 三種情況、rangeQuery、popMin）、`MergeSort`
- **驗收**：`make test` 全過；包含隨機測試（隨機插入／刪除 1000 次，與暴力解比對結果）；`make check` 通過

### Phase 3 — AuctionSystem 業務邏輯
- 實作 F1–F16 的核心邏輯（不含 UI）
- 出價驗證規則全部覆蓋
- **驗收**：業務邏輯測試全過，至少涵蓋：正常出價、低於最低出價、賣家自己出價、結標後出價、同價先到者勝、撤標後最高價回到前一位、無人出價流標、範圍查詢

### Phase 4 — CLI 介面與範例資料
- `cli/main.cpp` 數字選單操作全部功能
- `data/sample.txt`＋讀檔
- **驗收**：`make cli && ./build/auction_cli` 能完整跑一次「刊登 → 多人出價 → 撤標 → 快轉 → 結標」流程

### Phase 5 — 效能對照組
- `bench/bench.cpp`：n = 1,000 / 10,000 / 100,000
  - HashTable vs `std::unordered_map`（insert、find）
  - MaxHeap vs `std::priority_queue`（push、pop）
  - BST vs `std::map`（insert、find；另測「依序插入」的退化情況）
  - MergeSort vs `std::sort`
- 輸出 Markdown 表格到 `docs/benchmark.md`，並附 1–2 句解讀
- **驗收**：`make bench` 產出表格；`make check` 仍通過（bench 不在 core/ 內）

### Phase 6 — 網頁（JavaScript 移植版）與部署
- 把 `core/` 一比一移植到 `web/js/core/`（JS 版，不使用 Emscripten）；`tests/js/` 以 Node 驗證移植正確（含與 `data/sample.txt` 逐位元組比對）
- `scripts/build_web.sh`：驗證 JS 核心與網頁檔案齊全（無編譯步驟，`web/` 本身就是要部署的內容）
- 完成第 6 節介面與資料結構檢視面板
- 部署到 GitHub Pages：
  - Repo、remote、網址見第 0 節「GitHub 部署資訊」；Pages 來源已設為 GitHub Actions
  - 撰寫 `.github/workflows/deploy.yml`：push 到 `main` 時，先跑 C++ 與 JS 測試 → 用 `actions/upload-pages-artifact` 上傳 `web/` → `actions/deploy-pages` 發布（不需要 Emscripten）
  - 撰寫 `.gitignore`（`build/`、`*.o`、`*.exe`、編輯器暫存檔）
  - 網頁內所有資源路徑一律用相對路徑（因為網址有 `/bidding-system/` 子路徑）
  - 若 push 時要求登入，停下來請使用者完成 GitHub 驗證，不要自行處理帳密
- **驗收**：本機 `python -m http.server` 開 `web/` 可完整操作；重新整理後資料仍在（localStorage）；GitHub Actions 綠勾；`https://zhao-926.github.io/bidding-system/` 可開啟並正常操作
- 網頁版本採用 JS 移植（已不使用 Emscripten），HANDOFF.md 需註明這個決定

### Phase 7 — 文件與交接
- `README.md`：專案簡介、功能列表、編譯／執行方式（CLI、測試、bench、web）、線上 Demo 網址、截圖
- `docs/architecture.md`：Mermaid 架構圖（UI → bindings → AuctionSystem → 各資料結構）＋「出價」與「結標」兩條資料流程圖
- `docs/complexity.md`：見第 5 節
- `docs/demo-script.md`：3–5 分鐘錄影腳本（每一步要操作什麼、要講哪個資料結構與複雜度）
- `docs/contribution.md`：單人完成說明，列出每個模組
- **`HANDOFF.md`**（必做）：給其他 AI 的交接文件，內容包含：
  - 專案目的與課程規範限制（尤其「core/ 不可用 STL」）
  - 目錄與每個檔案的職責
  - 建置環境與指令（g++ 版本、Node 版本）
  - 資料流與主要 API 清單
  - 已知限制、備案是否啟用、未完成項目
  - 如何新增一個功能（步驟範例）
- **驗收**：照 README 在乾淨環境能編譯並執行；規劃表全部打勾

---

## 8. 範例資料要求

- 5 位使用者（例如 alice、bob、carol、dave、eve）
- 約 20 件二手商品（教科書、耳機、機械鍵盤、腳踏車、顯示器…），起標價 50–5000 元，結標時間錯開
- 部分商品已有 3–8 筆出價，至少 1 件含撤標紀錄，至少 1 件無人出價（Demo 流標）

---

## 9. 規劃表（完成後兼功能檢查表）

| # | 項目 | 對應 Phase | 驗收方式 | 完成 |
|---|---|---|---|---|
| 1 | 專案骨架、Makefile | 1 | `make` 各 target 可執行 | [x] |
| 2 | DynArray | 1 | 測試通過 | [x] |
| 3 | LinkedList | 1 | 測試通過 | [x] |
| 4 | HashTable（含 rehash） | 1 | 測試通過 | [x] |
| 5 | 不用 STL 檢查腳本 | 1 | `make check` 通過 | [x] |
| 6 | MaxHeap（含 removeById） | 2 | 隨機測試通過 | [x] |
| 7 | BST（含 remove、rangeQuery、popMin） | 2 | 隨機測試通過 | [x] |
| 8 | MergeSort | 2 | 測試通過、穩定性測試 | [x] |
| 9 | F1 註冊／切換使用者 | 3 | 業務測試 | [x] |
| 10 | F2 刊登商品 | 3 | 業務測試 | [x] |
| 11 | F3 ID 查詢、F4 關鍵字搜尋 | 3 | 業務測試 | [x] |
| 12 | F5 出價（全部驗證規則） | 3 | 業務測試 | [x] |
| 13 | F6 最高出價、F7 出價歷史 | 3 | 業務測試 | [x] |
| 14 | F8 撤標 | 3 | 業務測試 | [x] |
| 15 | F9 即將結標、F10 範圍查詢 | 3 | 業務測試 | [x] |
| 16 | F11 結標／流標 | 3 | 業務測試 | [x] |
| 17 | F12 排序 | 3 | 業務測試 | [x] |
| 18 | F13 我的頁面、F14 模擬時鐘 | 3 | 業務測試 | [x] |
| 19 | F15 存讀檔、F16 範例資料 | 3–4 | 重啟後資料正確 | [x] |
| 20 | CLI 完整流程 | 4 | 手動跑完流程 | [x] |
| 21 | Benchmark 與對照表 | 5 | `docs/benchmark.md` 產出 | [x] |
| 22 | JS 核心移植（取代 WASM） | 6 | JS 測試通過、與 `data/sample.txt` 逐位元組相同 | [x] |
| 23 | 網頁主要功能 | 6 | 瀏覽器手動測試 | [x] |
| 24 | 資料結構檢視面板 | 6 | Heap/Hash/BST 視覺化正確 | [x] |
| 25 | GitHub Pages 部署 | 6 | 線上網址可開 | [x] |
| 26 | README | 7 | 照步驟可編譯執行 | [ ] |
| 27 | 架構圖 architecture.md | 7 | Mermaid 可渲染 | [ ] |
| 28 | 複雜度分析 complexity.md | 7 | 每個操作都有標 | [ ] |
| 29 | Demo 錄影腳本 | 7 | 3–5 分鐘流程完整 | [ ] |
| 30 | 分工說明 contribution.md | 7 | 存在 | [ ] |
| 31 | HANDOFF.md | 7 | 其他 AI 可依此接手 | [ ] |

---

## 10. 給 Claude Code 的第一句指令範例

```
請閱讀 PLAN.md，從 Phase 1 開始依序實作。每個 Phase 完成後執行驗收指令、
在第 9 節規劃表打勾並 git commit，再回報結果後繼續下一個 Phase。
core/ 內嚴禁使用 STL 容器，遇到需要取捨時以「學生能口頭說明」為優先。
```
