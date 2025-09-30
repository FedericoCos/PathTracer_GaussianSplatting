#pragma once

#include "../Helpers/GeneralHeaders.h"

class Camera{
public:
    Camera(glm::vec3 position, glm::vec3 direction, glm::vec3 up,
            float fov, float aspect_ratio, float near, float far) : 
            position{position}, direction{direction}, up{up},
            fov{fov}, aspect_ratio{aspect_ratio}, 
            near_plane{near}, far_plane{far}
    {}

    Camera ():
    position{glm::vec3(2.f, 2.f, 2.f)}, direction{glm::vec3(-2.f, -2.f, -2.f)},
    up{glm::vec3(0.f, 0.f, 1.f)}, fov{45.f}, aspect_ratio{1.6f},
    near_plane{0.1f}, far_plane{10.f}
    {}


    glm::mat4& getViewMatrix();
    glm::mat4& getProjectionMatrix();

    void update(float &dtime, glm::vec4 &input);

    void setSpeed(float &speed){
        this -> speed = speed;
    }

    void setRotSpeed(float &rot_speed){
        this -> rot_speed = rot_speed;
    }

    void setFov(float &fov){
        this -> fov = fov;
    }

    void setNearPlane(float &near_plane){
        this -> near_plane = near_plane;
    }

    void setFarPlane(float &far_plane){
        this -> far_plane = far_plane;
    }

private:
    glm::vec3 position;
    glm::vec3 direction;
    glm::vec3 up;

    float fov;
    float near_plane, far_plane;
    float aspect_ratio;

    glm::mat4 view_matrix;
    glm::mat4 projection_matrix;

    float speed = 2.5f;
    float rot_speed = 1.5f;
    float sensitivity = 0.002f; 
};