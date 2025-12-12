#pragma once

#include "../Helpers/GeneralHeaders.h"

namespace Sampling{
    float halton(int index, int base);
    void generateHaltonSamples(std::vector<RaySample> &sampling_points, const int &num_rays);

    void generateStratifiedSamples(std::vector<RaySample> &sampling_points, const int &num_rays);


    void generateImportanceSamples(
        std::vector<RaySample> &sampling_points, 
        const int &num_rays, 
        const std::vector<RaySample>& prev_samples, 
        const std::vector<glm::vec4>& prev_colors,
        int grid_resolution = 256
    );

    void generateRandomSamples(std::vector<RaySample> &sampling_points, const int &num_rays);

    void generateUniformSamples(std::vector<RaySample> &sampling_points, const int &num_rays);

    void generateHitBasedImportanceSamples(
        std::vector<RaySample> &sampling_points, 
        const int &num_rays, 
        const std::vector<RaySample>& prev_samples, 
        const std::vector<float>& prev_flags, // <--- Uses flags (1.0 or -1.0) instead of color
        int grid_resolution = 256
    );

    void generateLatinHypercubeSamples(std::vector<RaySample> &sampling_points, const int &num_rays);

    void sortSamples(std::vector<RaySample> &samples);

    void updateSampling(const int &method_index, const int& num_rays, std::vector<RaySample> &sampling_points, 
        AllocatedBuffer &sample_data_buffer, AllocatedBuffer &hit_data_buffer, 
        VmaAllocator &vma_allocator, vk::raii::CommandPool &pool, vk::raii::Queue &queue, vk::raii::Device &device);
};