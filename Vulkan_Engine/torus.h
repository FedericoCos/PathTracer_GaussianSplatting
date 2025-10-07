#pragma once

#include "../Helpers/GeneralHeaders.h"

class Torus{
public:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    Torus() = default;

    Torus(float major_radius, float minor_radius, int major_segments, int minor_segments){
        generateMesh(major_radius, minor_radius, major_segments, minor_segments);
    }

    void generateMesh(float R, float r, int N, int n){
        vertices.clear();
        indices.clear();

        for(int i = 0; i < N; i++){
            for(int j = 0; j < n; j++){
                float u = (float)i / N * 2.f * M_PI;
                float v = (float)j / n * 2.f * M_PI;

                Vertex vertex;
                vertex.pos.x = (R+r * cos(v)) * cos(u);
                vertex.pos.y = r * sin(v);
                vertex.pos.z = (R+r*cos(v)) * sin(u);

                vertex.color = glm::normalize(vertex.pos);
                vertex.tex_coord = { (float)i / N, (float) j / n};

                vertices.push_back(vertex);

                int next_i = (i+1) % N;
                int next_j = (j+1) % n;

                int current_idx = i * n + j;
                int next_j_idx = i * n + next_j;
                int next_i_idx = next_i * n + j;
                int next_ij_idx = next_i * n + next_j;

                indices.push_back(current_idx);
                indices.push_back(next_j_idx);
                indices.push_back(next_i_idx);

                indices.push_back(next_j_idx);
                indices.push_back(next_ij_idx);
                indices.push_back(next_i_idx);
            }
        }
    }
};
