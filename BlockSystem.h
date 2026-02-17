#ifndef BLOCKSYSTEM_H
#define BLOCKSYSTEM_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>
#include <cmath>

enum BlockOpCode {
    OP_FLAG_CLICKED,OP_MOVE_STEPS, OP_TURN_RIGHT, OP_TURN_LEFT,OP_GOTO_XY, OP_GOTO_MOUSE, OP_GLIDE_SEC,OP_CHANGE_X, OP_SET_X, OP_CHANGE_Y, OP_SET_Y,OP_SAY_SEC, OP_SAY, OP_NEXT_COSTUME, OP_SET_SIZE, OP_CHANGE_SIZE,OP_WAIT_SEC, OP_REPEAT, OP_FOREVER,
    OP_DEFINE_CUSTOM,
    OP_CALL_CUSTOM,
    OP_UNKNOWN
};

enum BlockCategory {
    CAT_MOTION, CAT_LOOKS, CAT_EVENTS, CAT_CONTROL,CAT_MYBLOCKS};
enum BlockShape {SHAPE_HAT,SHAPE_STACK, SHAPE_C_SHAPE
};
struct Block {
    int id;
    BlockOpCode opCode;
    BlockCategory category;
    BlockShape shape;std::string text;
    SDL_Rect rect;float param1 = 0;
    float param2 = 0;std::string stringParam;
    Block* next = nullptr;
    Block* subStack = nullptr;
    bool isDragging = false;
};

struct BlockSystemContext {
    std::vector<Block> blocks;
    SDL_Rect paletteArea;
    SDL_Rect stageArea;
    int idCounter = 1;
};

BlockOpCode GetOpCodeFromText(std::string text) {
    if (text.find("Green Flag") != std::string::npos) return OP_FLAG_CLICKED;
    // توابع حرکت
    if (text.find("Move") != std::string::npos) return
    OP_MOVE_STEPS;
    if (text.find("Turn Right") != std::string::npos) return OP_TURN_RIGHT;
    if (text.find("Turn Left") != std::string::npos) return OP_TURN_LEFT;
    if (text.find("Go to Mouse") != std::string::npos) return OP_GOTO_MOUSE;
    if (text.find("Change X") != std::string::npos) return OP_CHANGE_X;
    if (text.find("Set X") != std::string::npos) return OP_SET_X;
    //Looks تواابع
    if (text.find("Say Hello") != std::string::npos) return OP_SAY_SEC;
    if (text.find("Next Costume") != std::string::npos) return OP_NEXT_COSTUME;
    if (text.find("Change Size") != std::string::npos)
        return OP_CHANGE_SIZE;
    // Control توابع
    if (text.find("Wait") != std::string::npos)
        return OP_WAIT_SEC;
    if (text.find("Repeat") != std::string::npos)
        return OP_REPEAT;
    if (text.find("Forever") != std::string::npos)
        return OP_FOREVER;
    // تابع کاستوم
    if (text.find("Define") != std::string::npos)
        return OP_DEFINE_CUSTOM;

    return OP_UNKNOWN;
}

SDL_Color GetCategoryColor(BlockCategory cat) {
    // رنگ های هر بخش از بلاک ها
    switch(cat) {
        case CAT_MOTION: return {76, 151, 255, 255};
        case CAT_LOOKS: return {153, 102, 255, 255};
        case CAT_EVENTS: return {255, 191, 0, 255};
        case CAT_CONTROL: return {255, 171, 25, 255};
        case CAT_MYBLOCKS: return {255, 102, 128, 255};
        default: return {200, 200, 200, 255};
    }
}

Block CreateBlock(int id, int x, int y, std::string text, BlockCategory cat, BlockShape shape) {Block b;
    b.id = id;
    b.text = text;
    b.category = cat;
    b.shape = shape;
    b.opCode = GetOpCodeFromText(text);
    b.rect = {x, y, 160, 40};

    if (shape == SHAPE_HAT || shape == SHAPE_C_SHAPE) b.rect.h = 50;

    size_t firstDigit = text.find_first_of("-0123456789");
    if (firstDigit != std::string::npos) {
        try { b.param1 = std::stof(text.substr(firstDigit)); } catch(...) {}
    }

    if (cat == CAT_MYBLOCKS && text.find("Define") == std::string::npos) {
        b.opCode = OP_CALL_CUSTOM;
        b.stringParam = text;
    }
    if (cat == CAT_MYBLOCKS && text.find("Define") != std::string::npos) {
        b.opCode = OP_DEFINE_CUSTOM;
        b.stringParam = text.substr(7);
    }

    return b;
}

void MoveBlockChain(Block* b, int dx, int dy) {
    if (!b)
        {return;}
    b->rect.x += dx;
    b->rect.y += dy;
    if (b->subStack)
        MoveBlockChain(b->subStack, dx, dy);
    if (b->next)
        MoveBlockChain(b->next, dx, dy);
}
#endif
