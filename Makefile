CXX = g++
CFLAGS = -std=c++20 -O2 -Iheaders $(shell sdl2-config --cflags)
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi $(shell sdl2-config --libs)


# List of source files
SRCS = $(wildcard **/*.cpp) $(wildcard *.cpp)

# Automatically generate a list of object files
OBJS = $(SRCS:.cpp=.o)

# Name of the executable
TARGET = VulkanTest

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
	glslc shaders/gradient.comp -o shaders/shader.comp.spv
	glslc shaders/sky.comp -o shaders/shader.sky.spv
	glslc shaders/gradient_color.comp -o shaders/shader.gradient_color.spv
	glslc shaders/colored_triangle.frag -o shaders/shader.triangleFrag.spv
	glslc shaders/colored_triangle.vert -o shaders/shader.triangleVertex.spv
	glslc shaders/colored_triangle_mesh.vert -o shaders/shader.triangleMeshVertex.spv
	./$(TARGET)

# Clean up the build files
clean:
	rm -f $(TARGET) $(OBJS)
