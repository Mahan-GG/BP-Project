#ifndef EXECUTIONSYSTEM_H
#define EXECUTIONSYSTEM_H

#include "BlockSystem.h"
#include "SpriteSystem.h"
#include "DebugSystem.h"
#include <stack>
#include <string>
#include <cstdlib>


// ساختار هر "نخ" اجرایی مستقل (برای اجرای همزمان)
struct ScriptThread {
    Block* currentBlock = nullptr;
    bool isWaiting = false;
    Uint32 waitEndTime = 0;
    std::stack<Block*> returnStack;
};

// کانتکست اصلی حالا لیستی از نخ‌ها رو مدیریت می‌کنه
struct ExecutionContext {
    std::vector<ScriptThread> threads;
    bool isRunning = false;
    bool isPaused = false;
};


Block* FindDefineBlock(BlockSystemContext* ctx, std::string name) {
    for (auto& b : ctx->blocks) {
        if (b.opCode == OP_DEFINE_CUSTOM && b.stringParam == name) return &b;
    } return nullptr;
}

// *** جادوی ریاضیات: ارزیاب بازگشتی مقادیر ***
// این تابع چک می‌کنه که آیا توی ورودی، یک بلاک دیگه (مثل جمع و ضرب) افتاده یا نه؟
// اگه بلاک بود، اونو حساب می‌کنه، اگه نبود، عددی که تایپ کردی رو می‌خونه!
// به تابع، کانتکست بلاک‌ها رو هم پاس می‌دیم
float GetBlockValue(Block* b, int paramIndex, BlockSystemContext* ctx) {
    if (!b) return 0;
    Block* argBlock = (paramIndex == 1) ? b->arg1 : b->arg2;
    std::string strVal = (paramIndex == 1) ? b->param1Str : b->param2Str;

    if (argBlock) {
        // خواندن تو در تو
        float v1 = GetBlockValue(argBlock, 1, ctx);
        float v2 = GetBlockValue(argBlock, 2, ctx);
        switch (argBlock->opCode) {
            case OP_ADD: return v1 + v2;
            case OP_SUB: return v1 - v2;
            case OP_MUL: return v1 * v2;
            case OP_DIV: return (v2 != 0) ? (v1 / v2) : 0;
            case OP_GT:  return (v1 > v2) ? 1 : 0;
            case OP_LT:  return (v1 < v2) ? 1 : 0;
            case OP_EQ:  return (v1 == v2) ? 1 : 0;

                // *** آپدیت: خواندن مقدار واقعی متغیر ***
            case OP_VAR_REPORTER: {
                for (auto& v : ctx->variables) if (v.name == argBlock->param1Str) return v.value;
                return 0; // اگه پیدا نشد
            }
            default: return 0;
        }
    } else {
        try { return std::stof(strVal); } catch (...) { return 0; }
    }
}


void RunStepFull(ExecutionContext* exec, SpriteContext* spriteCtx, BlockSystemContext* blockCtx, DebugContext* debugCtx) {
    if (!exec->isRunning || exec->isPaused || exec->threads.empty()) return;

    Sprite* s = (spriteCtx->selectedSpriteIndex != -1) ? &spriteCtx->sprites[spriteCtx->selectedSpriteIndex] : nullptr;
    if (!s) { exec->isRunning = false; return; }

    // اجرای نوبتی و همزمان تمام اسکریپت‌ها (Round-Robin)
    for (int i = 0; i < exec->threads.size(); i++) {
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

        switch (b->opCode) {
            case OP_FLAG_CLICKED: break;
            case OP_MOVE_STEPS: {
                float rad = (s->direction - 90) * (M_PI / 180.0f);
                s->x += std::cos(rad) * val1; s->y += std::sin(rad) * val1;
                break;
            }
            case OP_TURN_RIGHT: s->direction += val1; break;
            case OP_TURN_LEFT:  s->direction -= val1; break;
            case OP_GOTO_XY:    s->x = val1; s->y = val2; break;
            case OP_CHANGE_SIZE: s->scale += (val1 / 100.0f); if(s->scale < 0.1f) s->scale = 0.1f; break;
            case OP_SET_SIZE:    s->scale = (val1 / 100.0f); if(s->scale < 0.1f) s->scale = 0.1f; break;
            case OP_SHOW: s->isVisible = true; break;
            case OP_HIDE: s->isVisible = false; break;
            case OP_SET_VAR: {
                bool found = false;
                for(auto& v : blockCtx->variables) { if(v.name == b->param1Str) { v.value = val2; found = true; break; } }
                if(!found) blockCtx->variables.push_back({b->param1Str, val2, true});
                break;
            }
            case OP_CHANGE_VAR: {
                bool found = false;
                for(auto& v : blockCtx->variables) { if(v.name == b->param1Str) { v.value += val2; found = true; break; } }
                if(!found) blockCtx->variables.push_back({b->param1Str, val2, true});
                break;
            }
            case OP_SAY: s->currentDialog = b->param1Str; s->isThinking = false; s->dialogEndTime = 0; break;
            case OP_THINK: s->currentDialog = b->param1Str; s->isThinking = true; s->dialogEndTime = 0; break;

            case OP_FOREVER: {
                if (b->subStack) { t.returnStack.push(b); t.currentBlock = b->subStack; continue; }
                else { t.returnStack.push(b); continue; }
            }
            case OP_IF: {
                if (val1 != 0 && b->subStack) { t.returnStack.push(b->next); t.currentBlock = b->subStack; continue; }
                break;
            }
            case OP_IF_ELSE: {
                if (val1 != 0 && b->subStack) { t.returnStack.push(b->next); t.currentBlock = b->subStack; continue; }
                else if (val1 == 0 && b->subStack2) { t.returnStack.push(b->next); t.currentBlock = b->subStack2; continue; }
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

            default: break;
        }
        t.currentBlock = b->next;
    }
    if (exec->threads.empty()) exec->isRunning = false;
}

void StartProgram(ExecutionContext* exec, BlockSystemContext* blockCtx, DebugContext* debugCtx) {
    exec->threads.clear();
    for (auto& b : blockCtx->blocks) {
        // هر پرچم سبزی که تو صفحه هست رو پیدا می‌کنه و تو یک نخ جداگانه استارت میزنه!
        if (b.opCode == OP_FLAG_CLICKED && b.rect.x >= blockCtx->paletteArea.w) {
            ScriptThread t; t.currentBlock = &b;
            exec->threads.push_back(t);
        }
    }
    exec->isRunning = !exec->threads.empty();
    exec->isPaused = false;
}

void StopProgram(ExecutionContext* exec, DebugContext* debugCtx) {
    exec->isRunning = false;
    exec->threads.clear();
}

#endif
