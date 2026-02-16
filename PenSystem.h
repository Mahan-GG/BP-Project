#ifndef PENSYSTEM_H
#define PENSYSTEM_H
#include <SDL2/SDL.h>
#include <vector>
// استراکت مربوط به قلم
struct PenState{
    bool isExtensionAdded = false;
    bool isDown = false;      // آماده نوشتن بودن یا نه
    int size = 5;        //سایز قلم
    SDL_Color color = {0, 155, 0, 255}; //رنگ سبز پررنگ
    SDL_Texture* canvasTexture = nullptr;
    // مختصات موس برای کشیدن
    int lastX = -1;
    int lastY = -1;
};
void InitPenSystem(SDL_Renderer* renderer, int width, int height, PenState* pen);
void CleanupPen(PenState* pen);
// تمام توابع مربوط به  قلم
void Pen_AddExtension(PenState* pen);
void Pen_EraseAll(SDL_Renderer* renderer, PenState* pen, int w, int h);
void Pen_Stamp(SDL_Renderer* renderer, PenState* pen, int x, int y);
void Pen_Down(PenState* pen);
void Pen_Up(PenState* pen);
void Pen_SetColor(PenState* pen, Uint8 r, Uint8 g, Uint8 b);
void Pen_SetSize(PenState* pen, int newSize);
void Pen_Update(SDL_Renderer* renderer, PenState* pen, int mouseX, int mouseY);
void Pen_Render(SDL_Renderer* renderer, PenState* pen);

#endif