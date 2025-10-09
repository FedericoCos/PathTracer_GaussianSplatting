#pragma once

#include "../Helpers/GeneralHeaders.h"
#include "gameobject.h"

class P_object : public Gameobject{
public:
    using Gameobject::Gameobject;

    bool inputUpdate(InputState &input, float &dtime){
       return false; 
    }

private:


};