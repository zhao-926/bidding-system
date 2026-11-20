# 二手交易競標平台 Makefile
#
# 常用指令：
#   make test    編譯並執行單元測試
#   make check   檢查 core/ 沒有使用被禁止的 STL 容器
#   make cli     編譯 CLI 介面（build/auction_cli）
#   make bench   編譯並執行效能對照（自刻 vs STL）
#   make web     用 Emscripten 編譯 WebAssembly（見 scripts/build_web.sh）
#   make clean   刪除 build/
#
# Windows（MSYS2）若沒有 make 指令，請改用 mingw32-make，用法完全相同。

CXXFLAGS = -std=c++17 -Wall -Wextra -O2
BUILD    = build

# Windows 的執行檔會自動加 .exe
ifeq ($(OS),Windows_NT)
EXE = .exe
else
EXE =
endif

CORE_SRC = $(wildcard core/*.cpp)
CORE_HDR = $(wildcard core/*.h)
TEST_SRC = $(wildcard tests/*.cpp)
TEST_HDR = $(wildcard tests/*.h)

.PHONY: all test cli bench web check clean

all: test cli

# ---- 單元測試 ----
$(BUILD)/test_runner$(EXE): $(TEST_SRC) $(TEST_HDR) $(CORE_SRC) $(CORE_HDR)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_SRC) $(CORE_SRC)

test: $(BUILD)/test_runner$(EXE)
	./$(BUILD)/test_runner$(EXE)

# ---- CLI ----
$(BUILD)/auction_cli$(EXE): cli/main.cpp $(CORE_SRC) $(CORE_HDR)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -Icore -o $@ cli/main.cpp $(CORE_SRC)

cli: $(BUILD)/auction_cli$(EXE)

# ---- 效能對照（bench/ 是唯一允許使用 STL 的地方）----
$(BUILD)/bench$(EXE): bench/bench.cpp $(CORE_SRC) $(CORE_HDR)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -Icore -o $@ bench/bench.cpp $(CORE_SRC)

bench: $(BUILD)/bench$(EXE)
	./$(BUILD)/bench$(EXE)

# ---- WebAssembly ----
web:
	bash scripts/build_web.sh

# ---- 檢查 core/ 不得使用 STL 容器 ----
check:
	bash scripts/check_no_stl.sh

clean:
	rm -rf $(BUILD)
