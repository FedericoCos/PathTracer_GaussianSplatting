#pragma once

#include "../Helpers/GeneralHeaders.h"

class Camera{
public:
    Camera (float aspect_ratio){
        current.aspect_ratio = aspect_ratio;
        original.aspect_ratio = aspect_ratio;
    }

    Camera(){
        current.aspect_ratio = 1280.0 / 800.0;
        original.aspect_ratio = 1280.0 / 800.0;
    }


    glm::mat4& getViewMatrix();
    glm::mat4& getProjectionMatrix();
    

    void update(float &dtime, InputState &input, float &r, float &h);
    void freeCameraUpdate(float &dtime, InputState &input);
    void toroidalUpdate(float &dtime, InputState &input, float &r, float &h);
    void reset();

    void updateToroidalAngles(float alpha_degrees, float beta_degrees, float radius, float height);

    void modAspectRatio(float aspect_ratio){
        current.aspect_ratio = aspect_ratio;
        original.aspect_ratio = aspect_ratio;
    }

    void modSpeed(float ds){
        std::cout << "Modifying camera speed" << std::endl;
        current.f_camera.speed += ds;
        current.f_camera.speed = current.f_camera.speed > 0 ? current.f_camera.speed : 0.f;
        std::cout << "Current speed: " << current.f_camera.speed << std::endl << std::endl;
    }

    void modRot(float ds){
        std::cout << "Modifying rotation speed" << std::endl;
        current.f_camera.sensitivity += ds;
        current.f_camera.sensitivity = current.f_camera.sensitivity > 0 ? current.f_camera.sensitivity : 0.f;
        std::cout << "Current speed: " << current.f_camera.sensitivity << std::endl << std::endl;
    }

    void modFov(float ds){
        std::cout << "Modifying FOV" << std::endl;
        current.fov += ds;
        current.fov = current.fov > 0 ? current.fov : 0.f;
        current.fov = current.fov > 180 ? 180 : current.fov;
        std::cout << "Current FOV: " << current.fov << std::endl << std::endl;
    }

    void modAlphaSpeed(float ds){
        std::cout << "Modifying alpha speed" << std::endl;
        current.t_camera.alpha_speed += ds;
        current.t_camera.alpha_speed = current.t_camera.alpha_speed > 0 ? current.t_camera.alpha_speed : 0.f;
        std::cout << "Current speed: " << current.t_camera.alpha_speed << std::endl << std::endl;
    }

    void modBetaSpeed(float ds){
        std::cout << "Modifying beta speed" << std::endl;
        current.t_camera.beta_speed += ds;
        current.t_camera.beta_speed = current.t_camera.beta_speed > 0 ? current.t_camera.beta_speed : 0.f;
        std::cout << "Current speed: " << current.t_camera.beta_speed << std::endl << std::endl;
    }

    CameraState& getCurrentState(){
        return current;
    }

private:
    CameraState current, original;

    static constexpr float speed_incr = 0.5f;
    static constexpr float rot_incr = 0.001f;
    static constexpr float fov_incr = 5.f;
    static constexpr float alpha_incr = 0.05f;
    static constexpr float beta_incr = 0.05f;
    static constexpr float radius_incr = 0.5f;
};