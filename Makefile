CXX = g++
CFLAGS = -std=c++17 -O2 -Iheaders
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi

# List of source files
SRCS = main.cpp

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
	./$(TARGET)

# Clean up the build files
clean:
	rm -f $(TARGET) $(OBJS)
