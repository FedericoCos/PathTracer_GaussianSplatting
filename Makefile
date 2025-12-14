CXX = g++
CFLAGS = -std=c++20 -O2 -Iheaders $(shell sdl2-config --cflags)
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi $(shell sdl2-config --libs)

# List of source files
SRCS = $(wildcard **/*.cpp) $(wildcard *.cpp)

# Automatically generate a list of object files
OBJS = $(SRCS:.cpp=.o)

# Name of the executable
TARGET = Engine

# --- Shader Compilation Commands (defined once to keep it DRY) ---
define COMPILE_SHADERS
    glslc -fshader-stage=rgen --target-env=vulkan1.4 shaders/rt_datacollect/raygen.rgen -o shaders/rt_datacollect/raygen.rgen.spv
    glslc -fshader-stage=rchit --target-env=vulkan1.4 shaders/rt_datacollect/closesthit.rchit -o shaders/rt_datacollect/closesthit.rchit.spv
    glslc -fshader-stage=rmiss --target-env=vulkan1.4 shaders/rt_datacollect/miss.rmiss -o shaders/rt_datacollect/miss.rmiss.spv
    glslc -fshader-stage=rmiss --target-env=vulkan1.4 shaders/rt_datacollect/shadow.rmiss -o shaders/rt_datacollect/shadow.rmiss.spv
    glslc -fshader-stage=rahit --target-env=vulkan1.4 shaders/rt_datacollect/alpha.rahit -o shaders/rt_datacollect/alpha.rahit.spv
    glslc --target-env=vulkan1.4 shaders/rt_datacollect/lookup.comp -o shaders/rt_datacollect/lookup.comp.spv
    

    glslc -fshader-stage=rgen --target-env=vulkan1.4 shaders/rt_render/raygen_camera.rgen -o shaders/rt_render/raygen_camera.rgen.spv
    glslc -fshader-stage=rchit --target-env=vulkan1.4 shaders/rt_render/closesthit.rchit -o shaders/rt_render/closesthit.rchit.spv
    glslc -fshader-stage=rmiss --target-env=vulkan1.4 shaders/rt_render/miss.rmiss -o shaders/rt_render/miss.rmiss.spv
    glslc -fshader-stage=rmiss --target-env=vulkan1.4 shaders/rt_render/shadow.rmiss -o shaders/rt_render/shadow.rmiss.spv
    glslc -fshader-stage=rahit --target-env=vulkan1.4 shaders/rt_render/alpha.rahit -o shaders/rt_render/alpha.rahit.spv

    glslc shaders/pointcloud/pointcloud.frag -o shaders/pointcloud/pointcloud.frag.spv
    glslc shaders/pointcloud/pointcloud.vert --target-env=vulkan1.4 -o shaders/pointcloud/pointcloud.vert.spv
endef

# Default target
all: $(TARGET)

# Link the object files to create the executable
$(TARGET): $(OBJS)
	$(CXX) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# Compile each .cpp file into a .o file
%.o: %.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

# --- Target Specific Rules ---

# 1. Normal Test (Validation Layers ON)
test: $(TARGET)
	$(COMPILE_SHADERS)
	./$(TARGET)

# 2. Run / Release (Validation Layers OFF)
# We append -DNDEBUG to CFLAGS specifically for this target.
# NOTE: If you previously built with 'make', you must 'make clean' first
# for this flag to take effect on existing object files.
run: CFLAGS += -DNDEBUG
run: $(TARGET)
	$(COMPILE_SHADERS)
	./$(TARGET)

# Clean up the build files
clean:
	rm -f $(TARGET) $(OBJS) shaders/*.spv shaders/rt_datacollect/*.spv shaders/pointcloud/*.spv

# Phony targets
.PHONY: all clean test run