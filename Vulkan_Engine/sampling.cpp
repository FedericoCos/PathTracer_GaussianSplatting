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
    sampling_points.clear();
    sampling_points.resize(num_rays);

    // Same grid logic as Uniform
    int cols = static_cast<int>(std::ceil(std::sqrt(num_rays)));
    int rows = static_cast<int>(std::ceil(static_cast<float>(num_rays) / cols));

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);

    for (int i = 0; i < num_rays; i++) {
        int y = i / cols;
        int x = i % cols;

        // Stratified: Random point *inside* the grid cell
        // Cell width = 1.0 / cols, Cell height = 1.0 / rows
        float u = (x + dis(gen)) / cols;
        float v = (y + dis(gen)) / rows;

        sampling_points[i].uv = glm::vec2(u, v);
    }

    // Shuffle to prevent "half-rendered scene" artifacts if GPU dispatch is clipped
    // std::shuffle(sampling_points.begin(), sampling_points.end(), gen);

    std::cout << "Generated " << num_rays << " Stratified samples." << std::endl;
}

void Sampling::generateImportanceSamples(
    std::vector<RaySample> &sampling_points, 
    int &num_rays, 
    const std::vector<RaySample>& prev_samples, 
    const std::vector<glm::vec4>& prev_colors,
    int grid_resolution)
{
    // 1. Create a 2D accumulation grid (for color average)
    // grid[y][x] stores accumulated color
    std::vector<glm::vec3> grid_colors(grid_resolution * grid_resolution, glm::vec3(0.0f));
    std::vector<float> grid_counts(grid_resolution * grid_resolution, 0.0f);

    // 2. Bin the previous samples into the grid
    for(size_t i = 0; i < prev_samples.size(); i++) {
        if (i >= prev_colors.size()) break;

        // UV is in [0, 1]. Map to [0, grid_res-1]
        int x = std::clamp(static_cast<int>(prev_samples[i].uv.x * grid_resolution), 0, grid_resolution - 1);
        int y = std::clamp(static_cast<int>(prev_samples[i].uv.y * grid_resolution), 0, grid_resolution - 1);
        int idx = y * grid_resolution + x;

        grid_colors[idx] += glm::vec3(prev_colors[i]); // Accumulate RGB
        grid_counts[idx] += 1.0f;
    }

    // 3. Compute average color per cell
    for(size_t i = 0; i < grid_colors.size(); i++) {
        if(grid_counts[i] > 0.0f) {
            grid_colors[i] /= grid_counts[i];
        }
    }

    // 4. Calculate Importance Map (Gradient Magnitude / Edge Detection)
    std::vector<float> importance_map(grid_resolution * grid_resolution, 0.0f);
    float total_weight = 0.0f;

    auto get_lum = [&](int x, int y) -> float {
        if (x < 0 || x >= grid_resolution || y < 0 || y >= grid_resolution) return 0.0f;
        glm::vec3 c = grid_colors[y * grid_resolution + x];
        return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b; // Luminance
    };

    for (int y = 0; y < grid_resolution; y++) {
        for (int x = 0; x < grid_resolution; x++) {
            // Simple central difference for gradient
            float dx = get_lum(x + 1, y) - get_lum(x - 1, y);
            float dy = get_lum(x, y + 1) - get_lum(x, y - 1);
            float gradient = std::sqrt(dx*dx + dy*dy);

            // Weight = Gradient + small epsilon (to ensure we still sample flat areas a little)
            float weight = gradient + 0.05f; 
            
            importance_map[y * grid_resolution + x] = weight;
            total_weight += weight;
        }
    }

    // 5. Build CDF (Cumulative Distribution Function)
    std::vector<float> cdf(grid_resolution * grid_resolution);
    float current_sum = 0.0f;
    for (size_t i = 0; i < importance_map.size(); i++) {
        current_sum += importance_map[i];
        cdf[i] = current_sum;
    }
    // Normalize CDF
    for (size_t i = 0; i < cdf.size(); i++) {
        cdf[i] /= total_weight;
    }

    // 6. Inverse Transform Sampling
    // Generate new samples based on the CDF
    sampling_points.clear();
    sampling_points.resize(num_rays);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);

    for (int i = 0; i < num_rays; i++) {
        float r = dis(gen); // Random number [0, 1]

        // Binary search for 'r' in CDF
        auto it = std::lower_bound(cdf.begin(), cdf.end(), r);
        int idx = static_cast<int>(std::distance(cdf.begin(), it));
        
        // Map index back to UV
        int y = idx / grid_resolution;
        int x = idx % grid_resolution;

        // Jitter within the cell for smooth distribution
        float u = (x + dis(gen)) / static_cast<float>(grid_resolution);
        float v = (y + dis(gen)) / static_cast<float>(grid_resolution);

        sampling_points[i].uv = glm::vec2(u, v);
    }

    std::cout << "Generated " << num_rays << " Importance samples based on previous frame." << std::endl;
}

// --- 1. COMPLETELY RANDOM ---
void Sampling::generateRandomSamples(std::vector<RaySample> &sampling_points, int &num_rays)
{
    sampling_points.clear();
    sampling_points.resize(num_rays);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);

    for(int i = 0; i < num_rays; i++){
        // Pure random U and V
        sampling_points[i].uv = glm::vec2(dis(gen), dis(gen));
    }
    std::cout << "Generated " << num_rays << " Random samples." << std::endl;
}

void Sampling::generateUniformSamples(std::vector<RaySample> &sampling_points, int &num_rays)
{
    sampling_points.clear();
    sampling_points.resize(num_rays);

    // Calculate Grid Dimensions
    int cols = static_cast<int>(std::ceil(std::sqrt(num_rays)));
    int rows = static_cast<int>(std::ceil(static_cast<float>(num_rays) / cols));

    for (int i = 0; i < num_rays; i++) {
        int y = i / cols;
        int x = i % cols;

        // Use (x + 0.5) to sample the CENTER of the grid cell
        // This avoids edge bias and ensures better coverage
        float u = (static_cast<float>(x) + 0.5f) / cols;
        float v = (static_cast<float>(y) + 0.5f) / rows;

        sampling_points[i].uv = glm::vec2(u, v);
    }

    std::cout << "Generated " << num_rays << " Uniform samples (" << cols << "x" << rows << " grid)." << std::endl;
}

// --- 3. HIT-BASED IMPORTANCE SAMPLING ---
void Sampling::generateHitBasedImportanceSamples(
    std::vector<RaySample> &sampling_points, 
    int &num_rays, 
    const std::vector<RaySample>& prev_samples, 
    const std::vector<float>& prev_flags,
    int grid_resolution)
{
    // 1. Create Accumulation Grid
    std::vector<float> grid_hits(grid_resolution * grid_resolution, 0.0f);
    std::vector<float> grid_counts(grid_resolution * grid_resolution, 0.0f);

    // 2. Binning
    for(size_t i = 0; i < prev_samples.size(); i++) {
        if (i >= prev_flags.size()) break;

        int x = std::clamp(static_cast<int>(prev_samples[i].uv.x * grid_resolution), 0, grid_resolution - 1);
        int y = std::clamp(static_cast<int>(prev_samples[i].uv.y * grid_resolution), 0, grid_resolution - 1);
        int idx = y * grid_resolution + x;

        // If flag > 0 (Hit), add 1.0. If flag < 0 (Miss), add 0.0.
        float is_hit = (prev_flags[i] > 0.0f) ? 1.0f : 0.0f;
        
        grid_hits[idx] += is_hit;
        grid_counts[idx] += 1.0f;
    }

    // 3. Compute Hit Probability per cell
    std::vector<float> importance_map(grid_resolution * grid_resolution, 0.0f);
    float total_weight = 0.0f;

    for(size_t i = 0; i < importance_map.size(); i++) {
        float hit_ratio = 0.0f;
        if(grid_counts[i] > 0.0f) {
            hit_ratio = grid_hits[i] / grid_counts[i];
        }

        // WEIGHT CALCULATION:
        // If hit_ratio is high (1.0), weight is high.
        // We add a small epsilon (0.01) so we don't completely ignore empty space
        // (otherwise if an object moves into the void, we'd never see it)
        float weight = hit_ratio + 0.01f; 
        
        importance_map[i] = weight;
        total_weight += weight;
    }

    // 4. Build CDF
    std::vector<float> cdf(grid_resolution * grid_resolution);
    float current_sum = 0.0f;
    for (size_t i = 0; i < importance_map.size(); i++) {
        current_sum += importance_map[i];
        cdf[i] = current_sum;
    }
    // Normalize
    for (size_t i = 0; i < cdf.size(); i++) {
        cdf[i] /= total_weight;
    }

    // 5. Inverse Transform Sampling (Generate new rays)
    sampling_points.clear();
    sampling_points.resize(num_rays);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);

    for (int i = 0; i < num_rays; i++) {
        float r = dis(gen);
        auto it = std::lower_bound(cdf.begin(), cdf.end(), r);
        int idx = static_cast<int>(std::distance(cdf.begin(), it));
        
        int y = idx / grid_resolution;
        int x = idx % grid_resolution;

        // Jitter within the cell
        float u = (x + dis(gen)) / static_cast<float>(grid_resolution);
        float v = (y + dis(gen)) / static_cast<float>(grid_resolution);

        sampling_points[i].uv = glm::vec2(u, v);
    }

    std::cout << "Generated " << num_rays << " Hit-Based Importance samples." << std::endl;
}