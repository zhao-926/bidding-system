// test_main.cpp — 測試程式進入點：依序執行各資料結構／模組的測試
#include <iostream>

#include "test_framework.h"

// 各測試檔提供的函式（Phase 3 會在這裡繼續新增）
void run_dynarray_tests();
void run_linkedlist_tests();
void run_hashtable_tests();
void run_maxheap_tests();
void run_bst_tests();
void run_mergesort_tests();
void run_auction_tests();

int main() {
    std::cout << "== DynArray ==" << std::endl;
    run_dynarray_tests();

    std::cout << "== LinkedList ==" << std::endl;
    run_linkedlist_tests();

    std::cout << "== HashTable ==" << std::endl;
    run_hashtable_tests();

    std::cout << "== MaxHeap ==" << std::endl;
    run_maxheap_tests();

    std::cout << "== BST ==" << std::endl;
    run_bst_tests();

    std::cout << "== MergeSort ==" << std::endl;
    run_mergesort_tests();

    std::cout << "== AuctionSystem ==" << std::endl;
    run_auction_tests();

    return printTestSummary();
}
