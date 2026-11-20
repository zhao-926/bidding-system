// MergeSort.js — 自刻合併排序（對應 core/MergeSort.h）
//
// 分而治之：切成左右兩半各自排序，再把兩個已排好的半邊合併。O(n log n)，穩定，額外空間 O(n)。
// 穩定：合併時只有「右邊嚴格小於左邊」才先取右邊，相等時一律先取左邊。
// 注意：不使用 Array.prototype.sort。

// 把 arr 的 [low, high) 區間排序；temp 是暫存空間（固定大小的陣列）
// O((high-low) * log(high-low))
function mergeSortRange(arr, temp, low, high, less) {
    if (high - low <= 1) {
        return;                                        // 0 或 1 個元素：已經排好
    }
    const mid = low + Math.floor((high - low) / 2);
    mergeSortRange(arr, temp, low, mid, less);         // 排左半 [low, mid)
    mergeSortRange(arr, temp, mid, high, less);        // 排右半 [mid, high)

    // 合併：left 指向左半、right 指向右半，每次把較小的那個放進 temp
    let left = low;
    let right = mid;
    let out = low;
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
    for (let i = low; i < high; i++) {                 // 把排好的結果寫回原陣列
        arr.set(i, temp[i]);
    }
}

// 對整個 DynArray 原地排序。less(a, b) 回傳 true 代表 a 應該排在 b 前面
// O(n log n)，穩定，額外空間 O(n)
export function mergeSort(arr, less) {
    const count = arr.size();
    if (count <= 1) {
        return;
    }
    const temp = new Array(count);
    mergeSortRange(arr, temp, 0, count, less);
}
