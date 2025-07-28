# Makefile - 编译 main.cpp

# ================= 1. 变量定义 =================
# 编译器和编译选项
CXX = g++
CXXFLAGS = -Wall -std=c++17
LDFLAGS = -L/usr/lib/x86_64-linux-gnu
LDLIBS = -lpthread -lmysqlclient
# 源文件和目标文件路径
SRC_DIR = ../code
SRCS = $(SRC_DIR)/main.cpp \
       $(SRC_DIR)/buffer/buffer.cpp \
       $(SRC_DIR)/log/log.cpp \
       $(SRC_DIR)/pool/sqlconnpoll.cpp \
	   $(SRC_DIR)/http/httpconn.cpp \
	   $(SRC_DIR)/http/httprequest.cpp \
	   $(SRC_DIR)/http/httpresponse.cpp \
	   $(SRC_DIR)/timer/heaptimer.cpp \
	   $(SRC_DIR)/server/epoller.cpp \
	   $(SRC_DIR)/server/webserver.cpp
# 目标文件 （# 将 .cpp 映射成 build/*.o）
BUILD_DIR = ../build
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRCS))	# 将 ../code/xxx.cpp 映射为 ../build/xxx.o
# 可执行文件
TARGET = ../bin/server

# ================= 2. 伪目标防冲突 =================
.PHONY: all clean

# ================= 默认目标：编译所有 =================
all: $(TARGET) 
# ================= 3. 链接规则 =================
$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)

# ================= 4. 编译规则 =================
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ================= 5. 清理规则 =================
clean:
	rm -rf $(BUILD_DIR)/*.o $(BUILD_DIR)/*/*.o $(TARGET)