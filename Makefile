CC = clang
CXX = clang++
EXE = build/synesthesia
IMGUI_DIR = ./vendor/imgui
KISSFFT_DIR = ./vendor/kissfft
SRC_DIR = ./src
BUILD_DIR = ./build

SOURCES = \
	$(SRC_DIR)/main.mm \
	$(SRC_DIR)/audio_input.mm \
	$(SRC_DIR)/colour_mapper.mm \
	$(SRC_DIR)/fft_processor.mm \
	$(IMGUI_DIR)/imgui.cpp \
	$(IMGUI_DIR)/imgui_demo.cpp \
	$(IMGUI_DIR)/imgui_draw.cpp \
	$(IMGUI_DIR)/imgui_tables.cpp \
	$(IMGUI_DIR)/imgui_widgets.cpp \
	$(IMGUI_DIR)/backends/imgui_impl_glfw.cpp \
	$(IMGUI_DIR)/backends/imgui_impl_metal.mm \
	$(KISSFFT_DIR)/kiss_fft.c

OBJS = $(addprefix $(BUILD_DIR)/, $(addsuffix .o, $(basename $(notdir $(SOURCES)))))

COMMON_FLAGS = -Wall -Wformat
INCLUDES = -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends -I$(KISSFFT_DIR) \
           -I/usr/local/include -I/opt/homebrew/include -I/opt/local/include

CXXFLAGS = -std=c++17 $(INCLUDES) $(COMMON_FLAGS) -O3 -ffast-math -march=native
CFLAGS = -std=c11 $(INCLUDES) $(COMMON_FLAGS)
OBJCFLAGS = -ObjC++ -fobjc-weak -fobjc-arc

FRAMEWORKS = -framework Metal -framework MetalKit -framework Cocoa \
             -framework IOKit -framework CoreVideo -framework QuartzCore
LIB_DIRS = -L/usr/local/lib -L/opt/homebrew/lib -L/opt/local/lib
LDLIBS = -lglfw -lportaudio -lm
LIBS = $(FRAMEWORKS) $(LIB_DIRS) $(LDLIBS)

vpath %.cpp $(SRC_DIR) $(IMGUI_DIR) $(IMGUI_DIR)/backends
vpath %.mm $(SRC_DIR) $(IMGUI_DIR)/backends
vpath %.c $(KISSFFT_DIR)

.PHONY: all clean run

all: $(EXE)
	@echo "Build complete"

clean:
	rm -rf $(BUILD_DIR)

run: $(EXE)
	./$(EXE)

app: $(EXE)
	rm -rf ./synesthesia.app
	./meta/appify.sh -s ./build/synesthesia -i ./meta/icon/app.icns

$(BUILD_DIR):
	@mkdir -p $@

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: %.mm | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OBJCFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(EXE): $(OBJS) | $(BUILD_DIR)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LIBS)