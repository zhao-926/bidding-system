// MergeSort.h — 自刻合併排序（排序演算法）
//
// 概念（分而治之）：
//   1. 把陣列切成左右兩半，各自遞迴排序
//   2. 把兩個「已排好」的半邊合併成一個排好的陣列
// 每層合併總共走訪 n 個元素，共 log n 層，所以是 O(n log n)（最好、平均、最差都一樣）。
//
// 穩定（stable）：兩個元素「相等」時，原本在前面的仍然在前面。
//   做法是合併時，只有「右邊嚴格小於左邊」才先取右邊，相等時一律先取左邊。
//   好處：先依價格排序、再依出價數排序時，不會把前一次的排序結果打亂。
//
// 缺點：需要額外 O(n) 的暫存空間。

#ifndef MERGESORT_H
#define MERGESORT_H

#include "DynArray.h"

// 把 arr 的 [low, high) 區間排序；temp 是暫存空間（大小至少 high）
// O((high-low) * log(high-low))
template <typename T, typename Less>
void mergeSortRange(DynArray<T>& arr, T* temp, int low, int high, Less less) {
    if (high - low <= 1) {
        return;                                        // 0 或 1 個元素：已經排好
    }
    int mid = low + (high - low) / 2;
    mergeSortRange(arr, temp, low, mid, less);         // 排左半 [low, mid)
    mergeSortRange(arr, temp, mid, high, less);        // 排右半 [mid, high)

    // 合併：left 指向左半、right 指向右半，每次把較小的那個放進 temp
    int left = low;
    int right = mid;
    int out = low;
    while (left < mid && right < high) {
        if (less(arr.get(right), arr.get(left))) {
            temp[out] = arr.get(right);                // 右邊嚴格較小才先取右邊
            right++;
        } else {
            temp[out] = arr.get(left);                 // 相等時取左邊 -> 穩定
            left++;
        }
        out++;
    }
    while (left < mid) {                               // 左半還有剩
        temp[out++] = arr.get(left++);
    }
    while (right < high) {                             // 右半還有剩
        temp[out++] = arr.get(right++);
    }
    for (int i = low; i < high; i++) {                 // 把排好的結果寫回原陣列
        arr.set(i, temp[i]);
    }
}

// 對整個 DynArray 排序。less(a, b) 回傳 true 代表 a 應該排在 b 前面（例如 a < b 即由小到大）。
// O(n log n)，穩定，額外空間 O(n)
template <typename T, typename Less>
void mergeSort(DynArray<T>& arr, Less less) {
    int count = arr.size();
    if (count <= 1) {
        return;
    }
    T* temp = new T[count];
    mergeSortRange(arr, temp, 0, count, less);
    delete[] temp;
}

#endif  // MERGESORT_H
