// Handle the camera obj

#include "vk_types.h"
#include <SDL2/SDL_events.h>

class Camera{
public:
    float speed = 20.f; 
    glm::vec3 velocity;
    glm::vec3 position;
    float pitch {0.f};
    float yaw { 0.f };

    bool _moving { false };
    std::vector<int> mousePosition = {0, 0};

    glm::mat4 getViewMatrix();
    glm::mat4 getRotationMatrix();

    void processSDLEvent(SDL_Event& e);

    void update(float& dt);


};



