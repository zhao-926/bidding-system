// test_linkedlist.cpp — LinkedList 單元測試
#include <string>

#include "test_framework.h"
#include "../core/LinkedList.h"

// 空串列：大小 0、front 為 nullptr、forEach 不會呼叫 callback
static void test_empty_list() {
    LinkedList<int> list;
    ASSERT_EQ(0, list.size());
    ASSERT_TRUE(list.empty());
    ASSERT_TRUE(list.front() == nullptr);
    int calls = 0;
    list.forEach([&calls](const int&) { calls++; });
    ASSERT_EQ(0, calls);
}

// pushFront：新的放最前面，走訪順序為「由新到舊」
static void test_push_front_order() {
    LinkedList<int> list;
    list.pushFront(1);
    list.pushFront(2);
    list.pushFront(3);
    ASSERT_EQ(3, list.size());
    ASSERT_EQ(3, *list.front());
    std::string order;
    list.forEach([&order](const int& v) { order += std::to_string(v); });
    ASSERT_EQ(std::string("321"), order);
}

// popFront 依序移除最新的元素
static void test_pop_front() {
    LinkedList<int> list;
    list.pushFront(1);
    list.pushFront(2);
    ASSERT_TRUE(list.popFront());
    ASSERT_EQ(1, list.size());
    ASSERT_EQ(1, *list.front());
    ASSERT_TRUE(list.popFront());
    ASSERT_TRUE(list.empty());
}

// 空串列 popFront：回傳 false，不會壞掉
static void test_pop_front_on_empty() {
    LinkedList<int> list;
    ASSERT_FALSE(list.popFront());
    ASSERT_EQ(0, list.size());
    list.pushFront(5);                 // 之後還能正常使用
    ASSERT_EQ(5, *list.front());
}

// find：找得到回傳指標，找不到回傳 nullptr
static void test_find_existing_and_missing() {
    LinkedList<int> list;
    list.pushFront(10);
    list.pushFront(20);
    list.pushFront(30);
    int* found = list.find([](const int& v) { return v == 20; });
    ASSERT_TRUE(found != nullptr);
    ASSERT_EQ(20, *found);
    int* missing = list.find([](const int& v) { return v == 99; });
    ASSERT_TRUE(missing == nullptr);

    LinkedList<int> empty;
    ASSERT_TRUE(empty.find([](const int&) { return true; }) == nullptr);
}

// find 回傳的指標可以直接修改串列內的元素
static void test_find_allows_modification() {
    LinkedList<int> list;
    list.pushFront(1);
    list.pushFront(2);
    int* target = list.find([](const int& v) { return v == 1; });
    *target = 100;
    std::string order;
    list.forEach([&order](const int& v) { order += std::to_string(v) + ","; });
    ASSERT_EQ(std::string("2,100,"), order);
}

// clear 之後為空，且可以重新使用
static void test_clear() {
    LinkedList<int> list;
    for (int i = 0; i < 10; i++) {
        list.pushFront(i);
    }
    list.clear();
    ASSERT_EQ(0, list.size());
    ASSERT_TRUE(list.front() == nullptr);
    list.pushFront(7);
    ASSERT_EQ(1, list.size());
    ASSERT_EQ(7, *list.front());
}

// 複製建構：深拷貝且順序相同
static void test_copy_constructor() {
    LinkedList<int> original;
    original.pushFront(1);
    original.pushFront(2);
    original.pushFront(3);
    LinkedList<int> copy(original);
    original.popFront();               // 改原本，不應影響副本
    ASSERT_EQ(3, copy.size());
    std::string order;
    copy.forEach([&order](const int& v) { order += std::to_string(v); });
    ASSERT_EQ(std::string("321"), order);
}

// 指定運算子：取代舊內容；自我指定安全；指定空串列
static void test_assignment_operator() {
    LinkedList<int> a;
    a.pushFront(1);
    a.pushFront(2);
    LinkedList<int> b;
    b.pushFront(99);
    b.pushFront(98);
    b.pushFront(97);
    b = a;
    ASSERT_EQ(2, b.size());
    ASSERT_EQ(2, *b.front());

    LinkedList<int>& self = a;
    a = self;
    ASSERT_EQ(2, a.size());

    LinkedList<int> empty;
    b = empty;
    ASSERT_TRUE(b.empty());
}

// 大量資料：size 計數正確、走訪筆數相同
static void test_many_elements() {
    LinkedList<int> list;
    for (int i = 0; i < 10000; i++) {
        list.pushFront(i);
    }
    ASSERT_EQ(10000, list.size());
    int visited = 0;
    list.forEach([&visited](const int&) { visited++; });
    ASSERT_EQ(10000, visited);
    ASSERT_EQ(9999, *list.front());
}

// 元素為字串（之後 Bid 結構同理）
static void test_string_elements() {
    LinkedList<std::string> list;
    list.pushFront("first");
    list.pushFront("second");
    const LinkedList<std::string>& constList = list;
    const std::string* found = constList.find([](const std::string& s) { return s == "first"; });
    ASSERT_TRUE(found != nullptr);
    ASSERT_EQ(std::string("second"), *list.front());
}

void run_linkedlist_tests() {
    RUN_TEST(test_empty_list);
    RUN_TEST(test_push_front_order);
    RUN_TEST(test_pop_front);
    RUN_TEST(test_pop_front_on_empty);
    RUN_TEST(test_find_existing_and_missing);
    RUN_TEST(test_find_allows_modification);
    RUN_TEST(test_clear);
    RUN_TEST(test_copy_constructor);
    RUN_TEST(test_assignment_operator);
    RUN_TEST(test_many_elements);
    RUN_TEST(test_string_elements);
}
