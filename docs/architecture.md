# 系統架構

> 圖表使用 [Mermaid](https://mermaid.js.org/)，可直接在 GitHub 上渲染。

## 1. 模組分層

使用者介面（CLI 或網頁）只負責輸入／輸出；所有規則都在 `AuctionSystem`，由它組合各個自刻資料結構。
**C++ 版與 JavaScript 版是同一份設計的兩個實作**，介面與行為一致（以存檔格式逐位元組比對驗證）。

```mermaid
flowchart TB
    subgraph UI["使用者介面（只做輸入輸出）"]
        CLI["CLI<br/>cli/main.cpp"]
        WEB["網頁<br/>web/app.js + index.html"]
    end

    subgraph CPP["C++ 核心 core/"]
        AS_C["AuctionSystem.cpp<br/>業務邏輯 F1–F16"]
    end

    subgraph JS["JavaScript 核心 web/js/core/（一比一移植）"]
        AS_J["AuctionSystem.js<br/>業務邏輯 F1–F16"]
    end

    CLI --> AS_C
    WEB --> AS_J

    AS_C --- DS
    AS_J --- DS

    subgraph DS["自刻資料結構（兩個版本各一份，介面相同）"]
        HT["HashTable<br/>商品/使用者/bidId→位置"]
        HP["MaxHeap<br/>最高出價（含 removeById）"]
        LL["LinkedList<br/>出價歷史"]
        BT["BST<br/>(結標時間, ID) 排序"]
        MS["MergeSort<br/>商品排序"]
        DA["DynArray<br/>底層陣列與清單"]
    end

    HP --> DA
    HP --> HT
    BT --> DA
    MS --> DA

    LS[("localStorage<br/>文字存檔")]
    FILE[("data/*.txt<br/>文字存檔")]
    WEB <--> LS
    CLI <--> FILE
    FILE -.- LS
```

## 2. AuctionSystem 內部持有的資料結構

```mermaid
flowchart LR
    AS["AuctionSystem"]
    AS --> ITEMS["HashTable&lt;int, Item&gt;<br/>商品 ID → 商品"]
    AS --> USERS["HashTable&lt;string, User&gt;<br/>使用者名稱 → 使用者"]
    AS --> TREE["BST&lt;ItemKey, int&gt;<br/>進行中商品（依結標時間）"]
    AS --> ORDER["DynArray&lt;int&gt;<br/>刊登順序"]

    ITEMS --> ITEM["Item"]
    ITEM --> HEAP["MaxHeap&lt;Bid&gt;<br/>有效出價，堆頂 = 最高價"]
    ITEM --> HIST["LinkedList&lt;Bid&gt;<br/>全部出價（含已撤標），新的在最前面"]
    HEAP --> IDX["HashTable&lt;bidId, 位置&gt;<br/>讓撤標能 O(1) 找到位置"]

    USERS --> USER["User"]
    USER --> UARR["DynArray&lt;int&gt; × 3<br/>我賣的 / 我出價的 / 我得標的"]
```

## 3. 資料流程：出價（F5）

```mermaid
flowchart TD
    A["使用者按下「出價」<br/>placeBid(bidder, itemId, amount)"] --> B{"bidder 存在？<br/>HashTable.find O(1)"}
    B -- 否 --> E1["回傳錯誤：找不到使用者"]
    B -- 是 --> C{"商品存在？<br/>HashTable.find O(1)"}
    C -- 否 --> E2["回傳錯誤：找不到商品"]
    C -- 是 --> D{"狀態 OPEN 且<br/>now &lt; endTime？"}
    D -- 否 --> E3["回傳錯誤：已結標"]
    D -- 是 --> F{"bidder ≠ 賣家？"}
    F -- 否 --> E4["回傳錯誤：賣家不能出價"]
    F -- 是 --> G{"amount ≥ 最低出價？<br/>無人出價 = 起標價<br/>否則 = 堆頂 + 最小加價（堆頂 O(1)）"}
    G -- 否 --> E5["回傳錯誤：出價太低（附最低可出價金額）"]
    G -- 是 --> H["建立 Bid（bidId, time = now）"]
    H --> I["MaxHeap.push：放到尾端後 sift-up<br/>O(log n)；同步更新 bidId→位置"]
    I --> J["LinkedList.pushFront：加入歷史<br/>O(1)"]
    J --> K["DynArray：把商品記到使用者的「出價過」清單（不重複）"]
    K --> L["回傳成功（含 bidId）"]
```

**撤標（F8）** 是出價的反向：在歷史（由新到舊）中找到該使用者最新的有效出價 → 用 `bidId→位置` 的 HashTable 找到它在 Heap 的位置 → `removeById`（與最後一個元素交換、sift-up 或 sift-down，O(log n)）→ 把歷史中那筆標記為「已撤標」（歷史保留）。

## 4. 資料流程：結標（F11）

模擬時鐘每次前進（`advanceTime`），就讓所有「結標時間 ≤ 現在」的商品結標。BST 以 (結標時間, ID) 排序，所以最早到期的永遠是最小的節點。

```mermaid
flowchart TD
    A["advanceTime(minutes)<br/>now += minutes"] --> B["BST.peekMin<br/>看最早結標的商品 O(log n)"]
    B --> C{"有商品且<br/>endTime ≤ now？"}
    C -- 否 --> Z["結束：回報本次結標了幾件"]
    C -- 是 --> D["BST.popMin<br/>取出並刪除 O(log n)"]
    D --> E["HashTable.find 取得商品 O(1)"]
    E --> F{"Heap 有有效出價？<br/>（看堆頂 O(1)）"}
    F -- 是 --> G["得標者 = 堆頂的出價者<br/>得標價 = 堆頂金額<br/>加入得標者的「我得標的」清單"]
    F -- 否 --> H["流標：winner 為空、得標價 0"]
    G --> I["狀態改為 CLOSED"]
    H --> I
    I --> B
```

## 5. 存檔格式（C++ 與 JS 共用）

文字檔、一行一筆、欄位以 `|` 分隔（標題等文字中的 `%`、`|`、換行會先跳脫成 `%25`、`%7C`、`%0A`）。
出價依 bidId 由小到大存放，讀檔時依序「重播」就能還原 Heap 與出價歷史。
讀檔採「先在暫存系統試解析，成功才取代現有資料」，所以壞檔不會破壞目前資料。

```text
AUCTION_V1
now|60
nextItemId|21
nextBidId|46
user|alice
item|<id>|<賣家>|<標題>|<描述>|<起標價>|<最小加價>|<結標時間>|<狀態 0=進行中 1=已結標>|<得標者>|<得標價>
bid|<bidId>|<itemId>|<出價者>|<金額>|<時間>|<有效 1/0>
```
