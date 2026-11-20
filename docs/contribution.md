# 分工說明

本專題由**趙加銘一人獨立完成**（無組員），以下列出各模組內容供對照。

| 模組 | 檔案 | 內容 | 負責人 |
|---|---|---|---|
| 動態陣列 | `core/DynArray.h`、`web/js/core/DynArray.js` | 容量翻倍、邊界檢查、removeAt | 趙加銘 |
| 鏈結串列 | `core/LinkedList.h`、`web/js/core/LinkedList.js` | 單向串列、pushFront、find、forEach | 趙加銘 |
| 雜湊表 | `core/HashTable.h`、`web/js/core/HashTable.js` | separate chaining、djb2／乘法雜湊、rehash | 趙加銘 |
| 最大堆積 | `core/MaxHeap.h`、`web/js/core/MaxHeap.js` | sift-up／down、依 id 任意刪除（HashTable 記位置） | 趙加銘 |
| 二元搜尋樹 | `core/BST.h`、`web/js/core/BST.js` | insert／remove（三種情況）、rangeQuery、popMin、迴圈式走訪 | 趙加銘 |
| 合併排序 | `core/MergeSort.h`、`web/js/core/MergeSort.js` | 穩定排序、自訂比較函式 | 趙加銘 |
| 業務邏輯 | `core/AuctionSystem.*`、`web/js/core/AuctionSystem.js` | 註冊、刊登、搜尋、出價、撤標、結標、排序、時鐘、存讀檔、範例資料 | 趙加銘 |
| CLI 介面 | `cli/main.cpp` | 14 項功能的數字選單 | 趙加銘 |
| 網頁介面 | `web/index.html`、`style.css`、`app.js` | 商品列表、詳情與出價、刊登、我的頁面、資料結構檢視面板、操作複雜度顯示 | 趙加銘 |
| 測試 | `tests/`、`tests/js/` | C++ 107 個、JS 37 個測試案例（含隨機測試與暴力解比對） | 趙加銘 |
| 效能對照 | `bench/bench.cpp`、`docs/benchmark.md` | 自刻 vs STL，n = 1,000～100,000 | 趙加銘 |
| 檢查與部署 | `scripts/`、`Makefile`、`.github/workflows/deploy.yml` | 禁用內建容器檢查、自動測試與 GitHub Pages 部署 | 趙加銘 |
| 文件 | `README.md`、`docs/`、`HANDOFF.md` | 說明、架構圖、複雜度分析、Demo 腳本、交接文件 | 趙加銘 |

## 開發輔助工具說明

開發過程使用 AI 程式助理（Claude Code）協助撰寫程式碼與文件初稿；資料結構的設計、取捨與規格（課程限制、功能需求、複雜度要求、驗收標準）由開發者在 `PLAN.md` 中定義，並逐階段驗收（測試全數通過、隨機測試與暴力解比對、突變測試確認測試抓得到錯誤）。程式碼的撰寫準則為「開發者能看懂並口頭說明」：變數命名清楚、關鍵步驟有中文註解、不使用過度炫技的語法。
