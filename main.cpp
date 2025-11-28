#include"Vulkan_Engine/engine.h"


int main(int argc, char * argv[]){
    Engine engine;
    int val = -1;

    if(argc > 1){
        val = std::atoi(argv[1]);
    }
    engine.init(val);

    engine.run();


    return 0;
}