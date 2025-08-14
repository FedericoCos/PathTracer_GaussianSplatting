#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <algorithm>
#include <stdio.h>

#include <vulkan/vulkan_raii.hpp> // this library handles for us the vkCreateXXX
                                  // vkAllocateXXX, vkDestroyXXX, and vkFreeXXX
#include <vulkan/vk_platform.h>


// Taskflow is used for multithreading on the CPU side
#include "taskflow/taskflow.hpp"