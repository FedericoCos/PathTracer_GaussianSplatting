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
            rot = glm::rotate(rot, input.look_x * current.f_camera.sensitivity * dtime, current.f_camera.up);
            input.look_x = 0.f;
        }
        if (input.look_y != 0.f) {
            rot = glm::rotate(rot, input.look_y * current.f_camera.sensitivity * dtime, glm::cross(current.f_camera.direction, current.f_camera.up));
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
    // --- Input Handling (Unchanged) ---
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
        current.t_camera.alpha += current.t_camera.alpha_speed * dtime;
    }
    else if(input.move.x > 0.0f){
        current.t_camera.alpha -= current.t_camera.alpha_speed * dtime;
    }
    if(input.move.y > 0.0f){
        current.t_camera.beta += current.t_camera.beta_speed * dtime; 
    }
    else if(input.move.y < 0.0f){
        current.t_camera.beta -= current.t_camera.beta_speed * dtime;     
    }

    // --- Logic Fixes Start Here ---

    // 1. Wrap Angles (0-360) instead of clamping
    current.t_camera.alpha = glm::mod(current.t_camera.alpha, 360.0f);
    if (current.t_camera.alpha < 0.0f) current.t_camera.alpha += 360.0f;

    current.t_camera.beta = glm::mod(current.t_camera.beta, 360.0f);
    if (current.t_camera.beta < 0.0f) current.t_camera.beta += 360.0f;

    // 2. Calculate Camera Position (Fixed on the major ring)
    float a_rad = glm::radians(current.t_camera.alpha);
    float b_rad = glm::radians(current.t_camera.beta);

    // Position on the main ring (at height h)
    current.t_camera.position = glm::vec3(cos(a_rad), 0.f, sin(a_rad)) * r + glm::vec3(0.f, h, 0.f);

    // 3. Calculate Orientation Vectors
    // Base Forward vector: Pointing from Camera Position to the Torus Center (0, h, 0)
    // Since position is (r*cos, h, r*sin), direction to center is (-cos, 0, -sin)
    glm::vec3 base_forward = glm::normalize(glm::vec3(-cos(a_rad), 0.0f, -sin(a_rad)));
    
    // Base Up vector (World Up)
    glm::vec3 base_up = glm::vec3(0.0f, 1.0f, 0.0f);

    // Calculate the Local Right Vector (Tangent to the major ring)
    // We rotate around THIS vector to pitch up/down (beta)
    glm::vec3 right = glm::normalize(glm::cross(base_forward, base_up));

    // 4. Create Rotation Matrix for Beta (Pitch)
    // Rotate beta degrees around the Right axis
    glm::mat4 rot_matrix = glm::rotate(glm::mat4(1.0f), b_rad, right);

    // 5. Apply Rotation to Forward and Up vectors
    // This allows the Up vector to flip upside down when beta > 90, preventing the "snap"
    glm::vec3 new_forward = glm::vec3(rot_matrix * glm::vec4(base_forward, 0.0f));
    glm::vec3 new_up      = glm::vec3(rot_matrix * glm::vec4(base_up, 0.0f));

    // 6. Set View Matrix
    // We look from Position, along new_forward, using new_up
    current.view_matrix = glm::lookAt(
        current.t_camera.position, 
        current.t_camera.position + new_forward, 
        new_up
    );

    current.projection_matrix = glm::perspective(glm::radians(current.fov), current.aspect_ratio, current.near_plane, current.far_plane);
    current.projection_matrix[1][1] *= -1;
}

void Camera::reset()
{
    current = original;
}

void Camera::updateToroidalAngles(float alpha_degrees, float beta_degrees, float radius, float height) {
    // Wrap angles
    current.t_camera.alpha = glm::mod(alpha_degrees, 360.0f);
    if (current.t_camera.alpha < 0.0f) current.t_camera.alpha += 360.0f;

    current.t_camera.beta = glm::mod(beta_degrees, 360.0f);
    if (current.t_camera.beta < 0.0f) current.t_camera.beta += 360.0f;

    float a_rad = glm::radians(current.t_camera.alpha);
    float b_rad = glm::radians(current.t_camera.beta);

    // Position
    current.t_camera.position = glm::vec3(cos(a_rad), 0.f, sin(a_rad)) * radius + glm::vec3(0.f, height, 0.f);

    // Orientation
    glm::vec3 base_forward = glm::normalize(glm::vec3(-cos(a_rad), 0.0f, -sin(a_rad)));
    glm::vec3 base_up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(base_forward, base_up));

    // Rotate Frame
    glm::mat4 rot_matrix = glm::rotate(glm::mat4(1.0f), b_rad, right);
    glm::vec3 new_forward = glm::vec3(rot_matrix * glm::vec4(base_forward, 0.0f));
    glm::vec3 new_up      = glm::vec3(rot_matrix * glm::vec4(base_up, 0.0f));

    // Matrix
    current.view_matrix = glm::lookAt(
        current.t_camera.position, 
        current.t_camera.position + new_forward, 
        new_up
    );

    current.projection_matrix = glm::perspective(glm::radians(current.fov), current.aspect_ratio, current.near_plane, current.far_plane);
    current.projection_matrix[1][1] *= -1;
}
