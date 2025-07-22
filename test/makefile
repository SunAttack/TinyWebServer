# Makefile - 编译 test.cpp（测试POOL、LOG、BUFFER模块）

# ================= 1. 变量定义 =================
# 编译器和编译选项
CXX = g++
CXXFLAGS = -Wall -std=c++17
LDFLAGS = -L/usr/lib/x86_64-linux-gnu
LDLIBS = -lpthread -lmysqlclient
# 源文件和目标文件路径
SRCS = test.cpp \
       ../code/buffer/buffer.cpp \
       ../code/log/log.cpp \
       ../code/pool/sqlconnpoll.cpp
# 目标文件
OBJS = $(SRCS:.cpp=.o)
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
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ================= 6. 清理规则 =================
clean:
	rm -f $(OBJS) $(TARGET)