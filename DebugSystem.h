#ifndef DEBUGSYSTEM_H
#define DEBUGSYSTEM_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>
#include <fstream>

enum LogLevel {
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
};

struct LogEntry {
    std::string message;
    LogLevel level;
    std::string time;
};

struct DebugContext {
    bool showWindow = false;     // آیا پنجره دیباگ باز است؟
    std::vector<LogEntry> logs;  // لیست پیام‌ها با جزئیات
    SDL_Rect windowRect = {200, 150, 600, 400}; // موقعیت پنجره دیباگ
};

void InitDebug(DebugContext* ctx);
void Log(DebugContext* ctx, std::string message, LogLevel level = LOG_INFO);
void RenderDebugWindow(DebugContext* ctx, SDL_Renderer* r, TTF_Font* font);
void HandleDebugEvents(DebugContext* ctx, SDL_Event* e);

#endif
