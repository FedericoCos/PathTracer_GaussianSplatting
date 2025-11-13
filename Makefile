CXX = g++
CFLAGS = -std=c++20 -O2 -Iheaders $(shell sdl2-config --cflags)
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi $(shell sdl2-config --libs)


# List of source files
SRCS = $(wildcard **/*.cpp) $(wildcard *.cpp)

# Automatically generate a list of object files
OBJS = $(SRCS:.cpp=.o)

# Name of the executable
TARGET = Engine

# Default target
all: $(TARGET)

# Link the object files to create the executable
$(TARGET): $(OBJS)
	$(CXX) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# Compile each .cpp file into a .o file
%.o: %.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

# Phony targets
.PHONY: all clean test

# Run the program
test: $(TARGET)
	glslc shaders/basic/vertex.vert -o shaders/basic/vertex.spv
	glslc shaders/basic/fragment.frag -o shaders/basic/fragment.spv
	glslc shaders/basic/vertex_torus.vert -o shaders/basic/vertex_torus.spv
	glslc shaders/basic/fragment_torus.frag -o shaders/basic/fragment_torus.spv
	glslc shaders/basic/oit_write.frag -o shaders/basic/oit_write.spv
	glslc shaders/basic/oit_ppll_write.frag -o shaders/basic/oit_ppll_write.spv
	glslc shaders/basic/oit_composite.vert -o shaders/basic/oit_composite_vert.spv
	glslc shaders/basic/oit_composite.frag -o shaders/basic/oit_composite_frag.spv
	glslc shaders/basic/oit_ppll_composite.frag -o shaders/basic/oit_ppll_composite_frag.spv
	glslc shaders/basic/shadow.vert -o shaders/basic/shadow_vert.spv
	glslc shaders/basic/shadow.frag -o shaders/basic/shadow_frag.spv
	glslc -fshader-stage=rgen --target-env=vulkan1.4 shaders/rt_datacollect/raygen.rgen -o shaders/rt_datacollect/raygen.rgen.spv
	glslc -fshader-stage=rchit --target-env=vulkan1.4 shaders/rt_datacollect/closesthit.rchit -o shaders/rt_datacollect/closesthit.rchit.spv
	glslc -fshader-stage=rmiss --target-env=vulkan1.4 shaders/rt_datacollect/miss.rmiss -o shaders/rt_datacollect/miss.rmiss.spv
	glslc shaders/pointcloud/pointcloud.frag -o shaders/pointcloud/pointcloud.frag.spv
	glslc shaders/pointcloud/pointcloud.vert --target-env=vulkan1.4 -o shaders/pointcloud/pointcloud.vert.spv
	./$(TARGET)

# Clean up the build files
clean:
	rm -f $(TARGET) $(OBJS) shaders/*.spv