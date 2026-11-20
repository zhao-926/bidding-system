// bench/bench.cpp — 自刻資料結構 vs STL 效能對照
//
// 注意：bench/ 是整個專案中「唯一允許使用 STL 容器」的地方（當對照組）。
// 執行後會把結果印在螢幕上，並寫入 docs/benchmark.md（請從專案根目錄執行，make bench 就是）。
//
// 比較項目（n = 1,000 / 10,000 / 100,000）：
//   HashTable  vs std::unordered_map   insert、find
//   MaxHeap    vs std::priority_queue  push、pop
//   BST        vs std::map             insert、find（隨機順序）、依序插入（退化情況）
//   mergeSort  vs std::sort            排序
// 每個數字都是「重複量測取最小值」的毫秒數，以降低系統雜訊的影響。
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "BST.h"
#include "DynArray.h"
#include "HashTable.h"
#include "MaxHeap.h"
#include "MergeSort.h"

// ---------------------------------------------------------------- 量測工具

static volatile long long g_sink = 0;   // 把結果寫進 volatile，避免編譯器把整段量測最佳化掉

// 執行 body 共 reps 次，回傳最短耗時（毫秒）
template <typename Body>
static double bestOfMs(int reps, Body body) {
    double best = 1e18;
    for (int r = 0; r < reps; r++) {
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        body();
        std::chrono::steady_clock::time_point stop = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(stop - start).count();
        if (ms < best) {
            best = ms;
        }
    }
    return best;
}

static int repsFor(int n) {
    return (n <= 10000) ? 7 : 3;
}

// ---------------------------------------------------------------- 結果表格

struct Row {
    int n;
    double mine;   // 自刻（ms）
    double stl;    // STL（ms）
};

struct Table {
    std::string title;
    std::string mineName;
    std::string stlName;
    std::vector<Row> rows;
};

static std::vector<Table> g_tables;

static std::string fmt(double value, const char* format) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), format, value);
    return buffer;
}

static std::string withCommas(int n) {
    std::string digits = std::to_string(n);
    std::string result;
    int count = 0;
    for (int i = static_cast<int>(digits.size()) - 1; i >= 0; i--) {
        result.insert(result.begin(), digits[i]);
        if (++count % 3 == 0 && i > 0) {
            result.insert(result.begin(), ',');
        }
    }
    return result;
}

// 把 ratio 轉成一句人話：「約 X 倍（較慢）」「約 X 倍（較快）」
static std::string describeRatio(double ratio) {
    if (ratio >= 1.0) {
        return "為 STL 的 " + fmt(ratio, "%.1f") + " 倍時間（較慢）";
    }
    return "僅需 STL 的 " + fmt(ratio, "%.2f") + " 倍時間（較快）";
}

static std::string tableToMarkdown(const Table& table) {
    std::string md = "### " + table.title + "\n\n";
    md += "| n | " + table.mineName + "（ms） | " + table.stlName + "（ms） | 自刻 / STL |\n";
    md += "|---:|---:|---:|---:|\n";
    for (std::size_t i = 0; i < table.rows.size(); i++) {
        const Row& r = table.rows[i];
        double ratio = (r.stl > 0) ? r.mine / r.stl : 0;
        md += "| " + withCommas(r.n) + " | " + fmt(r.mine, "%.3f") + " | " + fmt(r.stl, "%.3f") + " | " +
              fmt(ratio, "%.2f") + "x |\n";
    }
    md += "\n";
    return md;
}

static const Table* findTableByTitle(const std::string& title) {
    for (std::size_t i = 0; i < g_tables.size(); i++) {
        if (g_tables[i].title == title) {
            return &g_tables[i];
        }
    }
    return nullptr;
}

// 取得某張表在最大 n 的倍率
static double lastRatio(const std::string& title) {
    const Table* table = findTableByTitle(title);
    if (table == nullptr || table->rows.empty()) {
        return 0;
    }
    const Row& r = table->rows[table->rows.size() - 1];
    return (r.stl > 0) ? r.mine / r.stl : 0;
}

static Table& newTable(const std::string& title, const std::string& mine, const std::string& stl) {
    Table table;
    table.title = title;
    table.mineName = mine;
    table.stlName = stl;
    g_tables.push_back(table);
    return g_tables.back();
}

// ---------------------------------------------------------------- 測試資料

// 模仿 Bid 的簡化元素：金額高者優先，金額相同時間早者優先
struct BenchBid {
    int id;
    long long amount;
    long long time;
    int getId() const { return id; }
    bool hasHigherPriority(const BenchBid& other) const {
        if (amount != other.amount) {
            return amount > other.amount;
        }
        return time < other.time;
    }
};

// 給 std::priority_queue 用的比較器：「a 的優先權低於 b」時回傳 true
struct BenchBidLower {
    bool operator()(const BenchBid& a, const BenchBid& b) const { return b.hasHigherPriority(a); }
};

static std::vector<int> makeRandomKeys(int n, unsigned seed) {
    std::mt19937 rng(seed);
    std::vector<int> keys(n);
    for (int i = 0; i < n; i++) {
        keys[i] = static_cast<int>(rng() % (10u * static_cast<unsigned>(n)));
    }
    return keys;
}

// ---------------------------------------------------------------- 各項對照

static void benchHashTable(const int* sizes, int sizeCount) {
    Table& insertTable = newTable("HashTable vs unordered_map：insert", "HashTable", "unordered_map");
    Table& findTable = newTable("HashTable vs unordered_map：find（全部命中）", "HashTable", "unordered_map");
    for (int s = 0; s < sizeCount; s++) {
        int n = sizes[s];
        int reps = repsFor(n);
        std::vector<int> keys = makeRandomKeys(n, 1);

        double mineInsert = bestOfMs(reps, [&] {
            HashTable<int, int> table;
            for (int i = 0; i < n; i++) table.insert(keys[i], i);
            g_sink += table.size();
        });
        double stlInsert = bestOfMs(reps, [&] {
            std::unordered_map<int, int> table;
            for (int i = 0; i < n; i++) table[keys[i]] = i;
            g_sink += static_cast<long long>(table.size());
        });
        insertTable.rows.push_back({n, mineInsert, stlInsert});

        HashTable<int, int> mine;
        std::unordered_map<int, int> stl;
        for (int i = 0; i < n; i++) {
            mine.insert(keys[i], i);
            stl[keys[i]] = i;
        }
        double mineFind = bestOfMs(reps, [&] {
            long long hits = 0;
            for (int i = 0; i < n; i++) if (mine.find(keys[i]) != nullptr) hits++;
            g_sink += hits;
        });
        double stlFind = bestOfMs(reps, [&] {
            long long hits = 0;
            for (int i = 0; i < n; i++) if (stl.find(keys[i]) != stl.end()) hits++;
            g_sink += hits;
        });
        findTable.rows.push_back({n, mineFind, stlFind});
    }
}

static void benchHeap(const int* sizes, int sizeCount) {
    Table& pushTable = newTable("MaxHeap vs priority_queue：push", "MaxHeap", "priority_queue");
    Table& popTable = newTable("MaxHeap vs priority_queue：pop（全部取出）", "MaxHeap", "priority_queue");
    for (int s = 0; s < sizeCount; s++) {
        int n = sizes[s];
        int reps = repsFor(n);
        std::mt19937 rng(2);
        std::vector<BenchBid> bids(n);
        for (int i = 0; i < n; i++) {
            bids[i].id = i;
            bids[i].amount = static_cast<long long>(rng() % 1000000u);
            bids[i].time = i;
        }

        double minePush = bestOfMs(reps, [&] {
            MaxHeap<BenchBid> heap;
            for (int i = 0; i < n; i++) heap.push(bids[i]);
            g_sink += heap.size();
        });
        double stlPush = bestOfMs(reps, [&] {
            std::priority_queue<BenchBid, std::vector<BenchBid>, BenchBidLower> heap;
            for (int i = 0; i < n; i++) heap.push(bids[i]);
            g_sink += static_cast<long long>(heap.size());
        });
        pushTable.rows.push_back({n, minePush, stlPush});

        // pop：每次重新建好堆積（建堆時間不計入），只量測 n 次 pop
        double minePop = 1e18;
        double stlPop = 1e18;
        for (int r = 0; r < reps; r++) {
            MaxHeap<BenchBid> heap;
            for (int i = 0; i < n; i++) heap.push(bids[i]);
            double t = bestOfMs(1, [&] {
                long long sum = 0;
                while (!heap.empty()) {
                    sum += heap.top()->amount;
                    heap.pop();
                }
                g_sink += sum;
            });
            if (t < minePop) minePop = t;

            std::priority_queue<BenchBid, std::vector<BenchBid>, BenchBidLower> stlHeap;
            for (int i = 0; i < n; i++) stlHeap.push(bids[i]);
            t = bestOfMs(1, [&] {
                long long sum = 0;
                while (!stlHeap.empty()) {
                    sum += stlHeap.top().amount;
                    stlHeap.pop();
                }
                g_sink += sum;
            });
            if (t < stlPop) stlPop = t;
        }
        popTable.rows.push_back({n, minePop, stlPop});
    }
}

// BST 樹高比較用
struct HeightRow {
    int n;
    int randomHeight;
    int sequentialHeight;
};
static std::vector<HeightRow> g_heightRows;

static void benchBST(const int* sizes, int sizeCount) {
    Table& insertTable = newTable("BST vs map：insert（隨機順序）", "BST", "map");
    Table& findTable = newTable("BST vs map：find（隨機順序，全部命中）", "BST", "map");
    Table& sequentialTable = newTable("BST vs map：insert（依序 0,1,2,... 的退化情況）", "BST", "map");
    for (int s = 0; s < sizeCount; s++) {
        int n = sizes[s];
        int reps = repsFor(n);
        std::vector<int> keys = makeRandomKeys(n, 3);

        double mineInsert = bestOfMs(reps, [&] {
            BST<int, int> tree;
            for (int i = 0; i < n; i++) tree.insert(keys[i], i);
            g_sink += tree.size();
        });
        double stlInsert = bestOfMs(reps, [&] {
            std::map<int, int> tree;
            for (int i = 0; i < n; i++) tree[keys[i]] = i;
            g_sink += static_cast<long long>(tree.size());
        });
        insertTable.rows.push_back({n, mineInsert, stlInsert});

        BST<int, int> mine;
        std::map<int, int> stl;
        for (int i = 0; i < n; i++) {
            mine.insert(keys[i], i);
            stl[keys[i]] = i;
        }
        double mineFind = bestOfMs(reps, [&] {
            long long hits = 0;
            for (int i = 0; i < n; i++) if (mine.find(keys[i]) != nullptr) hits++;
            g_sink += hits;
        });
        double stlFind = bestOfMs(reps, [&] {
            long long hits = 0;
            for (int i = 0; i < n; i++) if (stl.find(keys[i]) != stl.end()) hits++;
            g_sink += hits;
        });
        findTable.rows.push_back({n, mineFind, stlFind});

        // 依序插入：BST 會退化成一條鏈，每次插入都要走到最底，總共 O(n^2)
        int sequentialReps = (n >= 100000) ? 1 : reps;
        int sequentialHeight = 0;
        double mineSeq = bestOfMs(sequentialReps, [&] {
            BST<int, int> tree;
            for (int i = 0; i < n; i++) tree.insert(i, i);
            sequentialHeight = tree.height();
            g_sink += tree.size();
        });
        double stlSeq = bestOfMs(reps, [&] {
            std::map<int, int> tree;
            for (int i = 0; i < n; i++) tree[i] = i;
            g_sink += static_cast<long long>(tree.size());
        });
        sequentialTable.rows.push_back({n, mineSeq, stlSeq});

        HeightRow heightRow;
        heightRow.n = n;
        heightRow.randomHeight = mine.height();
        heightRow.sequentialHeight = sequentialHeight;
        g_heightRows.push_back(heightRow);
    }
}

static void benchSort(const int* sizes, int sizeCount) {
    Table& table = newTable("mergeSort vs std::sort：隨機整數排序（含複製資料的時間）", "mergeSort", "std::sort");
    for (int s = 0; s < sizeCount; s++) {
        int n = sizes[s];
        int reps = repsFor(n);
        std::mt19937 rng(4);
        std::vector<int> original(n);
        DynArray<int> originalArray;
        for (int i = 0; i < n; i++) {
            original[i] = static_cast<int>(rng() % 1000000u);
            originalArray.pushBack(original[i]);
        }
        double mine = bestOfMs(reps, [&] {
            DynArray<int> copy = originalArray;
            mergeSort(copy, [](const int& a, const int& b) { return a < b; });
            g_sink += copy.get(0);
        });
        double stl = bestOfMs(reps, [&] {
            std::vector<int> copy = original;
            std::sort(copy.begin(), copy.end());
            g_sink += copy[0];
        });
        table.rows.push_back({n, mine, stl});
    }
}

// ---------------------------------------------------------------- 輸出

static std::string buildReport() {
    std::string md;
    std::time_t now = std::time(nullptr);
    char dateText[32];
    std::strftime(dateText, sizeof(dateText), "%Y-%m-%d", std::localtime(&now));

    md += "# 效能對照結果（自刻資料結構 vs STL）\n\n";
    md += "> 由 `make bench` 自動產生（`bench/bench.cpp`），請勿手動修改；重新執行會覆蓋本檔。\n\n";
    md += "- 日期：" + std::string(dateText) + "\n";
    md += "- 編譯器：g++ " + std::string(__VERSION__) + "，編譯選項 `-std=c++17 -O2`\n";
    md += "- 量測方式：每個數字為重複量測（n ≤ 10,000 取 7 次、n = 100,000 取 3 次）的**最小值**，單位為毫秒（ms）\n";
    md += "- 「自刻 / STL」< 1 代表自刻較快，> 1 代表自刻較慢\n";
    md += "- 對照組只出現在 `bench/`，`core/` 內沒有使用任何 STL 容器（`make check` 會檢查）\n\n";

    md += "## 量測結果\n\n";
    for (std::size_t i = 0; i < g_tables.size(); i++) {
        md += tableToMarkdown(g_tables[i]);
    }

    md += "### BST 樹高：隨機插入 vs 依序插入\n\n";
    md += "| n | 理想樹高 ⌈log₂(n+1)⌉ | 隨機插入的樹高 | 依序插入的樹高 |\n|---:|---:|---:|---:|\n";
    for (std::size_t i = 0; i < g_heightRows.size(); i++) {
        const HeightRow& h = g_heightRows[i];
        int ideal = static_cast<int>(std::ceil(std::log2(static_cast<double>(h.n) + 1)));
        md += "| " + withCommas(h.n) + " | " + std::to_string(ideal) + " | " + std::to_string(h.randomHeight) +
              " | " + std::to_string(h.sequentialHeight) + " |\n";
    }
    md += "\n";

    md += "## 解讀\n\n";
    md += "- **HashTable**：和 `std::unordered_map` 同為平均 O(1)；n = 100,000 時 insert " +
          describeRatio(lastRatio("HashTable vs unordered_map：insert")) + "、find " +
          describeRatio(lastRatio("HashTable vs unordered_map：find（全部命中）")) +
          "。兩者階數相同（平均 O(1)），倍率落在 1 倍上下，差異來自常數因子（節點配置方式、雜湊函式、rehash 時機），不是演算法層級的差距。\n";
    md += "- **MaxHeap**：n = 100,000 時 push " + describeRatio(lastRatio("MaxHeap vs priority_queue：push")) +
          "、pop " + describeRatio(lastRatio("MaxHeap vs priority_queue：pop（全部取出）")) +
          "。自刻版每次交換都要同步更新「bidId → 位置」的 HashTable，這是為了支援 O(log n) 的 `removeById`（撤標）所付出的代價，`std::priority_queue` 沒有這個功能。\n";
    md += "- **BST**：隨機插入時樹高約為理想樹高的 2 倍多（見上表），insert " +
          describeRatio(lastRatio("BST vs map：insert（隨機順序）")) + "、find " +
          describeRatio(lastRatio("BST vs map：find（隨機順序，全部命中）")) +
          "（`std::map` 每次插入還要做紅黑樹的旋轉來維持平衡，所以隨機順序下未平衡的 BST 反而不輸）；" +
          "但**依序插入**時樹退化成一條鏈（樹高 = n），insert 變成 O(n²)，n = 100,000 時" +
          describeRatio(lastRatio("BST vs map：insert（依序 0,1,2,... 的退化情況）")) +
          "，這就是 `std::map` 要做自平衡的原因。本專題的 BST 只存「進行中」的商品、結標後即移除，" +
          "數量有限，所以採用未平衡 BST；但若連續刊登相同時長的商品，結標時間會遞增而讓樹變瘦長，" +
          "改良方向是改用 AVL 樹或紅黑樹。\n";
    md += "- **mergeSort**：兩者都是 O(n log n)；n = 100,000 時自刻合併排序" +
          describeRatio(lastRatio("mergeSort vs std::sort：隨機整數排序（含複製資料的時間）")) +
          "。自刻版需要 O(n) 額外空間、且每次存取都有邊界檢查，但換來「穩定排序」，多欄位排序時不會打亂前一次的順序。\n";
    return md;
}

int main() {
    const int sizes[] = {1000, 10000, 100000};
    const int sizeCount = 3;
    g_tables.reserve(16);   // newTable 回傳的是 vector 元素的參照，先預留空間避免搬移後參照失效

    std::cout << "Running benchmarks (this can take a little while)..." << std::endl;
    std::cout << "  HashTable..." << std::endl;
    benchHashTable(sizes, sizeCount);
    std::cout << "  MaxHeap..." << std::endl;
    benchHeap(sizes, sizeCount);
    std::cout << "  BST (including the sequential-insert worst case)..." << std::endl;
    benchBST(sizes, sizeCount);
    std::cout << "  mergeSort..." << std::endl;
    benchSort(sizes, sizeCount);

    std::string report = buildReport();
    std::cout << std::endl << report;

    std::ofstream out("docs/benchmark.md", std::ios::binary);
    if (!out) {
        std::cerr << "Cannot write docs/benchmark.md (run from the project root)." << std::endl;
        return 1;
    }
    out << report;
    std::cout << "Wrote docs/benchmark.md" << std::endl;
    return 0;
}
