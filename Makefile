CC = clang
CXX = clang++

EXE = build/synesthesia
IMGUI_DIR = ./vendor/imgui/
KISSFFT_DIR = ./vendor/kissfft
SRC_DIR = ./src
BUILD_DIR = ./build

$(shell mkdir -p $(BUILD_DIR))

SOURCES += $(SRC_DIR)/main.mm $(SRC_DIR)/audio_input.mm $(SRC_DIR)/colour_mapper.mm $(SRC_DIR)/fft_processor.mm
SOURCES += $(IMGUI_DIR)/imgui.cpp $(IMGUI_DIR)/imgui_demo.cpp $(IMGUI_DIR)/imgui_draw.cpp $(IMGUI_DIR)/imgui_tables.cpp $(IMGUI_DIR)/imgui_widgets.cpp
SOURCES += $(IMGUI_DIR)/backends/imgui_impl_glfw.cpp $(IMGUI_DIR)/backends/imgui_impl_metal.mm
SOURCES += $(KISSFFT_DIR)/kiss_fft.c

OBJS = $(patsubst %, $(BUILD_DIR)/%, $(addsuffix .o, $(basename $(notdir $(SOURCES)))))

LIBS = -framework Metal -framework MetalKit -framework Cocoa -framework IOKit -framework CoreVideo -framework QuartzCore
LIBS += -L/usr/local/lib -L/opt/homebrew/lib -L/opt/local/lib
LIBS += -lglfw
LIBS += -lportaudio
LIBS += -lm

CXXFLAGS = -std=c++17 -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends -I$(KISSFFT_DIR) -I/usr/local/include -I/opt/homebrew/include -I/opt/local/include
CXXFLAGS += -Wall -Wformat
CFLAGS = -std=c11 -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends -I$(KISSFFT_DIR) -I/usr/local/include -I/opt/homebrew/include -I/opt/local/include
CFLAGS += -Wall -Wformat

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: $(IMGUI_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: $(IMGUI_DIR)/backends/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.mm
	$(CXX) $(CXXFLAGS) -ObjC++ -fobjc-weak -fobjc-arc -c -o $@ $<

$(BUILD_DIR)/%.o: $(IMGUI_DIR)/backends/%.mm
	$(CXX) $(CXXFLAGS) -ObjC++ -fobjc-weak -fobjc-arc -c -o $@ $<

$(BUILD_DIR)/%.o: $(KISSFFT_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

all: $(EXE)
	@echo Build complete

$(EXE): $(OBJS)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LIBS)

clean:
	rm -rf $(BUILD_DIR)

run: $(EXE)
	./build/synesthesia