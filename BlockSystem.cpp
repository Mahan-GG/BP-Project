#include "BlockSystem.h"
#include <iostream>

// *** هوش مصنوعی چیدمان: محاسبه قد بلاک‌ها به صورت دینامیک ***
int LayoutBlockChain(Block* b, int x, int y) {
    if (!b) return 0;
    int currentY = y;
    Block* curr = b;

    while (curr) {
        curr->rect.x = x;
        curr->rect.y = currentY;

        // *** راز تمیزی کار: کشسانی شدن عرض بلاک‌ها (Width) ***
        int w = 160;
        if (curr->shape == SHAPE_REPORTER || curr->shape == SHAPE_BOOLEAN) w = 60; // عملگرها کوچیکترن

        // اگر بلاکی داخل ورودی‌ها افتاده بود، به عرض بلاک فعلی اضافه کن
        if (curr->arg1) w += curr->arg1->rect.w + 10;
        if (curr->arg2) w += curr->arg2->rect.w + 10;
        curr->rect.w = w;

        // جایگذاری دقیق فرزندان تو در تو بدون اینکه رو هم بیفتن
        int currentXOffset = x + 40;
        if (curr->arg1) {
            LayoutBlockChain(curr->arg1, currentXOffset, currentY + 2);
            currentXOffset += curr->arg1->rect.w + 20; // هول دادن دومی به جلو
        } else {
            currentXOffset += 30; // فاصله باکس خالی
        }
        if (curr->arg2) {
            LayoutBlockChain(curr->arg2, currentXOffset, currentY + 2);
        }
        if (curr->condition) {
            LayoutBlockChain(curr->condition, x + 70, currentY + 5);
        }

        // کشسانی شدن ارتفاع (Height)
        int h = 45;
        if (curr->shape == SHAPE_C_SHAPE) {
            int subH = curr->subStack ? LayoutBlockChain(curr->subStack, x + 20, currentY + 40) : 35;
            h = 40 + subH + 30;
        }
        else if (curr->shape == SHAPE_E_SHAPE) {
            int subH1 = curr->subStack ? LayoutBlockChain(curr->subStack, x + 20, currentY + 40) : 35;
            curr->midYOffset = 40 + subH1;
            int subH2 = curr->subStack2 ? LayoutBlockChain(curr->subStack2, x + 20, currentY + curr->midYOffset + 30) : 35;
            h = curr->midYOffset + 30 + subH2 + 30;
        }
        else if (curr->shape == SHAPE_HAT) h = 55;
        else if (curr->shape == SHAPE_REPORTER || curr->shape == SHAPE_BOOLEAN) h = 35;

        curr->rect.h = h;
        currentY += h;
        curr = curr->next;
    }
    return currentY - y;
}


Block CreateBlock(int id, int x, int y, std::string text, BlockOpCode op, BlockCategory cat, BlockShape shape) {
    Block b; b.id = id; b.text = text; b.opCode = op; b.category = cat; b.shape = shape;
    b.rect = {x, y, 160, 45};
    if (shape == SHAPE_REPORTER || shape == SHAPE_BOOLEAN) b.rect.h = 35;
    return b;
}

void InitBlockSystem(BlockSystemContext* ctx, int w, int h) {
    ctx->paletteArea = {0, 88, 280, h - 88};
    ctx->scrollY = 0;
    int y = ctx->paletteArea.y + 90; // جا باز کردن برای دو تا دکمه در بالای پالت

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
    ctx->blocks.push_back(CreateBlock(ctx->idCounter++, 10, y, "< > and < >", CAT_OPERATORS, SHAPE_BOOLEAN)); y+=40;
    ctx->blocks.push_back(CreateBlock(ctx->idCounter++, 10, y, "< > or < >", CAT_OPERATORS, SHAPE_BOOLEAN)); y+=40;
    ctx->blocks.push_back(CreateBlock(ctx->idCounter++, 10, y, "not < >", CAT_OPERATORS, SHAPE_BOOLEAN)); y+=45;

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
    for (auto& target : ctx->blocks) {
        if (&target == draggedBlock || target.rect.x < ctx->paletteArea.w) continue;

        // 1. چسبیدن به شیار ورودی اول (مثل انداختن یک عملگر جمع توی ورودی Move)
        if (draggedBlock->shape == SHAPE_REPORTER || draggedBlock->shape == SHAPE_BOOLEAN) {
            if (target.inputRect1.w > 0 && target.arg1 == nullptr) {
                int distX = std::abs(draggedBlock->rect.x - target.inputRect1.x);
                int distY = std::abs(draggedBlock->rect.y - target.inputRect1.y);
                if (distX < 40 && distY < 30) { target.arg1 = draggedBlock; return; }
            }
            // چسبیدن به شیار ورودی دوم
            if (target.inputRect2.w > 0 && target.arg2 == nullptr) {
                int distX = std::abs(draggedBlock->rect.x - target.inputRect2.x);
                int distY = std::abs(draggedBlock->rect.y - target.inputRect2.y);
                if (distX < 40 && distY < 30) { target.arg2 = draggedBlock; return; }
            }
            // چسبیدن به شرط If
            if (target.shape == SHAPE_C_SHAPE && target.condition == nullptr) {
                int distX = std::abs(draggedBlock->rect.x - (target.rect.x + 60));
                int distY = std::abs(draggedBlock->rect.y - (target.rect.y + 5));
                if (distX < 40 && distY < 30) { target.condition = draggedBlock; target.arg1 = draggedBlock; return; }
            }
        }

        // 2. چسبیدن به داخل شکم C-Shape
        if (target.shape == SHAPE_C_SHAPE && target.subStack == nullptr) {
            if (draggedBlock->shape == SHAPE_STACK || draggedBlock->shape == SHAPE_C_SHAPE) {
                int distY = std::abs(draggedBlock->rect.y - (target.rect.y + 40));
                int distX = std::abs(draggedBlock->rect.x - (target.rect.x + 20));
                if (distX < 30 && distY < 30) { target.subStack = draggedBlock; return; }
            }
        }
        // 2.5. چسبیدن به شکم دوم (Else) در E-Shape
        if (target.shape == SHAPE_E_SHAPE && target.subStack2 == nullptr) {
            if (draggedBlock->shape == SHAPE_STACK || draggedBlock->shape == SHAPE_C_SHAPE || draggedBlock->shape == SHAPE_E_SHAPE) {
                int distY = std::abs(draggedBlock->rect.y - (target.rect.y + target.midYOffset + 30));
                int distX = std::abs(draggedBlock->rect.x - (target.rect.x + 20));
                if (distX < 30 && distY < 30) { target.subStack2 = draggedBlock; return; }
            }
        }


        // 3. چسبیدن عادی به زیر بلاک‌ها
        if (target.next == nullptr && target.shape != SHAPE_CAP && target.opCode != OP_FOREVER && draggedBlock->shape != SHAPE_REPORTER && draggedBlock->shape != SHAPE_BOOLEAN) {
            int distY = std::abs(draggedBlock->rect.y - (target.rect.y + target.rect.h));
            int distX = std::abs(draggedBlock->rect.x - target.rect.x);
            if (distX < 30 && distY < 30) { target.next = draggedBlock; return; }
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
    }


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
    SDL_Color c = GetCategoryColor(b.category);

    // 1. سایه زیر بلاک (پشتیبانی از C-Shape و E-Shape)
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 30);
    if (b.shape == SHAPE_C_SHAPE) {
        SDL_Rect shadowT = {x+2, y+2, b.rect.w, 40}; SDL_RenderFillRect(renderer, &shadowT);
        SDL_Rect shadowL = {x+2, y+42, 20, b.rect.h-72}; SDL_RenderFillRect(renderer, &shadowL);
        SDL_Rect shadowB = {x+2, y+b.rect.h-30+2, b.rect.w, 30}; SDL_RenderFillRect(renderer, &shadowB);
    } else if (b.shape == SHAPE_E_SHAPE) {
        SDL_Rect shadowT = {x+2, y+2, b.rect.w, 40}; SDL_RenderFillRect(renderer, &shadowT);
        SDL_Rect shadowL1 = {x+2, y+42, 20, b.midYOffset-42}; SDL_RenderFillRect(renderer, &shadowL1);
        SDL_Rect shadowM = {x+2, y+b.midYOffset+2, b.rect.w-30, 30}; SDL_RenderFillRect(renderer, &shadowM);
        SDL_Rect shadowL2 = {x+2, y+b.midYOffset+32, 20, b.rect.h-(b.midYOffset+30)-30+2}; SDL_RenderFillRect(renderer, &shadowL2);
        SDL_Rect shadowB = {x+2, y+b.rect.h-30+2, b.rect.w, 30}; SDL_RenderFillRect(renderer, &shadowB);
    } else {
        SDL_Rect shadow = {x+2, y+2, b.rect.w, b.rect.h}; SDL_RenderFillRect(renderer, &shadow);
    }

    // 2. بدنه رنگی
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);

    if (b.shape == SHAPE_C_SHAPE || b.shape == SHAPE_E_SHAPE) {
        // نوار بالا
        SDL_Rect topBar = {x, y, b.rect.w, 40}; SDL_RenderFillRect(renderer, &topBar);

        if (b.shape == SHAPE_E_SHAPE) {
            // ساختار سه تکه برای If-Else
            SDL_Rect spine1 = {x, y + 40, 20, b.midYOffset - 40}; SDL_RenderFillRect(renderer, &spine1);
            SDL_Rect midBar = {x, y + b.midYOffset, b.rect.w - 30, 30}; SDL_RenderFillRect(renderer, &midBar);
            SDL_Rect spine2 = {x, y + b.midYOffset + 30, 20, b.rect.h - (b.midYOffset + 30) - 30}; SDL_RenderFillRect(renderer, &spine2);

            // چاپ کلمه Else روی نوار وسط
            if (font) {
                SDL_Surface* elseSurf = TTF_RenderText_Blended(font, "Else", {255,255,255,255});
                if (elseSurf) {
                    SDL_Rect elseRect = {x + 25, y + b.midYOffset + 5, elseSurf->w, elseSurf->h};
                    SDL_Texture* elseTex = SDL_CreateTextureFromSurface(renderer, elseSurf);
                    SDL_RenderCopy(renderer, elseTex, NULL, &elseRect);
                    SDL_DestroyTexture(elseTex); SDL_FreeSurface(elseSurf);
                }
            }
        } else {
            // ستون فقرات (چپ) برای C-Shape
            SDL_Rect spine = {x, y + 40, 20, b.rect.h - 70}; SDL_RenderFillRect(renderer, &spine);
        }

        // نوار پایین
        SDL_Rect botBar = {x, y + b.rect.h - 30, b.rect.w, 30}; SDL_RenderFillRect(renderer, &botBar);

        // جایگاه تیره برای شرط
        if (b.opCode == OP_IF || b.opCode == OP_IF_ELSE || b.opCode == OP_REPEAT || b.opCode == OP_WAIT_UNTIL) {
            SDL_SetRenderDrawColor(renderer, c.r-40>0?c.r-40:0, c.g-40>0?c.g-40:0, c.b-40>0?c.b-40:0, 255);
            SDL_Rect condSlot = {x + 70, y + 7, 40, 25}; SDL_RenderFillRect(renderer, &condSlot);
        }
    }
    else {
        SDL_Rect body = {x, y, b.rect.w, b.rect.h}; SDL_RenderFillRect(renderer, &body);

        // شکل‌های خاص برای عملگرها
        if (b.shape == SHAPE_REPORTER) {
            // شبیه‌سازی بیضی
            SDL_Rect lEdge = {x-5, y+5, 10, b.rect.h-10}; SDL_RenderFillRect(renderer, &lEdge);
            SDL_Rect rEdge = {x+b.rect.w-5, y+5, 10, b.rect.h-10}; SDL_RenderFillRect(renderer, &rEdge);
        } else if (b.shape == SHAPE_BOOLEAN) {
            // شبیه‌سازی شش ضلعی
            SDL_RenderDrawLine(renderer, x, y+b.rect.h/2, x+10, y);
            SDL_RenderDrawLine(renderer, x, y+b.rect.h/2, x+10, y+b.rect.h);
        }
    }

    // کلاهک شروع (Hat)
    if (b.shape == SHAPE_HAT) {
        SDL_Rect hat = {x, y - 10, 80, 10}; SDL_RenderFillRect(renderer, &hat);
    }

    // پین اتصال پایین (برآمدگی پازلی)
    // *** آپدیت: این پین برای بلاک Stop All (CAP) و Forever رسم نمیشه ***
    if ((b.shape == SHAPE_STACK || b.shape == SHAPE_HAT || b.shape == SHAPE_C_SHAPE || b.shape == SHAPE_E_SHAPE)
        && b.shape != SHAPE_CAP && b.opCode != OP_FOREVER) {
        SDL_Rect bump = {x + 15, y + b.rect.h, 30, 8}; SDL_RenderFillRect(renderer, &bump);
    }

    // 3. --- سیستم رندر پیشرفته متون و باکس‌های ورودی (Scratch Style) ---
    if (font) {
        int currentX = x + 10;
        int currentY = y + (b.shape == SHAPE_HAT ? 15 : 10);
        std::string txt = b.text;

        auto DrawTextPart = [&](std::string part) {
            if (part.empty()) return;
            SDL_Surface* s = TTF_RenderText_Blended(font, part.c_str(), {255,255,255,255});
            if (s) {
                SDL_Rect tr = {currentX, currentY, s->w, s->h};
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                SDL_RenderCopy(renderer, t, NULL, &tr); SDL_DestroyTexture(t);
                currentX += s->w + 5;
                SDL_FreeSurface(s);
            }
        };

        // به تابع DrawInputBox آرگومان argBlock رو اضافه کردیم
        auto DrawInputBox = [&](int paramNum, std::string& valStr, SDL_Rect& outRect, Block* argBlock) {
            // *** جادوی زیبایی: اگر بلاکی در این ورودی افتاده، باکس سفید رو محو کن! ***
            if (argBlock != nullptr) {
                outRect = {0, 0, 0, 0};
                currentX += argBlock->rect.w + 5; // کِش دادن متن تا بعد از بلاک
                return;
            }

            SDL_Surface* s = TTF_RenderText_Blended(font, valStr.empty() ? " " : valStr.c_str(), {0,0,0,255});
            int boxW = s ? (s->w < 20 ? 20 : s->w + 10) : 20;
            outRect = {currentX, currentY - 2, boxW, 22};

            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &outRect);
            SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderDrawRect(renderer, &outRect);

            if (s) {
                SDL_Rect tr = {currentX + 5, currentY, s->w, s->h};
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                SDL_RenderCopy(renderer, t, NULL, &tr); SDL_DestroyTexture(t); SDL_FreeSurface(s);
            }
            currentX += boxW + 5;
        };


        size_t p1 = txt.find("%1");
        size_t p2 = txt.find("%2");

        if (p1 != std::string::npos) {
            DrawTextPart(txt.substr(0, p1));
            DrawInputBox(1, b.param1Str, b.inputRect1, b.arg1);
            if (p2 != std::string::npos) {
                DrawTextPart(txt.substr(p1 + 2, p2 - (p1 + 2)));
                DrawInputBox(2, b.param2Str, b.inputRect2,b.arg2);
                DrawTextPart(txt.substr(p2 + 2));
            } else {
                DrawTextPart(txt.substr(p1 + 2));
                b.inputRect2 = {0,0,0,0};
            }
        } else {
            DrawTextPart(txt);
            b.inputRect1 = {0,0,0,0}; b.inputRect2 = {0,0,0,0};
        }
    }
}


void RenderBlockSystem(SDL_Renderer* renderer, BlockSystemContext* ctx, TTF_Font* font) {
    // 1. رسم پس‌زمینه پالت
    SDL_SetRenderDrawColor(renderer, 249, 249, 249, 255); SDL_RenderFillRect(renderer, &ctx->paletteArea);
    SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
    SDL_RenderDrawLine(renderer, ctx->paletteArea.w, ctx->paletteArea.y, ctx->paletteArea.w, 720);

    // 2. رسم دکمه‌های "Make a Variable" و "Make a Block" در بالای پالت
    SDL_Rect makeVarBtn = {10, ctx->paletteArea.y + 10 - ctx->scrollY, 140, 30};
    SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255); SDL_RenderFillRect(renderer, &makeVarBtn);
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255); SDL_RenderDrawRect(renderer, &makeVarBtn);

    SDL_Rect makeBlockBtn = {10, ctx->paletteArea.y + 45 - ctx->scrollY, 140, 30};
    SDL_SetRenderDrawColor(renderer, 255, 102, 128, 255); SDL_RenderFillRect(renderer, &makeBlockBtn); // صورتی
    SDL_SetRenderDrawColor(renderer, 200, 80, 100, 255); SDL_RenderDrawRect(renderer, &makeBlockBtn);

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
    for (auto& b : ctx->blocks) {
        if (b.rect.x < ctx->paletteArea.w) DrawScratchBlock(renderer, b, b.rect.x, b.rect.y + ctx->scrollY, font);
    }
    SDL_RenderSetClipRect(renderer, NULL);

    // 5. رسم بلاک‌های محیط کار (اول C-Shape و E-Shape تا بلاک‌های داخلی روی اونا قرار بگیرن)
    for (auto& b : ctx->blocks) {
        if (b.rect.x >= ctx->paletteArea.w && (b.shape == SHAPE_C_SHAPE || b.shape == SHAPE_E_SHAPE))
            DrawScratchBlock(renderer, b, b.rect.x, b.rect.y, font);
    }
    for (auto& b : ctx->blocks) {
        if (b.rect.x >= ctx->paletteArea.w && b.shape != SHAPE_C_SHAPE && b.shape != SHAPE_E_SHAPE)
            DrawScratchBlock(renderer, b, b.rect.x, b.rect.y, font);
    }

    // ==============================================================
    // 6. رسم پاپ‌آپ‌ها (متغیر و توابع دلخواه)
    // ==============================================================

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
}

