// test_random.h — 測試用的簡單亂數產生器（固定種子，每次執行結果都一樣，方便重現錯誤）
#ifndef TEST_RANDOM_H
#define TEST_RANDOM_H

class SimpleRandom {
public:
    explicit SimpleRandom(unsigned long long seed) : state_(seed) {}

    // 回傳 [0, bound) 之間的整數（bound 必須 > 0）
    int next(int bound) {
        // 線性同餘產生器（LCG）：state = state * a + c
        state_ = state_ * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<int>((state_ >> 33) % static_cast<unsigned long long>(bound));
    }

private:
    unsigned long long state_;
};

#endif  // TEST_RANDOM_H
