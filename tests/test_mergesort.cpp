// test_mergesort.cpp — MergeSort 單元測試（含穩定性與隨機測試）
#include <string>

#include "test_framework.h"
#include "test_random.h"
#include "../core/DynArray.h"
#include "../core/MergeSort.h"

static bool lessInt(const int& a, const int& b) { return a < b; }

static bool isSortedAscending(const DynArray<int>& arr) {
    for (int i = 1; i < arr.size(); i++) {
        if (arr.get(i - 1) > arr.get(i)) {
            return false;
        }
    }
    return true;
}

// 空陣列與單一元素
static void test_empty_and_single() {
    DynArray<int> empty;
    mergeSort(empty, lessInt);
    ASSERT_TRUE(empty.empty());

    DynArray<int> single;
    single.pushBack(42);
    mergeSort(single, lessInt);
    ASSERT_EQ(1, single.size());
    ASSERT_EQ(42, single.get(0));
}

// 兩個元素（順序正確 / 顛倒）
static void test_two_elements() {
    DynArray<int> a;
    a.pushBack(1);
    a.pushBack(2);
    mergeSort(a, lessInt);
    ASSERT_EQ(1, a.get(0));
    DynArray<int> b;
    b.pushBack(2);
    b.pushBack(1);
    mergeSort(b, lessInt);
    ASSERT_EQ(1, b.get(0));
    ASSERT_EQ(2, b.get(1));
}

// 已排序、逆序
static void test_sorted_and_reversed() {
    DynArray<int> sorted;
    DynArray<int> reversed;
    for (int i = 0; i < 100; i++) {
        sorted.pushBack(i);
        reversed.pushBack(99 - i);
    }
    mergeSort(sorted, lessInt);
    mergeSort(reversed, lessInt);
    ASSERT_TRUE(isSortedAscending(sorted));
    ASSERT_TRUE(isSortedAscending(reversed));
    ASSERT_EQ(0, reversed.get(0));
    ASSERT_EQ(99, reversed.get(99));
}

// 含大量重複值
static void test_duplicates() {
    DynArray<int> arr;
    for (int i = 0; i < 200; i++) {
        arr.pushBack(i % 5);
    }
    mergeSort(arr, lessInt);
    ASSERT_TRUE(isSortedAscending(arr));
    ASSERT_EQ(200, arr.size());
    ASSERT_EQ(0, arr.get(0));
    ASSERT_EQ(4, arr.get(199));
}

// 自訂比較函式：由大到小
static void test_custom_comparator_descending() {
    DynArray<int> arr;
    int values[] = {3, 9, 1, 7, 5};
    for (int i = 0; i < 5; i++) arr.pushBack(values[i]);
    mergeSort(arr, [](const int& a, const int& b) { return a > b; });
    ASSERT_EQ(9, arr.get(0));
    ASSERT_EQ(7, arr.get(1));
    ASSERT_EQ(1, arr.get(4));
}

// 各種長度（含奇數、2 的次方、2 的次方 ± 1）都要正確
static void test_various_sizes() {
    int sizes[] = {2, 3, 4, 5, 7, 8, 9, 15, 16, 17, 31, 32, 33, 100, 101};
    bool allSorted = true;
    SimpleRandom random(99);
    for (int s = 0; s < 15; s++) {
        DynArray<int> arr;
        for (int i = 0; i < sizes[s]; i++) {
            arr.pushBack(random.next(1000));
        }
        mergeSort(arr, lessInt);
        if (!isSortedAscending(arr) || arr.size() != sizes[s]) {
            allSorted = false;
        }
    }
    ASSERT_TRUE(allSorted);
}

// 字串排序
static void test_strings() {
    DynArray<std::string> arr;
    arr.pushBack("carol");
    arr.pushBack("alice");
    arr.pushBack("dave");
    arr.pushBack("bob");
    mergeSort(arr, [](const std::string& a, const std::string& b) { return a < b; });
    ASSERT_EQ(std::string("alice"), arr.get(0));
    ASSERT_EQ(std::string("bob"), arr.get(1));
    ASSERT_EQ(std::string("carol"), arr.get(2));
    ASSERT_EQ(std::string("dave"), arr.get(3));
}

// 穩定性：key 相同的記錄，排序後原本的先後順序（originalIndex）必須保持
struct Record {
    int key;
    int originalIndex;
    Record() : key(0), originalIndex(0) {}
    Record(int k, int i) : key(k), originalIndex(i) {}
};

static void test_stability() {
    DynArray<Record> records;
    SimpleRandom random(2024);
    for (int i = 0; i < 500; i++) {
        records.pushBack(Record(random.next(10), i));    // key 只有 10 種 -> 大量相同 key
    }
    mergeSort(records, [](const Record& a, const Record& b) { return a.key < b.key; });
    bool stable = true;
    bool sorted = true;
    for (int i = 1; i < records.size(); i++) {
        const Record& prev = records.get(i - 1);
        const Record& cur = records.get(i);
        if (prev.key > cur.key) sorted = false;
        if (prev.key == cur.key && prev.originalIndex > cur.originalIndex) stable = false;
    }
    ASSERT_TRUE(sorted);
    ASSERT_TRUE(stable);
}

// 連續兩次排序（先依次要條件、再依主要條件）：依賴穩定性才能得到正確的多欄位排序
static void test_two_pass_sort_relies_on_stability() {
    DynArray<Record> records;
    // key = 價格, originalIndex 當作「出價數」
    int prices[] = {20, 10, 20, 10, 30};
    int counts[] = {5, 1, 2, 9, 4};
    for (int i = 0; i < 5; i++) records.pushBack(Record(prices[i], counts[i]));
    mergeSort(records, [](const Record& a, const Record& b) { return a.originalIndex < b.originalIndex; });
    mergeSort(records, [](const Record& a, const Record& b) { return a.key < b.key; });
    // 價格相同者，應依出價數由小到大：(10,1) (10,9) (20,2) (20,5) (30,4)
    ASSERT_EQ(1, records.get(0).originalIndex);
    ASSERT_EQ(9, records.get(1).originalIndex);
    ASSERT_EQ(2, records.get(2).originalIndex);
    ASSERT_EQ(5, records.get(3).originalIndex);
    ASSERT_EQ(4, records.get(4).originalIndex);
}

// 隨機測試：排序結果必須有序，且與原本是「同一組元素」（總和與各值出現次數相同）
static void test_random_sorted_and_permutation() {
    bool ok = true;
    for (int seed = 1; seed <= 20; seed++) {
        SimpleRandom random(seed * 31337);
        int n = random.next(1000) + 1;
        DynArray<int> arr;
        int countOf[50] = {0};
        for (int i = 0; i < n; i++) {
            int value = random.next(50);
            arr.pushBack(value);
            countOf[value]++;
        }
        mergeSort(arr, lessInt);
        if (!isSortedAscending(arr) || arr.size() != n) ok = false;
        for (int i = 0; i < arr.size(); i++) {
            countOf[arr.get(i)]--;
        }
        for (int v = 0; v < 50; v++) {
            if (countOf[v] != 0) ok = false;               // 多了或少了元素
        }
    }
    ASSERT_TRUE(ok);
}

void run_mergesort_tests() {
    RUN_TEST(test_empty_and_single);
    RUN_TEST(test_two_elements);
    RUN_TEST(test_sorted_and_reversed);
    RUN_TEST(test_duplicates);
    RUN_TEST(test_custom_comparator_descending);
    RUN_TEST(test_various_sizes);
    RUN_TEST(test_strings);
    RUN_TEST(test_stability);
    RUN_TEST(test_two_pass_sort_relies_on_stability);
    RUN_TEST(test_random_sorted_and_permutation);
}
