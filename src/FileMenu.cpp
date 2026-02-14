#include "FileMenu.h"

// لود کردن فونت
bool InitResources(AppContext* app) {
    app->globalFont = TTF_OpenFont("../assets/font.ttf", 24);
    if (!app->globalFont) {
        app->globalFont = TTF_OpenFont("assets/font.ttf", 24);
    }

    if (!app->globalFont) {
        std::cout << "Failed to load font! Error: " << TTF_GetError() << std::endl;
        return false;
    }
    return true;
}


void InitFileMenu(Button buttons[], int& buttonCount) {
    buttonCount = 3;
    int startX = 10;
    int topPadding = 5;
    int btnWidth = 100;
    int btnHeight = 30;

    // دکمه New
    buttons[0].rect = {startX, topPadding, btnWidth, btnHeight};
    buttons[0].color = {76, 151, 255, 255};
    buttons[0].hoverColor = {100, 180, 255, 255};
    buttons[0].text = "New";
    buttons[0].isHovered = false;

    // دکمه Save
    buttons[1].rect = {startX + 110, topPadding, btnWidth, btnHeight};
    buttons[1].color = {76, 151, 255, 255};
    buttons[1].hoverColor = {100, 180, 255, 255};
    buttons[1].text = "Save";
    buttons[1].isHovered = false;

    // دکمه Load
    buttons[2].rect = {startX + 220, topPadding, btnWidth, btnHeight};
    buttons[2].color = {76, 151, 255, 255};
    buttons[2].hoverColor = {100, 180, 255, 255};
    buttons[2].text = "Load";
    buttons[2].isHovered = false;
}

// تابع کمکی برای رسم متن وسط دکمه
void DrawTextCentered(SDL_Renderer* renderer, TTF_Font* font, std::string text, SDL_Rect rect) {
    if (!font)
    {return;}
    SDL_Color textColor = {254, 255, 254, 255};  //رنگ تکست
    SDL_Surface* surface =  TTF_RenderText_Blended(font, text.c_str(), textColor);
    if (surface) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

        //وسط دکمه
        int textW = surface ->w;
        int textH = surface->  h;
        SDL_Rect textRect = {
                rect.x + (rect.w - textW) / 2,rect.y + (rect.h - textH) / 2,textW, textH
        };

        SDL_RenderCopy(renderer, texture, NULL, &textRect);
        SDL_FreeSurface(surface);
        SDL_DestroyTexture(texture);
    }
}

void DrawUI(AppContext* app, Button buttons[], int buttonCount) {
    // رسم نوار بالا
    SDL_Rect topBar = {0, 0, 1024, 45}; // به اندازه عرض صفحه که 1024 فرض کردیمش
    SDL_SetRenderDrawColor(app->renderer, 69, 69, 69, 255); // رنگ نوار
    SDL_RenderFillRect(app->renderer, &topBar);

    // دکمه ها
    for (int i = 0; i < buttonCount; i++) {
        // هاور کار
        SDL_Color c =  buttons[i].isHovered ?buttons[i].hoverColor:buttons[i].color ;

        SDL_SetRenderDrawColor(app->renderer, c.r, c.g, c.b, 255); // رنگ همون هاوری که داشتیم توی استراکت دکمه
        SDL_RenderFillRect(app -> renderer, & buttons[i].rect);

        // تکست دکمه
        DrawTextCentered(app->renderer, app->globalFont, buttons[i].text, buttons[i].rect);
    }

    // نوار استتوس پایین صفحه
    if (! app-> statusMessage.empty()) {
        SDL_Color msgColor = {51, 49, 50, 255}; // رنگ متن
        SDL_Surface* surface = TTF_RenderText_Blended(app->globalFont, app->statusMessage.c_str(), msgColor);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(app->renderer, surface);
            SDL_Rect msgRect = { 10, 768 - 40, surface->w, surface->h }; // پایین صفح ه
            SDL_RenderCopy(app->renderer, texture, NULL, &msgRect);
            SDL_FreeSurface(surface);
            SDL_DestroyTexture(texture);
        }
    }
}

void HandleFileMenuEvents(AppContext* app, Button buttons[], int buttonCount, SDL_Event* event) {
    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);

    // جرکت موس برای هاور افکت
    for (int i = 0; i < buttonCount; i++) {
        SDL_Rect r = buttons[i].rect;
        bool inside = (mouseX >= r.x && mouseX <= r.x + r.w && mouseY >= r.y && mouseY <= r.y + r.h); buttons[i].isHovered = inside;}

    // بررسی کلیک
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        for (int i = 0; i < buttonCount; i++) {
            if (buttons[i].isHovered) {
                if (buttons[i].text == "New")
                {NewProject(app);}
                else if (buttons[i].text == "Save")
                    SaveProject(app);
                else if (buttons[i].text == "Load")
                    LoadProject(app);
            }
        }}
}

void NewProject(AppContext* app) {
    app->bgColorR = 240; app->bgColorG = 240; app->bgColorB = 240;
    app->statusMessage = "CREATED NEW PROJECT!";
}

// سیو کردن پروژه
void SaveProject(AppContext* app) {
    std::ofstream file("project_data.txt");
    if (file.is_open()) {
        file << app->bgColorR <<  " "  << app->bgColorG <<  ' '  << app->bgColorB;
        file.close();
        app->statusMessage = "PROJECT SAVED!";
    }
    else
        app->statusMessage = "Error: Could not save file.";

}

// لود فایل
void LoadProject(AppContext* app) {
    std::ifstream file("project_data.txt");
    if (file.is_open()) {
        file >> app -> bgColorR >>app -> bgColorG >> app->bgColorB;
        file.close();
        app->statusMessage = "PROJECT LOADED!";
    }
    else
        app  ->statusMessage = "Error: No saved project found.";

}
