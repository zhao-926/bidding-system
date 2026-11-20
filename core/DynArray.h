// DynArray.h — 自刻動態陣列（資料結構 #1）
//
// 概念：底層是一塊 new[] 出來的連續記憶體。
//   size_     = 目前放了幾個元素
//   capacity_ = 這塊記憶體最多能放幾個元素
// 放滿時，配置一塊「兩倍大」的新記憶體，把舊資料搬過去，再釋放舊的。
// 這就是「容量翻倍」策略，所以 pushBack 的攤銷（amortized）時間是 O(1)。
//
// 用途：MaxHeap 的底層儲存、使用者的「我賣的／我出價的」商品清單。

#ifndef DYNARRAY_H
#define DYNARRAY_H

#include <cstdlib>   // std::abort：索引越界屬於程式錯誤，直接印訊息並終止
#include <iostream>

template <typename T>
class DynArray {
public:
    // O(1)
    DynArray() : data_(nullptr), size_(0), capacity_(0) {}

    // 複製建構：深拷貝（不能只複製指標，否則兩個物件會 delete 同一塊記憶體）
    // O(n)
    DynArray(const DynArray& other) : data_(nullptr), size_(0), capacity_(0) {
        copyFrom(other);
    }

    // 指定運算子：先釋放自己舊的記憶體，再深拷貝
    // O(n)
    DynArray& operator=(const DynArray& other) {
        if (this != &other) {            // 防止 a = a 把自己清掉
            delete[] data_;
            data_ = nullptr;
            size_ = 0;
            capacity_ = 0;
            copyFrom(other);
        }
        return *this;
    }

    // O(n)（要對每個元素呼叫解構子）
    ~DynArray() { delete[] data_; }

    // 在尾端加入一個元素
    // O(1) amortized（平常 O(1)；放滿時要搬資料 O(n)，但平均下來每次仍是 O(1)）
    void pushBack(const T& value) {
        if (size_ == capacity_) {
            grow();                      // 滿了，容量翻倍
        }
        data_[size_] = value;
        size_++;
    }

    // 移除最後一個元素（陣列不可為空）
    // O(1)
    void popBack() {
        if (size_ == 0) {
            fatal("popBack on empty array");
        }
        size_--;
    }

    // 取得第 index 個元素（可直接修改）；越界會終止程式
    // O(1)
    T& get(int index) {
        if (!isValidIndex(index)) {
            fatal("get index out of range");
        }
        return data_[index];
    }

    // 唯讀版本
    // O(1)
    const T& get(int index) const {
        if (!isValidIndex(index)) {
            fatal("get index out of range");
        }
        return data_[index];
    }

    // 設定第 index 個元素；越界會終止程式
    // O(1)
    void set(int index, const T& value) {
        if (!isValidIndex(index)) {
            fatal("set index out of range");
        }
        data_[index] = value;
    }

    // 最後一個元素（陣列不可為空）
    // O(1)
    T& back() {
        if (size_ == 0) {
            fatal("back on empty array");
        }
        return data_[size_ - 1];
    }

    // 刪除第 index 個元素，後面的元素整體往前補位；索引不合法回傳 false
    // O(n)（最壞要搬動 n-1 個元素）
    bool removeAt(int index) {
        if (!isValidIndex(index)) {
            return false;
        }
        for (int i = index; i < size_ - 1; i++) {
            data_[i] = data_[i + 1];     // 往前補位
        }
        size_--;
        return true;
    }

    // 線性搜尋：陣列內是否有等於 value 的元素
    // O(n)
    bool contains(const T& value) const {
        for (int i = 0; i < size_; i++) {
            if (data_[i] == value) {
                return true;
            }
        }
        return false;
    }

    // 清空（保留已配置的記憶體，之後可重複使用）
    // O(1)（對 int 這類簡單型別；不逐一呼叫解構子）
    void clear() { size_ = 0; }

    // O(1)
    bool isValidIndex(int index) const { return index >= 0 && index < size_; }

    // O(1)
    int size() const { return size_; }

    // O(1)
    int capacity() const { return capacity_; }

    // O(1)
    bool empty() const { return size_ == 0; }

private:
    T* data_;        // 指向連續記憶體
    int size_;       // 已使用數量
    int capacity_;   // 總容量

    // 容量翻倍（第一次從 0 變成 4）
    // O(n)
    void grow() {
        int newCapacity = (capacity_ == 0) ? 4 : capacity_ * 2;
        T* newData = new T[newCapacity];
        for (int i = 0; i < size_; i++) {
            newData[i] = data_[i];       // 把舊資料搬到新記憶體
        }
        delete[] data_;
        data_ = newData;
        capacity_ = newCapacity;
    }

    // 從 other 複製全部元素到自己（呼叫前自己必須是空的）
    // O(n)
    void copyFrom(const DynArray& other) {
        if (other.size_ == 0) {
            return;
        }
        capacity_ = other.size_;
        data_ = new T[capacity_];
        for (int i = 0; i < other.size_; i++) {
            data_[i] = other.data_[i];
        }
        size_ = other.size_;
    }

    static void fatal(const char* message) {
        std::cerr << "DynArray fatal error: " << message << std::endl;
        std::abort();
    }
};

#endif  // DYNARRAY_H
