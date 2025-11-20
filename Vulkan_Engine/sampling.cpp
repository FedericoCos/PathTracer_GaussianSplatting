#include "sampling.h"

float Sampling::halton(int index, int base)
{
    float f = 1.f;
    float r = 0.f;
    while(index > 0){
        f = f / (float)base;
        r = r + f * (index % base);
        index = index / base;
    }

    return r;
}

void Sampling::generateHaltonSamples(std::vector<RaySample> &sampling_points, int &num_rays)
{
    sampling_points.clear();
    sampling_points.resize(num_rays);

    for(int i = 0; i < num_rays; i++){
        float u = halton(i+1, 2);
        float v = halton(i+1, 3);

        sampling_points[i].uv = glm::vec2(u, v);
    }

    std::cout << "Generated " << sampling_points.size() << " Halton samples." << std::endl;
}

void Sampling::generateStratifiedSamples(std::vector<RaySample> &sampling_points, int &num_rays)
{
    // 1. Calculate grid dimensions (Square root)
    // We force a square grid to ensure even distribution on U and V
    int side = static_cast<int>(std::sqrt(num_rays));
    int actual_count = side * side;

    // 2. Update reference to reflect actual count used
    num_rays = actual_count;

    sampling_points.clear();
    sampling_points.resize(actual_count);

    // 3. RNG Setup
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);

    // 4. Generate Samples
    int i = 0;
    for (int y = 0; y < side; y++) {      // V axis
        for (int x = 0; x < side; x++) {  // U axis
            // Grid cell base + random offset
            float u = (x + dis(gen)) / static_cast<float>(side);
            float v = (y + dis(gen)) / static_cast<float>(side);

            sampling_points[i].uv = glm::vec2(u, v);
            i++;
        }
    }

    std::shuffle(sampling_points.begin(), sampling_points.end(), gen);

    std::cout << "Generated " << sampling_points.size() << " Stratified samples (" << side << "x" << side << " grid)." << std::endl;
}
