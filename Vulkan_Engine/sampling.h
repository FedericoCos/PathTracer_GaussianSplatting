#pragma once

#include "../Helpers/GeneralHeaders.h"

namespace Sampling{
    float halton(int index, int base);
    void generateHaltonSamples(std::vector<RaySample> &sampling_points, int &num_rays);

    void generateStratifiedSamples(std::vector<RaySample> &sampling_points, int &num_rays);
};