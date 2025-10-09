#pragma once

#include "../Helpers/GeneralHeaders.h"
#include "gameobject.h"

class Torus : public Gameobject{
public:
    using Gameobject::Gameobject;

    // Default constructor
    Torus() = default;

    // Constructor that immediately generates the mesh
    Torus(float major_radius, float minor_radius, float h, int major_segments, int minor_segments){
        generateMesh(major_radius, minor_radius, h, major_segments, minor_segments);
    }

    Torus(float major_radius, float minor_radius, float h, int major_segments, int minor_segments, std::string path_vertex, std::string path_fragment){
        vertex_shader = path_vertex;
        fragment_shader = path_fragment;
        generateMesh(major_radius, minor_radius, h, major_segments, minor_segments);
    }

    void modMajRad(float ds){
        std::cout << "Modifying major radius of torus" << std::endl;
        major_radius += ds;
        if(major_radius < 1.f){
            major_radius = 1.f;
        }

        std::cout << "Current radius: " << major_radius << std::endl << std::endl;

        generateMesh(major_radius, minor_radius, height, N, n);
    }

    void modMinRad(float ds){
        std::cout << "Modifying minor radius of torus" << std::endl;
        minor_radius += ds;
        if(minor_radius < 1.f){
            minor_radius = 1.f;
        }
        std::cout << "Current radius: " << minor_radius << std::endl << std::endl;
        generateMesh(major_radius, minor_radius, height, N, n);
    }

    void modHeight(float ds){
        std::cout << "Modifying torus' height" << std::endl;
        height += ds;
        if(height < 0.f){
            height = 0.f;
        }
        std::cout << "Current height: " << height << std::endl << std::endl;
        generateMesh(major_radius, minor_radius, height, N, n);
    }

    /**
     * @brief Generates the vertex and index data for the torus mesh.
     * @param R The major radius (distance from the center of the hole to the center of the tube).
     * @param r The minor radius (the radius of the tube itself).
     * @param h The vertical offset (height) of the torus's center.
     * @param N The number of segments around the major radius.
     * @param n The number of segments around the minor radius.
     */
    void generateMesh(float R, float r, float h, int N, int n){
        // Store dimensions
        this->major_radius = R;
        this->minor_radius = r;
        this->height = h;
        this -> N = N;
        this -> n = n;

        vertices.clear();
        indices.clear();

        for(int i = 0; i < N; i++){
            for(int j = 0; j < n; j++){
                float u = (float)i / N * 2.f * (float)M_PI;
                float v = (float)j / n * 2.f * (float)M_PI;

                Vertex vertex;
                // Calculate position using parametric equations for a torus
                vertex.pos.x = (R + r * cos(v)) * cos(u);
                vertex.pos.y = r * sin(v) + h; // Apply the height offset
                vertex.pos.z = (R + r * cos(v)) * sin(u);

                float norm_x = cos(v) * cos(u);
                float norm_y = sin(v);
                float norm_z = cos(v) * sin(u);
                vertex.normal = glm::normalize(glm::vec3(norm_x, norm_y, norm_z));

                // Set color based on normalized position for a rainbow effect
                vertex.color = glm::normalize(vertex.pos);
                vertex.tex_coord = { (float)i / N, (float)j / n };

                vertices.push_back(vertex);

                // Calculate indices for the four corners of the current quad
                int next_i = (i + 1) % N;
                int next_j = (j + 1) % n;

                int i0 = i * n + j;
                int i1 = i * n + next_j;
                int i2 = next_i * n + j;
                int i3 = next_i * n + next_j;

                // Create two triangles for the quad
                indices.push_back(i0);
                indices.push_back(i1);
                indices.push_back(i2);

                indices.push_back(i1);
                indices.push_back(i3);
                indices.push_back(i2);
            }
        }
    }

    /**
     * @brief Projects a 3D point from the scene onto the closest point on this torus's surface.
     * @param scenePoint The 3D point to project.
     * @return The corresponding 3D point on the surface of the torus.
     */
    glm::vec3 projectPoint(const glm::vec3& scenePoint) {
        // 1. Flatten the scene point onto the XZ plane (ignoring height for now)
        glm::vec2 p_xz(scenePoint.x, scenePoint.z);

        // 2. Find the closest point 'C' on the major radius circle (the centerline of the torus)
        glm::vec2 c_xz = glm::normalize(p_xz) * this->major_radius;

        // 3. Convert that 2D point back to a 3D point on the centerline, including height
        glm::vec3 C(c_xz.x, this->height, c_xz.y);

        // 4. Find the vector from the centerline point C to the original scene point
        glm::vec3 direction_to_p = scenePoint - C;

        // 5. The final projected point is C plus the direction vector, scaled to the minor radius
        glm::vec3 projected_point = C + glm::normalize(direction_to_p) * this->minor_radius;

        return projected_point;
    }

    float &getHeight(){
        return height;
    }

    float &getRadius(){
        return major_radius;
    }
    


    bool inputUpdate(InputState &input, float &dtime){
        if(!input.consumed){
            if(input.maj_rad_up){
                modMajRad(maj_rad_incr);
                input.consumed = true;
                return true;
            }
            else if(input.maj_rad_down){
                modMajRad(-maj_rad_incr);
                input.consumed = true;
                return true;
            }
            else if(input.min_rad_up){
                modMinRad(min_rad_incr);
                input.consumed = true;
                return true;
            }
            else if(input.min_rad_down){
                modMinRad(-min_rad_incr);
                input.consumed = true; 
                return true;
            }
            else if(input.height_up){
                modHeight(height_incr);
                input.consumed = true;
                return true;
            }
            else if(input.height_down){
                modHeight(-height_incr);
                input.consumed = true;
                return true;
            }
        }
        return false;
    }

    void createDescriptorSets(Engine& engine);

private:
    float major_radius = 0.0f;
    float minor_radius = 0.0f;
    float height = 0.0f;

    int N;
    int n;

    static constexpr float maj_rad_incr = 0.5f;
    static constexpr float min_rad_incr = 0.1f;
    static constexpr float height_incr = 0.25f;
};
