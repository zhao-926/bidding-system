#!/usr/bin/env bash
# check_no_stl.sh — 檢查 core/ 內沒有使用被禁止的 STL 容器與 sort
#
# 規則（來自 PLAN.md 第 0 節）：
#   禁止標頭：vector list forward_list deque array map set unordered_map
#             unordered_set stack queue algorithm
#             （multimap 在 <map>、priority_queue 在 <queue>，標頭一併被擋下）
#   禁止符號：std::vector、std::sort、std::priority_queue ... 以及未加 std:: 的 vector<
#
# 找到違規就印出「檔案:行號」並 exit 1；全部乾淨 exit 0。
# 註解內的文字會先被去除，所以註解裡提到這些名稱不算違規。

cd "$(dirname "$0")/.." || exit 1

HEADERS='vector|list|forward_list|deque|array|map|set|unordered_map|unordered_set|stack|queue|algorithm'
NAMES='vector|list|forward_list|deque|array|map|set|unordered_map|unordered_set|multimap|stack|queue|priority_queue|sort|stable_sort'

PATTERN_INCLUDE="#[[:space:]]*include[[:space:]]*<(${HEADERS})>"
PATTERN_STD="std[[:space:]]*::[[:space:]]*(${NAMES})\\b"
PATTERN_BARE="\\b(vector|list|forward_list|deque|array|map|set|unordered_map|unordered_set|multimap|stack|queue|priority_queue)[[:space:]]*<"

violations=0
found_any_file=0

for file in core/*.h core/*.cpp; do
    [ -f "$file" ] || continue
    found_any_file=1
    # 先移除 // 行註解與同一行內的 /* ... */ 區塊註解，再逐行比對
    result=$(sed -e 's://.*$::' -e 's:/\*.*\*/::g' "$file" \
        | grep -nE -e "$PATTERN_INCLUDE" -e "$PATTERN_STD" -e "$PATTERN_BARE")
    if [ -n "$result" ]; then
        while IFS= read -r line; do
            echo "VIOLATION $file:$line"
        done <<< "$result"
        violations=1
    fi
done

if [ "$found_any_file" -eq 0 ]; then
    echo "check_no_stl: core/ 內沒有任何檔案可檢查" >&2
    exit 1
fi

if [ "$violations" -ne 0 ]; then
    echo "check_no_stl: FAILED（core/ 內不得使用 STL 容器或 std::sort）"
    exit 1
fi

echo "check_no_stl: OK（core/ 沒有使用被禁止的 STL 容器）"
exit 0
