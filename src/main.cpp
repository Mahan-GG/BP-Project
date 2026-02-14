#include "FileMenu.h"
#include <iostream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>



Button buttons[10];
int buttonCount = 0;

// راه اندازی سیستم فونت
bool InitApp(AppContext* app) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        return false;

    if (TTF_Init() == -1) {
        std::cout << "TTF_Init Error: " << TTF_GetError() << std::endl;
        return false;
    }

    app->window = SDL_CreateWindow("SCRATCH!", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1024, 768, SDL_WINDOW_SHOWN);
    app->renderer = SDL_CreateRenderer(app->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    app->isRunning = true;
    app->statusMessage = "Welcome to Scratch!";

    // لود کردن فونت
    if (!InitResources(app))
    {return false;}

    InitFileMenu(buttons, buttonCount);

    return true;
}


void Render(AppContext* app) {
    // پس زمینه
    SDL_SetRenderDrawColor(app->renderer, app->bgColorR, app->bgColorG, app->bgColorB, 255);
    SDL_RenderClear(app->renderer);

    // رسم رابط کاربری
    DrawUI(app, buttons, buttonCount);

    SDL_RenderPresent(app->renderer);
}

// آخر کار که خواستیم تموم کنیم این تابعو صدا میزنیم
void Cleanup(AppContext* app) {
    if (app->globalFont) TTF_CloseFont(app->globalFont);
    SDL_DestroyRenderer(app->renderer);
    SDL_DestroyWindow(app->window);
    TTF_Quit();
    SDL_Quit();
}

int main(int argc, char* argv[]) {
    AppContext app;
    if (InitApp(&app)) {
        SDL_Event event;
        while (app.isRunning) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) app.isRunning = false;

                // بخش فایل***
                HandleFileMenuEvents(&app, buttons, buttonCount, &event);

                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_c) {
                    app.bgColorR = rand() % 255;
                    app.bgColorG = rand() % 255;
                    app.bgColorB = rand() % 255;
                    app.statusMessage = "Status: Color Changed (Press Save to keep it).";
                }
            }
            Render(&app);
        }
        Cleanup(&app);
    }

    return 0;
}
