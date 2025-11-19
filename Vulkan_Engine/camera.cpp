#include "camera.h"

glm::mat4& Camera::getViewMatrix(){
    return current.view_matrix;
}

glm::mat4& Camera::getProjectionMatrix(){
    return current.projection_matrix;
}

void Camera::update(float &dtime, InputState &input, float &r, float &h){
    if(!input.consumed){
        if(input.reset){
            reset();
            input.consumed = true;
        }
        else if(input.change){
            current.is_toroidal = !current.is_toroidal;
            input.consumed = true;
        }
    }

    if(current.is_toroidal){
        toroidalUpdate(dtime, input, r, h);
    }
    else{
        freeCameraUpdate(dtime, input);
    }
}

void Camera::freeCameraUpdate(float &dtime, InputState &input){
    if(!input.consumed){
        if(input.speed_up){
            modSpeed(speed_incr);
            input.consumed = true;
        }
        else if(input.speed_down){
            modSpeed(-speed_incr);
            input.consumed = true;
        }
        else if(input.rot_up){
            modRot(rot_incr);
            input.consumed = true;
        }
        else if(input.rot_down){
            modRot(-rot_incr);
            input.consumed = true;
        }
        else if(input.fov_up) {
            modFov(fov_incr);
            input.consumed = true;         
        }
        else if(input.fov_down){
            modFov(-fov_incr);
            input.consumed = true;
        }
    }

    glm::vec3 delta_pos(0.f);
    glm::mat4 rot(1.f);

    if(input.left_mouse){
        if (input.look_x != 0.f) {
            rot = glm::rotate(rot, input.look_x * current.f_camera.sensitivity, current.f_camera.up);
            input.look_x = 0.f;
        }
        if (input.look_y != 0.f) {
            rot = glm::rotate(rot, input.look_y * current.f_camera.sensitivity, glm::cross(current.f_camera.direction, current.f_camera.up));
            input.look_y = 0.f;
        }
    }

    current.f_camera.direction = glm::normalize(rot * glm::vec4(current.f_camera.direction, 0.f)); 
    current.f_camera.up        = glm::normalize(rot * glm::vec4(current.f_camera.up, 0.f));        

    if(input.move.y > 0.0f){
        delta_pos += glm::normalize(current.f_camera.direction);
    }
    else if(input.move.y < 0.0f){
        delta_pos -= glm::normalize(current.f_camera.direction);
    }

    if(input.move.x > 0.0f){
        delta_pos += glm::normalize(glm::cross(current.f_camera.direction, current.f_camera.up));
    }
    else if(input.move.x < 0.0f){
        delta_pos -= glm::normalize(glm::cross(current.f_camera.direction, current.f_camera.up));
    }
    
    if(glm::length(delta_pos) > 0)
        current.f_camera.position += current.f_camera.speed * dtime * glm::normalize(delta_pos);

    current.view_matrix = glm::lookAt(current.f_camera.position, current.f_camera.position + current.f_camera.direction, current.f_camera.up);
    current.projection_matrix = glm::perspective(glm::radians(current.fov), current.aspect_ratio, current.near_plane, current.far_plane);
    current.projection_matrix[1][1] *= -1;
}

void Camera::toroidalUpdate(float &dtime, InputState &input, float &r, float &h)
{  
    if(!input.consumed){
        if(input.speed_up){
            modAlphaSpeed(alpha_incr);
            input.consumed = true;
        }
        else if(input.speed_down){
            modAlphaSpeed(-alpha_incr);
            input.consumed = true;
        }
        else if(input.rot_up){
            modBetaSpeed(-beta_incr);
            input.consumed = true;
        }
        else if(input.rot_down){
            modBetaSpeed(beta_incr);
            input.consumed = true;
        }
        else if(input.fov_up) {
            modFov(fov_incr);
            input.consumed = true;         
        }
        else if(input.fov_down){
            modFov(-fov_incr);
            input.consumed = true;
        }
    }

    if(input.move.x < 0.0f){
        current.t_camera.alpha += current.t_camera.alpha_speed;
    }
    else if(input.move.x > 0.0f){
        current.t_camera.alpha -= current.t_camera.alpha_speed;
    }
    if(input.move.y > 0.0f){
        current.t_camera.beta += current.t_camera.beta_speed; 
    }
    else if(input.move.y < 0.0f){
        current.t_camera.beta -= current.t_camera.beta_speed;     
    }

    current.t_camera.alpha = current.t_camera.alpha > 360.f ? current.t_camera.alpha - 360.f : (current.t_camera.alpha < 0.f ? current.t_camera.alpha + 360.f : current.t_camera.alpha);
    current.t_camera.beta = glm::clamp(current.t_camera.beta, -89.f, 89.f);

    float a = glm::radians(current.t_camera.alpha);
    current.t_camera.postion = glm::vec3(cos(a), 0.f, sin(a)) * r + glm::vec3(0.f, h, 0.f);

    glm::vec3 look_at_target = glm::vec3(0.0f, h, 0.0f);

    float vertical_offset = r * tan(glm::radians(current.t_camera.beta));
    look_at_target.y += vertical_offset;

    current.view_matrix = glm::lookAt(current.t_camera.postion, look_at_target, glm::vec3(.0f, 1.f, .0f));
    current.projection_matrix = glm::perspective(glm::radians(current.fov), current.aspect_ratio, current.near_plane, current.far_plane);
    current.projection_matrix[1][1] *= -1;
}

void Camera::reset()
{
    current = original;
}

void Camera::updateToroidalAngles(float alpha_degrees, float beta_degrees, float radius, float height) {
    current.t_camera.alpha = alpha_degrees;
    current.t_camera.beta = glm::clamp(beta_degrees, -89.f, 89.f);
    
    // Since r and h are only available in update, we need to pass them or store them.
    // Assuming you store r and h references in Camera or pass them from Engine:
    
    // For now, re-using the logic from toroidalUpdate:
    float a = glm::radians(current.t_camera.alpha);

    // Recalculate position
    current.t_camera.postion = glm::vec3(cos(a), 0.f, sin(a)) * radius + glm::vec3(0.f, height, 0.f);

    glm::vec3 look_at_target = glm::vec3(0.0f, height, 0.0f);
    float vertical_offset = radius * tan(glm::radians(current.t_camera.beta));
    look_at_target.y += vertical_offset;

    current.view_matrix = glm::lookAt(current.t_camera.postion, look_at_target, glm::vec3(.0f, 1.f, .0f));
    current.projection_matrix = glm::perspective(glm::radians(current.fov), current.aspect_ratio, current.near_plane, current.far_plane);
    current.projection_matrix[1][1] *= -1;
}
