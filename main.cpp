#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <fstream>
#include <sstream>
#include <map>
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <stack>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

enum BlockOpCode {
    OP_UNKNOWN = 0, OP_FLAG_CLICKED, OP_MOVE_STEPS, OP_TURN_RIGHT, OP_TURN_LEFT,
    OP_GOTO_XY, OP_GOTO_MOUSE, OP_GLIDE_SEC, OP_CHANGE_X, OP_SET_X, OP_CHANGE_Y, OP_SET_Y,
    OP_SAY_SEC, OP_SAY, OP_NEXT_COSTUME, OP_SET_SIZE, OP_CHANGE_SIZE,

    OP_WAIT_SEC, OP_REPEAT, OP_FOREVER, OP_IF, OP_IF_ELSE, OP_WAIT_UNTIL, OP_STOP_ALL,

    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_ABS, OP_SQRT, OP_FLOOR, OP_SIN, OP_COS,
    OP_EQ, OP_LT, OP_GT, OP_AND, OP_OR, OP_NOT, OP_XOR,
    // متفرقه
    OP_LENGTH, OP_LETTER, OP_JOIN, OP_DEFINE_CUSTOM, OP_CALL_CUSTOM, OP_DIVIDE_SIZE,
    OP_GOTO_RANDOM,
    OP_IF_ON_EDGE_BOUNCE,
    OP_THINK_SEC, OP_THINK,
    OP_SWITCH_COSTUME,
    OP_SWITCH_BACKDROP, OP_NEXT_BACKDROP,
    OP_SHOW, OP_HIDE,
    OP_SET_VAR, OP_CHANGE_VAR, OP_VAR_REPORTER,
    // Pen Extension
    OP_PEN_DOWN, OP_PEN_UP, OP_PEN_CLEAR, OP_PEN_STAMP,
    OP_PEN_SET_COLOR, OP_PEN_SET_SIZE, OP_PEN_CHANGE_SIZE,
};

enum BlockCategory { CAT_MOTION, CAT_LOOKS, CAT_EVENTS, CAT_CONTROL, CAT_MYBLOCKS, CAT_OPERATORS, CAT_VARIABLES, CAT_PEN };

enum BlockShape { SHAPE_HAT, SHAPE_STACK, SHAPE_C_SHAPE, SHAPE_E_SHAPE, SHAPE_CAP, SHAPE_REPORTER, SHAPE_BOOLEAN };

struct Block {
    int id; BlockOpCode opCode; BlockCategory category; BlockShape shape;
    std::string text; SDL_Rect rect; float param1 = 0; float param2 = 0;
    std::string stringParam;

    std::string param1Str = ""; std::string param2Str = "";
    SDL_Rect inputRect1 = {0,0,0,0}; SDL_Rect inputRect2 = {0,0,0,0};

    Block* next = nullptr;
    Block* subStack = nullptr;
    Block* subStack2 = nullptr;
    int midYOffset = 0;

    Block* condition = nullptr;
    Block* arg1 = nullptr;
    Block* arg2 = nullptr;
    bool isDragging = false;
};

struct BlockSystemContext {
    std::vector<Block> blocks;
    SDL_Rect paletteArea;
    SDL_Rect stageArea;
    int idCounter = 1;
    int scrollY = 0;
    Block* editingBlock = nullptr; // بلاکی که در حال تایپ داخلش هستیم
    int editingParam = 0;          // کدام ورودی؟ (1 یا 2)
    // --- سیستم متغیرها ---
    struct Variable { std::string name; float value = 0.0f; bool visible = true; };
    std::vector<Variable> variables;
    bool isMakingVar = false;       // آیا پنجره ساخت متغیر باز است؟
    std::string newVarName = "";    // اسم متغیری که تایپ می‌شود
    std::string varErrorMsg = "";   // ارور اسم تکراری
    // --- سیستم توابع دلخواه (My Blocks) ---
    bool isMakingBlock = false;
    std::string newBlockName = "";
    std::string blockErrorMsg = "";
    std::vector<std::string> customBlocksList;
    int customBlocksPaletteY = 0; // برای دونستن اینکه بلاک‌های جدید رو کجای پالت بذاریم
    bool penExtensionEnabled = false;
    bool showExtensionMenu = false;
};

SDL_Color GetCategoryColor(BlockCategory cat) {
    switch(cat) {
        case CAT_MOTION: return {76, 151, 255, 255};
        case CAT_LOOKS: return {153, 102, 255, 255};
        case CAT_EVENTS: return {255, 191, 0, 255};
        case CAT_CONTROL: return {255, 171, 25, 255}; // نارنجی اسکرچ
        case CAT_MYBLOCKS: return {255, 102, 128, 255};
        case CAT_OPERATORS: return {89, 192, 89, 255}; // سبز روشن اسکرچ
        case CAT_VARIABLES: return {255, 140, 26, 255}; // نارنجی تیره اسکرچ
        case CAT_PEN: return {0, 189, 140, 255}; // سبز آبی pen
        default: return {200, 200, 200, 255};
    }
}

// تابع کمکی برای حرکت دادن کل درخت بلاک‌ها با هم
void MoveBlockChain(Block* b, int dx, int dy) {
    if (!b) return;
    b->rect.x += dx; b->rect.y += dy;
    if (b->subStack) MoveBlockChain(b->subStack, dx, dy);
    if (b->condition) MoveBlockChain(b->condition, dx, dy);
    if (b->arg1) MoveBlockChain(b->arg1, dx, dy);
    if (b->arg2) MoveBlockChain(b->arg2, dx, dy);
    if (b->subStack2) MoveBlockChain(b->subStack2, dx, dy);
    if (b->next) MoveBlockChain(b->next, dx, dy);
}

// *** این تابع رو دقیقاً بالای CreateBlock اضافه کن ***
// *** جایگزین تابع GetOpCodeFromText در BlockSystem.h ***
BlockOpCode GetOpCodeFromText(std::string text) {
    // رویدادها
    if (text.find("Green Flag") != std::string::npos) return OP_FLAG_CLICKED;

    // حرکت
    if (text.find("Move") != std::string::npos) return OP_MOVE_STEPS;
    if (text.find("Turn Right") != std::string::npos) return OP_TURN_RIGHT;
    if (text.find("Turn Left") != std::string::npos) return OP_TURN_LEFT;
    if (text.find("go to x:") != std::string::npos) return OP_GOTO_XY;
    if (text.find("random position") != std::string::npos) return OP_GOTO_RANDOM;
    if (text.find("mouse-pointer") != std::string::npos) return OP_GOTO_MOUSE;
    if (text.find("bounce") != std::string::npos) return OP_IF_ON_EDGE_BOUNCE;

    // ظاهر (Looks)
    if (text.find("Say") != std::string::npos && text.find("Secs") != std::string::npos) return OP_SAY_SEC;
    if (text.find("Say") != std::string::npos && text.find("Secs") == std::string::npos) return OP_SAY;
    if (text.find("Think") != std::string::npos && text.find("Secs") != std::string::npos) return OP_THINK_SEC;
    if (text.find("Think") != std::string::npos && text.find("Secs") == std::string::npos) return OP_THINK;

    if (text.find("Switch Costume") != std::string::npos) return OP_SWITCH_COSTUME;
    if (text.find("Next Costume") != std::string::npos) return OP_NEXT_COSTUME;

    if (text.find("Switch Backdrop") != std::string::npos) return OP_SWITCH_BACKDROP;
    if (text.find("Next Backdrop") != std::string::npos) return OP_NEXT_BACKDROP;

    if (text.find("Change Size") != std::string::npos) return OP_CHANGE_SIZE;
    if (text.find("Set Size") != std::string::npos) return OP_SET_SIZE;

    if (text.find("Show") != std::string::npos) return OP_SHOW;
    if (text.find("Hide") != std::string::npos) return OP_HIDE;

    // کنترل‌ها (ترتیب در اینجا به شدت حیاتی است!)
    if (text.find("Wait Until") != std::string::npos) return OP_WAIT_UNTIL; // قبل از Wait
    if (text.find("Wait") != std::string::npos) return OP_WAIT_SEC;
    if (text.find("Forever") != std::string::npos) return OP_FOREVER;
    if (text.find("Repeat") != std::string::npos) return OP_REPEAT;
    if (text.find("Else") != std::string::npos) return OP_IF_ELSE; // قبل از If
    if (text.find("If") != std::string::npos) return OP_IF;
    if (text.find("Stop All") != std::string::npos) return OP_STOP_ALL;

    // عملگرها
    if (text.find("+") != std::string::npos) return OP_ADD;
    if (text.find("-") != std::string::npos) return OP_SUB;
    if (text.find("*") != std::string::npos) return OP_MUL;
    if (text.find("/") != std::string::npos) return OP_DIV;
    if (text.find(">") != std::string::npos) return OP_GT;
    if (text.find("=") != std::string::npos) return OP_EQ;
    if (text.find("and") != std::string::npos) return OP_AND;
    if (text.find("or") != std::string::npos) return OP_OR;
    if (text.find("not") != std::string::npos) return OP_NOT;
    // برای علامت کوچکتر باید مطمئن شیم که جزء "not < >" نیست
    if (text.find("<") != std::string::npos && text.find("not") == std::string::npos) return OP_LT;

    if (text.find("Set var") != std::string::npos) return OP_SET_VAR;
    if (text.find("Change var") != std::string::npos) return OP_CHANGE_VAR;
    if (text.find("val of var") != std::string::npos) return OP_VAR_REPORTER;

    // توابع (My Blocks)
    if (text.find("Define") != std::string::npos) return OP_DEFINE_CUSTOM;
    // اگه هیچکدوم از متن‌های بالا نبود ولی توی لیست توابع ما بود، پس یه Call هست
    return OP_UNKNOWN;

    return OP_UNKNOWN;
}

// *** جایگزین این تابع در BlockSystem.h ***
// حذف پارامتر BlockOpCode از ورودی تابع تا با FileMenu و بقیه هم‌خوان شود
Block CreateBlock(int id, int x, int y, std::string text, BlockCategory cat, BlockShape shape) {
    Block b;
    b.id = id;
    b.text = text;
    b.category = cat;
    b.shape = shape;

    // تشخیص اتوماتیک OpCode از روی متن (دقیقاً مثل قبل)
    b.opCode = GetOpCodeFromText(text);

    b.rect = {x, y, 160, 45};
    if (shape == SHAPE_HAT) b.rect.h = 55;
    else if (shape == SHAPE_REPORTER || shape == SHAPE_BOOLEAN) b.rect.h = 35; // عملگرها باریک‌ترن

    size_t firstDigit = text.find_first_of("-0123456789");
    if (firstDigit != std::string::npos) {
        try { b.param1 = std::stof(text.substr(firstDigit)); } catch(...) {}
    }

    if (cat == CAT_MYBLOCKS && text.find("Define") == std::string::npos) { b.opCode = OP_CALL_CUSTOM; b.stringParam = text; }
    if (cat == CAT_MYBLOCKS && text.find("Define") != std::string::npos) { b.opCode = OP_DEFINE_CUSTOM; b.stringParam = text.substr(7); }

    return b;
}

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
    bool showWindow = false;
    std::vector<LogEntry> logs;
    SDL_Rect windowRect = {200, 80, 780, 500};
    // scroll
    int scrollOffset = 0;
    // drag
    bool dragging = false;
    int dragDX = 0, dragDY = 0;
    // filter
    bool showInfo = true, showWarn = true, showError = true;
    // stats
    int frameCount = 0;
    float fps = 0.0f;
    Uint32 fpsTimer = 0;
    // runtime stats from execution
    int threadCount = 0;
    int blockExecCount = 0;
    std::string lastOpName = "";
    // input bar
    bool inputFocused = false;
    std::string inputText = "";
};

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


// تابع بومی ویندوز برای باز کردن پنجره انتخاب فایل
inline std::string OpenFileDialog() {
    std::string filePath = "";
#ifdef _WIN32
    OPENFILENAMEA ofn;
    CHAR szFile[260] = {0};
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Images\0*.PNG;*.JPG;*.JPEG;*.BMP\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn) == TRUE) {
        filePath = ofn.lpstrFile;
    }
#endif
    return filePath;
}

struct Costume {
    std::string name;
    SDL_Texture* texture;
    int width;
    int height;
};

struct Sprite {
    std::string name;
    std::vector<Costume> costumes;
    int currentCostumeIndex = 0;
    float x = 0, y = 0;
    float direction = 90;
    float scale = 1.0f;
    // سیستم دیالوگ و فکر
    std::string currentDialog = ""; // متنی که الان داره میگه
    bool isThinking = false;        // آیا داره فکر میکنه (شکل ابر) یا حرف میزنه (شکل مستطیل)؟
    Uint32 dialogEndTime = 0;       // زمان پایان دیالوگ (0 = تا ابد روی صفحه بمونه)

    bool isVisible = true;
    // Pen state
    bool penDown = false;
    SDL_Color penColor = {0, 0, 200, 255};
    int penSize = 2;
};

struct Backdrop {
    std::string name;
    SDL_Texture* texture;
    int width, height;
};

struct PenLine { int x1, y1, x2, y2; SDL_Color color; int size; };

struct PenContext {
    std::vector<PenLine> lines;
    void clear() { lines.clear(); }
};

struct SpriteContext {
    std::vector<Sprite> sprites;
    std::vector<Backdrop> backdrops;
    int selectedSpriteIndex = -1;
    int currentBackdropIndex = 0;
};


// تبدیل عکس کاربر به تکسچر قابل ویرایش
SDL_Texture* LoadEditableTexture(SDL_Renderer* r, std::string filepath, int& w, int& h) {
    SDL_Surface* surf = IMG_Load(filepath.c_str());
    if (!surf) return nullptr;

    w = surf->w; h = surf->h;
    SDL_Texture* targetTex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
    SDL_SetTextureBlendMode(targetTex, SDL_BLENDMODE_BLEND);
    SDL_Texture* tempTex = SDL_CreateTextureFromSurface(r, surf);

    SDL_SetRenderTarget(r, targetTex);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 0); SDL_RenderClear(r);
    SDL_SetTextureBlendMode(tempTex, SDL_BLENDMODE_NONE);
    SDL_RenderCopy(r, tempTex, NULL, NULL);
    SDL_SetRenderTarget(r, NULL);

    SDL_DestroyTexture(tempTex); SDL_FreeSurface(surf);
    return targetTex;
}

// ساخت کاراکتر سفارشی با کد (سر گرد، بدن مربع، چشم)
SDL_Texture* CreateCharacterTexture(SDL_Renderer* r, int& w, int& h) {
    w = 120; h = 160;
    SDL_Texture* tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(r, tex);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 0); SDL_RenderClear(r); // پس‌زمینه شیشه‌ای

    // بدن (مربع) - بنفش روشن
    SDL_SetRenderDrawColor(r, 153, 102, 255, 255);
    SDL_Rect body = {30, 60, 60, 80};
    SDL_RenderFillRect(r, &body);

    // سر (دایره) - زرد خردلی
    int cx = 60, cy = 40, radius = 35;
    SDL_SetRenderDrawColor(r, 255, 200, 50, 255);
    for (int x = -radius; x < radius; x++) {
        for (int y = -radius; y < radius; y++) {
            if (x*x + y*y <= radius*radius) {
                SDL_RenderDrawPoint(r, cx + x, cy + y);
            }
        }
    }

    // چشم‌ها - مشکی
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_Rect eye1 = {45, 25, 8, 12}; SDL_RenderFillRect(r, &eye1);
    SDL_Rect eye2 = {65, 25, 8, 12}; SDL_RenderFillRect(r, &eye2);

    // دهان خندان
    SDL_Rect mouth = {50, 50, 20, 5}; SDL_RenderFillRect(r, &mouth);

    SDL_SetRenderTarget(r, NULL);
    return tex;
}

// ساخت دایره بی‌نقص آبی
SDL_Texture* CreateCircleTexture(SDL_Renderer* r, int radius, SDL_Color color, int& w, int& h) {
    w = radius * 2; h = radius * 2;
    SDL_Texture* tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(r, tex);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 0); SDL_RenderClear(r);

    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            int dx = radius - x; int dy = radius - y;
            if ((dx*dx + dy*dy) <= (radius * radius)) {
                SDL_RenderDrawPoint(r, x, y);
            }
        }
    }
    SDL_SetRenderTarget(r, NULL);
    return tex;
}

void AddCostumeToSprite(Sprite* s, SDL_Renderer* r, std::string path) {
    Costume c;
    size_t slash = path.find_last_of("/\\");
    c.name = (slash == std::string::npos) ? path : path.substr(slash + 1);

    c.texture = LoadEditableTexture(r, path, c.width, c.height);
    if (c.texture) {
        s->costumes.push_back(c);
        s->currentCostumeIndex = s->costumes.size() - 1; // سوئیچ به لباس جدید
    }
}

// ساخت رنگ یکپارچه برای بک‌گراند
SDL_Texture* CreateSolidColorTexture(SDL_Renderer* r, int w, int h, SDL_Color color) {
    SDL_Texture* tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(r, tex);
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
    SDL_RenderClear(r);
    SDL_SetRenderTarget(r, NULL);
    return tex;
}

// اضافه کردن بک‌گراند از فایل آپلودی
void AddBackdropFromFile(SDL_Renderer* r, SpriteContext* ctx, std::string path) {
    Backdrop b; size_t slash = path.find_last_of("/\\");
    b.name = (slash == std::string::npos) ? path : path.substr(slash + 1);
    b.texture = LoadEditableTexture(r, path, b.width, b.height);
    if (b.texture) { ctx->backdrops.push_back(b); ctx->currentBackdropIndex = ctx->backdrops.size() - 1; }
}

void RenderStageContent(SDL_Renderer* r, SpriteContext* ctx, PenContext* penCtx, TTF_Font* font) {
    if (!ctx->backdrops.empty()) SDL_RenderCopy(r, ctx->backdrops[ctx->currentBackdropIndex].texture, NULL, NULL);

    // رسم خطوط pen (قبل از اسپرایت‌ها تا زیرشون باشن)
    if (penCtx) {
        for (auto& ln : penCtx->lines) {
            SDL_SetRenderDrawColor(r, ln.color.r, ln.color.g, ln.color.b, ln.color.a);
            int half = ln.size / 2;
            if (half <= 0) {
                SDL_RenderDrawLine(r, ln.x1, ln.y1, ln.x2, ln.y2);
            } else {
                // خط ضخیم با چند پاس
                bool steep = std::abs(ln.y2-ln.y1) > std::abs(ln.x2-ln.x1);
                for (int d = -half; d <= half; d++) {
                    if (steep) SDL_RenderDrawLine(r, ln.x1+d, ln.y1, ln.x2+d, ln.y2);
                    else       SDL_RenderDrawLine(r, ln.x1, ln.y1+d, ln.x2, ln.y2+d);
                }
            }
        }
    }

    int centerX = 480 / 2; int centerY = 360 / 2;

    for (int i = 0; i < (int)ctx->sprites.size(); i++) {
        Sprite& s = ctx->sprites[i];
        if (!s.isVisible || s.costumes.empty()) continue;

        Costume& c = s.costumes[s.currentCostumeIndex];
        SDL_Rect dest;
        dest.w = c.width * s.scale; dest.h = c.height * s.scale;
        dest.x = centerX + s.x - (dest.w / 2); dest.y = centerY - s.y - (dest.h / 2);

        SDL_RenderCopyEx(r, c.texture, NULL, &dest, s.direction - 90, NULL, SDL_FLIP_NONE);

        if (i == ctx->selectedSpriteIndex) {
            SDL_SetRenderDrawColor(r, 76, 151, 255, 100); SDL_RenderDrawRect(r, &dest);
        }

        // پاک کردن دیالوگ در صورت اتمام زمان
        if (s.dialogEndTime > 0 && SDL_GetTicks() > s.dialogEndTime) {
            s.currentDialog = "";
            s.dialogEndTime = 0;
        }

        if (!s.currentDialog.empty() && font) {
            // استفاده از تابع Wrapped برای شکستن خطوط طولانی (مثل اسکرچ)
            SDL_Surface* textSurf = TTF_RenderText_Blended_Wrapped(font, s.currentDialog.c_str(), {0,0,0,255}, 180);
            if (textSurf) {
                int padding = 12;
                int bubbleW = textSurf->w + (padding * 2);
                int bubbleH = textSurf->h + (padding * 2);

                // قرار دادن حباب بالا و سمت راست کاراکتر
                int bubbleX = dest.x + dest.w - 10;
                int bubbleY = dest.y - bubbleH - 10;

                // جلوگیری از خروج حباب از کادر استیج (480x360)
                if (bubbleY < 5) bubbleY = 5;
                if (bubbleX + bubbleW > 475) bubbleX = 475 - bubbleW;

                SDL_Rect bubbleRect = {bubbleX, bubbleY, bubbleW, bubbleH};

                // رسم پس‌زمینه سفید حباب
                SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
                SDL_RenderFillRect(r, &bubbleRect);
                // رسم حاشیه خاکستری
                SDL_SetRenderDrawColor(r, 150, 150, 150, 255);
                SDL_RenderDrawRect(r, &bubbleRect);

                // رسم دنباله حباب (Tail)
                if (s.isThinking) {
                    SDL_Rect dot1 = {bubbleRect.x + 10, bubbleRect.y + bubbleRect.h + 2, 8, 8};
                    SDL_Rect dot2 = {bubbleRect.x + 5, bubbleRect.y + bubbleRect.h + 12, 5, 5};
                    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
                    SDL_RenderFillRect(r, &dot1); SDL_RenderFillRect(r, &dot2);
                    SDL_SetRenderDrawColor(r, 150, 150, 150, 255);
                    SDL_RenderDrawRect(r, &dot1); SDL_RenderDrawRect(r, &dot2);
                } else {
                    SDL_Rect tail = {bubbleRect.x + 10, bubbleRect.y + bubbleRect.h, 10, 10};
                    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
                    SDL_RenderFillRect(r, &tail);
                    SDL_SetRenderDrawColor(r, 150, 150, 150, 255);
                    SDL_RenderDrawLine(r, tail.x, tail.y, tail.x, tail.y + tail.h);
                    SDL_RenderDrawLine(r, tail.x, tail.y + tail.h, tail.x + tail.w, tail.y);
                }

                // رسم متن
                SDL_Rect textRect = {bubbleRect.x + padding, bubbleRect.y + padding, textSurf->w, textSurf->h};
                SDL_Texture* textTex = SDL_CreateTextureFromSurface(r, textSurf);
                SDL_RenderCopy(r, textTex, NULL, &textRect);
                SDL_DestroyTexture(textTex);
                SDL_FreeSurface(textSurf);
            }
        }
    }
}

void InitSpriteSystem(SDL_Renderer* r, SpriteContext* ctx) {
    // 1. اسپرایت کاراکتر اصلی (ساخته شده با کد)
    Sprite s1; s1.name = "My Character"; s1.x = -50; s1.y = 0; s1.currentCostumeIndex = 0;
    Costume c1; c1.name = "Costume 1";
    c1.texture = CreateCharacterTexture(r, c1.width, c1.height);
    s1.costumes.push_back(c1);
    s1.currentDialog = ""; s1.isThinking = false; s1.dialogEndTime = 0;
    ctx->sprites.push_back(s1);

    // 2. اسپرایت دایره آبی
    Sprite s2; s2.name = "Blue Circle"; s2.x = 100; s2.y = 50; s2.currentCostumeIndex = 0;
    Costume c2; c2.name = "Circle";
    c2.texture = CreateCircleTexture(r, 40, {76, 151, 255, 255}, c2.width, c2.height);
    s2.costumes.push_back(c2);
    s2.currentDialog = ""; s2.isThinking = false; s2.dialogEndTime = 0;
    ctx->sprites.push_back(s2);

    ctx->selectedSpriteIndex = 0;

    // 3. پس زمینه سفید استیج
    SDL_Surface* surf = SDL_CreateRGBSurface(0, 480, 360, 32, 0,0,0,0);
    SDL_FillRect(surf, NULL, SDL_MapRGB(surf->format, 255, 255, 255));
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    // 3. پس زمینه سفید و سیاه استیج
    ctx->backdrops.push_back({"White BG", CreateSolidColorTexture(r, 480, 360, {255,255,255,255}), 480, 360});
    ctx->backdrops.push_back({"Black BG", CreateSolidColorTexture(r, 480, 360, {0,0,0,255}), 480, 360});
    ctx->currentBackdropIndex = 0;

    SDL_FreeSurface(surf);
}


struct PaintTool {
    SDL_Color color = {0, 0, 0, 255};
    int size = 10;
    bool isEraser = false;
};

// مختصات و ابعاد اصلی بوم نقاشی
SDL_Rect GetCanvasRect(SDL_Rect area) {
    return {area.x + 50, area.y + 110, 350, 350}; // بوم بزرگتر و پایین‌تر برای جا دادن ابزارها
}

SDL_Rect GetUploadBtnRect(SDL_Rect area) {
    return {area.x + 50, area.y + 480, 200, 45};
}


// ساختار هر "نخ" اجرایی مستقل (برای اجرای همزمان)
struct ScriptThread {
    Block* currentBlock = nullptr;
    bool isWaiting = false;
    Uint32 waitEndTime = 0;
    std::stack<Block*> returnStack;
    std::map<Block*, int> repeatMap; // برای Repeat: شمارش تکرار هر بلاک
};

// کانتکست اصلی حالا لیستی از نخ‌ها رو مدیریت می‌کنه
struct ExecutionContext {
    std::vector<ScriptThread> threads;
    bool isRunning = false;
    bool isPaused = false;
};


void Log(DebugContext* ctx, std::string message, LogLevel level = LOG_INFO);
void InitBlockSystem(BlockSystemContext* ctx, int w, int h);

void AddPenBlocksToPalette(BlockSystemContext* ctx) {
    if (ctx->penExtensionEnabled) return; // قبلاً اضافه شده
    int y = ctx->customBlocksPaletteY + 10;
    auto add = [&](const char* txt, BlockOpCode op, const char* p1 = "") {
        Block b = CreateBlock(ctx->idCounter++, 10, y, txt, CAT_PEN, SHAPE_STACK);
        b.opCode = op;
        b.param1Str = p1;
        ctx->blocks.push_back(b);
        y += 50;
    };
    add("Pen Down",           OP_PEN_DOWN);
    add("Pen Up",             OP_PEN_UP);
    add("Clear",              OP_PEN_CLEAR);
    add("Stamp",              OP_PEN_STAMP);
    add("Set Pen Color %1",   OP_PEN_SET_COLOR, "#0000ff");
    add("Set Pen Size to %1", OP_PEN_SET_SIZE,  "2");
    add("Change Pen Size %1", OP_PEN_CHANGE_SIZE, "1");
    ctx->customBlocksPaletteY = y;
    ctx->penExtensionEnabled = true;
}

void AddNewSprite(SDL_Renderer* r, SpriteContext* ctx);
void DrawLabel(SDL_Renderer* r, TTF_Font* font, std::string text, int x, int y, SDL_Color color);
void AddNewSpriteFromFile(SDL_Renderer* r, SpriteContext* ctx, std::string filepath);

// *** هوش مصنوعی چیدمان: محاسبه قد بلاک‌ها به صورت دینامیک ***
// محاسبه عرض یک expression block (بازگشتی)
int CalcExprWidth(Block* b) {
    if (!b) return 28; // عرض slot خالی
    if (b->shape == SHAPE_REPORTER || b->shape == SHAPE_BOOLEAN) {
        // بر اساس محتوا حساب میکنیم
        int w = 16; // padding
        // طول متن (تقریبی - 8px per char)
        std::string txt = b->text;
        size_t p1 = txt.find("%1"), p2 = txt.find("%2");
        std::string label = (p1 != std::string::npos) ? txt.substr(0, p1) : txt;
        w += (int)label.size() * 8;
        if (b->opCode == OP_AND || b->opCode == OP_OR) {
            w += CalcExprWidth(b->arg1) + 8;
            // label وسطی ("and"/"or")
            w += (b->opCode==OP_AND ? 32 : 24);
            w += CalcExprWidth(b->arg2) + 8;
        } else if (b->opCode == OP_NOT) {
            w += 32 + CalcExprWidth(b->arg1) + 8;
        } else if (p1 != std::string::npos) {
            w += CalcExprWidth(b->arg1) + 6;
            if (p2 != std::string::npos) {
                std::string mid = txt.substr(p1+2, p2-(p1+2));
                w += (int)mid.size() * 8 + CalcExprWidth(b->arg2) + 6;
                w += (int)txt.substr(p2+2).size() * 8;
            }
        }
        // minimum
        if (b->opCode==OP_AND||b->opCode==OP_OR) w = std::max(w, 110);
        else if (b->opCode==OP_NOT) w = std::max(w, 70);
        else if (b->shape==SHAPE_BOOLEAN) w = std::max(w, 70);
        else w = std::max(w, 50);
        return w;
    }
    return b->rect.w;
}

int LayoutBlockChain(Block* b, int x, int y) {
    if (!b) return 0;
    int currentY = y;
    Block* curr = b;

    while (curr) {
        curr->rect.x = x;
        curr->rect.y = currentY;

        bool isExpr = (curr->shape==SHAPE_REPORTER||curr->shape==SHAPE_BOOLEAN);
        int h = isExpr ? 32 : 46;
        int PAD = 10; // padding داخلی

        if (isExpr) {
            // ---- عرض expression blocks ----
            int w = CalcExprWidth(curr);
            curr->rect.w = w;
            curr->rect.h = 32;

            // layout فرزندان داخلی
            // برای AND/OR: [hw_pad] [arg1] [label] [arg2] [hw_pad]
            int hw = 16; // half-width برای نوک شش‌ضلعی/بیضی
            if (curr->opCode==OP_AND || curr->opCode==OP_OR) {
                int a1w = CalcExprWidth(curr->arg1);
                int a2w = CalcExprWidth(curr->arg2);
                int labelW = (curr->opCode==OP_AND) ? 32 : 24;
                int startX = x + hw + PAD;
                if (curr->arg1) LayoutBlockChain(curr->arg1, startX, currentY);
                startX += a1w + PAD + labelW + PAD;
                if (curr->arg2) LayoutBlockChain(curr->arg2, startX, currentY);
            } else if (curr->opCode==OP_NOT) {
                int startX = x + hw + 32 + PAD; // "not" label
                if (curr->arg1) LayoutBlockChain(curr->arg1, startX, currentY);
            } else {
                // عملگرهای %1 op %2
                std::string txt = curr->text;
                size_t p1 = txt.find("%1"), p2 = txt.find("%2");
                int startX = x + PAD;
                if (p1 != std::string::npos) {
                    startX += (int)txt.substr(0,p1).size() * 8;
                    if (curr->shape==SHAPE_BOOLEAN) startX += hw;
                    int a1w = CalcExprWidth(curr->arg1);
                    if (curr->arg1) LayoutBlockChain(curr->arg1, startX, currentY);
                    startX += a1w + PAD;
                    if (p2 != std::string::npos) {
                        std::string mid = txt.substr(p1+2, p2-(p1+2));
                        startX += (int)mid.size() * 8;
                        if (curr->arg2) LayoutBlockChain(curr->arg2, startX, currentY);
                    }
                }
                if (curr->shape==SHAPE_REPORTER) startX = x; // بیضی
            }
        } else {
            // ---- بلاک‌های معمولی STACK/HAT/C/E ----
            int w = 160;

            // جای arg1/arg2 در عرض بلاک
            std::string txt = curr->text;
            size_t p1 = txt.find("%1"), p2 = txt.find("%2");
            if (p1 != std::string::npos) {
                int beforeW = (int)txt.substr(0,p1).size() * 8 + PAD + 10;
                int a1w = curr->arg1 ? CalcExprWidth(curr->arg1) : 30;
                w = beforeW + a1w;
                if (p2 != std::string::npos) {
                    std::string mid = txt.substr(p1+2, p2-(p1+2));
                    int a2w = curr->arg2 ? CalcExprWidth(curr->arg2) : 30;
                    w += (int)mid.size()*8 + a2w + PAD*2;
                    w += (int)txt.substr(p2+2).size()*8;
                } else {
                    w += (int)txt.substr(p1+2).size()*8 + PAD*2;
                }
                w = std::max(w, 160);
            }
            curr->rect.w = w;

            // layout فرزندان arg در داخل بلاک
            int cxOff = x + PAD + 10;
            if (p1 != std::string::npos) {
                cxOff += (int)txt.substr(0,p1).size() * 8;
                if (curr->arg1) {
                    LayoutBlockChain(curr->arg1, cxOff, currentY + 1);
                    cxOff += CalcExprWidth(curr->arg1) + PAD;
                } else {
                    cxOff += 34;
                }
                if (p2 != std::string::npos) {
                    std::string mid = txt.substr(p1+2, p2-(p1+2));
                    cxOff += (int)mid.size()*8 + 2;
                    if (curr->arg2) LayoutBlockChain(curr->arg2, cxOff, currentY + 1);
                }
            }
            if (curr->condition) {
                // condition slot در بلاک C-Shape
                LayoutBlockChain(curr->condition, x + 72, currentY + 7);
                // عرض بلاک رو بزرگتر کن اگه لازمه
                int condRight = curr->condition->rect.x + curr->condition->rect.w + 16;
                if (condRight > x + curr->rect.w) curr->rect.w = condRight - x;
            }

            // ارتفاع
            if (curr->shape == SHAPE_C_SHAPE) {
                int subH = curr->subStack ? LayoutBlockChain(curr->subStack, x+20, currentY+40) : 32;
                h = 40 + subH + 28;
            } else if (curr->shape == SHAPE_E_SHAPE) {
                int subH1 = curr->subStack  ? LayoutBlockChain(curr->subStack,  x+20, currentY+40) : 32;
                curr->midYOffset = 40 + subH1;
                int subH2 = curr->subStack2 ? LayoutBlockChain(curr->subStack2, x+20, currentY+curr->midYOffset+28) : 32;
                h = curr->midYOffset + 28 + subH2 + 28;
            } else if (curr->shape == SHAPE_HAT) {
                h = 54;
            }
            curr->rect.h = h;
        }

        currentY += curr->rect.h + (isExpr ? 0 : 2);
        curr = curr->next;
    }
    return currentY - y;
}

// نسخه 7-پارامتری CreateBlock (overload داخل BlockSystem.cpp اصلی)
Block CreateBlock(int id, int x, int y, std::string text, BlockOpCode op, BlockCategory cat, BlockShape shape) {
    Block b; b.id = id; b.text = text; b.opCode = op; b.category = cat; b.shape = shape;
    b.rect = {x, y, 160, 45};
    if (shape == SHAPE_REPORTER || shape == SHAPE_BOOLEAN) b.rect.h = 35;
    return b;
}

void InitBlockSystem(BlockSystemContext* ctx, int w, int h) {
    ctx->paletteArea = {0, 88, 280, h - 88};
    ctx->scrollY = 0;
    int y = ctx->paletteArea.y + 125; // جا باز کردن برای سه دکمه در بالای پالت

    int step = 50;
    Block b;

    // رویدادها (بدون ورودی)
    ctx->blocks.push_back(CreateBlock(ctx->idCounter++, 10, y, "When Green Flag Clicked", CAT_EVENTS, SHAPE_HAT)); y+=step+10;

    // حرکت
    b = CreateBlock(ctx->idCounter++, 10, y, "Move %1 Steps", CAT_MOTION, SHAPE_STACK);
    b.param1Str = "10"; ctx->blocks.push_back(b); y+=step;

    b = CreateBlock(ctx->idCounter++, 10, y, "Turn Right %1", CAT_MOTION, SHAPE_STACK);
    b.param1Str = "15"; ctx->blocks.push_back(b); y+=step;

    b = CreateBlock(ctx->idCounter++, 10, y, "Turn Left %1", CAT_MOTION, SHAPE_STACK);
    b.param1Str = "15"; ctx->blocks.push_back(b); y+=step;

    // *** بلاک‌های جدید حرکتی ***
    b = CreateBlock(ctx->idCounter++, 10, y, "go to x: %1 y: %2", CAT_MOTION, SHAPE_STACK);
    b.param1Str = "0"; b.param2Str = "0"; ctx->blocks.push_back(b); y+=step;

    ctx->blocks.push_back(CreateBlock(ctx->idCounter++, 10, y, "go to random position", CAT_MOTION, SHAPE_STACK)); y+=step;

    ctx->blocks.push_back(CreateBlock(ctx->idCounter++, 10, y, "go to mouse-pointer", OP_GOTO_MOUSE, CAT_MOTION, SHAPE_STACK)); y+=step;

    ctx->blocks.push_back(CreateBlock(ctx->idCounter++, 10, y, "if on edge, bounce", CAT_MOTION, SHAPE_STACK)); y+=step+10;

    // *** ظاهر (Looks) - رنگ بنفش ***
    y += 20; // فاصله از بخش حرکت

    b = CreateBlock(ctx->idCounter++, 10, y, "Say %1 for %2 Secs", CAT_LOOKS, SHAPE_STACK);
    b.param1Str = "Hello!"; b.param2Str = "2"; ctx->blocks.push_back(b); y+=step;

    b = CreateBlock(ctx->idCounter++, 10, y, "Say %1", CAT_LOOKS, SHAPE_STACK);
    b.param1Str = "Hello!"; ctx->blocks.push_back(b); y+=step;

    b = CreateBlock(ctx->idCounter++, 10, y, "Think %1 for %2 Secs", CAT_LOOKS, SHAPE_STACK);
    b.param1Str = "Hmm..."; b.param2Str = "2"; ctx->blocks.push_back(b); y+=step;

    // فعلا سوییچ کاستوم رو متنی می‌گیریم تا بعدا منوی کشویی بهش اضافه بشه
    b = CreateBlock(ctx->idCounter++, 10, y, "Switch Costume to %1", CAT_LOOKS, SHAPE_STACK);
    b.param1Str = "1"; ctx->blocks.push_back(b); y+=step;

    ctx->blocks.push_back(CreateBlock(ctx->idCounter++, 10, y, "Next Costume", CAT_LOOKS, SHAPE_STACK)); y+=step;

    b = CreateBlock(ctx->idCounter++, 10, y, "Switch Backdrop to %1", CAT_LOOKS, SHAPE_STACK);
    b.param1Str = "1"; ctx->blocks.push_back(b); y+=step;

    ctx->blocks.push_back(CreateBlock(ctx->idCounter++, 10, y, "Next Backdrop", CAT_LOOKS, SHAPE_STACK)); y+=step+10;

    b = CreateBlock(ctx->idCounter++, 10, y, "Change Size by %1", CAT_LOOKS, SHAPE_STACK);
    b.param1Str = "10"; ctx->blocks.push_back(b); y+=step;

    b = CreateBlock(ctx->idCounter++, 10, y, "Set Size to %1 %", CAT_LOOKS, SHAPE_STACK);
    b.param1Str = "100"; ctx->blocks.push_back(b); y+=step+10;

    ctx->blocks.push_back(CreateBlock(ctx->idCounter++, 10, y, "Show", CAT_LOOKS, SHAPE_STACK)); y+=step;
    ctx->blocks.push_back(CreateBlock(ctx->idCounter++, 10, y, "Hide", CAT_LOOKS, SHAPE_STACK)); y+=step;

    // شرط‌ها فعلاً ورودی متنی ندارن، قراره توشون عملگر بیفته
    ctx->blocks.push_back(CreateBlock(ctx->idCounter++, 10, y, "If < > Then", CAT_CONTROL, SHAPE_C_SHAPE)); y+=110;

    // کنترل‌ها
    b = CreateBlock(ctx->idCounter++, 10, y, "Wait %1 Secs", CAT_CONTROL, SHAPE_STACK);
    b.param1Str = "1"; ctx->blocks.push_back(b); y+=step;

    // بلاک Repeat
    b = CreateBlock(ctx->idCounter++, 10, y, "Repeat %1", CAT_CONTROL, SHAPE_C_SHAPE);
    b.param1Str = "10"; ctx->blocks.push_back(b); y+=110;

    // بلاک Forever (جدید)
    ctx->blocks.push_back(CreateBlock(ctx->idCounter++, 10, y, "Forever", CAT_CONTROL, SHAPE_C_SHAPE)); y+=110;

    // بلاک If (جدید)
    ctx->blocks.push_back(CreateBlock(ctx->idCounter++, 10, y, "If %1 Then", CAT_CONTROL, SHAPE_C_SHAPE)); y+=110;

    // بلاک غول‌پیکر If-Else (جدید)
    ctx->blocks.push_back(CreateBlock(ctx->idCounter++, 10, y, "If %1 Then Else", CAT_CONTROL, SHAPE_E_SHAPE)); y+=160;

    // بلاک Wait Until (جدید)
    b = CreateBlock(ctx->idCounter++, 10, y, "Wait Until %1", CAT_CONTROL, SHAPE_STACK);
    b.param1Str = ""; ctx->blocks.push_back(b); y+=step;

    // بلاک Stop All (جدید)
    ctx->blocks.push_back(CreateBlock(ctx->idCounter++, 10, y, "Stop All", CAT_CONTROL, SHAPE_CAP)); y+=step;

    // عملگرهای ریاضی (با دو ورودی %1 و %2)
    b = CreateBlock(ctx->idCounter++, 10, y, "%1 + %2", CAT_OPERATORS, SHAPE_REPORTER);
    b.param1Str = ""; b.param2Str = ""; ctx->blocks.push_back(b); y+=40;

    b = CreateBlock(ctx->idCounter++, 10, y, "%1 - %2", CAT_OPERATORS, SHAPE_REPORTER);
    b.param1Str = ""; b.param2Str = ""; ctx->blocks.push_back(b); y+=40;

    b = CreateBlock(ctx->idCounter++, 10, y, "%1 * %2", CAT_OPERATORS, SHAPE_REPORTER);
    b.param1Str = ""; b.param2Str = ""; ctx->blocks.push_back(b); y+=40;

    b = CreateBlock(ctx->idCounter++, 10, y, "%1 / %2", CAT_OPERATORS, SHAPE_REPORTER);
    b.param1Str = ""; b.param2Str = ""; ctx->blocks.push_back(b); y+=45;

    // عملگرهای مقایسه‌ای
    b = CreateBlock(ctx->idCounter++, 10, y, "%1 > %2", CAT_OPERATORS, SHAPE_BOOLEAN);
    b.param1Str = ""; b.param2Str = "50"; ctx->blocks.push_back(b); y+=40;

    b = CreateBlock(ctx->idCounter++, 10, y, "%1 < %2", CAT_OPERATORS, SHAPE_BOOLEAN);
    b.param1Str = ""; b.param2Str = "50"; ctx->blocks.push_back(b); y+=40;

    b = CreateBlock(ctx->idCounter++, 10, y, "%1 = %2", CAT_OPERATORS, SHAPE_BOOLEAN);
    b.param1Str = ""; b.param2Str = "50"; ctx->blocks.push_back(b); y+=45;

    // عملگرهای منطقی (فعلاً متن ساده دارن تا سیستم Snap کردن عملگرها کامل شه)
    { Block b=CreateBlock(ctx->idCounter++, 10, y, "%1 and %2", CAT_OPERATORS, SHAPE_BOOLEAN); b.opCode=OP_AND; b.rect.w=140; ctx->blocks.push_back(b); y+=40; }
    { Block b=CreateBlock(ctx->idCounter++, 10, y, "%1 or %2", CAT_OPERATORS, SHAPE_BOOLEAN); b.opCode=OP_OR; b.rect.w=140; ctx->blocks.push_back(b); y+=40; }
    { Block b=CreateBlock(ctx->idCounter++, 10, y, "not %1", CAT_OPERATORS, SHAPE_BOOLEAN); b.opCode=OP_NOT; b.rect.w=100; ctx->blocks.push_back(b); y+=45; }

    // رشته‌ها
    b = CreateBlock(ctx->idCounter++, 10, y, "join %1 %2", CAT_OPERATORS, SHAPE_REPORTER);
    b.param1Str = "apple"; b.param2Str = "banana"; ctx->blocks.push_back(b); y+=40;

    b = CreateBlock(ctx->idCounter++, 10, y, "length of %1", CAT_OPERATORS, SHAPE_REPORTER);
    b.param1Str = "apple"; ctx->blocks.push_back(b); y+=40;

    // *** متغیرها (Variables) ***
    y += 20;
    b = CreateBlock(ctx->idCounter++, 10, y, "Set var %1 to %2", CAT_VARIABLES, SHAPE_STACK);
    b.param1Str = "score"; b.param2Str = "0"; ctx->blocks.push_back(b); y+=step;

    b = CreateBlock(ctx->idCounter++, 10, y, "Change var %1 by %2", CAT_VARIABLES, SHAPE_STACK);
    b.param1Str = "score"; b.param2Str = "1"; ctx->blocks.push_back(b); y+=step;

    b = CreateBlock(ctx->idCounter++, 10, y, "val of var %1", CAT_VARIABLES, SHAPE_REPORTER);
    b.param1Str = "score"; ctx->blocks.push_back(b); y+=step;

    // شروع مختصات برای ساخت بلاک‌های کاستوم جدید در پالت
    y += 20;
    ctx->customBlocksPaletteY = y;
}

void TrySnapBlock(BlockSystemContext* ctx, Block* draggedBlock) {
    // snap radius بزرگتر = راحت‌تر چسبیدن
    const int SNAP_R = 45;

    for (auto& target : ctx->blocks) {
        if (&target == draggedBlock || target.rect.x < ctx->paletteArea.w) continue;

        bool dragIsExpr = (draggedBlock->shape == SHAPE_REPORTER || draggedBlock->shape == SHAPE_BOOLEAN);

        // ---- 1. slot های ورودی متنی (باکس‌های سفید %1 %2) ----
        if (dragIsExpr) {
            if (target.inputRect1.w > 0 && target.arg1 == nullptr) {
                int dx = std::abs(draggedBlock->rect.x - target.inputRect1.x);
                int dy = std::abs(draggedBlock->rect.y - target.inputRect1.y);
                if (dx < SNAP_R && dy < SNAP_R) { target.arg1 = draggedBlock; return; }
            }
            if (target.inputRect2.w > 0 && target.arg2 == nullptr) {
                int dx = std::abs(draggedBlock->rect.x - target.inputRect2.x);
                int dy = std::abs(draggedBlock->rect.y - target.inputRect2.y);
                if (dx < SNAP_R && dy < SNAP_R) { target.arg2 = draggedBlock; return; }
            }

            // ---- 2. slot شرط C-Shape/E-Shape (If/Wait Until) ----
            if ((target.shape==SHAPE_C_SHAPE||target.shape==SHAPE_E_SHAPE)
                && target.condition==nullptr
                && (target.opCode==OP_IF||target.opCode==OP_IF_ELSE||target.opCode==OP_WAIT_UNTIL)) {
                // condition slot نزدیک به ناحیه شرط در نوار بالای بلاک
                int slotX = target.rect.x + 72, slotY = target.rect.y + 10;
                int dx = std::abs(draggedBlock->rect.x - slotX);
                int dy = std::abs(draggedBlock->rect.y - slotY);
                if (dx < SNAP_R*2 && dy < SNAP_R) {
                    target.condition = draggedBlock;
                    return;
                }
            }
            // ---- 2b. ورودی عددی REPEAT ----
            if (target.opCode==OP_REPEAT && target.arg1==nullptr && draggedBlock->shape==SHAPE_REPORTER) {
                int slotX = target.rect.x + 72, slotY = target.rect.y + 12;
                int dx = std::abs(draggedBlock->rect.x - slotX);
                int dy = std::abs(draggedBlock->rect.y - slotY);
                if (dx < SNAP_R*2 && dy < SNAP_R) {
                    target.arg1 = draggedBlock;
                    return;
                }
            }

            // ---- 3. slot های arg1/arg2 بلاک‌های boolean (AND/OR/NOT) ----
            if (target.shape == SHAPE_BOOLEAN) {
                // slot چپ (arg1) - بر اساس inputRect که در Draw تنظیم شده
                if (target.arg1 == nullptr) {
                    int slotX = target.inputRect1.x > 0 ? target.inputRect1.x : target.rect.x + target.rect.h/2 + 4;
                    int slotY = target.inputRect1.y > 0 ? target.inputRect1.y : target.rect.y + 4;
                    int dx = std::abs(draggedBlock->rect.x - slotX);
                    int dy = std::abs(draggedBlock->rect.y - slotY);
                    if (dx < SNAP_R && dy < SNAP_R) { target.arg1 = draggedBlock; return; }
                }
                // slot راست (arg2) - فقط AND/OR
                if (target.arg2 == nullptr && (target.opCode==OP_AND||target.opCode==OP_OR)) {
                    int slotX = target.inputRect2.x > 0 ? target.inputRect2.x : target.rect.x + target.rect.w - target.rect.h/2 - 44;
                    int slotY = target.inputRect2.y > 0 ? target.inputRect2.y : target.rect.y + 4;
                    int dx = std::abs(draggedBlock->rect.x - slotX);
                    int dy = std::abs(draggedBlock->rect.y - slotY);
                    if (dx < SNAP_R && dy < SNAP_R) { target.arg2 = draggedBlock; return; }
                }
            }

            // ---- 4. slot reporter (%1/%2 با inputRect) ----
            if (target.shape == SHAPE_REPORTER) {
                if (target.arg1 == nullptr && target.inputRect1.w > 0) {
                    int dx = std::abs(draggedBlock->rect.x - target.inputRect1.x);
                    int dy = std::abs(draggedBlock->rect.y - target.inputRect1.y);
                    if (dx < SNAP_R && dy < SNAP_R) { target.arg1 = draggedBlock; return; }
                }
                if (target.arg2 == nullptr && target.inputRect2.w > 0) {
                    int dx = std::abs(draggedBlock->rect.x - target.inputRect2.x);
                    int dy = std::abs(draggedBlock->rect.y - target.inputRect2.y);
                    if (dx < SNAP_R && dy < SNAP_R) { target.arg2 = draggedBlock; return; }
                }
            }
        }

        // ---- 5. داخل شکم C-Shape ----
        if (target.shape == SHAPE_C_SHAPE && target.subStack == nullptr) {
            if (draggedBlock->shape==SHAPE_STACK||draggedBlock->shape==SHAPE_C_SHAPE||draggedBlock->shape==SHAPE_E_SHAPE||draggedBlock->shape==SHAPE_CAP) {
                int dx = std::abs(draggedBlock->rect.x - (target.rect.x + 20));
                int dy = std::abs(draggedBlock->rect.y - (target.rect.y + 42));
                if (dx < 40 && dy < 40) { target.subStack = draggedBlock; return; }
            }
        }

        // ---- 6. داخل شکم اول و دوم E-Shape ----
        if (target.shape == SHAPE_E_SHAPE) {
            if (draggedBlock->shape!=SHAPE_REPORTER && draggedBlock->shape!=SHAPE_BOOLEAN) {
                if (target.subStack == nullptr) {
                    int dx = std::abs(draggedBlock->rect.x - (target.rect.x + 20));
                    int dy = std::abs(draggedBlock->rect.y - (target.rect.y + 42));
                    if (dx < 40 && dy < 40) { target.subStack = draggedBlock; return; }
                }
                if (target.subStack2 == nullptr) {
                    int dx = std::abs(draggedBlock->rect.x - (target.rect.x + 20));
                    int dy = std::abs(draggedBlock->rect.y - (target.rect.y + target.midYOffset + 32));
                    if (dx < 40 && dy < 40) { target.subStack2 = draggedBlock; return; }
                }
            }
        }

        // ---- 7. چسبیدن عادی زنجیره‌ای (next) ----
        if (!dragIsExpr && target.next==nullptr && target.shape!=SHAPE_CAP && target.opCode!=OP_FOREVER && target.opCode!=OP_STOP_ALL) {
            int dx = std::abs(draggedBlock->rect.x - target.rect.x);
            int dy = std::abs(draggedBlock->rect.y - (target.rect.y + target.rect.h));
            if (dx < 35 && dy < 30) { target.next = draggedBlock; return; }
        }
    }
}

void HandleBlockEvents(BlockSystemContext* ctx, SDL_Event* e) {
    static Block* activeBlock = nullptr;
    static bool createdFromPalette = false;
    // گرفتن مختصات موس همون اول تابع تا همه بتونن ازش استفاده کنن

    // هندل کردن پنجره ساخت متغیر (جلوگیری از کلیک روی بقیه چیزها)
    if (ctx->isMakingVar) {
        if (e->type == SDL_KEYDOWN) {
            if (e->key.keysym.sym == SDLK_ESCAPE) { ctx->isMakingVar = false; SDL_StopTextInput(); return; }
            if (e->key.keysym.sym == SDLK_BACKSPACE && !ctx->newVarName.empty()) { ctx->newVarName.pop_back(); ctx->varErrorMsg = ""; return; }
            if (e->key.keysym.sym == SDLK_RETURN && !ctx->newVarName.empty()) {
                // چک کردن اسم تکراری (دقیقاً مثل اسکرچ)
                bool exists = false;
                for (auto& v : ctx->variables) if (v.name == ctx->newVarName) exists = true;
                if (exists) { ctx->varErrorMsg = "Variable name already exists!"; }
                else {
                    ctx->variables.push_back({ctx->newVarName, 0.0f, true});
                    ctx->isMakingVar = false; SDL_StopTextInput();
                }
                return;
            }
        } else if (e->type == SDL_TEXTINPUT) { ctx->newVarName += e->text.text; ctx->varErrorMsg = ""; return; }
        return; // تا وقتی پنجره بازه، اجازه هیچ کار دیگه‌ای رو نده!
    }
    // --- سیستم کیبورد برای ساخت تابع جدید (My Block) ---
    if (ctx->isMakingBlock) {
        if (e->type == SDL_KEYDOWN) {
            if (e->key.keysym.sym == SDLK_ESCAPE) { ctx->isMakingBlock = false; SDL_StopTextInput(); return; }
            if (e->key.keysym.sym == SDLK_BACKSPACE && !ctx->newBlockName.empty()) { ctx->newBlockName.pop_back(); ctx->blockErrorMsg = ""; return; }
            if (e->key.keysym.sym == SDLK_RETURN && !ctx->newBlockName.empty()) {
                bool exists = false;
                for (auto& n : ctx->customBlocksList) if (n == ctx->newBlockName) exists = true;
                if (exists) { ctx->blockErrorMsg = "Block name already exists!"; }
                else {
                    ctx->customBlocksList.push_back(ctx->newBlockName);

                    // 1. جادو: ساخت اتوماتیک بلاک Define کلاهدار در وسط محیط کار
                    Block defBlock = CreateBlock(ctx->idCounter++, 350, 100, "Define " + ctx->newBlockName, CAT_MYBLOCKS, SHAPE_HAT);
                    defBlock.stringParam = ctx->newBlockName;
                    ctx->blocks.push_back(defBlock);

                    // 2. جادو: ساخت اتوماتیک بلاک فراخوانی در پالت سمت چپ
                    Block callBlock = CreateBlock(ctx->idCounter++, 10, ctx->customBlocksPaletteY, ctx->newBlockName, CAT_MYBLOCKS, SHAPE_STACK);
                    callBlock.opCode = OP_CALL_CUSTOM; // تنظیم دستی آپکد
                    callBlock.stringParam = ctx->newBlockName;
                    ctx->blocks.push_back(callBlock);

                    ctx->customBlocksPaletteY += 50; // جا باز کردن برای تابع بعدی
                    ctx->isMakingBlock = false; SDL_StopTextInput();
                }
                return;
            }
        } else if (e->type == SDL_TEXTINPUT) { ctx->newBlockName += e->text.text; ctx->blockErrorMsg = ""; return; }
        return; // قفل کردن صفحه
    }

    int mx, my; SDL_GetMouseState(&mx, &my);
    // --- هندل کردن کلیک روی دکمه Make a Block ---
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        // (اینجا مختصات دکمه متغیر بود که تغییر دادیمش به بالای صفحه)
        if (mx > 10 && mx < 150 && my > ctx->paletteArea.y + 10 - ctx->scrollY && my < ctx->paletteArea.y + 40 - ctx->scrollY) {
            ctx->isMakingVar = true; ctx->newVarName = ""; ctx->varErrorMsg = ""; SDL_StartTextInput(); return;
        }
        // کلیک دکمه تابع (صورتی)
        if (mx > 10 && mx < 150 && my > ctx->paletteArea.y + 45 - ctx->scrollY && my < ctx->paletteArea.y + 75 - ctx->scrollY) {
            ctx->isMakingBlock = true; ctx->newBlockName = ""; ctx->blockErrorMsg = ""; SDL_StartTextInput(); return;
        }
        // کلیک دکمه Add Extension
        if (mx > 10 && mx < 150 && my > ctx->paletteArea.y + 83 - ctx->scrollY && my < ctx->paletteArea.y + 113 - ctx->scrollY) {
            if (!ctx->penExtensionEnabled) ctx->showExtensionMenu = true;
            return;
        }
    }

    // هندل کردن کلیک داخل popup extension
    if (ctx->showExtensionMenu && e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        // کلیک X (بستن)
        if (mx > 960 && mx < 990 && my > 90 && my < 120) { ctx->showExtensionMenu = false; return; }
        // کلیک کارت Pen
        if (mx > 310 && mx < 490 && my > 155 && my < 375) {
            if (!ctx->penExtensionEnabled) AddPenBlocksToPalette(ctx);
            ctx->showExtensionMenu = false; return;
        }
        // کلیک خارج از پنجره
        if (mx < 280 || mx > 1000 || my < 80 || my > 640) { ctx->showExtensionMenu = false; return; }
        return; // جلوگیری از کلیک روی بقیه وقتی popup بازه
    }
    if (ctx->showExtensionMenu) return; // قفل صفحه

    // هندل کردن دکمه "Make a Variable"
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        if (mx > 10 && mx < 150 && my > 95 - ctx->scrollY && my < 125 - ctx->scrollY) { // مختصات دکمه
            ctx->isMakingVar = true; ctx->newVarName = ""; ctx->varErrorMsg = ""; SDL_StartTextInput(); return;
        }
    }

    // --- سیستم تایپ اعداد ---
    if (ctx->editingBlock != nullptr) {
        if (e->type == SDL_KEYDOWN) {
            if (e->key.keysym.sym == SDLK_RETURN || e->key.keysym.sym == SDLK_ESCAPE) {
                ctx->editingBlock = nullptr; ctx->editingParam = 0; SDL_StopTextInput(); return;
            }
            if (e->key.keysym.sym == SDLK_BACKSPACE) {
                std::string& str = (ctx->editingParam == 1) ? ctx->editingBlock->param1Str : ctx->editingBlock->param2Str;
                if (!str.empty()) str.pop_back();
                return;
            }
        } else if (e->type == SDL_TEXTINPUT) {
            std::string& str = (ctx->editingParam == 1) ? ctx->editingBlock->param1Str : ctx->editingBlock->param2Str;
            str += e->text.text; return;
        }
    }

    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        for (int i = ctx->blocks.size() - 1; i >= 0; i--) {
            Block& b = ctx->blocks[i];
            bool isPalette = b.rect.x < ctx->paletteArea.w;
            int currentY = isPalette ? b.rect.y + ctx->scrollY : b.rect.y;

            // تشخیص کلیک روی باکس‌های سفید (برای شروع تایپ)
            if (!isPalette && b.arg1 == nullptr && mx > b.inputRect1.x && mx < b.inputRect1.x + b.inputRect1.w && my > b.inputRect1.y && my < b.inputRect1.y + b.inputRect1.h) {
                ctx->editingBlock = &b; ctx->editingParam = 1; SDL_StartTextInput(); activeBlock = nullptr; return;
            }
            if (!isPalette && b.arg2 == nullptr && b.inputRect2.w > 0 && mx > b.inputRect2.x && mx < b.inputRect2.x + b.inputRect2.w && my > b.inputRect2.y && my < b.inputRect2.y + b.inputRect2.h) {
                ctx->editingBlock = &b; ctx->editingParam = 2; SDL_StartTextInput(); activeBlock = nullptr; return;
            }
            // بستن تایپ اگه جای دیگه کلیک شد
            ctx->editingBlock = nullptr; ctx->editingParam = 0; SDL_StopTextInput();

            if (mx > b.rect.x && mx < b.rect.x + b.rect.w && my > currentY && my < currentY + b.rect.h) {
                if (isPalette) {
                    if (my < ctx->paletteArea.y || my > ctx->paletteArea.y + ctx->paletteArea.h) continue;
                    Block newB = b; newB.id = ctx->idCounter++; newB.next = nullptr;
                    newB.subStack = nullptr; newB.condition = nullptr; newB.arg1 = nullptr; newB.arg2 = nullptr;
                    newB.rect.y = currentY;
                    ctx->blocks.push_back(newB);
                    activeBlock = &ctx->blocks.back();
                    createdFromPalette = true;
                } else {
                    activeBlock = &b; createdFromPalette = false;
                    // جدا کردن بلاک از هر پدری که داره (حتی اگر تو در تو بود)
                    for(auto& parent : ctx->blocks) {
                        if(parent.next == activeBlock) parent.next = nullptr;
                        if(parent.subStack == activeBlock) parent.subStack = nullptr;
                        if(parent.condition == activeBlock) { parent.condition = nullptr; parent.arg1 = nullptr; }
                        if(parent.arg1 == activeBlock) parent.arg1 = nullptr;
                        if(parent.arg2 == activeBlock) parent.arg2 = nullptr;
                    }
                }
                activeBlock->isDragging = true; break;
            }
        }
    }
    else if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
        if (activeBlock) {
            activeBlock->isDragging = false;
            if (!createdFromPalette) TrySnapBlock(ctx, activeBlock);
            activeBlock = nullptr;
        }
    }
    else if (e->type == SDL_MOUSEMOTION) {
        if (activeBlock && activeBlock->isDragging) MoveBlockChain(activeBlock, e->motion.xrel, e->motion.yrel);
    }
}

// *** رسم بلاک‌ها با شکل‌های حرفه‌ای (C-Shape، Oval، Hexagon) ***
void DrawScratchBlock(SDL_Renderer* renderer, Block& b, int x, int y, TTF_Font* font) {
    SDL_Color c  = GetCategoryColor(b.category);
    SDL_Color cd = {(Uint8)(c.r*75/100),(Uint8)(c.g*75/100),(Uint8)(c.b*75/100),255}; // dark
    SDL_Color cl = {(Uint8)std::min(255,c.r+55),(Uint8)std::min(255,c.g+55),(Uint8)std::min(255,c.b+55),255}; // light

    int W = b.rect.w, H = b.rect.h;

    // ── helper lambdas ──────────────────────────────────────────
    auto FR  = [&](int rx,int ry,int rw,int rh,SDL_Color col){
        if(rw<=0||rh<=0) return;
        SDL_SetRenderDrawColor(renderer,col.r,col.g,col.b,col.a);
        SDL_Rect r={rx,ry,rw,rh}; SDL_RenderFillRect(renderer,&r); };
    auto DRP = [&](int rx,int ry,int px,int py,SDL_Color col){
        SDL_SetRenderDrawColor(renderer,col.r,col.g,col.b,col.a);
        SDL_RenderDrawLine(renderer,rx,ry,px,py); };

    // rounded rect helper (corner radius r)
    auto RoundRect = [&](int rx,int ry,int rw,int rh,int r,SDL_Color col){
        if(rw<=0||rh<=0) return;
        FR(rx+r,ry,rw-2*r,rh,col);
        FR(rx,ry+r,r,rh-2*r,col);
        FR(rx+rw-r,ry+r,r,rh-2*r,col);
        // corners
        for(int i=0;i<r;i++){
            int len=(int)std::sqrt((float)(r*r-(r-1-i)*(r-1-i)));
            FR(rx+r-len,ry+i,len,1,col);
            FR(rx+rw-r,ry+i,len,1,col);
            FR(rx+r-len,ry+rh-1-i,len,1,col);
            FR(rx+rw-r,ry+rh-1-i,len,1,col);
        }
    };

    // ── سایه ────────────────────────────────────────────────────
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer,0,0,0,40);
    SDL_Rect sh={x+3,y+3,W,H}; SDL_RenderFillRect(renderer,&sh);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);

    // ════════════════════════════════════════════════════════════
    if (b.shape == SHAPE_REPORTER) {
        // ── بیضی (oval) ─────────────────────────────────────────
        int r = H/2;
        FR(x+r, y, W-2*r, H, c);
        for(int i=0;i<r;i++){
            int len=(int)std::sqrt((float)(r*r-(r-1-i)*(r-1-i)));
            FR(x+r-len,y+i,len*2+(W-2*r),1,c);
        }
        // highlight strip
        FR(x+r,y+1,W-2*r,2,cl);
        for(int i=0;i<r-1;i++){
            int len=(int)std::sqrt((float)((r-1)*(r-1)-(r-2-i)*(r-2-i)));
            FR(x+r-1-len,y+1+i,1,1,cl);
            FR(x+W-r+len,y+1+i,1,1,cl);
        }

    } else if (b.shape == SHAPE_BOOLEAN) {
        // ── شش‌ضلعی (hexagon) ───────────────────────────────────
        int hp = H/2; // نوک تیزی
        // بدنه مستطیل وسط
        FR(x+hp, y, W-2*hp, H, c);
        // مثلث چپ
        for(int i=0;i<hp;i++){
            int len = i+1;
            FR(x+hp-len, y+hp-i-1, len, 1, c);
            FR(x+hp-len, y+hp+i,   len, 1, c);
        }
        // مثلث راست
        for(int i=0;i<hp;i++){
            int len = i+1;
            FR(x+W-hp, y+hp-i-1, len, 1, c);
            FR(x+W-hp, y+hp+i,   len, 1, c);
        }
        // highlight
        FR(x+hp,y+1,W-2*hp,1,cl);
        DRP(x+hp,y+1,x+1,y+hp,cl);
        DRP(x+W-hp,y+1,x+W-1,y+hp,cl);

    } else if (b.shape==SHAPE_C_SHAPE || b.shape==SHAPE_E_SHAPE) {
        // ── C-Shape / E-Shape ────────────────────────────────────
        int topH=40, botH=28, armW=20;

        // نوار بالا
        RoundRect(x,y,W,topH,6,c);
        FR(x,y+topH-6,W,6,c); // پایین نوار بالا رو پر کن (بدون گردی)
        FR(x,y+1,W,2,cl); // highlight

        if (b.shape==SHAPE_E_SHAPE) {
            int mid = b.midYOffset;
            // بازوی چپ اول
            FR(x,y+topH,armW,mid-topH,c);
            // نوار else
            FR(x,y+mid,W-armW,28,c);
            FR(x,y+mid+1,W-armW,2,cl);
            // بازوی چپ دوم
            FR(x,y+mid+28,armW,H-(mid+28)-botH,c);
        } else {
            // بازوی چپ
            FR(x,y+topH,armW,H-topH-botH,c);
        }

        // نوار پایین
        FR(x,y+H-botH,W,botH,c);
        FR(x,y+H-botH,W,botH,cd);
        RoundRect(x,y+H-botH,W,botH,6,cd);
        FR(x,y+H-botH,W,6,cd);
        FR(x,y+H-botH,W,2,c);

        // ناحیه داخل (رنگ کمی تیره‌تر = نشون دهنده عمق)
        SDL_Color inner = {(Uint8)(cd.r*90/100),(Uint8)(cd.g*90/100),(Uint8)(cd.b*90/100),255};
        if(b.shape==SHAPE_C_SHAPE){
            FR(x+armW,y+topH,W-armW,H-topH-botH,inner);
        } else {
            int mid=b.midYOffset;
            FR(x+armW,y+topH,W-armW,mid-topH,inner);
            FR(x+armW,y+mid+28,W-armW,H-(mid+28)-botH,inner);
        }

        // نوشتن "else" روی نوار میانی
        if(b.shape==SHAPE_E_SHAPE && font){
            SDL_Surface* es=TTF_RenderText_Blended(font,"else",{255,255,255,200});
            if(es){
                SDL_Rect er={x+armW+6,y+b.midYOffset+6,es->w,es->h};
                SDL_Texture* et=SDL_CreateTextureFromSurface(renderer,es);
                SDL_RenderCopy(renderer,et,NULL,&er);
                SDL_DestroyTexture(et); SDL_FreeSurface(es);
            }
        }

        // condition slot خالی
        if(!b.condition && (b.opCode==OP_IF||b.opCode==OP_IF_ELSE||b.opCode==OP_WAIT_UNTIL)){
            // شکل شش‌ضلعی کوچک برای condition slot
            int sx=x+70,sy=y+10,sw=44,sh=20,shp=sh/2;
            SDL_Color sc={(Uint8)(cd.r*80/100),(Uint8)(cd.g*80/100),(Uint8)(cd.b*80/100),255};
            FR(sx+shp,sy,sw-2*shp,sh,sc);
            for(int i=0;i<shp;i++){
                FR(sx+shp-i-1,sy+shp-i-1,i+1,1,sc);
                FR(sx+shp-i-1,sy+shp+i,  i+1,1,sc);
                FR(sx+sw-shp, sy+shp-i-1,i+1,1,sc);
                FR(sx+sw-shp, sy+shp+i,  i+1,1,sc);
            }
        }
        // condition slot خالی برای REPEAT
        if(!b.arg1 && b.opCode==OP_REPEAT){
            SDL_Color sc={cd.r,cd.g,cd.b,255};
            FR(x+68,y+10,32,22,sc);
            RoundRect(x+68,y+10,32,22,4,sc);
        }

    } else {
        // ── STACK / HAT / CAP ────────────────────────────────────
        RoundRect(x,y,W,H,6,c);
        FR(x,y+1,W,2,cl); // highlight
        FR(x,y+H-4,W,3,cd); // shadow bottom
    }

    // ── کلاهک HAT ────────────────────────────────────────────────
    if(b.shape==SHAPE_HAT){
        // گنبد بالا
        for(int i=0;i<16;i++){
            int hw2=(int)(70*(1.0f-std::pow((i-8)/8.0f,2)));
            FR(x+5+70-hw2,y-16+i,hw2*2,1,c);
        }
        FR(x,y,W,3,cl);
    }

    // ── پین پازلی ────────────────────────────────────────────────
    bool hasBump = (b.shape==SHAPE_STACK||b.shape==SHAPE_HAT||
                    b.shape==SHAPE_C_SHAPE||b.shape==SHAPE_E_SHAPE)
                   && b.opCode!=OP_FOREVER && b.opCode!=OP_STOP_ALL;
    bool hasNotch = (b.shape==SHAPE_STACK||b.shape==SHAPE_C_SHAPE||
                     b.shape==SHAPE_E_SHAPE||b.shape==SHAPE_CAP);

    if(hasBump){
        // برآمدگی پایین (trapezoidal bump)
        FR(x+14,y+H,  28,4,cd);
        FR(x+16,y+H+4,24,3,cd);
        FR(x+18,y+H+7,20,2,cd);
    }
    if(hasNotch){
        // شکاف بالا
        FR(x+14,y-2,28,4,{(Uint8)(cd.r*80/100),(Uint8)(cd.g*80/100),(Uint8)(cd.b*80/100),255});
        FR(x+16,y-5,24,3,{(Uint8)(cd.r*80/100),(Uint8)(cd.g*80/100),(Uint8)(cd.b*80/100),255});
    }

    if(!font) return;

    bool isExpr=(b.shape==SHAPE_REPORTER||b.shape==SHAPE_BOOLEAN);
    int cX = x + (isExpr ? (b.shape==SHAPE_BOOLEAN?(H/2)+4:8) : 10);
    int cY = y + (isExpr ? 7 : (b.shape==SHAPE_HAT?16:13));
    int txtH = 18; // ارتفاع خط متن

    SDL_Color txtCol = {255,255,255,255};

    auto DrawTxt = [&](const std::string& s) -> int {
        if(s.empty()) return 0;
        SDL_Surface* sf=TTF_RenderText_Blended(font,s.c_str(),txtCol);
        if(!sf) return 0;
        int sw2=sf->w;
        SDL_Rect tr={cX,cY,sf->w,sf->h};
        SDL_Texture* tx=SDL_CreateTextureFromSurface(renderer,sf);
        SDL_RenderCopy(renderer,tx,NULL,&tr);
        SDL_DestroyTexture(tx); SDL_FreeSurface(sf);
        cX+=sw2+4;
        return sw2;
    };

    // رسم slot خالی (مستطیل/شش‌ضلعی) برای ورودی
    auto DrawSlot = [&](bool isBoolean, SDL_Rect& outR) {
        int slotW = isBoolean ? 40 : 32;
        int slotH = isBoolean ? (H>30?24:18) : (isExpr?18:22);
        outR = {cX, cY-2, slotW, slotH};
        if(isBoolean){
            // شش‌ضلعی کوچک
            int hp2=slotH/2;
            SDL_Color sc={(Uint8)(cd.r*80/100),(Uint8)(cd.g*80/100),(Uint8)(cd.b*80/100),255};
            FR(cX+hp2,cY-2,slotW-2*hp2,slotH,sc);
            for(int i=0;i<hp2;i++){
                FR(cX+hp2-i-1,cY-2+hp2-i-1,i+1,1,sc);
                FR(cX+hp2-i-1,cY-2+hp2+i,  i+1,1,sc);
                FR(cX+slotW-hp2,cY-2+hp2-i-1,i+1,1,sc);
                FR(cX+slotW-hp2,cY-2+hp2+i,  i+1,1,sc);
            }
        } else {
            // بیضی کوچک
            int rr=slotH/2;
            SDL_Color sc={255,255,255,255};
            FR(cX+rr,cY-2,slotW-2*rr,slotH,sc);
            for(int i=0;i<rr;i++){
                int len2=(int)std::sqrt((float)(rr*rr-(rr-1-i)*(rr-1-i)));
                FR(cX+rr-len2,cY-2+i,len2*2+(slotW-2*rr),1,sc);
            }
        }
        cX += slotW + 4;
    };

    // رسم ورودی (عدد/متن قابل ویرایش)
    auto DrawInput = [&](std::string& val, SDL_Rect& outR, Block* arg, bool forBool=false) {
        if(arg){
            // بلاک داخل slot - فقط جا باز میکنیم (خودش جدا رندر میشه)
            outR={0,0,0,0};
            cX += arg->rect.w + 4;
            return;
        }
        bool needBool = forBool;
        if(needBool){
            DrawSlot(true, outR);
        } else {
            // باکس سفید گرد برای ورودی عدد
            SDL_Surface* sv=TTF_RenderText_Blended(font, val.empty()?" ":val.c_str(), {30,30,30,255});
            int bw = sv ? std::max(30, sv->w+16) : 30;
            int bh = isExpr ? 18 : 22;
            outR={cX,cY-2,bw,bh};
            int rr2=bh/2;
            FR(cX+rr2,cY-2,bw-2*rr2,bh,{255,255,255,255});
            for(int i=0;i<rr2;i++){
                int len2=(int)std::sqrt((float)(rr2*rr2-(rr2-1-i)*(rr2-1-i)));
                FR(cX+rr2-len2,cY-2+i,len2*2+(bw-2*rr2),1,{255,255,255,255});
            }
            if(sv){
                SDL_Rect tr={cX+8,cY,sv->w,sv->h};
                SDL_Texture* tv=SDL_CreateTextureFromSurface(renderer,sv);
                SDL_RenderCopy(renderer,tv,NULL,&tr);
                SDL_DestroyTexture(tv); SDL_FreeSurface(sv);
            }
            cX+=bw+5;
        }
    };

    // ── رندر بلاک‌های خاص AND/OR/NOT ────────────────────────────
    if(b.opCode==OP_AND || b.opCode==OP_OR){
        // [hex slot] "and/or" [hex slot]
        int hp=H/2;
        cX = x+hp+4; cY=y+(H-18)/2;
        // slot اول
        if(b.arg1){ b.inputRect1={0,0,0,0}; cX+=b.arg1->rect.w+4; }
        else { DrawSlot(true, b.inputRect1); }
        // label
        DrawTxt(b.opCode==OP_AND ? "and" : "or");
        // slot دوم
        if(b.arg2){ b.inputRect2={0,0,0,0}; cX+=b.arg2->rect.w+4; }
        else { DrawSlot(true, b.inputRect2); }
        return;
    }
    if(b.opCode==OP_NOT){
        int hp=H/2;
        cX=x+hp+4; cY=y+(H-18)/2;
        DrawTxt("not");
        if(b.arg1){ b.inputRect1={0,0,0,0}; cX+=b.arg1->rect.w+4; }
        else { DrawSlot(true, b.inputRect1); }
        b.inputRect2={0,0,0,0};
        return;
    }

    std::string txt = b.text;
    size_t p1=txt.find("%1"), p2=txt.find("%2");

    if(p1!=std::string::npos){
        DrawTxt(txt.substr(0,p1));
        bool needBool1 = (b.shape==SHAPE_C_SHAPE||b.shape==SHAPE_E_SHAPE) && p1==txt.find("%1") && b.opCode!=OP_REPEAT;
        DrawInput(b.param1Str, b.inputRect1, b.arg1, needBool1);
        if(p2!=std::string::npos){
            DrawTxt(txt.substr(p1+2, p2-(p1+2)));
            DrawInput(b.param2Str, b.inputRect2, b.arg2);
            DrawTxt(txt.substr(p2+2));
        } else {
            DrawTxt(txt.substr(p1+2));
            b.inputRect2={0,0,0,0};
        }
    } else {
        DrawTxt(txt);
        b.inputRect1=b.inputRect2={0,0,0,0};
    }
}

void RenderBlockSystem(SDL_Renderer* renderer, BlockSystemContext* ctx, TTF_Font* font) {
    // 1. پالت (روشن)
    SDL_SetRenderDrawColor(renderer, 250, 250, 250, 255); SDL_RenderFillRect(renderer, &ctx->paletteArea);
    // workspace (با dot-grid جذاب)
    SDL_Rect wsArea = {ctx->paletteArea.w, ctx->paletteArea.y, 1280-ctx->paletteArea.w, ctx->paletteArea.h};
    SDL_SetRenderDrawColor(renderer, 248, 248, 252, 255); SDL_RenderFillRect(renderer, &wsArea);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 180, 185, 205, 80);
    for(int gx=ctx->paletteArea.w+20; gx<1280; gx+=20)
        for(int gy=ctx->paletteArea.y+20; gy<720; gy+=20)
            SDL_RenderDrawPoint(renderer, gx, gy);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    // خط جدا کننده
    SDL_SetRenderDrawColor(renderer, 210, 212, 222, 255);
    SDL_RenderDrawLine(renderer, ctx->paletteArea.w, ctx->paletteArea.y, ctx->paletteArea.w, 720);

    // 2. رسم دکمه‌های "Make a Variable" و "Make a Block" در بالای پالت
    SDL_Rect makeVarBtn = {10, ctx->paletteArea.y + 10 - ctx->scrollY, 140, 30};
    SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255); SDL_RenderFillRect(renderer, &makeVarBtn);
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255); SDL_RenderDrawRect(renderer, &makeVarBtn);

    SDL_Rect makeBlockBtn = {10, ctx->paletteArea.y + 45 - ctx->scrollY, 140, 30};
    SDL_SetRenderDrawColor(renderer, 255, 102, 128, 255); SDL_RenderFillRect(renderer, &makeBlockBtn); // صورتی
    SDL_SetRenderDrawColor(renderer, 200, 80, 100, 255); SDL_RenderDrawRect(renderer, &makeBlockBtn);

    // دکمه Add Extension
    SDL_Rect addExtBtn = {10, ctx->paletteArea.y + 83 - ctx->scrollY, 140, 30};
    SDL_Color extBtnColor = ctx->penExtensionEnabled ? SDL_Color{0,189,140,255} : SDL_Color{100,180,200,255};
    SDL_SetRenderDrawColor(renderer, extBtnColor.r, extBtnColor.g, extBtnColor.b, 255);
    SDL_RenderFillRect(renderer, &addExtBtn);
    SDL_SetRenderDrawColor(renderer, extBtnColor.r-30, extBtnColor.g-30, extBtnColor.b-30, 255);
    SDL_RenderDrawRect(renderer, &addExtBtn);

    if(font) {
        auto DrawBtnText = [&](std::string txt, SDL_Rect btnRect, SDL_Color col) {
            SDL_Surface* ms = TTF_RenderText_Blended(font, txt.c_str(), col);
            if(ms) {
                SDL_Rect tr = {btnRect.x + 10, btnRect.y + 5, ms->w, ms->h};
                SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ms);
                SDL_RenderCopy(renderer, tt, NULL, &tr); SDL_DestroyTexture(tt); SDL_FreeSurface(ms);
            }
        };
        DrawBtnText("Make a Variable", makeVarBtn, {50,50,50,255});
        DrawBtnText("Make a Block", makeBlockBtn, {255,255,255,255});
        if (ctx->penExtensionEnabled)
            DrawBtnText("Pen: ON", addExtBtn, {255,255,255,255});
        else
            DrawBtnText("+ Add Extension", addExtBtn, {255,255,255,255});
    }

    // 3. اعمال سیستم چیدمان درختی (Layout) برای بلاک‌های محیط کار
    for (auto& b : ctx->blocks) {
        if (b.rect.x >= ctx->paletteArea.w) {
            // پیدا کردن بلاک‌های ریشه (که پدر ندارند)
            bool isRoot = true;
            for (auto& parent : ctx->blocks) {
                if (parent.next == &b || parent.subStack == &b || parent.subStack2 == &b ||
                    parent.condition == &b || parent.arg1 == &b || parent.arg2 == &b) {
                    isRoot = false; break;
                }
            }
            if (isRoot) LayoutBlockChain(&b, b.rect.x, b.rect.y);
        }
    }

    // 4. رسم بلاک‌های پالت با اسکرول (برش تصویر تا بیرون نزنند)
    SDL_RenderSetClipRect(renderer, &ctx->paletteArea);
    // پاس اول پالت: بلاک‌های غیر expression
    for (auto& b : ctx->blocks) {
        if (b.rect.x < ctx->paletteArea.w && b.shape!=SHAPE_REPORTER && b.shape!=SHAPE_BOOLEAN)
            DrawScratchBlock(renderer, b, b.rect.x, b.rect.y + ctx->scrollY, font);
    }
    // پاس دوم پالت: expression blocks (روی بقیه)
    for (auto& b : ctx->blocks) {
        if (b.rect.x < ctx->paletteArea.w && (b.shape==SHAPE_REPORTER||b.shape==SHAPE_BOOLEAN))
            DrawScratchBlock(renderer, b, b.rect.x, b.rect.y + ctx->scrollY, font);
    }
    SDL_RenderSetClipRect(renderer, NULL);

    // 5. رسم بلاک‌های محیط کار
    // پاس اول: C-Shape و E-Shape (پس‌زمینه)
    for (auto& b : ctx->blocks) {
        if (b.rect.x >= ctx->paletteArea.w && (b.shape==SHAPE_C_SHAPE||b.shape==SHAPE_E_SHAPE))
            DrawScratchBlock(renderer, b, b.rect.x, b.rect.y, font);
    }
    // پاس دوم: بقیه بلاک‌های workspace (STACK/HAT/CAP)
    for (auto& b : ctx->blocks) {
        if (b.rect.x >= ctx->paletteArea.w && b.shape!=SHAPE_C_SHAPE && b.shape!=SHAPE_E_SHAPE
            && b.shape!=SHAPE_REPORTER && b.shape!=SHAPE_BOOLEAN)
            DrawScratchBlock(renderer, b, b.rect.x, b.rect.y, font);
    }
    // پاس سوم: بلاک‌های expression (reporter/boolean) که روی بلاک‌های دیگه قرار دارن
    // اینا باید آخر رندر بشن تا روی بدنه بلاک host نشون داده بشن
    for (auto& b : ctx->blocks) {
        if (b.rect.x >= ctx->paletteArea.w && (b.shape==SHAPE_REPORTER||b.shape==SHAPE_BOOLEAN)) {
            // چک: آیا این بلاک داخل یه بلاک دیگه‌ای افتاده؟
            bool isChild = false;
            for (auto& p : ctx->blocks) {
                if (p.arg1==&b || p.arg2==&b || p.condition==&b) { isChild=true; break; }
            }
            DrawScratchBlock(renderer, b, b.rect.x, b.rect.y, font);
        }
    }


    // الف) پاپ‌آپ ساخت متغیر (نارنجی/آبی)
    if (ctx->isMakingVar) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
        SDL_Rect f = {0,0,1280,720}; SDL_RenderFillRect(renderer, &f); // تاریک کردن کل صفحه
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

        SDL_Rect box = {400, 250, 400, 180};
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &box);
        SDL_SetRenderDrawColor(renderer, 76, 151, 255, 255); SDL_RenderDrawRect(renderer, &box);

        if(font) {
            SDL_Surface* ts = TTF_RenderText_Blended(font, "New Variable Name:", {0,0,0,255});
            if(ts) {
                SDL_Rect tr = {box.x + 20, box.y + 20, ts->w, ts->h}; SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
                SDL_RenderCopy(renderer, tt, NULL, &tr); SDL_DestroyTexture(tt); SDL_FreeSurface(ts);
            }
            SDL_Rect inBox = {box.x + 20, box.y + 60, 360, 35};
            SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255); SDL_RenderFillRect(renderer, &inBox);
            SDL_SetRenderDrawColor(renderer, 76, 151, 255, 255); SDL_RenderDrawRect(renderer, &inBox);

            std::string disp = ctx->newVarName + "|";
            SDL_Surface* is = TTF_RenderText_Blended(font, disp.c_str(), {0,0,0,255});
            if(is) {
                SDL_Rect ir = {inBox.x + 10, inBox.y + 8, is->w, is->h}; SDL_Texture* it = SDL_CreateTextureFromSurface(renderer, is);
                SDL_RenderCopy(renderer, it, NULL, &ir); SDL_DestroyTexture(it); SDL_FreeSurface(is);
            }
            if (!ctx->varErrorMsg.empty()) {
                SDL_Surface* es = TTF_RenderText_Blended(font, ctx->varErrorMsg.c_str(), {255,0,0,255});
                if(es) {
                    SDL_Rect er = {box.x + 20, box.y + 105, es->w, es->h}; SDL_Texture* et = SDL_CreateTextureFromSurface(renderer, es);
                    SDL_RenderCopy(renderer, et, NULL, &er); SDL_DestroyTexture(et); SDL_FreeSurface(es);
                }
            }
            SDL_Surface* hs = TTF_RenderText_Blended(font, "Press ENTER to Create, ESC to Cancel", {150,150,150,255});
            if(hs) {
                SDL_Rect hr = {box.x + 20, box.y + 140, hs->w, hs->h}; SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
                SDL_RenderCopy(renderer, ht, NULL, &hr); SDL_DestroyTexture(ht); SDL_FreeSurface(hs);
            }
        }
    }

    // ب) پاپ‌آپ ساخت تابع دلخواه (صورتی)
    if (ctx->isMakingBlock) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
        SDL_Rect f = {0,0,1280,720}; SDL_RenderFillRect(renderer, &f);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

        SDL_Rect box = {400, 250, 400, 180};
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &box);
        SDL_SetRenderDrawColor(renderer, 255, 102, 128, 255); SDL_RenderDrawRect(renderer, &box); // حاشیه صورتی

        if(font) {
            SDL_Surface* ts = TTF_RenderText_Blended(font, "New Block Name:", {0,0,0,255});
            if(ts) {
                SDL_Rect tr = {box.x + 20, box.y + 20, ts->w, ts->h}; SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
                SDL_RenderCopy(renderer, tt, NULL, &tr); SDL_DestroyTexture(tt); SDL_FreeSurface(ts);
            }
            SDL_Rect inBox = {box.x + 20, box.y + 60, 360, 35};
            SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255); SDL_RenderFillRect(renderer, &inBox);
            SDL_SetRenderDrawColor(renderer, 255, 102, 128, 255); SDL_RenderDrawRect(renderer, &inBox); // اینپوت صورتی

            std::string disp = ctx->newBlockName + "|";
            SDL_Surface* is = TTF_RenderText_Blended(font, disp.c_str(), {0,0,0,255});
            if(is) {
                SDL_Rect ir = {inBox.x + 10, inBox.y + 8, is->w, is->h}; SDL_Texture* it = SDL_CreateTextureFromSurface(renderer, is);
                SDL_RenderCopy(renderer, it, NULL, &ir); SDL_DestroyTexture(it); SDL_FreeSurface(is);
            }
            if (!ctx->blockErrorMsg.empty()) {
                SDL_Surface* es = TTF_RenderText_Blended(font, ctx->blockErrorMsg.c_str(), {255,0,0,255});
                if(es) {
                    SDL_Rect er = {box.x + 20, box.y + 105, es->w, es->h}; SDL_Texture* et = SDL_CreateTextureFromSurface(renderer, es);
                    SDL_RenderCopy(renderer, et, NULL, &er); SDL_DestroyTexture(et); SDL_FreeSurface(es);
                }
            }
            SDL_Surface* hs = TTF_RenderText_Blended(font, "Press ENTER to Create, ESC to Cancel", {150,150,150,255});
            if(hs) {
                SDL_Rect hr = {box.x + 20, box.y + 140, hs->w, hs->h}; SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
                SDL_RenderCopy(renderer, ht, NULL, &hr); SDL_DestroyTexture(ht); SDL_FreeSurface(hs);
            }
        }
    }

    if (ctx->showExtensionMenu) {
        // تاریک کردن پس‌زمینه
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
        SDL_Rect full = {0,0,1280,720}; SDL_RenderFillRect(renderer, &full);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

        // پنجره اصلی
        SDL_Rect win = {280, 80, 720, 560};
        SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255); SDL_RenderFillRect(renderer, &win);
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); SDL_RenderDrawRect(renderer, &win);

        // نوار عنوان
        SDL_Rect titleBar = {win.x, win.y, win.w, 50};
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &titleBar);
        SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
        SDL_RenderDrawLine(renderer, win.x, win.y + 50, win.x + win.w, win.y + 50);

        if (font) {
            auto RenderText = [&](const char* txt, int x, int y, SDL_Color col) {
                SDL_Surface* s = TTF_RenderText_Blended(font, txt, col);
                if (!s) return;
                SDL_Rect r = {x, y, s->w, s->h};
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                SDL_RenderCopy(renderer, t, NULL, &r);
                SDL_DestroyTexture(t); SDL_FreeSurface(s);
            };
            RenderText("Choose an Extension", win.x + 20, win.y + 14, {30,30,30,255});

            // کارت Pen
            SDL_Rect penCard = {win.x + 30, win.y + 75, 180, 220};
            SDL_Color penBg = ctx->penExtensionEnabled ? SDL_Color{0,189,140,255} : SDL_Color{255,255,255,255};
            SDL_SetRenderDrawColor(renderer, penBg.r, penBg.g, penBg.b, 255);
            SDL_RenderFillRect(renderer, &penCard);
            SDL_SetRenderDrawColor(renderer, 0,189,140,255); SDL_RenderDrawRect(renderer, &penCard);

            // آیکون قلم (دایره رنگی)
            SDL_Rect penIcon = {penCard.x + 55, penCard.y + 30, 70, 70};
            SDL_SetRenderDrawColor(renderer, 0,189,140,255); SDL_RenderFillRect(renderer, &penIcon);
            SDL_SetRenderDrawColor(renderer, 0,150,110,255); SDL_RenderDrawRect(renderer, &penIcon);

            SDL_Color penTxtCol = ctx->penExtensionEnabled ? SDL_Color{255,255,255,255} : SDL_Color{30,30,30,255};
            RenderText("Pen", penCard.x + 72, penCard.y + 115, penTxtCol);
            const char* statusTxt = ctx->penExtensionEnabled ? "(Added)" : "Click to add";
            RenderText(statusTxt, penCard.x + 30, penCard.y + 150, {100,100,100,255});
            RenderText("Draw with the pen,", penCard.x + 5, penCard.y + 178, {120,120,120,255});
            RenderText("stamp sprites", penCard.x + 25, penCard.y + 196, {120,120,120,255});
        }

        // دکمه بستن (X)
        SDL_Rect closeBtn = {win.x + win.w - 40, win.y + 10, 30, 30};
        SDL_SetRenderDrawColor(renderer, 220,220,220,255); SDL_RenderFillRect(renderer, &closeBtn);
        SDL_SetRenderDrawColor(renderer, 180,180,180,255); SDL_RenderDrawRect(renderer, &closeBtn);
        if (font) {
            SDL_Surface* xs = TTF_RenderText_Blended(font, "X", {80,80,80,255});
            if (xs) {
                SDL_Rect xr = {closeBtn.x+8, closeBtn.y+5, xs->w, xs->h};
                SDL_Texture* xt = SDL_CreateTextureFromSurface(renderer, xs);
                SDL_RenderCopy(renderer, xt, NULL, &xr);
                SDL_DestroyTexture(xt); SDL_FreeSurface(xs);
            }
        }
    }
}

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
    if (!ctx->showWindow || !font) return;

    // آپدیت FPS
    ctx->frameCount++;
    Uint32 now = SDL_GetTicks();
    if (now - ctx->fpsTimer >= 500) {
        ctx->fps = ctx->frameCount * 1000.0f / (now - ctx->fpsTimer);
        ctx->frameCount = 0; ctx->fpsTimer = now;
    }

    SDL_Rect& wr = ctx->windowRect;
    int W = wr.w, H = wr.h;

    // ---- سایه ----
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0,0,0,80);
    SDL_Rect shadow = {wr.x+6, wr.y+6, W, H};
    SDL_RenderFillRect(r, &shadow);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    // ---- بدنه اصلی ----
    SDL_SetRenderDrawColor(r, 22, 26, 35, 255);
    SDL_RenderFillRect(r, &wr);

    // ---- helper لمبدا ----
    auto T = [&](const char* txt, int x, int y, SDL_Color c, int maxW=0) {
        SDL_Surface* s = maxW>0
                         ? TTF_RenderText_Blended_Wrapped(font, txt, c, maxW)
                         : TTF_RenderText_Blended(font, txt, c);
        if (!s) return;
        SDL_Rect tr = {x, y, s->w, s->h};
        SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
        SDL_RenderCopy(r, t, NULL, &tr);
        SDL_DestroyTexture(t); SDL_FreeSurface(s);
    };
    auto Rect = [&](int x,int y,int w,int h, SDL_Color c, bool fill=true) {
        SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);
        SDL_Rect rc={x,y,w,h};
        if(fill) SDL_RenderFillRect(r,&rc); else SDL_RenderDrawRect(r,&rc);
    };

    // ---- نوار عنوان ----
    Rect(wr.x, wr.y, W, 34, {30,34,46,255});
    // سه دایره macOS style
    int cy = wr.y+17;
    SDL_Color cols[3] = {{255,95,86,255},{255,189,46,255},{39,201,63,255}};
    for(int i=0;i<3;i++){
        int cx = wr.x+16+i*22;
        for(int dy=-6;dy<=6;dy++) for(int dx=-6;dx<=6;dx++)
                if(dx*dx+dy*dy<=36){ SDL_SetRenderDrawColor(r,cols[i].r,cols[i].g,cols[i].b,255); SDL_RenderDrawPoint(r,cx+dx,cy+dy); }
    }
    T("  Debug Console", wr.x+75, wr.y+10, {180,190,210,255});
    // FPS badge
    char fpsBuf[32]; snprintf(fpsBuf,32,"FPS: %.0f", ctx->fps);
    T(fpsBuf, wr.x+W-110, wr.y+10, {100,220,100,255});
    // Threads badge
    char thBuf[32]; snprintf(thBuf,32,"Threads: %d", ctx->threadCount);
    T(thBuf, wr.x+W-210, wr.y+10, {100,180,255,255});

    // ---- نوار فیلتر ----
    int fy = wr.y+34;
    Rect(wr.x, fy, W, 26, {28,32,42,255});
    // دکمه‌های فیلتر
    struct FBtn { const char* label; bool* on; SDL_Color on_c; };
    FBtn fbtns[] = {
            {"INFO",  &ctx->showInfo,  {80,160,255,255}},
            {"WARN",  &ctx->showWarn,  {255,185,0,255}},
            {"ERROR", &ctx->showError, {255,80,80,255}},
    };
    int fx = wr.x+10;
    for(auto& fb : fbtns){
        SDL_Color bg = *fb.on ? fb.on_c : SDL_Color{45,50,65,255};
        Rect(fx, fy+4, 52, 18, bg);
        T(fb.label, fx+6, fy+6, *fb.on ? SDL_Color{255,255,255,255} : SDL_Color{120,120,140,255});
        fx += 58;
    }
    if(!ctx->lastOpName.empty()){
        std::string opLabel = "last: " + ctx->lastOpName;
        T(opLabel.c_str(), wr.x+W-250, fy+6, {140,140,160,255});
    }

    // ---- ناحیه لاگ ----
    int logY0 = fy+26;
    int logH = H - 26 - 34 - 36; // کسر هدر + فیلتر + input bar
    SDL_Rect logArea = {wr.x, logY0, W, logH};
    SDL_RenderSetClipRect(r, &logArea);

    int lineH = 19;
    // فیلتر کردن لاگ‌ها
    std::vector<int> visible;
    for(int i=0;i<(int)ctx->logs.size();i++){
        auto& lg = ctx->logs[i];
        if(lg.level==LOG_INFO && !ctx->showInfo) continue;
        if(lg.level==LOG_WARNING && !ctx->showWarn) continue;
        if(lg.level==LOG_ERROR && !ctx->showError) continue;
        visible.push_back(i);
    }
    int totalLines = (int)visible.size();
    int maxScroll = std::max(0, totalLines*lineH - logH + 10);
    if(ctx->scrollOffset > maxScroll) ctx->scrollOffset = maxScroll;
    if(ctx->scrollOffset < 0) ctx->scrollOffset = 0;

    // رسم ردیف‌ها از پایین به بالا (جدیدترین پایین)
    int drawY = logY0 + logH - lineH - ctx->scrollOffset;
    for(int vi = totalLines-1; vi >= 0; vi--){
        int i = visible[vi];
        auto& lg = ctx->logs[i];
        if(drawY + lineH < logY0 - 5) break;
        if(drawY > logY0 + logH) { drawY -= lineH; continue; }

        // رنگ پس‌زمینه ردیف متناوب
        SDL_Color rowBg = (vi%2==0) ? SDL_Color{25,29,40,255} : SDL_Color{28,33,44,255};
        Rect(wr.x, drawY, W, lineH, rowBg);

        // نشانگر رنگی کنار
        SDL_Color levelC = (lg.level==LOG_WARNING) ? SDL_Color{255,185,0,255}
                                                   : (lg.level==LOG_ERROR)   ? SDL_Color{255,80,80,255}
                                                                             : SDL_Color{80,160,255,255};
        Rect(wr.x, drawY, 3, lineH, levelC);

        // timestamp
        T(lg.time.c_str(), wr.x+8, drawY+2, {80,90,110,255});
        // badge
        const char* badge = lg.level==LOG_WARNING?"WARN":lg.level==LOG_ERROR?"ERR":"INF";
        Rect(wr.x+75, drawY+3, 30, 13, levelC);
        T(badge, wr.x+76, drawY+3, {255,255,255,255});
        // message
        T(lg.message.c_str(), wr.x+112, drawY+2, levelC, W-130);

        drawY -= lineH;
    }
    SDL_RenderSetClipRect(r, NULL);

    // ---- خط جداکننده ----
    Rect(wr.x, logY0+logH, W, 1, {50,55,70,255});

    // ---- نوار ورودی (command bar) ----
    int ibY = logY0+logH+1;
    Rect(wr.x, ibY, W, 35, {28,32,42,255});
    Rect(wr.x+8, ibY+7, W-16, 22, {40,45,58,255});
    Rect(wr.x+8, ibY+7, W-16, 22, {60,65,80,255}, false);
    T("> ", wr.x+12, ibY+9, {80,160,255,255});
    std::string displayInput = ctx->inputText + (ctx->inputFocused ? "|" : "");
    if(!displayInput.empty())
        T(displayInput.c_str(), wr.x+30, ibY+9, {200,210,230,255});
    else
        T("type 'clear' or 'help' ...", wr.x+30, ibY+9, {60,65,80,255});

    // ---- scrollbar ----
    if(totalLines*lineH > logH){
        int sbH = std::max(20, logH*logH/(totalLines*lineH));
        int sbY = logY0 + (ctx->scrollOffset*(logH-sbH))/std::max(1,maxScroll);
        Rect(wr.x+W-6, logY0, 6, logH, {35,40,52,255});
        Rect(wr.x+W-6, sbY+logY0, 6, sbH, {80,90,115,255});
    }

    // ---- حاشیه ----
    Rect(wr.x, wr.y, W, H, {60,65,85,255}, false);
}

void HandleDebugEvents(DebugContext* ctx, SDL_Event* e) {
    if (!ctx->showWindow) return;
    SDL_Rect& wr = ctx->windowRect;
    int W = wr.w;

    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        int mx = e->button.x, my = e->button.y;
        // کلیک روی دکمه قرمز (بستن)
        if (mx>=wr.x+8 && mx<=wr.x+22 && my>=wr.y+11 && my<=wr.y+23) {
            ctx->showWindow = false; return;
        }
        // کلیک روی هدر برای drag
        if (mx>=wr.x && mx<=wr.x+W && my>=wr.y && my<=wr.y+34) {
            ctx->dragging = true; ctx->dragDX = mx-wr.x; ctx->dragDY = my-wr.y; return;
        }
        // کلیک دکمه‌های فیلتر
        int fy = wr.y+34;
        int fx = wr.x+10;
        bool* fPtrs[3] = {&ctx->showInfo, &ctx->showWarn, &ctx->showError};
        for(int i=0;i<3;i++){
            if(mx>=fx && mx<=fx+52 && my>=fy+4 && my<=fy+22){
                *fPtrs[i] = !*fPtrs[i]; return;
            }
            fx+=58;
        }
        // کلیک روی input bar
        int ibY = wr.y + wr.h - 35;
        if(mx>=wr.x && mx<=wr.x+W && my>=ibY && my<=ibY+35){
            ctx->inputFocused = true; SDL_StartTextInput(); return;
        }
        // کلیک خارج → unfocus input
        ctx->inputFocused = false;
    }

    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
        ctx->dragging = false;
    }

    if (e->type == SDL_MOUSEMOTION && ctx->dragging) {
        int nx = e->motion.x - ctx->dragDX;
        int ny = e->motion.y - ctx->dragDY;
        if(nx<0) nx=0; if(ny<0) ny=0;
        if(nx+wr.w>1280) nx=1280-wr.w;
        if(ny+wr.h>720) ny=720-wr.h;
        wr.x = nx; wr.y = ny;
    }

    if (e->type == SDL_MOUSEWHEEL) {
        int mx,my; SDL_GetMouseState(&mx,&my);
        if(mx>=wr.x && mx<=wr.x+W && my>=wr.y && my<=wr.y+wr.h){
            ctx->scrollOffset -= e->wheel.y * 19;
            if(ctx->scrollOffset < 0) ctx->scrollOffset = 0;
        }
    }

    // ورودی متن در command bar
    if (ctx->inputFocused) {
        if(e->type == SDL_KEYDOWN){
            if(e->key.keysym.sym == SDLK_RETURN){
                // اجرای دستورات ساده
                std::string& cmd = ctx->inputText;
                if(cmd == "clear") ctx->logs.clear();
                else if(cmd == "help"){
                    ctx->logs.push_back({"Commands: clear, help, info, warn, error", LOG_INFO, "00:00:00"});
                }
                else if(cmd == "info")  ctx->showInfo  = !ctx->showInfo;
                else if(cmd == "warn")  ctx->showWarn  = !ctx->showWarn;
                else if(cmd == "error") ctx->showError = !ctx->showError;
                else if(!cmd.empty())
                    ctx->logs.push_back({"> " + cmd, LOG_INFO, "00:00:00"});
                cmd.clear();
            } else if(e->key.keysym.sym == SDLK_BACKSPACE && !ctx->inputText.empty()) {
                ctx->inputText.pop_back();
            } else if(e->key.keysym.sym == SDLK_ESCAPE) {
                ctx->inputFocused = false; SDL_StopTextInput();
            }
        } else if(e->type == SDL_TEXTINPUT) {
            ctx->inputText += e->text.text;
        }
        return; // جلوگیری از pass شدن input به بقیه
    }
}


void InitResources(AppContext* app) {
    std::vector<std::string> possiblePaths = {
            "assets/font.ttf", "../assets/font.ttf", "../../assets/font.ttf", "font.ttf", "../font.ttf"
    };

    bool found = false;
    for (const auto& path : possiblePaths) {
        app->globalFont = TTF_OpenFont(path.c_str(), 16);
        if (app->globalFont) { found = true; break; }
    }
    if (!found) std::cerr << "CRITICAL ERROR: Font not found!" << std::endl;
}

void InitFileMenu(Button* buttons, int& count, MenuState* menuState) {
    count = 3;
    SDL_Color scratchBlue = {76, 151, 255, 255};
    SDL_Color hoverBlue = {51, 115, 204, 255};

    // دکمه‌ها در هدر آبی قرار می‌گیرند (بدون فاصله از بالا)
    buttons[0] = {{120, 0, 70, 48}, "New", scratchBlue, hoverBlue, false};
    buttons[1] = {{190, 0, 70, 48}, "Save", scratchBlue, hoverBlue, false};
    buttons[2] = {{260, 0, 70, 48}, "Load", scratchBlue, hoverBlue, false};

    menuState->isSaving = false;
    menuState->isLoading = false;
    menuState->inputName = "";
}

void UpdateProjectIndex(std::string newName) {
    std::vector<std::string> projects;
    std::ifstream in("projects_index.txt");
    std::string line;
    while(std::getline(in, line)) { if(!line.empty()) projects.push_back(line); }
    in.close();

    bool exists = false;
    for(const auto& p : projects) if(p == newName) exists = true;

    if(!exists) {
        std::ofstream out("projects_index.txt", std::ios::app);
        out << newName << "\n";
        out.close();
    }
}

std::vector<std::string> GetProjectList() {
    std::vector<std::string> projects;
    std::ifstream in("projects_index.txt");
    std::string line;
    while(std::getline(in, line)) { if(!line.empty()) projects.push_back(line); }
    return projects;
}

void DrawUI(AppContext* app, Button* buttons, int count, MenuState* menuState) {
    // 1. نوار آبی رنگ هدر (دقیقاً مثل اسکرچ)
    SDL_Rect topBar = {0, 0, 1280, 48};
    SDL_SetRenderDrawColor(app->renderer, 76, 151, 255, 255);
    SDL_RenderFillRect(app->renderer, &topBar);

    // متن فیک لوگوی اسکرچ
    if(app->globalFont) {
        SDL_Surface* lSurf = TTF_RenderText_Blended(app->globalFont, "SCRATCH CLONE", {255,255,255,255});
        if(lSurf) {
            SDL_Rect lr = {15, 15, lSurf->w, lSurf->h};
            SDL_Texture* lt = SDL_CreateTextureFromSurface(app->renderer, lSurf);
            if(lt) { SDL_RenderCopy(app->renderer, lt, NULL, &lr); SDL_DestroyTexture(lt); }
            SDL_FreeSurface(lSurf);
        }
    }

    // 2. دکمه‌های هدر
    for (int i = 0; i < count; i++) {
        Button& b = buttons[i];
        if (b.isHovered) SDL_SetRenderDrawColor(app->renderer, b.hoverColor.r, b.hoverColor.g, b.hoverColor.b, 255);
        else SDL_SetRenderDrawColor(app->renderer, b.color.r, b.color.g, b.color.b, 255);

        SDL_RenderFillRect(app->renderer, &b.rect);

        if (app->globalFont) {
            SDL_Surface* surf = TTF_RenderText_Blended(app->globalFont, b.label.c_str(), {255,255,255,255});
            if (surf) {
                SDL_Rect tr = {b.rect.x + (b.rect.w - surf->w)/2, b.rect.y + (b.rect.h - surf->h)/2, surf->w, surf->h};
                SDL_Texture* tex = SDL_CreateTextureFromSurface(app->renderer, surf);
                if(tex) { SDL_RenderCopy(app->renderer, tex, NULL, &tr); SDL_DestroyTexture(tex); }
                SDL_FreeSurface(surf);
            }
        }
    }

    // 3. پنجره Popup ذخیره
    if (menuState->isSaving) {
        SDL_SetRenderDrawBlendMode(app->renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 150);
        SDL_Rect fullScreen = {0,0,1280,720};
        SDL_RenderFillRect(app->renderer, &fullScreen);
        SDL_SetRenderDrawBlendMode(app->renderer, SDL_BLENDMODE_NONE);

        SDL_Rect box = {400, 200, 480, 200};
        SDL_SetRenderDrawColor(app->renderer, 255, 255, 255, 255); SDL_RenderFillRect(app->renderer, &box);
        SDL_SetRenderDrawColor(app->renderer, 76, 151, 255, 255); SDL_RenderDrawRect(app->renderer, &box);

        if(app->globalFont) {
            SDL_Surface* tSurf = TTF_RenderText_Blended(app->globalFont, "Enter Project Name:", {0,0,0,255});
            if(tSurf) {
                SDL_Rect tr = {box.x + 20, box.y + 20, tSurf->w, tSurf->h};
                SDL_Texture* tt = SDL_CreateTextureFromSurface(app->renderer, tSurf);
                SDL_RenderCopy(app->renderer, tt, NULL, &tr);
                SDL_FreeSurface(tSurf); SDL_DestroyTexture(tt);
            }

            SDL_Rect inputRect = {box.x + 20, box.y + 70, 440, 40};
            SDL_SetRenderDrawColor(app->renderer, 240, 240, 240, 255); SDL_RenderFillRect(app->renderer, &inputRect);
            SDL_SetRenderDrawColor(app->renderer, 100, 100, 100, 255); SDL_RenderDrawRect(app->renderer, &inputRect);

            std::string disp = menuState->inputName + "|";
            SDL_Surface* iSurf = TTF_RenderText_Blended(app->globalFont, disp.c_str(), {0,0,0,255});
            if(iSurf) {
                SDL_Rect ir = {inputRect.x + 10, inputRect.y + 10, iSurf->w, iSurf->h};
                SDL_Texture* it = SDL_CreateTextureFromSurface(app->renderer, iSurf);
                SDL_RenderCopy(app->renderer, it, NULL, &ir);
                SDL_FreeSurface(iSurf); SDL_DestroyTexture(it);
            }

            SDL_Surface* hSurf = TTF_RenderText_Blended(app->globalFont, "Press ENTER to Save, ESC to Cancel", {100,100,100,255});
            if(hSurf) {
                SDL_Rect hr = {box.x + 20, box.y + 150, hSurf->w, hSurf->h};
                SDL_Texture* ht = SDL_CreateTextureFromSurface(app->renderer, hSurf);
                SDL_RenderCopy(app->renderer, ht, NULL, &hr);
                SDL_FreeSurface(hSurf); SDL_DestroyTexture(ht);
            }
        }
    }

    // 4. پنجره Popup لود
    if (menuState->isLoading) {
        SDL_SetRenderDrawBlendMode(app->renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 150);
        SDL_Rect fullScreen = {0,0,1280,720};
        SDL_RenderFillRect(app->renderer, &fullScreen);
        SDL_SetRenderDrawBlendMode(app->renderer, SDL_BLENDMODE_NONE);

        SDL_Rect box = {340, 100, 600, 500};
        SDL_SetRenderDrawColor(app->renderer, 255, 255, 255, 255); SDL_RenderFillRect(app->renderer, &box);

        if(app->globalFont) {
            SDL_Surface* tSurf = TTF_RenderText_Blended(app->globalFont, "Select a Project to Load (or click outside to cancel):", {0,0,0,255});
            if(tSurf) {
                SDL_Rect tr = {box.x + 20, box.y + 20, tSurf->w, tSurf->h};
                SDL_Texture* tt = SDL_CreateTextureFromSurface(app->renderer, tSurf);
                SDL_RenderCopy(app->renderer, tt, NULL, &tr);
                SDL_FreeSurface(tSurf); SDL_DestroyTexture(tt);
            }
        }

        int y = box.y + 60;
        int mx, my; SDL_GetMouseState(&mx, &my);
        for (const auto& name : menuState->savedProjects) {
            SDL_Rect row = {box.x + 20, y, 560, 35};
            bool hover = (mx > row.x && mx < row.x + row.w && my > row.y && my < row.y + row.h);

            SDL_SetRenderDrawColor(app->renderer, hover ? 230 : 250, hover ? 240 : 250, 255, 255);
            SDL_RenderFillRect(app->renderer, &row);
            SDL_SetRenderDrawColor(app->renderer, 200, 200, 200, 255);
            SDL_RenderDrawRect(app->renderer, &row);

            if(app->globalFont) {
                SDL_Surface* nSurf = TTF_RenderText_Blended(app->globalFont, name.c_str(), {0,0,0,255});
                if(nSurf) {
                    SDL_Rect nr = {row.x + 10, row.y + 8, nSurf->w, nSurf->h};
                    SDL_Texture* nt = SDL_CreateTextureFromSurface(app->renderer, nSurf);
                    SDL_RenderCopy(app->renderer, nt, NULL, &nr);
                    SDL_FreeSurface(nSurf); SDL_DestroyTexture(nt);
                }
            }
            y += 40;
        }
    }
}

// ---- string helpers ----
static std::string Encode(const std::string& s) {
    std::string o; o.reserve(s.size());
    for (char c : s) {
        if      (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else                 o += c;
    }
    return o;
}
static std::string Decode(const std::string& s) {
    std::string o; o.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i]=='\\' && i+1 < s.size()) {
            char nx = s[++i];
            if      (nx=='n')  o += '\n';
            else if (nx=='r')  o += '\r';
            else if (nx=='\\') o += '\\';
            else               { o += '\\'; o += nx; }
        } else o += s[i];
    }
    return o;
}

void SaveProject(std::string name, BlockSystemContext* blockCtx, SpriteContext* spriteCtx) {
    UpdateProjectIndex(name);
    std::ofstream f(name + ".scr");
    if (!f.is_open()) { std::cerr << "Save: cannot open file.\n"; return; }

    auto ID = [](const Block* p) -> int { return p ? p->id : -1; };

    f << "SCRATCH_CLONE_SAVE_V3\n";

    // ---- META ----
    f << "[META]\n"
      << "idCounter:" << blockCtx->idCounter << "\n"
      << "customPaletteY:" << blockCtx->customBlocksPaletteY << "\n";

    // ---- CUSTOM BLOCKS (My Blocks names) ----
    f << "[CUSTOMBLOCKS]\n"
      << "count:" << blockCtx->customBlocksList.size() << "\n";
    for (auto& n : blockCtx->customBlocksList)
        f << Encode(n) << "\n";

    // ---- VARIABLES ----
    f << "[VARIABLES]\n"
      << "count:" << blockCtx->variables.size() << "\n";
    for (auto& v : blockCtx->variables)
        f << Encode(v.name) << "\t" << v.value << "\t" << (int)v.visible << "\n";

    // ---- SPRITES ----
    f << "[SPRITES]\n"
      << "count:" << spriteCtx->sprites.size() << "\n"
      << "selected:" << spriteCtx->selectedSpriteIndex << "\n";
    for (auto& s : spriteCtx->sprites)
        f << Encode(s.name) << "\t" << s.x << "\t" << s.y << "\t"
          << s.direction << "\t" << s.scale << "\t" << (int)s.isVisible << "\n";

    std::vector<const Block*> ws;
    ws.reserve(blockCtx->blocks.size());
    for (auto& b : blockCtx->blocks)
        if (b.rect.x >= blockCtx->paletteArea.w)
            ws.push_back(&b);

    f << "[BLOCKS]\n"
      << "count:" << ws.size() << "\n";

    for (auto* b : ws) {
        f << "B\t"
          << b->id          << "\t"
          << (int)b->opCode << "\t"
          << (int)b->category << "\t"
          << (int)b->shape  << "\t"
          << b->rect.x      << "\t"
          << b->rect.y      << "\t"
          << b->param1      << "\t"
          << b->param2      << "\t"
          << ID(b->next)    << "\t"
          << ID(b->subStack) << "\t"
          << ID(b->subStack2) << "\t"
          << ID(b->condition) << "\t"
          << ID(b->arg1)    << "\t"
          << ID(b->arg2)    << "\t"
          << Encode(b->param1Str) << "\t"
          << Encode(b->param2Str) << "\t"
          << Encode(b->stringParam) << "\t"
          << Encode(b->text) << "\n";
    }

    f.close();
    std::cout << "[Save] \"" << name << "\" — "
              << ws.size() << " blocks, "
              << blockCtx->variables.size() << " vars, "
              << spriteCtx->sprites.size() << " sprites.\n";
}


void LoadProject(SDL_Renderer* renderer, std::string name, BlockSystemContext* blockCtx, SpriteContext* spriteCtx) {
    std::ifstream f(name + ".scr");
    if (!f.is_open()) { std::cerr << "[Load] Cannot open: " << name << ".scr\n"; return; }

    std::string header;
    std::getline(f, header);
    if (header != "SCRATCH_CLONE_SAVE_V3") {
        std::cerr << "[Load] Unknown format: " << header << "\n";
        return;
    }

    // texture های اسپرایت‌ها رو آزاد کن
    for (auto& s : spriteCtx->sprites)
        for (auto& c : s.costumes)
            if (c.texture) SDL_DestroyTexture(c.texture);
    spriteCtx->sprites.clear();
    spriteCtx->selectedSpriteIndex = -1;

    // بلاک‌ها رو پاک کن
    blockCtx->blocks.clear();
    blockCtx->variables.clear();
    blockCtx->customBlocksList.clear();
    blockCtx->editingBlock = nullptr;

    // palette رو از نو بساز
    InitBlockSystem(blockCtx, 1280, 720);

    std::string line;
    std::string section = "";

    // برای بلاک‌ها: اول همه رو می‌خونیم، بعد pointer ها رو وصل می‌کنیم
    struct BRec {
        Block b;
        int nID, ssID, ss2ID, cndID, a1ID, a2ID;
    };
    std::vector<BRec> recs;
    int blockCount = 0;

    auto split = [](const std::string& s, char d) -> std::vector<std::string> {
        std::vector<std::string> v; std::stringstream ss(s); std::string t;
        while (std::getline(ss, t, d)) v.push_back(t);
        return v;
    };

    while (std::getline(f, line)) {
        if (line.empty()) continue;

        // section headers
        if (line == "[META]")         { section = "META"; continue; }
        if (line == "[CUSTOMBLOCKS]") { section = "CUSTOMBLOCKS"; continue; }
        if (line == "[VARIABLES]")    { section = "VARIABLES"; continue; }
        if (line == "[SPRITES]")      { section = "SPRITES"; continue; }
        if (line == "[BLOCKS]")       { section = "BLOCKS"; continue; }

        if (section == "META") {
            auto p = line.find(':');
            if (p == std::string::npos) continue;
            std::string k = line.substr(0, p), v = line.substr(p+1);
            if (k == "idCounter")      blockCtx->idCounter = std::stoi(v);
            if (k == "customPaletteY") blockCtx->customBlocksPaletteY = std::stoi(v);
        }
        else if (section == "CUSTOMBLOCKS") {
            if (line.substr(0,6) == "count:") continue;
            std::string bname = Decode(line);
            if (bname.empty()) continue;
            blockCtx->customBlocksList.push_back(bname);
            // اضافه کردن Call block به palette
            Block cb = CreateBlock(blockCtx->idCounter++, 10, blockCtx->customBlocksPaletteY,
                                   bname, CAT_MYBLOCKS, SHAPE_STACK);
            cb.opCode = OP_CALL_CUSTOM; cb.stringParam = bname;
            blockCtx->blocks.push_back(cb);
            blockCtx->customBlocksPaletteY += 50;
        }
        else if (section == "VARIABLES") {
            if (line.substr(0,6) == "count:") continue;
            auto parts = split(line, '\t');
            if (parts.size() < 3) continue;
            BlockSystemContext::Variable v;
            v.name    = Decode(parts[0]);
            v.value   = std::stof(parts[1]);
            v.visible = (parts[2] == "1");
            blockCtx->variables.push_back(v);
        }
        else if (section == "SPRITES") {
            if (line.substr(0,6) == "count:") continue;
            if (line.substr(0,9) == "selected:") {
                spriteCtx->selectedSpriteIndex = std::stoi(line.substr(9));
                continue;
            }
            auto parts = split(line, '\t');
            if (parts.size() < 6) continue;

            Sprite s;
            s.name      = Decode(parts[0]);
            s.x         = std::stof(parts[1]);
            s.y         = std::stof(parts[2]);
            s.direction = std::stof(parts[3]);
            s.scale     = std::stof(parts[4]);
            s.isVisible = (parts[5] == "1");
            s.currentDialog = ""; s.isThinking = false; s.dialogEndTime = 0;
            // costume پیش‌فرض بساز
            Costume c; c.name = "Costume 1";
            c.texture = CreateCharacterTexture(renderer, c.width, c.height);
            s.costumes.push_back(c);
            spriteCtx->sprites.push_back(s);
        }
        else if (section == "BLOCKS") {
            if (line.substr(0,6) == "count:") {
                blockCount = std::stoi(line.substr(6));
                recs.reserve(blockCount);
                continue;
            }
            if (line[0] != 'B') continue;
            auto p = split(line, '\t');
            // B id opcode category shape x y p1 p2 next sub sub2 cnd a1 a2 p1s p2s sp text
            if (p.size() < 19) continue;

            BRec rec;
            rec.b.id         = std::stoi(p[1]);
            rec.b.opCode     = (BlockOpCode)std::stoi(p[2]);
            rec.b.category   = (BlockCategory)std::stoi(p[3]);
            rec.b.shape      = (BlockShape)std::stoi(p[4]);
            rec.b.rect.x     = std::stoi(p[5]);
            rec.b.rect.y     = std::stoi(p[6]);
            rec.b.param1     = std::stof(p[7]);
            rec.b.param2     = std::stof(p[8]);
            rec.nID          = std::stoi(p[9]);
            rec.ssID         = std::stoi(p[10]);
            rec.ss2ID        = std::stoi(p[11]);
            rec.cndID        = std::stoi(p[12]);
            rec.a1ID         = std::stoi(p[13]);
            rec.a2ID         = std::stoi(p[14]);
            rec.b.param1Str  = Decode(p[15]);
            rec.b.param2Str  = Decode(p[16]);
            rec.b.stringParam= Decode(p[17]);
            rec.b.text       = Decode(p[18]);
            rec.b.rect.w = 160;
            rec.b.rect.h = (rec.b.shape == SHAPE_HAT) ? 55 :
                           (rec.b.shape == SHAPE_REPORTER || rec.b.shape == SHAPE_BOOLEAN) ? 35 : 45;
            rec.b.isDragging = false;
            rec.b.inputRect1 = rec.b.inputRect2 = {0,0,0,0};
            rec.b.midYOffset = 0;
            rec.b.next = rec.b.subStack = rec.b.subStack2 =
            rec.b.condition = rec.b.arg1 = rec.b.arg2 = nullptr;
            recs.push_back(rec);
        }
    }
    f.close();

    // ---- اضافه کردن بلاک‌ها به vector (reserve تا آدرس‌ها ثابت بمونن) ----
    blockCtx->blocks.reserve(blockCtx->blocks.size() + recs.size());
    size_t ins = blockCtx->blocks.size();
    for (auto& r : recs) blockCtx->blocks.push_back(r.b);

    // ---- ساختن نقشه id → Block* ----
    std::map<int, Block*> idMap;
    for (size_t i = ins; i < blockCtx->blocks.size(); i++)
        idMap[blockCtx->blocks[i].id] = &blockCtx->blocks[i];

    // ---- وصل کردن pointer ها ----
    auto R = [&](int id) -> Block* {
        return (id == -1) ? nullptr : (idMap.count(id) ? idMap[id] : nullptr);
    };
    for (size_t i = 0; i < recs.size(); i++) {
        Block* bp = idMap[recs[i].b.id];
        if (!bp) continue;
        bp->next       = R(recs[i].nID);
        bp->subStack   = R(recs[i].ssID);
        bp->subStack2  = R(recs[i].ss2ID);
        bp->condition  = R(recs[i].cndID);
        bp->arg1       = R(recs[i].a1ID);
        bp->arg2       = R(recs[i].a2ID);
    }

    // selectedSpriteIndex رو کنترل کن
    if (spriteCtx->selectedSpriteIndex >= (int)spriteCtx->sprites.size())
        spriteCtx->selectedSpriteIndex = spriteCtx->sprites.empty() ? -1 : 0;

    std::cout << "[Load] \"" << name << "\" — "
              << recs.size() << " blocks, "
              << blockCtx->variables.size() << " vars, "
              << spriteCtx->sprites.size() << " sprites.\n";
}

void HandleFileMenuEvents(AppContext* app, Button* buttons, int count, SDL_Event* e, BlockSystemContext* blockCtx, SpriteContext* spriteCtx, MenuState* menuState) {
    int mx, my; SDL_GetMouseState(&mx, &my);

    if (menuState->isSaving) {
        if (e->type == SDL_KEYDOWN) {
            if (e->key.keysym.sym == SDLK_RETURN && !menuState->inputName.empty()) {
                SaveProject(menuState->inputName, blockCtx, spriteCtx);
                menuState->isSaving = false; SDL_StopTextInput();
            } else if (e->key.keysym.sym == SDLK_ESCAPE) { menuState->isSaving = false; SDL_StopTextInput(); }
            else if (e->key.keysym.sym == SDLK_BACKSPACE && !menuState->inputName.empty()) { menuState->inputName.pop_back(); }
        } else if (e->type == SDL_TEXTINPUT) { menuState->inputName += e->text.text; }
        return;
    }

    if (menuState->isLoading) {
        if (e->type == SDL_MOUSEBUTTONDOWN) {
            int boxX = 340, boxY = 100; int y = boxY + 60;
            for (const auto& name : menuState->savedProjects) {
                if (mx > boxX + 20 && mx < boxX + 580 && my > y && my < y + 35) {
                    LoadProject(app->renderer, name, blockCtx, spriteCtx);
                    menuState->isLoading = false; return;
                }
                y += 40;
            }
            if (mx < boxX || mx > boxX + 600 || my < boxY || my > boxY + 500) { menuState->isLoading = false; }
        }
        return;
    }

    for (int i = 0; i < count; i++) {
        buttons[i].isHovered = (mx > buttons[i].rect.x && mx < buttons[i].rect.x + buttons[i].rect.w && my > buttons[i].rect.y && my < buttons[i].rect.y + buttons[i].rect.h);
    }

    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        for (int i = 0; i < count; i++) {
            if (buttons[i].isHovered) {
                if (buttons[i].label == "New") {
                    spriteCtx->sprites.clear(); blockCtx->blocks.clear();
                    InitBlockSystem(blockCtx, 1280, 720); AddNewSprite(app->renderer, spriteCtx);
                }
                else if (buttons[i].label == "Save") {
                    menuState->isSaving = true; menuState->inputName = ""; SDL_StartTextInput();
                }
                else if (buttons[i].label == "Load") {
                    menuState->savedProjects = GetProjectList(); menuState->isLoading = true;
                }
            }
        }
    }
}

void AddNewSprite(SDL_Renderer *pRenderer, SpriteContext *pContext) {
    // (empty implementation as in original)
}


void DrawLabel(SDL_Renderer* r, TTF_Font* font, std::string text, int x, int y, SDL_Color color) {
    if (text.empty() || font == nullptr) return;
    SDL_Surface* surf = TTF_RenderText_Blended(font, text.c_str(), color);
    if (!surf) return;
    SDL_Rect rect = {x, y, surf->w, surf->h};
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    if (tex) {
        SDL_RenderCopy(r, tex, NULL, &rect);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

void RenderSpriteProperties(SDL_Renderer* renderer, TTF_Font* font, SpriteContext* ctx, SDL_Rect area) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer, &area);
    SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
    SDL_RenderDrawRect(renderer, &area);

    if (ctx->selectedSpriteIndex == -1 || ctx->sprites.empty() || ctx->selectedSpriteIndex >= (int)ctx->sprites.size()) {
        DrawLabel(renderer, font, "No Sprite Selected", area.x + 20, area.y + 20, {150,150,150,255});
        return;
    }

    Sprite* s = &ctx->sprites[ctx->selectedSpriteIndex];
    int sX = area.x + 15, sY = area.y + 15;
    SDL_Color tCol = {100, 100, 100, 255};

    auto DrawInput = [&](std::string label, std::string val, int x, int y, int w) {
        DrawLabel(renderer, font, label, x, y+5, tCol);
        SDL_Rect box = {x + 35, y, w, 30};
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &box);
        SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255); SDL_RenderDrawRect(renderer, &box);
        DrawLabel(renderer, font, val, box.x + 10, box.y + 5, {0,0,0,255});
    };

    // ردیف اول مشخصات
    DrawInput("Sprite", s->name, sX, sY, 120);
    DrawInput("X", std::to_string((int)s->x), sX + 170, sY, 60);
    DrawInput("Y", std::to_string((int)s->y), sX + 270, sY, 60);

    // ردیف دوم مشخصات
    DrawInput("Size", std::to_string((int)(s->scale * 100)), sX, sY + 40, 60);
    DrawInput("Dir", std::to_string((int)s->direction), sX + 120, sY + 40, 60);

    DrawLabel(renderer, font, "Show", sX + 230, sY + 45, tCol);
    SDL_Rect eyeOn = {sX + 280, sY + 40, 30, 30};
    SDL_Rect eyeOff = {sX + 320, sY + 40, 30, 30};

    // دکمه نمایش (O)
    SDL_SetRenderDrawColor(renderer, s->isVisible ? 76 : 240, s->isVisible ? 151 : 240, s->isVisible ? 255 : 240, 255);
    SDL_RenderFillRect(renderer, &eyeOn);
    DrawLabel(renderer, font, "O", eyeOn.x + 8, eyeOn.y + 5, s->isVisible ? SDL_Color{255,255,255,255} : SDL_Color{100,100,100,255});

    // دکمه مخفی کردن (X)
    SDL_SetRenderDrawColor(renderer, !s->isVisible ? 76 : 240, !s->isVisible ? 151 : 240, !s->isVisible ? 255 : 240, 255);
    SDL_RenderFillRect(renderer, &eyeOff);
    DrawLabel(renderer, font, "X", eyeOff.x + 10, eyeOff.y + 5, !s->isVisible ? SDL_Color{255,255,255,255} : SDL_Color{100,100,100,255});
}

void RenderSpriteList(SDL_Renderer* renderer, TTF_Font* font, SpriteContext* ctx, SDL_Rect area) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer, &area);

    int padding = 10;
    int itemSize = 70;
    int x = area.x + padding;
    int y = area.y + padding;

    for (int i = 0; i < (int)ctx->sprites.size(); i++) {
        SDL_Rect itemRect = {x, y, itemSize, itemSize};

        if (i == ctx->selectedSpriteIndex) {
            SDL_SetRenderDrawColor(renderer, 76, 151, 255, 255);
            SDL_Rect border = {x-2, y-2, itemSize+4, itemSize+4};
            SDL_RenderFillRect(renderer, &border);
        }

        SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
        SDL_RenderFillRect(renderer, &itemRect);

        if (!ctx->sprites[i].costumes.empty() && ctx->sprites[i].costumes[0].texture) {
            SDL_Texture* tex = ctx->sprites[i].costumes[0].texture;
            SDL_Rect thumb = {x+10, y+10, itemSize-20, itemSize-20};
            SDL_RenderCopy(renderer, tex, NULL, &thumb);
        }

        // دکمه حذف قرمز فقط برای اسپرایت انتخاب شده
        if (i == ctx->selectedSpriteIndex) {
            SDL_Rect delBtn = {x + itemSize - 15, y - 5, 20, 20};
            SDL_SetRenderDrawColor(renderer, 255, 70, 70, 255);
            SDL_RenderFillRect(renderer, &delBtn);
            DrawLabel(renderer, font, "x", delBtn.x + 6, delBtn.y, {255,255,255,255});
        }

        x += itemSize + padding;
    }

    // دکمه + سبز برای اضافه کردن
    SDL_Rect addBtn = {x, y, itemSize, itemSize};
    SDL_SetRenderDrawColor(renderer, 240, 250, 240, 255);
    SDL_RenderFillRect(renderer, &addBtn);
    SDL_SetRenderDrawColor(renderer, 76, 175, 80, 255);
    SDL_RenderDrawRect(renderer, &addBtn);
    DrawLabel(renderer, font, "+", addBtn.x + 25, addBtn.y + 15, {76, 175, 80, 255});
}

void RenderBackdropPanel(SDL_Renderer* renderer, TTF_Font* font, SpriteContext* ctx, SDL_Rect area) {
    SDL_SetRenderDrawColor(renderer, 250, 250, 250, 255); SDL_RenderFillRect(renderer, &area);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); SDL_RenderDrawLine(renderer, area.x, area.y, area.x, area.y + area.h);

    DrawLabel(renderer, font, "Stage", area.x + 10, area.y + 5, {100,100,100,255});
    int thumbW = 70, thumbH = 50, padding = 10, yPos = area.y + 30;

    for (int i = 0; i < (int)ctx->backdrops.size(); i++) {
        if (yPos + thumbH > area.y + area.h) break;
        SDL_Rect preview = {area.x + 10, yPos, thumbW, thumbH};

        if (i == ctx->currentBackdropIndex) {
            SDL_SetRenderDrawColor(renderer, 76, 151, 255, 255);
            SDL_Rect border = {preview.x-2, preview.y-2, preview.w+4, preview.h+4};
            SDL_RenderFillRect(renderer, &border);
        }

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &preview);
        SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255); SDL_RenderDrawRect(renderer, &preview);
        if (ctx->backdrops[i].texture) SDL_RenderCopy(renderer, ctx->backdrops[i].texture, NULL, &preview);
        yPos += thumbH + padding;
    }
}

// با پاس دادن currentView می‌فهمیم باید اسپرایت بسازیم یا بک‌گراند!
inline void HandleDropEvent(SDL_Event* e, SDL_Renderer* r, SpriteContext* ctx, int currentViewMode) {
    if (e->type == SDL_DROPFILE) {
        char* droppedFile = e->drop.file;

        // 2 برابر است با VIEW_BACKDROPS
        if (currentViewMode == 2) {
            AddBackdropFromFile(r, ctx, std::string(droppedFile));
            std::cout << "New Backdrop Uploaded: " << droppedFile << std::endl;
        } else {
            std::cout << "New Sprite Uploaded: " << droppedFile << std::endl;
        }
        SDL_free(droppedFile);
    }
}

// *** هسته مرکزی تعاملات (جادوی درگ و دراپ، چرخش و دکمه‌ها) ***
inline void HandleStageGUIEvents(SDL_Event* e, SpriteContext* ctx, SDL_Renderer* r) {
    static bool isDraggingSprite = false;
    int mx, my; SDL_GetMouseState(&mx, &my);

    int stageX = 800, stageY = 88, stageW = 480, stageH = 360;
    int centerX = stageX + (stageW / 2);
    int centerY = stageY + (stageH / 2);

    // 1. چرخش اسپرایت با غلتک موس (اسکرول روی استیج)
    if (e->type == SDL_MOUSEWHEEL) {
        if (mx > stageX && mx < stageX + stageW && my > stageY && my < stageY + stageH) {
            if (ctx->selectedSpriteIndex != -1 && !ctx->sprites.empty()) {
                ctx->sprites[ctx->selectedSpriteIndex].direction += e->wheel.y * 15; // هر اسکرول 15 درجه
            }
        }
        return;
    }

    // 2. کشیدن کاراکتر روی استیج (Drag on Stage)
    if (e->type == SDL_MOUSEMOTION) {
        if (isDraggingSprite && ctx->selectedSpriteIndex != -1 && !ctx->sprites.empty()) {
            Sprite& s = ctx->sprites[ctx->selectedSpriteIndex];
            s.x += e->motion.xrel;
            s.y -= e->motion.yrel; // در اسکرچ محور Y برعکسه
        }
        return;
    }

    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
        isDraggingSprite = false;
        return;
    }

    if (e->type != SDL_MOUSEBUTTONDOWN || e->button.button != SDL_BUTTON_LEFT) return;

    if (mx > stageX && mx < stageX + stageW && my > stageY && my < stageY + stageH) {
        for (int i = ctx->sprites.size() - 1; i >= 0; i--) {
            Sprite& s = ctx->sprites[i];
            if (!s.isVisible || s.costumes.empty()) continue;

            int dW = s.costumes[s.currentCostumeIndex].width * s.scale;
            int dH = s.costumes[s.currentCostumeIndex].height * s.scale;
            int dX = centerX + s.x - (dW / 2);
            int dY = centerY - s.y - (dH / 2);

            if (mx >= dX && mx <= dX + dW && my >= dY && my <= dY + dH) {
                ctx->selectedSpriteIndex = i; // انتخاب کاراکتر
                isDraggingSprite = true; // شروع حرکت
                return;
            }
        }
    }

    int propX = 800, propY = 458;
    if (mx >= propX && mx <= propX + 480 && my >= propY && my <= propY + 100) {
        if (ctx->selectedSpriteIndex != -1 && !ctx->sprites.empty()) {
            Sprite& s = ctx->sprites[ctx->selectedSpriteIndex];
            int sX = propX + 15, sY = propY + 15;

            // دکمه Show
            if (mx >= sX + 280 && mx <= sX + 310 && my >= sY + 40 && my <= sY + 70) s.isVisible = true;
            // دکمه Hide
            if (mx >= sX + 320 && mx <= sX + 350 && my >= sY + 40 && my <= sY + 70) s.isVisible = false;
        }
    }

    // --- کلیک در پنل لیست اسپرایت‌ها (+ / حذف / انتخاب) ---
    int listX = 800, listY = 568;
    if (mx >= listX && mx <= listX + 390 && my >= listY && my <= listY + 142) {
        int padding = 10, itemSize = 70;
        int x = listX + padding, y = listY + padding;

        for (int i = 0; i < (int)ctx->sprites.size(); i++) {
            // دکمه حذف
            if (i == ctx->selectedSpriteIndex) {
                if (mx >= x + itemSize - 15 && mx <= x + itemSize + 5 && my >= y - 5 && my <= y + 15) {
                    ctx->sprites.erase(ctx->sprites.begin() + i);
                    ctx->selectedSpriteIndex = ctx->sprites.empty() ? -1 : 0;
                    return;
                }
            }

            // کلیک برای انتخاب اسپرایت
            if (mx >= x && mx <= x + itemSize && my >= y && my <= y + itemSize) {
                ctx->selectedSpriteIndex = i;
                return;
            }
            x += itemSize + padding;
        }

        // دکمه + (اضافه کردن اسپرایت)
        if (mx >= x && mx <= x + itemSize && my >= y && my <= y + itemSize) {
            AddNewSprite(r, ctx);
        }
    }
    // کلیک روی لیست پس‌زمینه‌ها در پنل پایین راست
    int bgX = 1190, bgY = 568;
    if (mx >= bgX && mx <= bgX + 90 && my >= bgY && my <= bgY + 142) {
        int thumbW = 70, thumbH = 50, padding = 10, yPos = bgY + 30;
        for (int i = 0; i < (int)ctx->backdrops.size(); i++) {
            if (mx >= bgX + 10 && mx <= bgX + 10 + thumbW && my >= yPos && my <= yPos + thumbH) {
                ctx->currentBackdropIndex = i;
                return;
            }
            yPos += thumbH + padding;
        }
    }
}


Block* FindDefineBlock(BlockSystemContext* ctx, std::string name) {
    for (auto& b : ctx->blocks) {
        if (b.opCode == OP_DEFINE_CUSTOM && b.stringParam == name) return &b;
    } return nullptr;
}

float EvalBlock(Block* b, BlockSystemContext* ctx) {
    if (!b) return 0;
    // محاسبه بازگشتی arg1 و arg2
    float v1 = b->arg1 ? EvalBlock(b->arg1, ctx)
                       : (b->param1Str.empty() ? 0.0f : [&]{ try{ return std::stof(b->param1Str); } catch(...){ return 0.0f; } }());
    float v2 = b->arg2 ? EvalBlock(b->arg2, ctx)
                       : (b->param2Str.empty() ? 0.0f : [&]{ try{ return std::stof(b->param2Str); } catch(...){ return 0.0f; } }());

    switch (b->opCode) {
        case OP_ADD: return v1 + v2;
        case OP_SUB: return v1 - v2;
        case OP_MUL: return v1 * v2;
        case OP_DIV: return (v2 != 0) ? v1/v2 : 0;
        case OP_MOD: return (v2 != 0) ? std::fmod(v1,v2) : 0;
        case OP_GT:  return (v1 > v2)  ? 1 : 0;
        case OP_LT:  return (v1 < v2)  ? 1 : 0;
        case OP_EQ:  return (std::fabs(v1-v2)<0.0001f) ? 1 : 0;
        case OP_AND: return (v1 != 0 && v2 != 0) ? 1 : 0;
        case OP_OR:  return (v1 != 0 || v2 != 0) ? 1 : 0;
        case OP_NOT: return (v1 == 0) ? 1 : 0;
        case OP_VAR_REPORTER:
            for (auto& v : ctx->variables) if (v.name == b->param1Str) return v.value;
            return 0;
        default:
            // بلاک خودش عدد نداره - مقدار param1Str رو برگردون
            try { return std::stof(b->param1Str); } catch (...) { return 0; }
    }
}

// برای سازگاری با کدهای قدیمی
float GetBlockValue(Block* b, int paramIndex, BlockSystemContext* ctx) {
    if (!b) return 0;
    Block* argBlock = (paramIndex == 1) ? b->arg1 : b->arg2;
    std::string strVal = (paramIndex == 1) ? b->param1Str : b->param2Str;
    if (argBlock) return EvalBlock(argBlock, ctx);
    try { return std::stof(strVal); } catch (...) { return 0; }
}

void RunStepFull(ExecutionContext* exec, SpriteContext* spriteCtx, BlockSystemContext* blockCtx, DebugContext* debugCtx, PenContext* penCtx) {
    if (!exec->isRunning || exec->isPaused || exec->threads.empty()) return;

    Sprite* s = (spriteCtx->selectedSpriteIndex != -1) ? &spriteCtx->sprites[spriteCtx->selectedSpriteIndex] : nullptr;
    if (!s) { exec->isRunning = false; return; }

    // اجرای نوبتی و همزمان تمام اسکریپت‌ها (Round-Robin)
    for (int i = 0; i < (int)exec->threads.size(); i++) {
        ScriptThread& t = exec->threads[i];

        if (!t.currentBlock) {
            if (!t.returnStack.empty()) {
                t.currentBlock = t.returnStack.top();
                t.returnStack.pop();
            } else {
                exec->threads.erase(exec->threads.begin() + i); // حذف نخِ تمام شده
                i--;
                continue;
            }
        }

        Block* b = t.currentBlock;
        if (!b) continue;

        float val1 = GetBlockValue(b, 1, blockCtx);
        float val2 = GetBlockValue(b, 2, blockCtx);

        // هندل کردن توقف‌های زمان‌دار مستقل برای هر نخ
        if (b->opCode == OP_WAIT_SEC || b->opCode == OP_SAY_SEC || b->opCode == OP_THINK_SEC) {
            if (!t.isWaiting) {
                t.isWaiting = true;
                if (b->opCode == OP_WAIT_SEC) t.waitEndTime = SDL_GetTicks() + (Uint32)(val1 * 1000);
                else {
                    s->currentDialog = b->param1Str;
                    s->isThinking = (b->opCode == OP_THINK_SEC);
                    s->dialogEndTime = SDL_GetTicks() + (Uint32)(val2 * 1000);
                    t.waitEndTime = s->dialogEndTime;
                }
                continue;
            } else if (SDL_GetTicks() < t.waitEndTime) continue;

            if (b->opCode != OP_WAIT_SEC) s->currentDialog = "";
            t.isWaiting = false;
            t.currentBlock = b->next;
            continue;
        }

        // ذخیره موقعیت قبلی برای رسم خط pen
        float preX = s->x, preY = s->y;

        // آپدیت stats برای debug
        if(debugCtx) {
            debugCtx->threadCount = (int)exec->threads.size();
            debugCtx->blockExecCount++;
        }

        float condVal = b->condition ? EvalBlock(b->condition, blockCtx)
                                     : (b->arg1    ? EvalBlock(b->arg1,      blockCtx)
                                                   : val1);

        switch (b->opCode) {
            case OP_FLAG_CLICKED: break;
            case OP_MOVE_STEPS: if(debugCtx) debugCtx->lastOpName="Move Steps"; {
            float rad = (s->direction - 90) * (M_PI / 180.0f);
            s->x += std::cos(rad) * val1; s->y += std::sin(rad) * val1;
            break;
        }
            case OP_TURN_RIGHT: if(debugCtx) debugCtx->lastOpName="Turn Right"; s->direction += val1; break;
            case OP_TURN_LEFT:  if(debugCtx) debugCtx->lastOpName="Turn Left";  s->direction -= val1; break;
            case OP_GOTO_XY:   if(debugCtx) debugCtx->lastOpName="Goto XY";    s->x = val1; s->y = val2; break;
            case OP_CHANGE_SIZE: s->scale += (val1/100.0f); if(s->scale<0.1f) s->scale=0.1f; break;
            case OP_SET_SIZE:    s->scale  = (val1/100.0f); if(s->scale<0.1f) s->scale=0.1f; break;
            case OP_SHOW: s->isVisible = true;  break;
            case OP_HIDE: s->isVisible = false; break;
            case OP_SET_VAR: {
                bool found = false;
                for(auto& v : blockCtx->variables) { if(v.name==b->param1Str){ v.value=val2; found=true; break; } }
                if(!found) blockCtx->variables.push_back({b->param1Str, val2, true});
                break;
            }
            case OP_CHANGE_VAR: {
                bool found = false;
                for(auto& v : blockCtx->variables) { if(v.name==b->param1Str){ v.value+=val2; found=true; break; } }
                if(!found) blockCtx->variables.push_back({b->param1Str, val2, true});
                break;
            }
            case OP_SAY:   if(debugCtx) debugCtx->lastOpName="Say";
                s->currentDialog=b->param1Str; s->isThinking=false; s->dialogEndTime=0; break;
            case OP_THINK: if(debugCtx) debugCtx->lastOpName="Think";
                s->currentDialog=b->param1Str; s->isThinking=true;  s->dialogEndTime=0; break;

            case OP_FOREVER: if(debugCtx) debugCtx->lastOpName="Forever"; {
            if (b->subStack) { t.returnStack.push(b); t.currentBlock = b->subStack; continue; }
            else             { t.returnStack.push(b); continue; }
        }
            case OP_IF: if(debugCtx) debugCtx->lastOpName="If"; {
            // condVal از condition بلاک ارزیابی شده (نه arg1)
            if (condVal != 0 && b->subStack) { t.returnStack.push(b->next); t.currentBlock = b->subStack; continue; }
            break;
        }
            case OP_IF_ELSE: if(debugCtx) debugCtx->lastOpName="If Else"; {
            if      (condVal != 0 && b->subStack)  { t.returnStack.push(b->next); t.currentBlock = b->subStack;  continue; }
            else if (condVal == 0 && b->subStack2) { t.returnStack.push(b->next); t.currentBlock = b->subStack2; continue; }
            break;
        }
            case OP_WAIT_UNTIL: {
                if (condVal != 0) break; // شرط برقراره، ادامه بده
                // شرط برقرار نیست - همین بلاک رو دوباره اجرا کن
                t.currentBlock = b; continue;
            }
            case OP_REPEAT: {
                if (!t.repeatMap.count(b)) t.repeatMap[b] = (int)val1;
                if (t.repeatMap[b] > 0) {
                    t.repeatMap[b]--;
                    if (b->subStack) { t.returnStack.push(b); t.currentBlock = b->subStack; continue; }
                } else {
                    t.repeatMap.erase(b);
                }
                break;
            }
            case OP_STOP_ALL: { exec->threads.clear(); exec->isRunning = false; return; }
                // *** جادوی توابع: پرش به بدنه تابع و بازگشت به خط فعلی ***
            case OP_CALL_CUSTOM: {
                Block* defBlock = FindDefineBlock(blockCtx, b->stringParam);
                if (defBlock && defBlock->next) {
                    // آدرس بلاکِ بعدیِ همین تابع رو توی پشته (Stack) ذخیره می‌کنیم
                    // تا وقتی کار تابع تموم شد بدونه باید به کجا برگرده
                    t.returnStack.push(b->next);

                    // جریان اجرا رو منتقل می‌کنیم به زیرِ بلاک Define
                    t.currentBlock = defBlock->next;
                    continue; // مستقیماً برو به خط جدید، نرو بلاک بعدی!
                }
                break;
            }

            case OP_PEN_DOWN: if(debugCtx) debugCtx->lastOpName="Pen Down";  s->penDown = true;  break;
            case OP_PEN_UP: if(debugCtx) debugCtx->lastOpName="Pen Up";    s->penDown = false; break;
            case OP_PEN_CLEAR: if(debugCtx) debugCtx->lastOpName="Clear"; if(penCtx) penCtx->lines.clear(); break;
            case OP_PEN_SET_SIZE:   s->penSize = (int)val1; if(s->penSize < 1) s->penSize = 1; break;
            case OP_PEN_CHANGE_SIZE: s->penSize += (int)val1; if(s->penSize < 1) s->penSize = 1; break;
            case OP_PEN_SET_COLOR: {
                std::string hex = b->param1Str;
                if (hex.size() == 7 && hex[0] == '#') {
                    s->penColor.r = (Uint8)std::stoi(hex.substr(1,2), nullptr, 16);
                    s->penColor.g = (Uint8)std::stoi(hex.substr(3,2), nullptr, 16);
                    s->penColor.b = (Uint8)std::stoi(hex.substr(5,2), nullptr, 16);
                    s->penColor.a = 255;
                }
                break;
            }
            case OP_PEN_STAMP: {
                if (penCtx) {
                    // رسم یک دایره (چند خط) در موقعیت اسپرایت
                    int cx = (int)(240 + s->x), cy = (int)(180 - s->y);
                    int r = 15;
                    for (int dy = -r; dy <= r; dy++) {
                        int dx = (int)std::sqrt((float)(r*r - dy*dy));
                        PenLine ln; ln.x1 = cx-dx; ln.y1 = cy+dy; ln.x2 = cx+dx; ln.y2 = cy+dy;
                        ln.color = s->penColor; ln.size = 1;
                        penCtx->lines.push_back(ln);
                    }
                }
                break;
            }
            default: break;
        }

        // اگه penDown بود و اسپرایت حرکت کرد، خط ذخیره کن
        if (penCtx && s->penDown && (s->x != preX || s->y != preY)) {
            PenLine ln;
            ln.x1 = (int)(240 + preX); ln.y1 = (int)(180 - preY);
            ln.x2 = (int)(240 + s->x);  ln.y2 = (int)(180 - s->y);
            ln.color = s->penColor; ln.size = s->penSize;
            penCtx->lines.push_back(ln);
        }

        t.currentBlock = b->next;
    }
    if (exec->threads.empty()) exec->isRunning = false;
}

void StartProgram(ExecutionContext* exec, BlockSystemContext* blockCtx, DebugContext* debugCtx) {
    exec->threads.clear();
    int found = 0;
    for (auto& b : blockCtx->blocks) {
        if (b.opCode == OP_FLAG_CLICKED && b.rect.x >= blockCtx->paletteArea.w) {
            ScriptThread t; t.currentBlock = &b;
            exec->threads.push_back(t); found++;
        }
    }
    exec->isRunning = !exec->threads.empty();
    exec->isPaused = false;
    if(debugCtx){
        char buf[64]; snprintf(buf,64,"Program started — %d script(s) running", found);
        Log(debugCtx, buf, found>0 ? LOG_INFO : LOG_WARNING);
    }
}

void StopProgram(ExecutionContext* exec, DebugContext* debugCtx) {
    exec->isRunning = false;
    exec->threads.clear();
    if(debugCtx) Log(debugCtx, "Program stopped.", LOG_INFO);
}


// تابع کمکی برای قرینه کردن (Flip) تکسچر و جایگزینی آن
void FlipCostumeTexture(SDL_Renderer* r, Costume& c, SDL_RendererFlip flipType) {
    // 1. ساخت یک تکسچر جدید و خالی با قابلیت نقاشی
    SDL_Texture* newTex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, c.width, c.height);
    SDL_SetTextureBlendMode(newTex, SDL_BLENDMODE_BLEND);

    // 2. تنظیم تکسچر جدید به عنوان هدف رندر
    SDL_SetRenderTarget(r, newTex);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 0); // پس زمینه کاملا شفاف
    SDL_RenderClear(r);

    // 3. کپی کردن عکس قدیمی روی عکس جدید با اعمال Flip
    SDL_SetTextureBlendMode(c.texture, SDL_BLENDMODE_NONE);
    SDL_RenderCopyEx(r, c.texture, NULL, NULL, 0, NULL, flipType);

    // 4. بازگرداندن هدف رندر به صفحه اصلی و جایگزینی تکسچرها
    SDL_SetRenderTarget(r, NULL);
    SDL_DestroyTexture(c.texture); // پاک کردن عکس قبلی از حافظه
    c.texture = newTex; // قرار دادن عکس برگردانده شده به عنوان لباس فعلی
}

void RenderCostumeEditor(SDL_Renderer* r, SpriteContext* ctx, SDL_Rect area, PaintTool* tool, TTF_Font* font) {
    SDL_SetRenderDrawColor(r, 249, 249, 249, 255); // رنگ پس‌زمینه اسکرچ
    SDL_RenderFillRect(r, &area);
    SDL_SetRenderDrawColor(r, 220, 220, 220, 255);
    SDL_RenderDrawLine(r, area.x + area.w, area.y, area.x + area.w, area.y + area.h);

    if (ctx->selectedSpriteIndex == -1 || ctx->sprites.empty()) return;
    Sprite& s = ctx->sprites[ctx->selectedSpriteIndex];
    if (s.costumes.empty()) return;

    Costume& currentCostume = s.costumes[s.currentCostumeIndex];

    auto DrawBtn = [&](std::string text, SDL_Rect rect, bool isActive, bool isDanger = false) {
        int mx, my; SDL_GetMouseState(&mx, &my);
        bool hover = (mx > rect.x && mx < rect.x + rect.w && my > rect.y && my < rect.y + rect.h);

        SDL_Color bg = {240, 240, 240, 255};
        if (hover) bg = {220, 230, 255, 255};
        if (isActive) bg = {200, 220, 255, 255}; // آبی ملایم برای دکمه فعال
        if (isDanger && hover) bg = {255, 200, 200, 255}; // قرمز ملایم برای دکمه‌های خطرناک (Clear)

        SDL_SetRenderDrawColor(r, bg.r, bg.g, bg.b, 255);
        SDL_RenderFillRect(r, &rect);
        SDL_SetRenderDrawColor(r, isActive ? 76 : 180, isActive ? 151 : 180, isActive ? 255 : 180, 255);
        SDL_RenderDrawRect(r, &rect);

        if (font) {
            SDL_Surface* surf = TTF_RenderText_Blended(font, text.c_str(), {50, 50, 50, 255});
            if (surf) {
                SDL_Rect tr = {rect.x + (rect.w - surf->w)/2, rect.y + (rect.h - surf->h)/2, surf->w, surf->h};
                SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
                if (tex) { SDL_RenderCopy(r, tex, NULL, &tr); SDL_DestroyTexture(tex); }
                SDL_FreeSurface(surf);
            }
        }
    };

    int row1Y = area.y + 20;

    // پالت رنگ
    SDL_Color colors[] = {{0,0,0}, {255,0,0}, {0,255,0}, {0,0,255}, {255,255,255}, {255,165,0}};
    for(int i=0; i<6; i++) {
        SDL_Rect cRect = {area.x + 50 + (i*35), row1Y, 30, 30};
        SDL_SetRenderDrawColor(r, colors[i].r, colors[i].g, colors[i].b, 255);
        SDL_RenderFillRect(r, &cRect);

        // هایلایت رنگ انتخاب شده
        if(tool->color.r == colors[i].r && tool->color.g == colors[i].g && !tool->isEraser) {
            SDL_SetRenderDrawColor(r, 76, 151, 255, 255);
            SDL_Rect border = {cRect.x-2, cRect.y-2, cRect.w+4, cRect.h+4};
            SDL_RenderDrawRect(r, &border);
        } else {
            SDL_SetRenderDrawColor(r, 150, 150, 150, 255);
            SDL_RenderDrawRect(r, &cRect);
        }
    }

    // دکمه پاک‌کن (Eraser) و پاک‌کردن کل بوم (Clear)
    DrawBtn("Eraser", {area.x + 280, row1Y, 70, 30}, tool->isEraser);
    DrawBtn("Clear All", {area.x + 360, row1Y, 80, 30}, false, true);

    int row2Y = area.y + 60;

    // سایز قلم‌ها (S=5, M=10, L=20)
    DrawBtn("S", {area.x + 50, row2Y, 35, 30}, tool->size == 5);
    DrawBtn("M", {area.x + 95, row2Y, 35, 30}, tool->size == 10);
    DrawBtn("L", {area.x + 140, row2Y, 35, 30}, tool->size == 20);

    // دکمه‌های Flip (قرینه سازی)
    DrawBtn("Flip H", {area.x + 200, row2Y, 70, 30}, false);
    DrawBtn("Flip V", {area.x + 280, row2Y, 70, 30}, false);


    SDL_Rect canvas = GetCanvasRect(area);

    // رسم الگوی شطرنجی پس‌زمینه (نشان‌دهنده شفافیت)
    for (int x = 0; x < canvas.w; x += 15) {
        for (int y = 0; y < canvas.h; y += 15) {
            if ((x / 15 + y / 15) % 2 == 0) SDL_SetRenderDrawColor(r, 220, 220, 220, 255);
            else SDL_SetRenderDrawColor(r, 255, 255, 255, 255);

            // جلوگیری از بیرون زدن شطرنجی از کادر
            int cellW = (x + 15 > canvas.w) ? canvas.w - x : 15;
            int cellH = (y + 15 > canvas.h) ? canvas.h - y : 15;
            SDL_Rect cell = {canvas.x + x, canvas.y + y, cellW, cellH};
            SDL_RenderFillRect(r, &cell);
        }
    }

    // رسم تکسچر لباس روی بوم
    SDL_RenderCopy(r, currentCostume.texture, NULL, &canvas);

    // حاشیه زیبای دور بوم
    SDL_SetRenderDrawColor(r, 150, 150, 150, 255);
    SDL_RenderDrawRect(r, &canvas);


    SDL_Rect uploadBtn = GetUploadBtnRect(area);
    int mx, my; SDL_GetMouseState(&mx, &my);
    bool hoverUp = (mx > uploadBtn.x && mx < uploadBtn.x + uploadBtn.w && my > uploadBtn.y && my < uploadBtn.y + uploadBtn.h);

    SDL_SetRenderDrawColor(r, hoverUp ? 51 : 76, hoverUp ? 115 : 151, hoverUp ? 204 : 255, 255);
    SDL_RenderFillRect(r, &uploadBtn);

    if (font) {
        SDL_Surface* btnSurf = TTF_RenderText_Blended(font, "Upload Costume Image", {255,255,255,255});
        if (btnSurf) {
            SDL_Rect tr = {uploadBtn.x + (uploadBtn.w - btnSurf->w)/2, uploadBtn.y + 12, btnSurf->w, btnSurf->h};
            SDL_Texture* btnTex = SDL_CreateTextureFromSurface(r, btnSurf);
            SDL_RenderCopy(r, btnTex, NULL, &tr);
            SDL_FreeSurface(btnSurf); SDL_DestroyTexture(btnTex);
        }
    }
}

void HandleCostumeEvents(SDL_Event* e, SpriteContext* ctx, SDL_Rect area, PaintTool* tool, SDL_Renderer* r) {
    if (ctx->selectedSpriteIndex == -1 || ctx->sprites.empty()) return;
    Sprite& s = ctx->sprites[ctx->selectedSpriteIndex];
    if (s.costumes.empty()) return;

    Costume& currentCostume = s.costumes[s.currentCostumeIndex];
    int mx, my; SDL_GetMouseState(&mx, &my);

    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {

        // 1. کلیک روی دکمه آپلود
        SDL_Rect uploadBtn = GetUploadBtnRect(area);
        if (mx > uploadBtn.x && mx < uploadBtn.x + uploadBtn.w && my > uploadBtn.y && my < uploadBtn.y + uploadBtn.h) {
            std::string filePath = OpenFileDialog(); // استفاده از API ویندوز که در SpriteSystem.h است
            if (!filePath.empty()) {
                AddCostumeToSprite(&s, r, filePath);
                std::cout << "Costume uploaded successfully!" << std::endl;
            }
            return;
        }

        // 2. کلیک روی ابزارهای ردیف اول (رنگ‌ها، پاک‌کن، Clear)
        int row1Y = area.y + 20;
        if (my > row1Y && my < row1Y + 30) {
            // رنگ‌ها
            if (mx >= area.x + 50 && mx <= area.x + 250) {
                int idx = (mx - area.x - 50) / 35;
                SDL_Color colors[] = {{0,0,0}, {255,0,0}, {0,255,0}, {0,0,255}, {255,255,255}, {255,165,0}};
                if(idx >= 0 && idx < 6) {
                    tool->color = colors[idx];
                    tool->isEraser = false;
                }
            }
                // پاک‌کن
            else if (mx >= area.x + 280 && mx <= area.x + 350) {
                tool->isEraser = true;
            }
                // Clear All (پاک کردن کل بوم)
            else if (mx >= area.x + 360 && mx <= area.x + 440) {
                SDL_SetRenderTarget(r, currentCostume.texture);
                SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
                SDL_SetRenderDrawColor(r, 0, 0, 0, 0); // پر کردن با رنگ کاملاً شفاف
                SDL_RenderClear(r);
                SDL_SetRenderTarget(r, NULL);
                std::cout << "Canvas Cleared!" << std::endl;
            }
            return;
        }

        // 3. کلیک روی ابزارهای ردیف دوم (سایز قلم، Flip)
        int row2Y = area.y + 60;
        if (my > row2Y && my < row2Y + 30) {
            // سایزها
            if (mx >= area.x + 50 && mx <= area.x + 85) tool->size = 5;       // S
            else if (mx >= area.x + 95 && mx <= area.x + 130) tool->size = 10;  // M
            else if (mx >= area.x + 140 && mx <= area.x + 175) tool->size = 20; // L

                // Flip Horizontal (برگرداندن افقی)
            else if (mx >= area.x + 200 && mx <= area.x + 270) {
                FlipCostumeTexture(r, currentCostume, SDL_FLIP_HORIZONTAL);
                std::cout << "Flipped Horizontal!" << std::endl;
            }
                // Flip Vertical (برگرداندن عمودی)
            else if (mx >= area.x + 280 && mx <= area.x + 350) {
                FlipCostumeTexture(r, currentCostume, SDL_FLIP_VERTICAL);
                std::cout << "Flipped Vertical!" << std::endl;
            }
            return;
        }
    }

    // 4. نقاشی روی بوم (با قابلیت رسم و پاک کردن واقعی)
    SDL_Rect canvas = GetCanvasRect(area);
    if ((e->type == SDL_MOUSEMOTION && (e->motion.state & SDL_BUTTON_LMASK)) || e->type == SDL_MOUSEBUTTONDOWN) {
        if (mx > canvas.x && mx < canvas.x + canvas.w && my > canvas.y && my < canvas.y + canvas.h) {

            float scaleX = (float)currentCostume.width / (float)canvas.w;
            float scaleY = (float)currentCostume.height / (float)canvas.h;
            int texX = (mx - canvas.x) * scaleX;
            int texY = (my - canvas.y) * scaleY;

            SDL_SetRenderTarget(r, currentCostume.texture);

            if(tool->isEraser) {
                // راز یک پاک‌کن واقعی در SDL2: خاموش کردن BlendMode و کشیدن رنگ کاملا شفاف (Alpha 0)
                SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
                SDL_SetRenderDrawColor(r, 0, 0, 0, 0);
            } else {
                SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(r, tool->color.r, tool->color.g, tool->color.b, 255);
            }

            // رسم براش (مربع شکل برای سرعت، میتونیم دایره هم بذاریم ولی مربع تو اسکرچ هم استفاده میشه)
            int scaledSize = tool->size * scaleX; // تنظیم سایز براش نسبت به رزولوشن عکس
            if (scaledSize < 1) scaledSize = 1;

            SDL_Rect brush = {texX - (scaledSize/2), texY - (scaledSize/2), scaledSize, scaledSize};
            SDL_RenderFillRect(r, &brush);

            SDL_SetRenderTarget(r, NULL);
        }
    }
}

// تابع کمکی برای قرینه کردن (Flip) تکسچر
void FlipTexture(SDL_Renderer* r, SDL_Texture*& tex, int w, int h, SDL_RendererFlip flipType) {
    SDL_Texture* newTex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
    SDL_SetTextureBlendMode(newTex, SDL_BLENDMODE_BLEND);

    SDL_SetRenderTarget(r, newTex);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 0);
    SDL_RenderClear(r);

    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);
    SDL_RenderCopyEx(r, tex, NULL, NULL, 0, NULL, flipType);

    SDL_SetRenderTarget(r, NULL);
    SDL_DestroyTexture(tex);
    tex = newTex;
}

void RenderImageEditor(SDL_Renderer* r, SpriteContext* ctx, SDL_Rect area, PaintTool* tool, TTF_Font* font, bool isBackdrop) {
    SDL_SetRenderDrawColor(r, 249, 249, 249, 255); SDL_RenderFillRect(r, &area);
    SDL_SetRenderDrawColor(r, 220, 220, 220, 255); SDL_RenderDrawLine(r, area.x + area.w, area.y, area.x + area.w, area.y + area.h);

    SDL_Texture* targetTex = nullptr;
    std::string itemName = "";

    if (!isBackdrop) {
        if (ctx->selectedSpriteIndex == -1 || ctx->sprites.empty()) return;
        Sprite& s = ctx->sprites[ctx->selectedSpriteIndex];
        if (s.costumes.empty()) return;
        targetTex = s.costumes[s.currentCostumeIndex].texture; itemName = s.costumes[s.currentCostumeIndex].name;
    } else {
        if (ctx->backdrops.empty()) return;
        targetTex = ctx->backdrops[ctx->currentBackdropIndex].texture; itemName = ctx->backdrops[ctx->currentBackdropIndex].name;
    }

    auto DrawBtn = [&](std::string text, SDL_Rect rect, bool isActive, bool isDanger = false) {
        int mx, my; SDL_GetMouseState(&mx, &my);
        bool hover = (mx > rect.x && mx < rect.x + rect.w && my > rect.y && my < rect.y + rect.h);
        SDL_Color bg = {240, 240, 240, 255};
        if (hover) bg = {220, 230, 255, 255};
        if (isActive) bg = {200, 220, 255, 255};
        if (isDanger && hover) bg = {255, 200, 200, 255};
        SDL_SetRenderDrawColor(r, bg.r, bg.g, bg.b, 255); SDL_RenderFillRect(r, &rect);
        SDL_SetRenderDrawColor(r, isActive ? 76 : 180, isActive ? 151 : 180, isActive ? 255 : 180, 255); SDL_RenderDrawRect(r, &rect);
        if (font) {
            SDL_Surface* surf = TTF_RenderText_Blended(font, text.c_str(), {50, 50, 50, 255});
            if (surf) {
                SDL_Rect tr = {rect.x + (rect.w - surf->w)/2, rect.y + (rect.h - surf->h)/2, surf->w, surf->h};
                SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
                if (tex) { SDL_RenderCopy(r, tex, NULL, &tr); SDL_DestroyTexture(tex); }
                SDL_FreeSurface(surf);
            }
        }
    };

    int row1Y = area.y + 20;
    SDL_Color colors[] = {{0,0,0}, {255,0,0}, {0,255,0}, {0,0,255}, {255,255,255}, {255,165,0}};
    for(int i=0; i<6; i++) {
        SDL_Rect cRect = {area.x + 50 + (i*35), row1Y, 30, 30};
        SDL_SetRenderDrawColor(r, colors[i].r, colors[i].g, colors[i].b, 255); SDL_RenderFillRect(r, &cRect);
        if(tool->color.r == colors[i].r && tool->color.g == colors[i].g && !tool->isEraser) {
            SDL_SetRenderDrawColor(r, 76, 151, 255, 255); SDL_Rect border = {cRect.x-2, cRect.y-2, cRect.w+4, cRect.h+4}; SDL_RenderDrawRect(r, &border);
        } else { SDL_SetRenderDrawColor(r, 150, 150, 150, 255); SDL_RenderDrawRect(r, &cRect); }
    }
    DrawBtn("Eraser", {area.x + 280, row1Y, 70, 30}, tool->isEraser);
    DrawBtn("Clear All", {area.x + 360, row1Y, 80, 30}, false, true);

    int row2Y = area.y + 60;
    DrawBtn("S", {area.x + 50, row2Y, 35, 30}, tool->size == 5);
    DrawBtn("M", {area.x + 95, row2Y, 35, 30}, tool->size == 10);
    DrawBtn("L", {area.x + 140, row2Y, 35, 30}, tool->size == 20);
    DrawBtn("Flip H", {area.x + 200, row2Y, 70, 30}, false);
    DrawBtn("Flip V", {area.x + 280, row2Y, 70, 30}, false);

    if(font) {
        SDL_Surface* ts = TTF_RenderText_Blended(font, ("Editing: " + itemName).c_str(), {100,100,100,255});
        if(ts) {
            SDL_Rect tr = {area.x + 370, row2Y + 5, ts->w, ts->h};
            SDL_Texture* tt = SDL_CreateTextureFromSurface(r, ts);
            SDL_RenderCopy(r, tt, NULL, &tr); SDL_DestroyTexture(tt); SDL_FreeSurface(ts);
        }
    }

    SDL_Rect canvas = GetCanvasRect(area);
    for (int x = 0; x < canvas.w; x += 15) {
        for (int y = 0; y < canvas.h; y += 15) {
            if ((x / 15 + y / 15) % 2 == 0) SDL_SetRenderDrawColor(r, 220, 220, 220, 255); else SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
            int cellW = (x + 15 > canvas.w) ? canvas.w - x : 15; int cellH = (y + 15 > canvas.h) ? canvas.h - y : 15;
            SDL_Rect cell = {canvas.x + x, canvas.y + y, cellW, cellH}; SDL_RenderFillRect(r, &cell);
        }
    }
    SDL_RenderCopy(r, targetTex, NULL, &canvas);
    SDL_SetRenderDrawColor(r, 150, 150, 150, 255); SDL_RenderDrawRect(r, &canvas);

    SDL_Rect uploadBtn = GetUploadBtnRect(area);
    int mx, my; SDL_GetMouseState(&mx, &my);
    bool hoverUp = (mx > uploadBtn.x && mx < uploadBtn.x + uploadBtn.w && my > uploadBtn.y && my < uploadBtn.y + uploadBtn.h);
    SDL_SetRenderDrawColor(r, hoverUp ? 51 : 76, hoverUp ? 115 : 151, hoverUp ? 204 : 255, 255); SDL_RenderFillRect(r, &uploadBtn);
    if (font) {
        std::string btnText = isBackdrop ? "Upload Backdrop" : "Upload Costume";
        SDL_Surface* btnSurf = TTF_RenderText_Blended(font, btnText.c_str(), {255,255,255,255});
        if (btnSurf) {
            SDL_Rect tr = {uploadBtn.x + (uploadBtn.w - btnSurf->w)/2, uploadBtn.y + 12, btnSurf->w, btnSurf->h};
            SDL_Texture* btnTex = SDL_CreateTextureFromSurface(r, btnSurf);
            SDL_RenderCopy(r, btnTex, NULL, &tr); SDL_FreeSurface(btnSurf); SDL_DestroyTexture(btnTex);
        }
    }
}

void HandleImageEditorEvents(SDL_Event* e, SpriteContext* ctx, SDL_Rect area, PaintTool* tool, SDL_Renderer* r, bool isBackdrop) {
    SDL_Texture** targetTexPtr = nullptr; int tWidth = 0, tHeight = 0;
    if (!isBackdrop) {
        if (ctx->selectedSpriteIndex == -1 || ctx->sprites.empty()) return;
        if (ctx->sprites[ctx->selectedSpriteIndex].costumes.empty()) return;
        targetTexPtr = &ctx->sprites[ctx->selectedSpriteIndex].costumes[ctx->sprites[ctx->selectedSpriteIndex].currentCostumeIndex].texture;
        tWidth = ctx->sprites[ctx->selectedSpriteIndex].costumes[ctx->sprites[ctx->selectedSpriteIndex].currentCostumeIndex].width;
        tHeight = ctx->sprites[ctx->selectedSpriteIndex].costumes[ctx->sprites[ctx->selectedSpriteIndex].currentCostumeIndex].height;
    } else {
        if (ctx->backdrops.empty()) return;
        targetTexPtr = &ctx->backdrops[ctx->currentBackdropIndex].texture;
        tWidth = ctx->backdrops[ctx->currentBackdropIndex].width;
        tHeight = ctx->backdrops[ctx->currentBackdropIndex].height;
    }

    int mx, my; SDL_GetMouseState(&mx, &my);

    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        SDL_Rect uploadBtn = GetUploadBtnRect(area);
        if (mx > uploadBtn.x && mx < uploadBtn.x + uploadBtn.w && my > uploadBtn.y && my < uploadBtn.y + uploadBtn.h) {
            std::string filePath = OpenFileDialog();
            if (!filePath.empty()) {
                if (isBackdrop) AddBackdropFromFile(r, ctx, filePath);
                else AddCostumeToSprite(&ctx->sprites[ctx->selectedSpriteIndex], r, filePath);
            }
            return;
        }

        int row1Y = area.y + 20;
        if (my > row1Y && my < row1Y + 30) {
            if (mx >= area.x + 50 && mx <= area.x + 250) {
                int idx = (mx - area.x - 50) / 35; SDL_Color colors[] = {{0,0,0}, {255,0,0}, {0,255,0}, {0,0,255}, {255,255,255}, {255,165,0}};
                if(idx >= 0 && idx < 6) { tool->color = colors[idx]; tool->isEraser = false; }
            }
            else if (mx >= area.x + 280 && mx <= area.x + 350) tool->isEraser = true;
            else if (mx >= area.x + 360 && mx <= area.x + 440) {
                SDL_SetRenderTarget(r, *targetTexPtr); SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
                SDL_SetRenderDrawColor(r, 0, 0, 0, 0); SDL_RenderClear(r); SDL_SetRenderTarget(r, NULL);
            } return;
        }

        int row2Y = area.y + 60;
        if (my > row2Y && my < row2Y + 30) {
            if (mx >= area.x + 50 && mx <= area.x + 85) tool->size = 5;
            else if (mx >= area.x + 95 && mx <= area.x + 130) tool->size = 10;
            else if (mx >= area.x + 140 && mx <= area.x + 175) tool->size = 20;
            else if (mx >= area.x + 200 && mx <= area.x + 270) FlipTexture(r, *targetTexPtr, tWidth, tHeight, SDL_FLIP_HORIZONTAL);
            else if (mx >= area.x + 280 && mx <= area.x + 350) FlipTexture(r, *targetTexPtr, tWidth, tHeight, SDL_FLIP_VERTICAL);
            return;
        }
    }

    SDL_Rect canvas = GetCanvasRect(area);
    if ((e->type == SDL_MOUSEMOTION && (e->motion.state & SDL_BUTTON_LMASK)) || e->type == SDL_MOUSEBUTTONDOWN) {
        if (mx > canvas.x && mx < canvas.x + canvas.w && my > canvas.y && my < canvas.y + canvas.h) {
            float scaleX = (float)tWidth / (float)canvas.w; float scaleY = (float)tHeight / (float)canvas.h;
            int texX = (mx - canvas.x) * scaleX; int texY = (my - canvas.y) * scaleY;

            SDL_SetRenderTarget(r, *targetTexPtr);
            if(tool->isEraser) {
                SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE); SDL_SetRenderDrawColor(r, 0, 0, 0, 0);
            } else {
                SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(r, tool->color.r, tool->color.g, tool->color.b, 255);
            }
            int scaledSize = tool->size * scaleX; if (scaledSize < 1) scaledSize = 1;
            SDL_Rect brush = {texX - (scaledSize/2), texY - (scaledSize/2), scaledSize, scaledSize};
            SDL_RenderFillRect(r, &brush); SDL_SetRenderTarget(r, NULL);
        }
    }
}


enum AppView { VIEW_CODE, VIEW_COSTUMES , VIEW_BACKDROPS};
AppView currentView = VIEW_CODE;
PaintTool paintTool;

const int WIN_W = 1280;
const int WIN_H = 720;
const int STAGE_W = 480;
const int STAGE_H = 360;

const int TOP_BAR_H = 48;
const int TABS_H = 40;
const int LEFT_PANEL_W = 280;
const int RIGHT_PANEL_X = 1280 - STAGE_W - 10;

SDL_Rect STAGE_RECT = {RIGHT_PANEL_X, TOP_BAR_H + 40, STAGE_W, STAGE_H};

void RenderControls(SDL_Renderer* r, ExecutionContext* exec, DebugContext* debug, TTF_Font* font) {
    SDL_Rect bar = {RIGHT_PANEL_X, TOP_BAR_H, STAGE_W, 40};
    SDL_SetRenderDrawColor(r, 245, 245, 245, 255);
    SDL_RenderFillRect(r, &bar);

    SDL_Rect goBtn = {bar.x + 10, bar.y + 5, 30, 30};
    SDL_SetRenderDrawColor(r, 76, 175, 80, 255); SDL_RenderFillRect(r, &goBtn);

    SDL_Rect stopBtn = {bar.x + 50, bar.y + 5, 30, 30};
    SDL_SetRenderDrawColor(r, 244, 67, 54, 255); SDL_RenderFillRect(r, &stopBtn);

    SDL_Rect debugBtn = {bar.x + STAGE_W - 90, bar.y + 5, 80, 30};
    if (debug->showWindow) SDL_SetRenderDrawColor(r, 153, 102, 255, 255);
    else SDL_SetRenderDrawColor(r, 220, 220, 220, 255);
    SDL_RenderFillRect(r, &debugBtn);

    if (font) {
        SDL_Color txtCol = debug->showWindow ? SDL_Color{255, 255, 255, 255} : SDL_Color{100, 100, 100, 255};
        DrawLabel(r, font, "Debug", debugBtn.x + 15, debugBtn.y + 5, txtCol);
    }
}

int main(int argc, char* argv[]) {
    AppContext app; DebugContext debug; BlockSystemContext blockSys;
    SpriteContext spriteSys; ExecutionContext execSys; MenuState menuState;
    PenContext penCtx;
    Button menuButtons[3]; int menuButtonCount = 0;

    // *** تغییر مهم: روشن کردن تمام موتورها از جمله موتور عکس (PNG) ***
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    IMG_Init(IMG_INIT_PNG); // روشن کردن پشتیبانی از PNG

    app.window = SDL_CreateWindow("Scratch 3.0 Clone - Pro Engine", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H, SDL_WINDOW_SHOWN);
    app.renderer = SDL_CreateRenderer(app.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
    app.isRunning = true;

    InitResources(&app);
    InitFileMenu(menuButtons, menuButtonCount, &menuState);
    InitDebug(&debug);
    InitBlockSystem(&blockSys, WIN_W, WIN_H);
    blockSys.paletteArea.w = LEFT_PANEL_W;
    blockSys.paletteArea.y = TOP_BAR_H + TABS_H;
    blockSys.paletteArea.h = WIN_H - (TOP_BAR_H + TABS_H);
    InitSpriteSystem(app.renderer, &spriteSys);

    SDL_Rect propRect = {RIGHT_PANEL_X, STAGE_RECT.y + STAGE_H + 10, STAGE_W, 100};
    SDL_Rect listRect = {RIGHT_PANEL_X, propRect.y + 110, STAGE_W - 90, WIN_H - (propRect.y + 120)};
    SDL_Rect bgRect = {RIGHT_PANEL_X + STAGE_W - 80, listRect.y, 80, listRect.h};

    SDL_Event event;

    while (app.isRunning) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) app.isRunning = false;

            if (menuState.isSaving || menuState.isLoading) {
                HandleFileMenuEvents(&app, menuButtons, menuButtonCount, &event, &blockSys, &spriteSys, &menuState);
                continue;
            }

            if (event.type == SDL_MOUSEWHEEL) {
                int mx, my; SDL_GetMouseState(&mx, &my);
                if (mx < LEFT_PANEL_W && my > TOP_BAR_H + TABS_H && currentView == VIEW_CODE) {
                    blockSys.scrollY += event.wheel.y * 35;
                    if (blockSys.scrollY > 0) blockSys.scrollY = 0;
                }
            }

            HandleDropEvent(&event, app.renderer, &spriteSys, currentView);
            HandleStageGUIEvents(&event, &spriteSys, app.renderer);
            HandleDebugEvents(&debug, &event);
            HandleFileMenuEvents(&app, menuButtons, menuButtonCount, &event, &blockSys, &spriteSys, &menuState);

            if (event.type == SDL_MOUSEBUTTONDOWN) {
                int mx = event.button.x, my = event.button.y;

                if (mx > RIGHT_PANEL_X && mx < RIGHT_PANEL_X + STAGE_W && my > TOP_BAR_H && my < TOP_BAR_H + 40) {
                    if (mx < RIGHT_PANEL_X + 40) StartProgram(&execSys, &blockSys, &debug);
                    else if (mx < RIGHT_PANEL_X + 80) StopProgram(&execSys, &debug);
                    else if (mx > RIGHT_PANEL_X + STAGE_W - 90 && mx < RIGHT_PANEL_X + STAGE_W - 10) {
                        debug.showWindow = !debug.showWindow;
                    }
                }

                if (mx < 350 && my > TOP_BAR_H && my < TOP_BAR_H + TABS_H) {
                    if (mx < 100) currentView = VIEW_CODE;
                    else if (mx >= 100 && mx < 220) currentView = VIEW_COSTUMES;
                    else if (mx >= 220 && mx < 340) currentView = VIEW_BACKDROPS;
                }

            }

            int mx, my; SDL_GetMouseState(&mx, &my);
            if (mx < RIGHT_PANEL_X && my > TOP_BAR_H + TABS_H) {
                if (currentView == VIEW_CODE) HandleBlockEvents(&blockSys, &event);
                else {
                    SDL_Rect editorArea = {0, TOP_BAR_H + TABS_H, RIGHT_PANEL_X, WIN_H - (TOP_BAR_H + TABS_H)};
                    // این خط حیاتیه! به ادیتور میگه الان توی تب بک‌گراند هستیم (true) یا کاستوم (false)
                    HandleImageEditorEvents(&event, &spriteSys, editorArea, &paintTool, app.renderer, currentView == VIEW_BACKDROPS);
                }
            }

        }

        RunStepFull(&execSys, &spriteSys, &blockSys, &debug, &penCtx);

        SDL_SetRenderDrawColor(app.renderer, 245, 245, 245, 255);
        SDL_RenderClear(app.renderer);

        if (currentView == VIEW_CODE) {
            RenderBlockSystem(app.renderer, &blockSys, app.globalFont);
        } else {
            SDL_Rect editorArea = {0, TOP_BAR_H + TABS_H, RIGHT_PANEL_X, WIN_H - (TOP_BAR_H + TABS_H)};
            RenderImageEditor(app.renderer, &spriteSys, editorArea, &paintTool, app.globalFont, currentView == VIEW_BACKDROPS);
        }

        // *** جایگزین کدی که فرستادی ***

        // یک تابع جادویی کوچیک برای رسم بی‌نقص تب‌ها
        auto DrawTab = [&](int x, int w, std::string text, bool isActive) {
            SDL_Rect tab = {x, TOP_BAR_H, w, TABS_H};
            SDL_SetRenderDrawColor(app.renderer, isActive ? 255 : 230, isActive ? 255 : 230, isActive ? 255 : 230, 255);
            SDL_RenderFillRect(app.renderer, &tab);

            // خط آبی بالای تب فعال
            if (isActive) {
                SDL_SetRenderDrawColor(app.renderer, 76, 151, 255, 255);
                SDL_Rect border = {x, TOP_BAR_H, w, 4};
                SDL_RenderFillRect(app.renderer, &border);
            }

            // تنظیم وسط چین بودن متن تب
            int textX = x + (w - text.length() * 8) / 2;
            DrawLabel(app.renderer, app.globalFont, text, textX, TOP_BAR_H + 10, {100,100,100,255});
        };

        // رسم ۳ تب حرفه‌ای ما
        DrawTab(0, 100, "Code", currentView == VIEW_CODE);
        DrawTab(100, 120, "Costumes", currentView == VIEW_COSTUMES);
        DrawTab(220, 120, "Backdrops", currentView == VIEW_BACKDROPS);

        // بقیه کدهای رندر استیج و پنل‌ها (سر جای خودشون موندن)
        RenderControls(app.renderer, &execSys, &debug, app.globalFont);

        SDL_SetRenderDrawColor(app.renderer, 255, 255, 255, 255);
        SDL_RenderFillRect(app.renderer, &STAGE_RECT);

        int varY = STAGE_RECT.y + 10;
        for (const auto& v : blockSys.variables) {
            if (v.visible && app.globalFont) {
                std::string txt = v.name + ": " + std::to_string((int)v.value); // تبدیل به عدد صحیح برای زیبایی
                SDL_Surface* vSurf = TTF_RenderText_Blended(app.globalFont, txt.c_str(), {255,255,255,255});
                if (vSurf) {
                    SDL_Rect vRect = {STAGE_RECT.x + 10, varY, vSurf->w + 16, vSurf->h + 6};

                    // پس زمینه خاکستری تیره مانیتور
                    SDL_SetRenderDrawColor(app.renderer, 200, 200, 200, 220);
                    SDL_RenderFillRect(app.renderer, &vRect);

                    // باکس نارنجی دور مقدار (استایل اسکرچ)
                    SDL_Rect valBox = {vRect.x + vRect.w - 30, vRect.y + 2, 26, vRect.h - 4};
                    SDL_SetRenderDrawColor(app.renderer, 255, 140, 26, 255); // نارنجی متغیر
                    SDL_RenderFillRect(app.renderer, &valBox);

                    SDL_Rect tr = {vRect.x + 8, vRect.y + 3, vSurf->w, vSurf->h};
                    SDL_Texture* vTex = SDL_CreateTextureFromSurface(app.renderer, vSurf);
                    SDL_RenderCopy(app.renderer, vTex, NULL, &tr);

                    SDL_DestroyTexture(vTex); SDL_FreeSurface(vSurf);
                    varY += vRect.h + 5;
                }
            }
        }

        SDL_RenderSetViewport(app.renderer, &STAGE_RECT);
        RenderStageContent(app.renderer, &spriteSys, &penCtx, app.globalFont);
        SDL_RenderSetViewport(app.renderer, NULL);

        SDL_SetRenderDrawColor(app.renderer, 210, 210, 210, 255);
        SDL_RenderDrawRect(app.renderer, &STAGE_RECT);

        RenderSpriteProperties(app.renderer, app.globalFont, &spriteSys, propRect);
        RenderSpriteList(app.renderer, app.globalFont, &spriteSys, listRect);
        RenderBackdropPanel(app.renderer, app.globalFont, &spriteSys, bgRect);

        RenderDebugWindow(&debug, app.renderer, app.globalFont);

        DrawUI(&app, menuButtons, menuButtonCount, &menuState);

        SDL_RenderPresent(app.renderer);

    }

    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    return 0;
}
