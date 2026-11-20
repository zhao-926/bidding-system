// test_maxheap.cpp — MaxHeap 單元測試（含與暴力解比對的隨機測試）
#include "test_framework.h"
#include "test_random.h"
#include "../core/DynArray.h"
#include "../core/MaxHeap.h"

// 測試用元素：模仿 Bid（金額高者優先；金額相同時時間早者優先）
struct TestBid {
    int id;
    long long amount;
    long long time;

    TestBid() : id(0), amount(0), time(0) {}
    TestBid(int i, long long a, long long t) : id(i), amount(a), time(t) {}

    int getId() const { return id; }
    bool hasHigherPriority(const TestBid& other) const {
        if (amount != other.amount) {
            return amount > other.amount;
        }
        return time < other.time;
    }
};

// 空堆積：top 為 nullptr、pop 回傳 false
static void test_empty_heap() {
    MaxHeap<TestBid> heap;
    ASSERT_TRUE(heap.empty());
    ASSERT_EQ(0, heap.size());
    ASSERT_TRUE(heap.top() == nullptr);
    ASSERT_FALSE(heap.pop());
    ASSERT_FALSE(heap.removeById(1));
    ASSERT_TRUE(heap.isValid());
}

// push 之後 top 是金額最高者
static void test_push_and_top() {
    MaxHeap<TestBid> heap;
    heap.push(TestBid(1, 100, 1));
    ASSERT_EQ(1, heap.top()->id);
    heap.push(TestBid(2, 300, 2));
    ASSERT_EQ(2, heap.top()->id);
    heap.push(TestBid(3, 200, 3));
    ASSERT_EQ(2, heap.top()->id);
    ASSERT_EQ(3, heap.size());
    ASSERT_TRUE(heap.isValid());
}

// 連續 pop 的順序必須是金額由高到低
static void test_pop_order_descending() {
    MaxHeap<TestBid> heap;
    long long amounts[] = {50, 80, 20, 90, 10, 70, 60, 30, 40, 100};
    for (int i = 0; i < 10; i++) {
        heap.push(TestBid(i + 1, amounts[i], i));
    }
    long long previous = 1000000;
    bool descending = true;
    while (!heap.empty()) {
        long long current = heap.top()->amount;
        if (current > previous) {
            descending = false;
        }
        previous = current;
        heap.pop();
        ASSERT_TRUE(heap.isValid());
    }
    ASSERT_TRUE(descending);
}

// 金額相同時，時間早者優先（先出價者勝）
static void test_tie_break_earlier_time_wins() {
    MaxHeap<TestBid> heap;
    heap.push(TestBid(1, 500, 30));
    heap.push(TestBid(2, 500, 10));    // 時間最早
    heap.push(TestBid(3, 500, 20));
    ASSERT_EQ(2, heap.top()->id);
    heap.pop();
    ASSERT_EQ(3, heap.top()->id);
    heap.pop();
    ASSERT_EQ(1, heap.top()->id);
}

// 重複 id 不可加入
static void test_duplicate_id_rejected() {
    MaxHeap<TestBid> heap;
    ASSERT_TRUE(heap.push(TestBid(1, 100, 1)));
    ASSERT_FALSE(heap.push(TestBid(1, 999, 2)));
    ASSERT_EQ(1, heap.size());
    ASSERT_EQ(100, heap.top()->amount);
}

// removeById：刪堆頂（撤掉最高價 -> 最高價回到前一位）
static void test_remove_top_by_id() {
    MaxHeap<TestBid> heap;
    heap.push(TestBid(1, 100, 1));
    heap.push(TestBid(2, 200, 2));
    heap.push(TestBid(3, 300, 3));
    ASSERT_TRUE(heap.removeById(3));
    ASSERT_EQ(2, heap.top()->id);
    ASSERT_EQ(2, heap.size());
    ASSERT_FALSE(heap.contains(3));
    ASSERT_TRUE(heap.isValid());
}

// removeById：刪中間與最後一格的元素
static void test_remove_middle_and_last() {
    MaxHeap<TestBid> heap;
    for (int i = 1; i <= 15; i++) {
        heap.push(TestBid(i, i * 10, i));
    }
    ASSERT_TRUE(heap.removeById(8));
    ASSERT_TRUE(heap.isValid());
    ASSERT_TRUE(heap.removeById(1));   // 最小的，通常在葉節點
    ASSERT_TRUE(heap.isValid());
    ASSERT_EQ(13, heap.size());
    ASSERT_EQ(15, heap.top()->id);
}

// removeById：不存在的 id 回傳 false，堆積不變
static void test_remove_nonexistent() {
    MaxHeap<TestBid> heap;
    heap.push(TestBid(1, 100, 1));
    ASSERT_FALSE(heap.removeById(99));
    ASSERT_EQ(1, heap.size());
    ASSERT_TRUE(heap.removeById(1));
    ASSERT_FALSE(heap.removeById(1));  // 已刪過
    ASSERT_TRUE(heap.empty());
}

// 只剩一個元素時刪除
static void test_remove_only_element() {
    MaxHeap<TestBid> heap;
    heap.push(TestBid(7, 100, 1));
    ASSERT_TRUE(heap.removeById(7));
    ASSERT_TRUE(heap.empty());
    ASSERT_TRUE(heap.top() == nullptr);
    heap.push(TestBid(8, 5, 2));       // 刪空後可再使用
    ASSERT_EQ(8, heap.top()->id);
}

// 刪除後，被刪的 id 可以再次加入
static void test_reinsert_after_remove() {
    MaxHeap<TestBid> heap;
    heap.push(TestBid(1, 100, 1));
    heap.push(TestBid(2, 200, 2));
    heap.removeById(2);
    ASSERT_TRUE(heap.push(TestBid(2, 50, 3)));
    ASSERT_EQ(1, heap.top()->id);
    ASSERT_TRUE(heap.isValid());
}

// clear 與 at()
static void test_clear_and_at() {
    MaxHeap<TestBid> heap;
    heap.push(TestBid(1, 100, 1));
    heap.push(TestBid(2, 200, 2));
    ASSERT_EQ(2, heap.at(0).id);       // at(0) 就是堆頂
    heap.clear();
    ASSERT_TRUE(heap.empty());
    ASSERT_FALSE(heap.contains(1));
    ASSERT_TRUE(heap.isValid());
}

// 大量資料：push 5000 筆後依序 pop，順序必須正確
static void test_many_elements() {
    MaxHeap<TestBid> heap;
    SimpleRandom random(12345);
    for (int i = 0; i < 5000; i++) {
        heap.push(TestBid(i, random.next(100000), i));
    }
    ASSERT_TRUE(heap.isValid());
    long long previousAmount = 1000000;
    long long previousTime = -1;
    bool ordered = true;
    while (!heap.empty()) {
        const TestBid* t = heap.top();
        if (t->amount > previousAmount) {
            ordered = false;
        }
        if (t->amount == previousAmount && t->time < previousTime) {
            ordered = false;           // 同金額時，時間必須遞增（先到先出）
        }
        previousAmount = t->amount;
        previousTime = t->time;
        heap.pop();
    }
    ASSERT_TRUE(ordered);
}

// 暴力解：在 DynArray 裡線性掃描找最高者
static int bruteForceTopIndex(const DynArray<TestBid>& list) {
    int best = 0;
    for (int i = 1; i < list.size(); i++) {
        if (list.get(i).hasHigherPriority(list.get(best))) {
            best = i;
        }
    }
    return best;
}

// 隨機測試：隨機 push / pop / removeById 共 1000 次（重複 5 組種子），
// 每一步都與暴力解比對 size 與堆頂，並檢查堆積性質
static void test_random_against_brute_force() {
    bool allMatched = true;
    for (int seed = 1; seed <= 5; seed++) {
        SimpleRandom random(seed * 7919);
        MaxHeap<TestBid> heap;
        DynArray<TestBid> brute;
        int nextId = 1;
        for (int step = 0; step < 1000; step++) {
            int action = random.next(10);
            if (action < 5 || brute.empty()) {                  // 50%：push
                TestBid bid(nextId++, random.next(50), step);   // 金額範圍小，容易同價
                heap.push(bid);
                brute.pushBack(bid);
            } else if (action < 7) {                            // 20%：pop
                int top = bruteForceTopIndex(brute);
                heap.pop();
                brute.removeAt(top);
            } else {                                            // 30%：removeById 隨機一筆
                int victim = random.next(brute.size());
                int victimId = brute.get(victim).id;
                if (!heap.removeById(victimId)) {
                    allMatched = false;
                }
                brute.removeAt(victim);
            }
            if (heap.size() != brute.size() || !heap.isValid()) {
                allMatched = false;
            }
            if (!brute.empty()) {
                int top = bruteForceTopIndex(brute);
                if (heap.top()->id != brute.get(top).id) {
                    allMatched = false;
                }
            } else if (heap.top() != nullptr) {
                allMatched = false;
            }
        }
    }
    ASSERT_TRUE(allMatched);
}

void run_maxheap_tests() {
    RUN_TEST(test_empty_heap);
    RUN_TEST(test_push_and_top);
    RUN_TEST(test_pop_order_descending);
    RUN_TEST(test_tie_break_earlier_time_wins);
    RUN_TEST(test_duplicate_id_rejected);
    RUN_TEST(test_remove_top_by_id);
    RUN_TEST(test_remove_middle_and_last);
    RUN_TEST(test_remove_nonexistent);
    RUN_TEST(test_remove_only_element);
    RUN_TEST(test_reinsert_after_remove);
    RUN_TEST(test_clear_and_at);
    RUN_TEST(test_many_elements);
    RUN_TEST(test_random_against_brute_force);
}
