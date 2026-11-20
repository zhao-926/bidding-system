// test_dynarray.cpp — DynArray 單元測試
#include <string>

#include "test_framework.h"
#include "../core/DynArray.h"

// 空陣列：大小為 0、容量為 0、isValidIndex 都為 false
static void test_empty_array() {
    DynArray<int> arr;
    ASSERT_EQ(0, arr.size());
    ASSERT_TRUE(arr.empty());
    ASSERT_EQ(0, arr.capacity());
    ASSERT_FALSE(arr.isValidIndex(0));
    ASSERT_FALSE(arr.isValidIndex(-1));
}

// pushBack 後可用 get 讀回，順序正確
static void test_push_back_and_get() {
    DynArray<int> arr;
    arr.pushBack(10);
    arr.pushBack(20);
    arr.pushBack(30);
    ASSERT_EQ(3, arr.size());
    ASSERT_FALSE(arr.empty());
    ASSERT_EQ(10, arr.get(0));
    ASSERT_EQ(20, arr.get(1));
    ASSERT_EQ(30, arr.get(2));
    ASSERT_EQ(30, arr.back());
}

// 放入遠超過初始容量的元素（觸發多次翻倍），資料仍正確
static void test_growth_keeps_data() {
    DynArray<int> arr;
    for (int i = 0; i < 1000; i++) {
        arr.pushBack(i * 2);
    }
    ASSERT_EQ(1000, arr.size());
    ASSERT_TRUE(arr.capacity() >= 1000);
    bool allCorrect = true;
    for (int i = 0; i < 1000; i++) {
        if (arr.get(i) != i * 2) {
            allCorrect = false;
        }
    }
    ASSERT_TRUE(allCorrect);
}

// set 會覆蓋指定位置；get 回傳的參考可直接修改
static void test_set_and_modify_by_reference() {
    DynArray<int> arr;
    arr.pushBack(1);
    arr.pushBack(2);
    arr.set(0, 99);
    ASSERT_EQ(99, arr.get(0));
    arr.get(1) = 55;
    ASSERT_EQ(55, arr.get(1));
}

// removeAt：刪頭、刪中間、刪尾，後面的元素要補位且順序不變
static void test_remove_at() {
    DynArray<int> arr;
    for (int i = 1; i <= 5; i++) {
        arr.pushBack(i);               // 1 2 3 4 5
    }
    ASSERT_TRUE(arr.removeAt(2));      // 刪中間 -> 1 2 4 5
    ASSERT_EQ(4, arr.size());
    ASSERT_EQ(4, arr.get(2));
    ASSERT_TRUE(arr.removeAt(0));      // 刪頭 -> 2 4 5
    ASSERT_EQ(2, arr.get(0));
    ASSERT_TRUE(arr.removeAt(arr.size() - 1));   // 刪尾 -> 2 4
    ASSERT_EQ(2, arr.size());
    ASSERT_EQ(4, arr.get(1));
}

// removeAt 索引不合法：回傳 false，內容不變
static void test_remove_at_invalid_index() {
    DynArray<int> arr;
    arr.pushBack(7);
    ASSERT_FALSE(arr.removeAt(-1));
    ASSERT_FALSE(arr.removeAt(1));
    ASSERT_FALSE(arr.removeAt(100));
    ASSERT_EQ(1, arr.size());
    ASSERT_EQ(7, arr.get(0));

    DynArray<int> empty;
    ASSERT_FALSE(empty.removeAt(0));   // 空陣列刪除
}

// popBack 與 clear；清空後可重複使用
static void test_pop_back_and_clear() {
    DynArray<int> arr;
    arr.pushBack(1);
    arr.pushBack(2);
    arr.popBack();
    ASSERT_EQ(1, arr.size());
    ASSERT_EQ(1, arr.back());
    arr.clear();
    ASSERT_TRUE(arr.empty());
    arr.pushBack(42);                  // 清空後再放
    ASSERT_EQ(1, arr.size());
    ASSERT_EQ(42, arr.get(0));
}

// contains 線性搜尋
static void test_contains() {
    DynArray<int> arr;
    ASSERT_FALSE(arr.contains(1));     // 空陣列
    arr.pushBack(5);
    arr.pushBack(8);
    ASSERT_TRUE(arr.contains(5));
    ASSERT_TRUE(arr.contains(8));
    ASSERT_FALSE(arr.contains(6));
}

// 複製建構是深拷貝：改副本不影響原本
static void test_copy_constructor_is_deep() {
    DynArray<int> original;
    original.pushBack(1);
    original.pushBack(2);
    DynArray<int> copy(original);
    copy.set(0, 100);
    copy.pushBack(3);
    ASSERT_EQ(1, original.get(0));
    ASSERT_EQ(2, original.size());
    ASSERT_EQ(100, copy.get(0));
    ASSERT_EQ(3, copy.size());
}

// 指定運算子：目標原本有資料會被取代；自己指定給自己不會壞
static void test_assignment_operator() {
    DynArray<int> a;
    a.pushBack(1);
    a.pushBack(2);
    a.pushBack(3);
    DynArray<int> b;
    b.pushBack(9);
    b = a;
    ASSERT_EQ(3, b.size());
    ASSERT_EQ(3, b.get(2));
    b.set(0, 50);
    ASSERT_EQ(1, a.get(0));            // a 不受影響

    DynArray<int>& self = a;
    a = self;                          // 自我指定
    ASSERT_EQ(3, a.size());
    ASSERT_EQ(1, a.get(0));

    DynArray<int> empty;
    b = empty;                         // 指定成空陣列
    ASSERT_TRUE(b.empty());
}

// 元素型別為 std::string 也能運作
static void test_string_elements() {
    DynArray<std::string> names;
    names.pushBack("alice");
    names.pushBack("bob");
    ASSERT_EQ(std::string("bob"), names.get(1));
    ASSERT_TRUE(names.contains("alice"));
    names.removeAt(0);
    ASSERT_EQ(std::string("bob"), names.get(0));
}

void run_dynarray_tests() {
    RUN_TEST(test_empty_array);
    RUN_TEST(test_push_back_and_get);
    RUN_TEST(test_growth_keeps_data);
    RUN_TEST(test_set_and_modify_by_reference);
    RUN_TEST(test_remove_at);
    RUN_TEST(test_remove_at_invalid_index);
    RUN_TEST(test_pop_back_and_clear);
    RUN_TEST(test_contains);
    RUN_TEST(test_copy_constructor_is_deep);
    RUN_TEST(test_assignment_operator);
    RUN_TEST(test_string_elements);
}
