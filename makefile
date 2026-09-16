CXX      := aarch64-linux-gnu-g++
STRIP    := aarch64-linux-gnu-strip

TARGET   := appops
BUILD_DIR := build

SRC_DIR  := src
SRCS     := $(wildcard $(SRC_DIR)/*.cc)
OBJS     := $(patsubst $(SRC_DIR)/%.cc,$(BUILD_DIR)/%.o,$(SRCS))

CXXFLAGS := -std=c++17 -O2 -Wall -Wextra \
            -I$(SRC_DIR) \
            -static \
            -pthread

LDFLAGS  := -static -pthread

.PHONY: all clean strip

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)
	@echo "==> Build done: $@"

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cc | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	@mkdir -p $@

# 可选：剥离符号，减小体积
strip: $(TARGET)
	$(STRIP) $(TARGET)
	@echo "==> Stripped: $(TARGET)"

clean:
	@rm -rf $(BUILD_DIR) $(TARGET)
	@echo "==> Cleaned"

check:
	@which $(CXX) || (echo "Error: $(CXX) not found"; exit 1)