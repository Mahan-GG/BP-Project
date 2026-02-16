#include "PenSystem.h"
#include <iostream>

void InitPenSystem(SDL_Renderer* renderer, int width, int height, PenState* pen) {
    pen->  canvasTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET, width, height);

    SDL_SetTextureBlendMode(pen->canvasTexture, SDL_BLENDMODE_BLEND);

    Pen_EraseAll(renderer, pen, width, height);

    pen ->isExtensionAdded =  true;;
    pen-> isDown = false ;
}

void CleanupPen(PenState* pen) {
    if (pen->canvasTexture)
        SDL_DestroyTexture(pen->canvasTexture);

}

void Pen_EraseAll(SDL_Renderer* renderer, PenState* pen, int w, int h) {
    SDL_SetRenderTarget(renderer, pen->canvasTexture);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    SDL_SetRenderTarget(renderer, NULL);
}

void Pen_Stamp(SDL_Renderer* renderer, PenState* pen, int x, int y) {
    SDL_SetRenderTarget(renderer, pen->canvasTexture);

    // استامپ: فعلاً یک مربع توپر هم‌رنگ قلم می‌کشیم
    // (در آینده اینجا عکس اسپرایت کشیده می‌شود)
    SDL_SetRenderDrawColor(renderer, pen->color.r, pen->color.g, pen->color.b, pen->color.a);
    SDL_Rect rect = {x - 10, y - 10, 20, 20};
    SDL_RenderFillRect(renderer, &rect);

    SDL_SetRenderTarget(renderer, NULL);
}

void Pen_Down(PenState* pen) {
    pen->isDown = true; }
void Pen_Up(PenState* pen) {
    pen->isDown = false;
    pen->lastX = -1;
}

void Pen_SetColor(PenState* pen, Uint8 r, Uint8 g, Uint8 b) { pen->color = {r, g, b, 255}; }
void Pen_SetSize(PenState* pen, int newSize) { if(newSize > 0) pen->size = newSize; }

void DrawThickLine(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int thickness) {

    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);

    // تاکتیک های ناشناخته برای ضخامت قلم :)
    for(int i=1; i < thickness; i++) {
        SDL_RenderDrawLine(renderer, x1+i, y1+i, x2+i, y2+i);
        SDL_RenderDrawLine(renderer, x1-i, y1-i, x2-i, y2-i);
    }
}

void Pen_Update(SDL_Renderer* renderer, PenState* pen, int mouseX, int mouseY) {
    if (!pen->isExtensionAdded)
    {return;}
    // بررسی فشرده شدن دکمه موس
    Uint32 mouseState = SDL_GetMouseState(NULL, NULL);
    bool isLeftPressed = (mouseState & SDL_BUTTON(SDL_BUTTON_LEFT));

    if (pen->isDown && isLeftPressed) {
        if (pen->lastX != -1 and pen->lastY != -1) {
            SDL_SetRenderTarget(renderer, pen->canvasTexture);
            SDL_SetRenderDrawColor(renderer, pen->color.r, pen->color.g, pen->color.b, pen->color.a);

            DrawThickLine(renderer, pen->lastX, pen->lastY, mouseX, mouseY, pen->size);

            SDL_SetRenderTarget(renderer, NULL);
        }
        //آپدیت موفعیت قلم
        pen->lastX = mouseX;
        pen->lastY = mouseY;
    }
    else
    {
        pen->lastX = -1;
        pen->lastY = -1;
    }
}

void Pen_Render(SDL_Renderer* renderer, PenState* pen) {
    if (pen->canvasTexture)SDL_RenderCopy(renderer, pen->canvasTexture, NULL, NULL);

}

void InitPenUI(Button* buttons, int& btnCount, int startX, int startY) {
    // دکمه های مربوط به قلم
    buttons[btnCount].rect = {startX, startY, 111, 31};
    buttons[btnCount].text = "Pen: DOWN";
    buttons[btnCount].isHovered = false;btnCount++;

    buttons[btnCount].rect = {startX +121, startY,111, 31};
    buttons[btnCount].text = "Set Color";
    buttons[btnCount].isHovered = false;btnCount++;

    buttons[btnCount].rect = {startX+ 241, startY, 80, 30};
    buttons[btnCount].text = "+Size";
    buttons[btnCount].isHovered = false;btnCount++;

    buttons[btnCount].rect = {startX + 330, startY, 110, 30};
    buttons[btnCount].text = "Erase All";
    buttons[btnCount].isHovered = false;
    btnCount++;
}
