#include "camera.h"


glm::mat4& Camera::getViewMatrix(){
    return view_matrix;
}

glm::mat4& Camera::getProjectionMatrix(){
    return projection_matrix;
}

void Camera::update(float &dtime, glm::vec4 &input){
    glm::vec3 delta_pos(0.f);
    glm::mat4 rot(1.f);

    if (input.w != 0.f) { // mouse X → yaw
        rot = glm::rotate(rot, input.w * sensitivity, up);
    }
    if (input.y != 0.f) { // mouse Y → pitch
        rot = glm::rotate(rot, input.y * sensitivity, glm::cross(direction, up));
    }

    direction = glm::normalize(rot * glm::vec4(direction, 0.f)); 
    up        = glm::normalize(rot * glm::vec4(up, 0.f));        


     if(input.z > 0.0f){
        delta_pos += glm::normalize(direction);
    }
    else if(input.z < 0.0f){
        delta_pos -= glm::normalize(direction);
    }

    if(input.x > 0.0f){
        delta_pos += glm::normalize(glm::cross(direction, up));
    }
    else if(input.x < 0.0f){
        delta_pos -= glm::normalize(glm::cross(direction, up));
    }



    
    if(glm::length(delta_pos) > 0)
        position += speed * dtime * glm::normalize(delta_pos);

    view_matrix = glm::lookAt(position, position + direction, up);
    projection_matrix = glm::perspective(glm::radians(fov), aspect_ratio, near_plane, far_plane);
    projection_matrix[1][1] *= -1;

}

