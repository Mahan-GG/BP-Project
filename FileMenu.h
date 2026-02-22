#ifndef FILEMENU_H
#define FILEMENU_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>

struct BlockSystemContext;
struct SpriteContext;

struct Button {
    SDL_Rect rect;
    std::string label;
    SDL_Color color;
    SDL_Color hoverColor;
    bool isHovered = false;
};

struct AppContext {
    SDL_Window* window;
    SDL_Renderer* renderer;
    bool isRunning;
    TTF_Font* globalFont;
};

// وضعیت‌های منو برای مدیریت پنجره‌های پاپ‌آپ
struct MenuState {
    bool isSaving = false;       // آیا پنجره ذخیره باز است؟
    bool isLoading = false;      // آیا پنجره لود باز است؟
    std::string inputName = "";  // متنی که کاربر تایپ می‌کند
    std::vector<std::string> savedProjects; // لیست پروژه‌های پیدا شده
};

void InitResources(AppContext* app);
void InitFileMenu(Button* buttons, int& count, MenuState* menuState);
void DrawUI(AppContext* app, Button* buttons, int count, MenuState* menuState);
void HandleFileMenuEvents(AppContext* app, Button* buttons, int count, SDL_Event* e, BlockSystemContext* blockCtx, SpriteContext* spriteCtx, MenuState* menuState);

void SaveProject(std::string name, BlockSystemContext* blockCtx, SpriteContext* spriteCtx);
void LoadProject(SDL_Renderer* renderer, std::string name, BlockSystemContext* blockCtx, SpriteContext* spriteCtx);

#endif
