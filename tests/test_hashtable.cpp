// test_hashtable.cpp — HashTable 單元測試
#include <string>

#include "test_framework.h"
#include "../core/HashTable.h"

// 空表：找不到任何 key、size 為 0、load factor 為 0
static void test_empty_table() {
    HashTable<int, int> table;
    ASSERT_EQ(0, table.size());
    ASSERT_TRUE(table.empty());
    ASSERT_TRUE(table.find(1) == nullptr);
    ASSERT_FALSE(table.contains(1));
    ASSERT_TRUE(table.loadFactor() == 0.0);
}

// 插入後可以找回，找不到的 key 回傳 nullptr
static void test_insert_and_find() {
    HashTable<int, std::string> table;
    ASSERT_TRUE(table.insert(1, "one"));
    ASSERT_TRUE(table.insert(2, "two"));
    ASSERT_EQ(2, table.size());
    ASSERT_TRUE(table.find(1) != nullptr);
    ASSERT_EQ(std::string("one"), *table.find(1));
    ASSERT_EQ(std::string("two"), *table.find(2));
    ASSERT_TRUE(table.find(3) == nullptr);
}

// 重複 key：回傳 false、覆蓋 value、size 不增加
static void test_insert_duplicate_key_updates() {
    HashTable<int, int> table;
    ASSERT_TRUE(table.insert(5, 100));
    ASSERT_FALSE(table.insert(5, 200));
    ASSERT_EQ(1, table.size());
    ASSERT_EQ(200, *table.find(5));
}

// 刪除存在的 key
static void test_remove_existing() {
    HashTable<int, int> table;
    table.insert(1, 10);
    table.insert(2, 20);
    ASSERT_TRUE(table.remove(1));
    ASSERT_EQ(1, table.size());
    ASSERT_TRUE(table.find(1) == nullptr);
    ASSERT_EQ(20, *table.find(2));
}

// 刪除不存在的 key：回傳 false，表不變（含空表）
static void test_remove_nonexistent() {
    HashTable<int, int> table;
    ASSERT_FALSE(table.remove(42));    // 空表
    table.insert(1, 10);
    ASSERT_FALSE(table.remove(42));
    ASSERT_EQ(1, table.size());
    ASSERT_EQ(10, *table.find(1));
}

// rehash：插入 1000 筆，bucket 數增長、負載因子 <= 0.75、資料全部還在
static void test_rehash_keeps_all_data() {
    HashTable<int, int> table;
    int initialBuckets = table.bucketCount();
    for (int i = 0; i < 1000; i++) {
        table.insert(i, i * 3);
    }
    ASSERT_EQ(1000, table.size());
    ASSERT_TRUE(table.bucketCount() > initialBuckets);
    ASSERT_TRUE(table.loadFactor() <= 0.75);
    bool allCorrect = true;
    for (int i = 0; i < 1000; i++) {
        const int* value = table.find(i);
        if (value == nullptr || *value != i * 3) {
            allCorrect = false;
        }
    }
    ASSERT_TRUE(allCorrect);
}

// 字串 key（使用者名稱用）
static void test_string_keys() {
    HashTable<std::string, int> table;
    table.insert("alice", 1);
    table.insert("bob", 2);
    table.insert("carol", 3);
    ASSERT_EQ(2, *table.find("bob"));
    ASSERT_TRUE(table.find("dave") == nullptr);
    ASSERT_TRUE(table.remove("alice"));
    ASSERT_FALSE(table.contains("alice"));
    ASSERT_TRUE(table.contains("carol"));
}

// 碰撞處理：所有鏈長度總和 == size；大量資料時鏈有長度 >= 2 的（確實發生碰撞仍能正確存取）
static void test_collisions_chain_consistency() {
    HashTable<int, int> table;
    for (int i = 0; i < 500; i++) {
        table.insert(i, i);
    }
    int totalLength = 0;
    for (int i = 0; i < table.bucketCount(); i++) {
        totalLength += table.bucketLength(i);
    }
    ASSERT_EQ(500, totalLength);
    ASSERT_TRUE(table.maxChainLength() >= 1);
    ASSERT_EQ(0, table.bucketLength(-1));                    // 不合法的 bucket 索引
    ASSERT_EQ(0, table.bucketLength(table.bucketCount()));
}

// 全部刪光後 size 為 0，且可再重新插入（包含 rehash 之後才刪除）
static void test_remove_all_then_reinsert() {
    HashTable<int, int> table;
    for (int i = 0; i < 300; i++) {
        table.insert(i, i);
    }
    bool allRemoved = true;
    for (int i = 0; i < 300; i++) {
        if (!table.remove(i)) {
            allRemoved = false;
        }
    }
    ASSERT_TRUE(allRemoved);
    ASSERT_EQ(0, table.size());
    ASSERT_TRUE(table.find(150) == nullptr);
    table.insert(150, 7);
    ASSERT_EQ(7, *table.find(150));
}

// forEach 每一筆恰好走訪一次
static void test_for_each_visits_all() {
    HashTable<int, int> table;
    for (int i = 1; i <= 100; i++) {
        table.insert(i, i * 2);
    }
    int count = 0;
    int keySum = 0;
    int valueSum = 0;
    table.forEach([&](const int& key, const int& value) {
        count++;
        keySum += key;
        valueSum += value;
    });
    ASSERT_EQ(100, count);
    ASSERT_EQ(5050, keySum);
    ASSERT_EQ(10100, valueSum);
}

// find 回傳指標可直接修改 value
static void test_find_allows_modification() {
    HashTable<int, int> table;
    table.insert(1, 10);
    *table.find(1) = 99;
    ASSERT_EQ(99, *table.find(1));
}

// clear 後為空，可重新使用
static void test_clear() {
    HashTable<int, int> table;
    for (int i = 0; i < 50; i++) {
        table.insert(i, i);
    }
    table.clear();
    ASSERT_EQ(0, table.size());
    ASSERT_TRUE(table.find(10) == nullptr);
    table.insert(10, 1);
    ASSERT_EQ(1, table.size());
}

// 複製建構與指定運算子都是深拷貝
static void test_copy_and_assignment() {
    HashTable<int, int> original;
    for (int i = 0; i < 100; i++) {
        original.insert(i, i);
    }
    HashTable<int, int> copy(original);
    original.remove(5);
    original.insert(1000, 1);
    ASSERT_EQ(100, copy.size());
    ASSERT_TRUE(copy.find(5) != nullptr);
    ASSERT_TRUE(copy.find(1000) == nullptr);

    HashTable<int, int> assigned;
    assigned.insert(-1, -1);
    assigned = copy;
    ASSERT_EQ(100, assigned.size());
    ASSERT_TRUE(assigned.find(-1) == nullptr);
    ASSERT_EQ(50, *assigned.find(50));

    HashTable<int, int>& self = assigned;
    assigned = self;                   // 自我指定
    ASSERT_EQ(100, assigned.size());
}

// 負數 key 也能正常運作
static void test_negative_keys() {
    HashTable<int, int> table;
    table.insert(-1, 1);
    table.insert(-100, 2);
    table.insert(0, 3);
    ASSERT_EQ(1, *table.find(-1));
    ASSERT_EQ(2, *table.find(-100));
    ASSERT_EQ(3, *table.find(0));
}

void run_hashtable_tests() {
    RUN_TEST(test_empty_table);
    RUN_TEST(test_insert_and_find);
    RUN_TEST(test_insert_duplicate_key_updates);
    RUN_TEST(test_remove_existing);
    RUN_TEST(test_remove_nonexistent);
    RUN_TEST(test_rehash_keeps_all_data);
    RUN_TEST(test_string_keys);
    RUN_TEST(test_collisions_chain_consistency);
    RUN_TEST(test_remove_all_then_reinsert);
    RUN_TEST(test_for_each_visits_all);
    RUN_TEST(test_find_allows_modification);
    RUN_TEST(test_clear);
    RUN_TEST(test_copy_and_assignment);
    RUN_TEST(test_negative_keys);
}
