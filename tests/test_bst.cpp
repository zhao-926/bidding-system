// test_bst.cpp — BST 單元測試（含與暴力解比對的隨機測試）
#include <string>

#include "test_framework.h"
#include "test_random.h"
#include "../core/BST.h"
#include "../core/DynArray.h"

// 測試用複合 key：模仿 (結標時間, 商品ID)；先比時間，時間相同再比 ID
struct TimeKey {
    long long endTime;
    int itemId;
    TimeKey() : endTime(0), itemId(0) {}
    TimeKey(long long t, int id) : endTime(t), itemId(id) {}
    bool operator<(const TimeKey& other) const {
        if (endTime != other.endTime) {
            return endTime < other.endTime;
        }
        return itemId < other.itemId;
    }
};

// 把中序走訪的 key 串成字串，方便比對，例如 "1,3,5,"
static std::string inorderKeys(const BST<int, int>& tree) {
    std::string result;
    tree.inorder([&result](const int& key, const int&) { result += std::to_string(key) + ","; });
    return result;
}

static std::string rangeKeys(const BST<int, int>& tree, int low, int high) {
    std::string result;
    tree.rangeQuery(low, high, [&result](const int& key, const int&) { result += std::to_string(key) + ","; });
    return result;
}

// 空樹
static void test_empty_tree() {
    BST<int, int> tree;
    ASSERT_TRUE(tree.empty());
    ASSERT_EQ(0, tree.size());
    ASSERT_EQ(0, tree.height());
    ASSERT_TRUE(tree.find(1) == nullptr);
    ASSERT_FALSE(tree.remove(1));
    int k = 0, v = 0;
    ASSERT_FALSE(tree.popMin(k, v));
    ASSERT_FALSE(tree.peekMin(k, v));
    ASSERT_EQ(std::string(""), inorderKeys(tree));
    ASSERT_EQ(std::string(""), rangeKeys(tree, 0, 100));
}

// 插入與查找
static void test_insert_and_find() {
    BST<int, std::string> tree;
    ASSERT_TRUE(tree.insert(50, "fifty"));
    ASSERT_TRUE(tree.insert(30, "thirty"));
    ASSERT_TRUE(tree.insert(70, "seventy"));
    ASSERT_EQ(3, tree.size());
    ASSERT_EQ(std::string("thirty"), *tree.find(30));
    ASSERT_TRUE(tree.find(99) == nullptr);
    ASSERT_TRUE(tree.contains(70));
}

// 重複 key：更新 value、回傳 false、size 不變
static void test_insert_duplicate_updates() {
    BST<int, int> tree;
    ASSERT_TRUE(tree.insert(5, 1));
    ASSERT_FALSE(tree.insert(5, 2));
    ASSERT_EQ(1, tree.size());
    ASSERT_EQ(2, *tree.find(5));
}

// 中序走訪必須由小到大
static void test_inorder_sorted() {
    BST<int, int> tree;
    int keys[] = {50, 30, 70, 20, 40, 60, 80, 35, 45};
    for (int i = 0; i < 9; i++) {
        tree.insert(keys[i], i);
    }
    ASSERT_EQ(std::string("20,30,35,40,45,50,60,70,80,"), inorderKeys(tree));
}

// remove 情況 1：刪葉節點
static void test_remove_leaf() {
    BST<int, int> tree;
    int keys[] = {50, 30, 70, 20, 40};
    for (int i = 0; i < 5; i++) tree.insert(keys[i], i);
    ASSERT_TRUE(tree.remove(20));
    ASSERT_EQ(4, tree.size());
    ASSERT_TRUE(tree.find(20) == nullptr);
    ASSERT_EQ(std::string("30,40,50,70,"), inorderKeys(tree));
}

// remove 情況 2：只有一個子節點（分別測只有左子、只有右子）
static void test_remove_one_child() {
    BST<int, int> tree;
    int keys[] = {50, 30, 70, 20, 80, 90};      // 30 只有左子 20；80 只有右子 90
    for (int i = 0; i < 6; i++) tree.insert(keys[i], i);
    ASSERT_TRUE(tree.remove(30));
    ASSERT_EQ(std::string("20,50,70,80,90,"), inorderKeys(tree));
    ASSERT_TRUE(tree.remove(80));
    ASSERT_EQ(std::string("20,50,70,90,"), inorderKeys(tree));
    ASSERT_TRUE(tree.find(90) != nullptr);
}

// remove 情況 3：有兩個子節點（刪根、刪中間節點；後繼者是直接右子、或在更深處）
static void test_remove_two_children() {
    BST<int, int> tree;
    int keys[] = {50, 30, 70, 20, 40, 60, 80, 65};
    for (int i = 0; i < 8; i++) tree.insert(keys[i], i * 10);
    ASSERT_TRUE(tree.remove(50));              // 根，後繼者是 60（在右子樹深處）
    ASSERT_EQ(std::string("20,30,40,60,65,70,80,"), inorderKeys(tree));
    ASSERT_TRUE(tree.remove(70));              // 70 的兩個子：60 已上移 -> 結構改變後仍須正確
    ASSERT_EQ(std::string("20,30,40,60,65,80,"), inorderKeys(tree));
    ASSERT_TRUE(tree.remove(30));              // 後繼者 40 就是直接右子
    ASSERT_EQ(std::string("20,40,60,65,80,"), inorderKeys(tree));
    ASSERT_EQ(5, tree.size());
    // value 要跟著 key 一起搬動（60 是第 6 個插入的，value = 50；它曾被後繼者機制搬到根）
    ASSERT_EQ(50, *tree.find(60));
    ASSERT_EQ(70, *tree.find(65));
}

// remove 不存在的 key；刪光所有節點後樹為空
static void test_remove_nonexistent_and_all() {
    BST<int, int> tree;
    tree.insert(2, 0);
    tree.insert(1, 0);
    tree.insert(3, 0);
    ASSERT_FALSE(tree.remove(99));
    ASSERT_EQ(3, tree.size());
    ASSERT_TRUE(tree.remove(2));
    ASSERT_TRUE(tree.remove(1));
    ASSERT_TRUE(tree.remove(3));
    ASSERT_TRUE(tree.empty());
    ASSERT_EQ(0, tree.height());
    tree.insert(9, 9);                         // 刪空後可重新使用
    ASSERT_EQ(1, tree.size());
}

// popMin 依序取出由小到大；peekMin 不會刪除
static void test_pop_min_and_peek_min() {
    BST<int, int> tree;
    int keys[] = {40, 10, 30, 20, 50};
    for (int i = 0; i < 5; i++) tree.insert(keys[i], keys[i] * 2);
    int k = 0, v = 0;
    ASSERT_TRUE(tree.peekMin(k, v));
    ASSERT_EQ(10, k);
    ASSERT_EQ(5, tree.size());                 // peek 不刪除
    std::string order;
    while (tree.popMin(k, v)) {
        ASSERT_EQ(k * 2, v);
        order += std::to_string(k) + ",";
    }
    ASSERT_EQ(std::string("10,20,30,40,50,"), order);
    ASSERT_TRUE(tree.empty());
}

// rangeQuery：邊界包含、空範圍、範圍超出、單點
static void test_range_query() {
    BST<int, int> tree;
    for (int k = 10; k <= 100; k += 10) {
        tree.insert(k, k);
    }
    ASSERT_EQ(std::string("30,40,50,"), rangeKeys(tree, 30, 50));   // 兩端都含
    ASSERT_EQ(std::string(""), rangeKeys(tree, 31, 39));            // 範圍內沒有任何 key -> 空
    ASSERT_EQ(std::string("10,20,30,40,50,60,70,80,90,100,"), rangeKeys(tree, -5, 1000));
    ASSERT_EQ(std::string("70,"), rangeKeys(tree, 70, 70));
    ASSERT_EQ(std::string(""), rangeKeys(tree, 200, 300));
    ASSERT_EQ(std::string(""), rangeKeys(tree, 50, 40));            // low > high
}

// 樹高：平衡插入約 log n；依序插入退化成 n
static void test_height_balanced_vs_degenerate() {
    BST<int, int> balanced;
    // 用「中間值優先」的順序插入 1..127 -> 完美平衡，高度 7
    int step = 64;
    balanced.insert(64, 0);
    while (step > 1) {
        step /= 2;
        for (int k = step; k < 128; k += 2 * step) {
            balanced.insert(k, 0);
        }
    }
    ASSERT_EQ(127, balanced.size());
    ASSERT_EQ(7, balanced.height());

    BST<int, int> degenerate;
    for (int k = 1; k <= 1000; k++) {
        degenerate.insert(k, 0);               // 依序插入
    }
    ASSERT_EQ(1000, degenerate.height());      // 退化成鏈
}

// 大型退化樹不可 stack overflow（所有操作都是迴圈實作），且解構正常
static void test_deep_degenerate_tree_no_overflow() {
    BST<int, int> tree;
    for (int k = 0; k < 20000; k++) {
        tree.insert(k, k);
    }
    ASSERT_EQ(20000, tree.height());
    int count = 0;
    tree.inorder([&count](const int&, const int&) { count++; });
    ASSERT_EQ(20000, count);
    ASSERT_TRUE(tree.remove(0));
    ASSERT_TRUE(tree.remove(19999));
    ASSERT_EQ(19998, tree.size());
    // 離開作用域時解構子要能正常刪除 20000 層深的樹
}

// 複合 key（結標時間, 商品ID）：同時間的商品以 ID 區分、不會互相覆蓋
static void test_composite_key() {
    BST<TimeKey, int> tree;
    tree.insert(TimeKey(100, 2), 2);
    tree.insert(TimeKey(100, 1), 1);           // 同時間、ID 較小
    tree.insert(TimeKey(50, 9), 9);
    tree.insert(TimeKey(200, 3), 3);
    ASSERT_EQ(4, tree.size());
    std::string order;
    tree.inorder([&order](const TimeKey&, const int& id) { order += std::to_string(id); });
    ASSERT_EQ(std::string("9123"), order);
    std::string inRange;
    tree.rangeQuery(TimeKey(60, 0), TimeKey(150, 1000),
                    [&inRange](const TimeKey&, const int& id) { inRange += std::to_string(id); });
    ASSERT_EQ(std::string("12"), inRange);
    TimeKey minKey;
    int minValue = 0;
    ASSERT_TRUE(tree.popMin(minKey, minValue));
    ASSERT_EQ(9, minValue);
}

// 隨機測試：隨機 insert / remove 共 1000 次（5 組種子），與暴力解（DynArray 存放現有 key）比對
static void test_random_against_brute_force() {
    bool allMatched = true;
    for (int seed = 1; seed <= 5; seed++) {
        SimpleRandom random(seed * 104729);
        BST<int, int> tree;
        DynArray<int> present;                  // 暴力解：目前存在的 key（不排序）
        for (int step = 0; step < 1000; step++) {
            int key = random.next(200);
            if (random.next(2) == 0) {
                bool isNew = tree.insert(key, key * 3);
                bool bruteNew = !present.contains(key);
                if (isNew != bruteNew) allMatched = false;
                if (bruteNew) present.pushBack(key);
            } else {
                bool removed = tree.remove(key);
                bool bruteHad = present.contains(key);
                if (removed != bruteHad) allMatched = false;
                if (bruteHad) {
                    for (int i = 0; i < present.size(); i++) {
                        if (present.get(i) == key) {
                            present.removeAt(i);
                            break;
                        }
                    }
                }
            }
            if (tree.size() != present.size()) allMatched = false;
        }
        // 最後比對：每個 key 的存在性、中序嚴格遞增且筆數相同、隨機範圍查詢筆數
        for (int key = 0; key < 200; key++) {
            const int* value = tree.find(key);
            bool bruteHas = present.contains(key);
            if ((value != nullptr) != bruteHas) allMatched = false;
            if (value != nullptr && *value != key * 3) allMatched = false;
        }
        int previous = -1;
        int visited = 0;
        tree.inorder([&](const int& key, const int&) {
            if (key <= previous) allMatched = false;
            previous = key;
            visited++;
        });
        if (visited != present.size()) allMatched = false;
        for (int trial = 0; trial < 20; trial++) {
            int a = random.next(200);
            int b = random.next(200);
            int low = (a < b) ? a : b;
            int high = (a < b) ? b : a;
            int treeCount = 0;
            tree.rangeQuery(low, high, [&treeCount](const int&, const int&) { treeCount++; });
            int bruteCount = 0;
            for (int i = 0; i < present.size(); i++) {
                if (present.get(i) >= low && present.get(i) <= high) bruteCount++;
            }
            if (treeCount != bruteCount) allMatched = false;
        }
        // popMin 取出的順序也要是遞增
        int k = 0, v = 0;
        int lastPopped = -1;
        while (tree.popMin(k, v)) {
            if (k <= lastPopped) allMatched = false;
            lastPopped = k;
        }
        if (!tree.empty()) allMatched = false;
    }
    ASSERT_TRUE(allMatched);
}

void run_bst_tests() {
    RUN_TEST(test_empty_tree);
    RUN_TEST(test_insert_and_find);
    RUN_TEST(test_insert_duplicate_updates);
    RUN_TEST(test_inorder_sorted);
    RUN_TEST(test_remove_leaf);
    RUN_TEST(test_remove_one_child);
    RUN_TEST(test_remove_two_children);
    RUN_TEST(test_remove_nonexistent_and_all);
    RUN_TEST(test_pop_min_and_peek_min);
    RUN_TEST(test_range_query);
    RUN_TEST(test_height_balanced_vs_degenerate);
    RUN_TEST(test_deep_degenerate_tree_no_overflow);
    RUN_TEST(test_composite_key);
    RUN_TEST(test_random_against_brute_force);
}
