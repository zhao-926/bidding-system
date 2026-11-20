#!/usr/bin/env bash
# build_web.sh — 網頁（JavaScript 版）沒有編譯步驟，web/ 資料夾本身就是要部署的靜態網站。
# 這個腳本只做「部署前檢查」：必要檔案是否齊全、JS 核心測試是否通過。
#
# 本機預覽：  python -m http.server -d web 8000   然後開 http://localhost:8000/

cd "$(dirname "$0")/.." || exit 1

missing=0
for file in web/index.html web/style.css web/app.js \
            web/js/core/DynArray.js web/js/core/LinkedList.js web/js/core/HashTable.js \
            web/js/core/MaxHeap.js web/js/core/BST.js web/js/core/MergeSort.js \
            web/js/core/Models.js web/js/core/AuctionSystem.js; do
    if [ ! -f "$file" ]; then
        echo "build_web: 缺少檔案 $file" >&2
        missing=1
    fi
done
if [ "$missing" -ne 0 ]; then
    exit 1
fi

# 網址在 GitHub Pages 會有 /bidding-system/ 子路徑，所以頁面裡的資源必須是相對路徑
if grep -nE '(src|href)="/' web/index.html; then
    echo "build_web: index.html 內有以 / 開頭的絕對路徑，請改成相對路徑" >&2
    exit 1
fi

if ! command -v node > /dev/null 2>&1; then
    echo "build_web: 找不到 node，略過 JS 測試（網頁檔案檢查已通過）。"
    exit 0
fi

node tests/js/test_core.mjs || exit 1
echo "build_web: OK（web/ 檔案齊全、JS 核心測試通過）"
echo "本機預覽：python -m http.server -d web 8000"
