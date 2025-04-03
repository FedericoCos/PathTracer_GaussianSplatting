// Controls the camera

#include "camera.h"

void Camera::update(float& dt){
    glm::mat4 cameraRotation = getRotationMatrix();
    if(glm::length2(velocity) > 0)
        position += glm::vec3(cameraRotation * glm::vec4(glm::normalize(velocity) * speed * dt, 0.f));
}

void Camera::processSDLEvent(SDL_Event& e){
    if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_w) { velocity.z = -1; }
        if (e.key.keysym.sym == SDLK_s) { velocity.z = 1; }
        if (e.key.keysym.sym == SDLK_a) { velocity.x = -1; }
        if (e.key.keysym.sym == SDLK_d) { velocity.x = 1; }
    }

    if (e.type == SDL_KEYUP) {
        if (e.key.keysym.sym == SDLK_w) { velocity.z = 0; }
        if (e.key.keysym.sym == SDLK_s) { velocity.z = 0; }
        if (e.key.keysym.sym == SDLK_a) { velocity.x = 0; }
        if (e.key.keysym.sym == SDLK_d) { velocity.x = 0; }
    }

    if (e.type == SDL_MOUSEBUTTONDOWN) {
        SDL_GetMouseState(&mousePosition[0], &mousePosition[1]);
        _moving = true;
    }
    else if(e.type == SDL_MOUSEBUTTONUP){
        _moving = false;
    }

    if(e.type == SDL_MOUSEMOTION && _moving){
        int x, y;
        SDL_GetMouseState(&x, &y);

        yaw += (float)(x - mousePosition[0]) / 200.f;
        pitch -= (float)(y - mousePosition[1]) / 200.f;

        mousePosition[0] = x;
        mousePosition[1] = y;
    }
}

glm::mat4 Camera::getViewMatrix(){
    glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), position);
    glm::mat4 cameraRotation = getRotationMatrix();
    return glm::inverse(cameraTranslation * cameraRotation);
}

glm::mat4 Camera::getRotationMatrix(){
    glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3 {1.f, 0.f, 0.f });
    glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3 { 0.f, -1.f, 0.f });

    return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}
