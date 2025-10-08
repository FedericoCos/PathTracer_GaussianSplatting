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
	glslc shaders/basic/fragment_torus.frag -o shaders/basic/fragment_torus.spv
	./$(TARGET)

# Clean up the build files
clean:
	rm -f $(TARGET) $(OBJS) shaders/*.spv