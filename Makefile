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
	glslc -fshader-stage=rgen --target-env=vulkan1.4 shaders/rt_datacollect/raygen.rgen -o shaders/rt_datacollect/raygen.rgen.spv
	glslc -fshader-stage=rgen --target-env=vulkan1.4 shaders/rt_datacollect/raygen_camera.rgen -o shaders/rt_datacollect/raygen_camera.rgen.spv
	glslc -fshader-stage=rchit --target-env=vulkan1.4 shaders/rt_datacollect/closesthit.rchit -o shaders/rt_datacollect/closesthit.rchit.spv
	glslc -fshader-stage=rmiss --target-env=vulkan1.4 shaders/rt_datacollect/miss.rmiss -o shaders/rt_datacollect/miss.rmiss.spv
	glslc -fshader-stage=rmiss --target-env=vulkan1.4 shaders/rt_datacollect/shadow.rmiss -o shaders/rt_datacollect/shadow.rmiss.spv
	glslc shaders/pointcloud/pointcloud.frag -o shaders/pointcloud/pointcloud.frag.spv
	glslc shaders/pointcloud/pointcloud.vert --target-env=vulkan1.4 -o shaders/pointcloud/pointcloud.vert.spv
	glslc --target-env=vulkan1.4 shaders/rt_datacollect/lookup.comp -o shaders/rt_datacollect/lookup.comp.spv
	./$(TARGET)

# Clean up the build files
clean:
	rm -f $(TARGET) $(OBJS) shaders/*.spv