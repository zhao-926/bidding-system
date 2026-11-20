# 資料結構選用與時間複雜度分析

符號：**n** = 資料筆數（商品數、有效出價數…），**m** = 出價歷史筆數，**k** = 結果筆數，**h** = BST 樹高，**L** = 字串長度。
C++（`core/`）與 JavaScript（`web/js/core/`）兩個版本的複雜度相同，且每個公開函式上方都有同樣的複雜度註解。

## 1. 各資料結構操作的複雜度

| 結構 | 操作 | 複雜度 | 實作重點 |
|---|---|---|---|
| **HashTable<K,V>** | insert / find / remove | 平均 O(1)，最差 O(n) | separate chaining；字串用 djb2、整數用乘法雜湊；負載因子 > 0.75 就 rehash（容量翻倍，單次 O(n)，攤銷 O(1)） |
| | clear / forEach | O(n + bucket 數) | |
| **MaxHeap<T>** | push | O(log n) | 底層用 DynArray；sift-up |
| | top | O(1) | 堆頂 = 最高出價 |
| | pop | O(log n) | 最後一個補到堆頂，sift-down |
| | removeById | O(log n) | 用 HashTable 記 bidId→位置；與最後一個交換後 sift-up 或 sift-down，並同步更新索引 |
| **LinkedList<T>** | pushFront | O(1) | 單向串列，新出價插在頭 |
| | forEach / find | O(n) | 由新到舊走訪 |
| | size | O(1) | 插入時維護計數 |
| **BST<K,V>** | insert / find / remove | 平均 O(log n)，最差 O(n) | key = (結標時間, 商品ID) 避免重複；remove 處理 0／1／2 個子節點；全部以迴圈實作，退化成鏈也不會 stack overflow |
| | inorder | O(n) | 迴圈 + 自己的堆疊（DynArray） |
| | rangeQuery | O(log n + k) | 剪枝：太小的連同左子樹跳過、超過上界就結束 |
| | popMin / peekMin | O(h) | 一路往左走到底 |
| **DynArray<T>** | pushBack | 攤銷 O(1) | 容量翻倍 |
| | get / set | O(1) | 含邊界檢查 |
| | removeAt / contains | O(n) | 後面的元素往前補位／線性搜尋 |
| **mergeSort** | sort | O(n log n)，穩定 | 接受比較函式；額外空間 O(n) |

## 2. 系統功能對應的複雜度

| 功能 | 使用的資料結構 | 複雜度 |
|---|---|---|
| F1 註冊使用者 | HashTable.insert | O(1) 平均 |
| F2 刊登商品 | HashTable.insert + BST.insert | O(log n) 平均 |
| F3 依 ID 查商品 | HashTable.find | O(1) 平均 |
| F4 關鍵字搜尋 | DynArray 線性掃描（每件商品比對標題） | O(n·m)（取捨案例，見下） |
| F5 出價 | MaxHeap.push + LinkedList.pushFront | O(log n) |
| F6 查最高出價 | MaxHeap.top | O(1) |
| F7 查出價歷史 | LinkedList 走訪 | O(m) |
| F8 撤回出價 | LinkedList.find（找自己最新的）+ MaxHeap.removeById | O(m + log n) |
| F9 即將結標列表 | BST.inorder | O(n) |
| F10 時段範圍查詢 | BST.rangeQuery | O(log n + k) |
| F11 結標 | BST.popMin（每件）+ MaxHeap.top | O(k log n)（k 件到期） |
| F12 商品排序 | mergeSort | O(n log n) |
| F13 我的頁面 | HashTable.find + DynArray 走訪 | O(1) 平均 + O(k) |
| F15 存檔 | 走訪全部 + 出價依 bidId 做 mergeSort | O(n log n) |

## 3. 為什麼選這個資料結構？

**最高出價 → Max-Heap，而不是「每次查詢都排序」**
競標的操作模式是「很多次出價、很多次查最高價」。Heap 的 push 是 O(log n)、看最高價是 O(1)；如果每次查最高價都排序一次要 O(n log n)，保持陣列有序則每次插入要 O(n)。再加上撤標需要刪除任意一筆出價，所以我們在 Heap 外另用 HashTable 記錄「出價 → 陣列位置」，讓刪除仍維持 O(log n)（否則要先 O(n) 掃描才找得到）。

**出價歷史 → Linked List**
歷史永遠是「新的加在最前面、由新到舊瀏覽」，pushFront 只要 O(1)，也不用像陣列那樣預留容量或搬資料。代價是不能用索引隨機存取，但這個需求用不到。

**ID／使用者名稱查詢 → Hash Table**
所有操作都要「先用 ID 或名字找到物件」，Hash Table 平均 O(1)，比有序結構的 O(log n) 更快。我們不需要它保持順序（順序由 BST 與 DynArray 各自負責）。

**依結標時間 → BST**
「即將結標」、「時段範圍查詢」、「結標時取出最早到期的」三件事都是「依時間排序」的操作。BST 可以同時做到：插入 O(log n)、取最小 O(h)、範圍查詢 O(log n + k)、中序走訪就是排好的清單。Hash Table 做不到有序操作；排序後的陣列則插入要 O(n)。

**商品列表排序 → Merge Sort**
O(n log n) 且**穩定**：價格相同的商品會維持原本的刊登順序，且先依次要欄位、再依主要欄位排序時不會被打亂（測試中有專門驗證）。代價是需要 O(n) 額外空間。

## 4. 取捨與改進方向

**關鍵字搜尋是 O(n·m) 的線性掃描（F4）。**
我們選擇不為標題另外建索引：商品數量不大，掃描最簡單、也沒有「商品刊登／結標時還要維護索引」的額外成本。若商品數量成長，可以改用倒排索引（關鍵字 → 商品 ID 的 HashTable）或字典樹（Trie）。這是報告中「簡單 vs 效能」的取捨案例。

**BST 最差情況退化成 O(n)。**
本專題的 BST 只存「進行中」的商品，結標後即移除，所以節點數有限；但如果連續刊登相同時長的商品，結標時間會遞增，樹就會變得瘦長。效能對照中，依序插入 100,000 筆時 BST 比 `std::map`（紅黑樹）慢約 1,800 倍（樹高 = n）；隨機順序則與 `std::map` 同量級（見下節）。改進方向是改用**自平衡**的 AVL 樹或紅黑樹，保證 O(log n)。

**Hash Table 的最差情況與 rehash。**
所有 key 碰撞在同一個 bucket 時會退化成 O(n)；我們用負載因子 0.75 的 rehash 讓鏈保持很短（實測最長鏈通常只有 2–3）。單次 rehash 是 O(n)，但容量每次翻倍，所以攤銷後每次插入仍是 O(1)。

**撤標的 O(m)。**
找「自己最新的有效出價」需要由新到舊掃描歷史（O(m)）；實際上通常在前幾筆就找到。若 m 很大，可以在 User 裡記錄「每件商品的最新有效出價 ID」。

## 5. 效能對照摘要（自刻 vs STL）

完整數據見 [`benchmark.md`](benchmark.md)（`make bench` 產生；g++ 14.2.0、`-O2`，n = 100,000，單位 ms，自刻 / STL < 1 代表自刻較快）。

| 項目 | 自刻 | STL | 自刻 / STL | 說明 |
|---|---:|---:|---:|---|
| HashTable vs unordered_map：insert | 6.198 | 6.820 | 0.91x | 同為平均 O(1)，差在常數因子 |
| HashTable vs unordered_map：find | 0.697 | 0.825 | 0.85x | |
| MaxHeap vs priority_queue：push | 10.859 | 2.398 | 4.53x | 自刻版要同步維護 bidId→位置，換來 O(log n) 的 removeById（STL 沒有） |
| MaxHeap vs priority_queue：pop | 20.334 | 5.687 | 3.58x | 同上 |
| BST vs map：insert（隨機） | 11.779 | 15.588 | 0.76x | 未平衡 BST 不用做旋轉，隨機資料下不輸紅黑樹 |
| BST vs map：find（隨機） | 7.989 | 9.546 | 0.84x | |
| BST vs map：insert（依序，最差情況） | 10477.849 | 5.784 | 1811.68x | 退化成鏈，O(n²)，這就是需要自平衡樹的原因 |
| mergeSort vs std::sort | 見 benchmark.md | | 約 1.7x | 同為 O(n log n)；自刻版需要額外空間，但是穩定排序 |

**解讀**：自刻的 Hash Table、BST（隨機資料）與 STL 在同一個量級，驗證複雜度分析正確；Heap 較慢是為了多出的「任意刪除」功能所付出的常數代價；而依序插入 BST 的數據直接展示了「為什麼最差情況很重要」。
