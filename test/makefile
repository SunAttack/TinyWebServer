# Makefile - 编译 test.cpp（测试POOL、LOG、BUFFER模块）

# ================= 1. 变量定义 =================
# 编译器和编译选项
CXX = g++
CXXFLAGS = -Wall -std=c++17 -g
LDFLAGS = -L/usr/lib/x86_64-linux-gnu
LDLIBS = -lpthread -lmysqlclient
# 源文件和目标文件路径
SRC_DIR = ..
SRCS = test.cpp \
       $(SRC_DIR)/code/buffer/buffer.cpp \
       $(SRC_DIR)/code/log/log.cpp \
       $(SRC_DIR)/code/pool/sqlconnpoll.cpp
# 目标文件 （# 将 .cpp 映射成 build/*.o）
BUILD_DIR = ../build
OBJS = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(notdir $(SRCS)))
# 可执行文件
TARGET = test

# ================= 2. 伪目标防冲突 =================
.PHONY: all clean

# ================= 3. 默认目标：编译所有 =================
all: $(TARGET) 
# ================= 4. 链接规则 =================
# $(LDLIBS)必须在-o后面
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)
# ================= 5. 编译规则 =================
# 把每个 .o 都编译到 build/
$(BUILD_DIR)/%.o: $(SRC_DIR)/code/*/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/test.o: test.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@


# ================= 7. 清理规则 =================
clean:
	rm -rf $(OBJS) $(TARGET) log1 log2