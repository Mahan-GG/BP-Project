#ifndef FILE_MENU_H
#define  FILE_MENU_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include  <fstream>
#include <iostream>

// استراکت مربوط به بخش دکمه کار
// شامل ویژگیای رنگ و هاور و تکست داخلش و غیره
struct Button {
    SDL_Rect rect;
    SDL_Color color;
    SDL_Color hoverColor;
    bool isHovered;
    std::string text;

};

//استراکت مربوط به برنامه
struct AppContext {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer=   nullptr;
    TTF_Font*globalFont = nullptr;
    bool isRunning= false;

    // وضعیت پروژه
    std::string  statusMessage;  //   پیا می که  پایین صفحه  نمایش داده میشه
    int bgColorR = 242;
    int bgColorG = 242;
    int bgColorB = 242;
};

bool InitResources(AppContext* app); // لود کردن فونت
void InitFileMenu(Button buttons[], int& buttonCount);// مربوط به درست کردن نوار فایل
void DrawUI(AppContext* app, Button buttons[], int buttonCount);  // صفحه رابط  کاربری
void HandleFileMenuEvents(AppContext* app, Button buttons[], int buttonCount, SDL_Event* event); // ایونت های مربوط به بخش فایل


void NewProject(AppContext* app);
void SaveProject(AppContext* app);
void LoadProject(AppContext* app);

#endif
