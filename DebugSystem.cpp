#include "DebugSystem.h"
#include <iostream>
#include <ctime>

void InitDebug(DebugContext* ctx) {
    ctx->showWindow = false;
    Log(ctx, "Debug System 2.0 Initialized...", LOG_INFO);
}

void Log(DebugContext* ctx, std::string message, LogLevel level) {
    std::time_t now = std::time(0);
    struct tm tstruct;
    char buf[80];
    tstruct = *std::localtime(&now);
    strftime(buf, sizeof(buf), "%H:%M:%S", &tstruct);

    LogEntry entry;
    entry.message = message;
    entry.level = level;
    entry.time = std::string(buf);

    // چاپ در کنسول استاندارد هم برای اطمینان
    std::cout << "[" << entry.time << "] " << message << std::endl;

    ctx->logs.push_back(entry);
    // نگه داشتن فقط 50 خط آخر برای جلوگیری از کندی
    if (ctx->logs.size() > 50) {
        ctx->logs.erase(ctx->logs.begin());
    }

    // اگر ارور بود، پنجره دیباگ را خودکار باز کن
    if (level == LOG_ERROR) {
        ctx->showWindow = true;
    }
}

void RenderDebugWindow(DebugContext* ctx, SDL_Renderer* r, TTF_Font* font) {
    if (!ctx->showWindow) return;

    // 1. بدنه پنجره (طوسی تیره - شبیه ترمینال)
    SDL_SetRenderDrawColor(r, 40, 44, 52, 240); // شفافیت کم
    SDL_RenderFillRect(r, &ctx->windowRect);

    // 2. هدر پنجره (نوار بالا)
    SDL_Rect header = {ctx->windowRect.x, ctx->windowRect.y, ctx->windowRect.w, 30};
    SDL_SetRenderDrawColor(r, 33, 37, 43, 255);
    SDL_RenderFillRect(r, &header);

    // عنوان
    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface* titleSurf = TTF_RenderText_Blended(font, "Debug Console & Guide", white);
    if(titleSurf) {
        SDL_Rect tr = {header.x + 10, header.y + 5, titleSurf->w, titleSurf->h};
        SDL_Texture* tt = SDL_CreateTextureFromSurface(r, titleSurf);
        SDL_RenderCopy(r, tt, NULL, &tr);
        SDL_FreeSurface(titleSurf);
        SDL_DestroyTexture(tt);
    }

    // دکمه بستن (ضربدر قرمز)
    SDL_Rect closeBtn = {header.x + header.w - 25, header.y + 5, 20, 20};
    SDL_SetRenderDrawColor(r, 255, 95, 86, 255);
    SDL_RenderFillRect(r, &closeBtn);

    // 3. محتوای لاگ‌ها
    int startY = ctx->windowRect.y + 40;
    int lineHeight = 20;

    // نمایش از آخر به اول (جدیدترین پایین باشد)
    // اما چون اسکرول نداریم، 15 تای آخر را نشان میدهیم
    int count = 0;
    for (int i = ctx->logs.size() - 1; i >= 0; i--) {
        if (count > 15) break;

        LogEntry& e = ctx->logs[i];
        SDL_Color c = {200, 200, 200, 255}; // پیش فرض سفید/طوسی
        if (e.level == LOG_WARNING) c = {255, 200, 0, 255}; // زرد
        if (e.level == LOG_ERROR) c = {255, 80, 80, 255};   // قرمز

        std::string fullText = "[" + e.time + "] " + e.message;
        SDL_Surface* txtSurf = TTF_RenderText_Blended(font, fullText.c_str(), c);
        if (txtSurf) {
            SDL_Rect tr = {ctx->windowRect.x + 10, startY + (15 - count) * lineHeight, txtSurf->w, txtSurf->h};
            // محاسبه موقعیت: از پایین پنجره به بالا می‌چینیم
            tr.y = (ctx->windowRect.y + ctx->windowRect.h - 30) - (count * lineHeight);

            SDL_Texture* tt = SDL_CreateTextureFromSurface(r, txtSurf);
            SDL_RenderCopy(r, tt, NULL, &tr);
            SDL_FreeSurface(txtSurf);
            SDL_DestroyTexture(tt);
        }
        count++;
    }
}

void HandleDebugEvents(DebugContext* ctx, SDL_Event* e) {
    if (e->type == SDL_MOUSEBUTTONDOWN) {
        int mx = e->button.x;
        int my = e->button.y;

        // دکمه بستن پنجره
        if (ctx->showWindow) {
            int closeX = ctx->windowRect.x + ctx->windowRect.w - 25;
            int closeY = ctx->windowRect.y + 5;
            if (mx >= closeX && mx <= closeX + 20 && my >= closeY && my <= closeY + 20) {
                ctx->showWindow = false;
            }
        }
    }
    // (می‌توان قابلیت درگ کردن پنجره را هم اینجا اضافه کرد)
}
