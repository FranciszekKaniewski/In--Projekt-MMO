#include "../Vendor/SDL2/include/SDL2/SDL.h"

class UIElement {
private:
public:
    SDL_Rect rect;

    void draw() {};
    void setHover(bool hover) {};
    void handleClick() {};
    void clean() {};
};