#ifndef STAGEGUI_H
#define STAGEGUI_H
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "SpriteSystem.h"
#include <string>
//رسم متن
void DrawLabel(SDL_Renderer* r, TTF_Font* font, std::string text, int x, int y, SDL_Color color) {
    if (text.empty())
    {return;}
    SDL_Surface* surf = TTF_RenderText_Blended(font, text.c_str(), color);
    SDL_Rect rect = {
            x,
            y,
            surf -> w,
            surf -> h};
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_RenderCopy(r, tex, NULL, &rect);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

//صفحه مختص اسپرایت
void RenderSpriteProperties(SDL_Renderer* renderer, TTF_Font* font, SpriteContext* ctx, SDL_Rect area) {
    SDL_SetRenderDrawColor(renderer, 230, 240, 255, 255); //رنگ بک گراند
    SDL_RenderFillRect(renderer, &area);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderDrawLine(renderer, area.x, area.y, area.x + area.w, area.y);
    if (ctx ->  selectedSpriteIndex == -1 or ctx-> sprites.empty() ) {
        DrawLabel(renderer, font, "No Sprite Selected", area.x + 20, area.y + 20, {100,100,100,255});
        return;}

    Sprite* s = &ctx  -> sprites[ctx->selectedSpriteIndex];
    int startX = area.x + 10;
    int startY = area.y + 10;
    SDL_Color txtCol =
            {
            80, 80, 80, 255
    };
    DrawLabel(renderer, font, "Sprite", startX, startY, txtCol);
    SDL_Rect nameBox = {startX + 50, startY, 100, 25};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &nameBox);
    DrawLabel(renderer, font, s->name, nameBox.x + 5, nameBox.y + 5, {0,0,0,255});
    int xPos = startX + 170;
    DrawLabel(renderer, font, "X", xPos, startY, txtCol);
    SDL_Rect xBox = {xPos + 20, startY, 50, 25};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &xBox);
    DrawLabel(renderer, font, std::to_string((int)s->x), xBox.x + 5, xBox.y + 5, {0,0,0,255});

    int yPos = xPos + 80;
    DrawLabel(renderer, font, "Y", yPos, startY, txtCol);
    SDL_Rect yBox = {yPos + 20, startY, 50, 25};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &yBox);
    DrawLabel(renderer, font, std::to_string((int)s->y), yBox.x + 5, yBox.y + 5, {0,0,0,255});

    //دکمه شو
    int showPos = yPos + 90;
    DrawLabel(renderer, font, "Show", showPos, startY, txtCol);

    SDL_Rect eyeOn = {showPos + 45, startY, 30, 25};
    if(s->isVisible) SDL_SetRenderDrawColor(renderer, 200, 230, 255, 255); // هایلایت
    else SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer, &eyeOn);
    DrawLabel(renderer, font, "O", eyeOn.x+8, eyeOn.y+5, {0,0,0,255}); // نماد چشم

    SDL_Rect eyeOff = {showPos + 80, startY, 30, 25};
    if(!s->isVisible) SDL_SetRenderDrawColor(renderer, 200, 230, 255, 255);
    else SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer, &eyeOff);
    DrawLabel(renderer, font, "X", eyeOff.x+8, eyeOff.y+5, {0,0,0,255});

    int row2Y = startY + 35;
    DrawLabel(renderer, font, "Size", startX, row2Y, txtCol);
    SDL_Rect sizeBox = {startX + 40, row2Y, 50, 25};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &sizeBox);
    DrawLabel(renderer, font, std::to_string((int)s->scale), sizeBox.x+5, sizeBox.y+5, {0,0,0,255});

    DrawLabel(renderer, font, "Dir", startX + 110, row2Y, txtCol);
    SDL_Rect dirBox = {startX + 140, row2Y, 50, 25};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &dirBox);
    DrawLabel(renderer, font, std::to_string((int)s->direction), dirBox.x+5, dirBox.y+5, {0,0,0,255});
}

void RenderSpriteList(SDL_Renderer* renderer, TTF_Font* font, SpriteContext* ctx, SDL_Rect area) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer, &area);

    int padding = 10;
    int itemSize = 70;
    int x = area.x + padding;
    int y = area.y + padding;

    for (int i = 0; i < ctx->sprites.size(); i++) {
        SDL_Rect itemRect = {x, y, itemSize, itemSize};

        if (i == ctx->selectedSpriteIndex) {
            SDL_SetRenderDrawColor(renderer, 76, 151, 255, 255); // آبی اسکرچ
            SDL_Rect border = {x-2, y-2, itemSize+4, itemSize+4};
            SDL_RenderFillRect(renderer, &border);
        }

        SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
        SDL_RenderFillRect(renderer, &itemRect);

        // تصویر بندانگشتی (Thumbnail)
        if (!ctx->sprites[i].costumes.empty()) {
            SDL_Texture* tex = ctx->sprites[i].costumes[0].texture;
            SDL_Rect thumb = {x+10, y+10, itemSize-20, itemSize-20};
            SDL_RenderCopy(renderer, tex, NULL, &thumb);
        }

        if (i == ctx->selectedSpriteIndex) {
            SDL_Rect delBtn = {x + itemSize - 15, y, 15, 15};
            SDL_SetRenderDrawColor(renderer, 255, 50, 50, 255);
            SDL_RenderFillRect(renderer, &delBtn);
        }

        x += itemSize + padding;
    }
    SDL_Rect addBtn = {x, y, itemSize, itemSize};
    SDL_SetRenderDrawColor(renderer, 200, 255, 200, 255); // سبز کمرنگ
    SDL_RenderFillRect(renderer, &addBtn);
    DrawLabel(renderer, font, "+", addBtn.x + 25, addBtn.y + 15, {0,100,0,255});
}
void RenderBackdropPanel(SDL_Renderer* renderer, TTF_Font* font, SpriteContext* ctx, SDL_Rect area) {
    SDL_SetRenderDrawColor(renderer, 250, 250, 250, 255);
    SDL_RenderFillRect(renderer, &area);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); // مرز چپ
    SDL_RenderDrawLine(renderer, area.x, area.y, area.x, area.y + area.h);

    DrawLabel(renderer, font, "Stage", area.x + 10, area.y + 5, {100,100,100,255});
    SDL_Rect preview = {area.x + 10, area.y + 30, 80, 60};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer, &preview);
    SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
    SDL_RenderDrawRect(renderer, &preview);

    if (!ctx->backdrops.empty()) {
        SDL_RenderCopy(renderer, ctx->backdrops[ctx->currentBackdropIndex].texture, NULL, &preview);
    }
    SDL_Rect addBgBtn = {area.x + 10, area.y + 100, 80, 25};
    SDL_SetRenderDrawColor(renderer, 76, 151, 255, 255);
    SDL_RenderFillRect(renderer, &addBgBtn);
    DrawLabel(renderer, font, "New BG", addBgBtn.x + 10, addBgBtn.y + 5, {255,255,255,255});
}

inline void HandleDropEvent(SDL_Event* e, SDL_Renderer* r, SpriteContext* ctx) {
    if (e->type == SDL_DROPFILE) {
        char* droppedFile = e->drop.file;
        SDL_Surface* surf = SDL_LoadBMP(droppedFile);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
            ctx->backdrops.push_back({"Imported BG", tex, surf->w, surf->h});
            ctx->currentBackdropIndex = ctx->backdrops.size() - 1;
            SDL_FreeSurface(surf);
            std::cout << "Background Updated: " << droppedFile << std::endl;
        }
        SDL_free(droppedFile);
    }
}

inline void HandleStageGUIEvents(SDL_Event* e, SpriteContext* ctx, SDL_Renderer* r) {
    if (e->type != SDL_MOUSEBUTTONDOWN) return;
    int mx = e->button.x;
    int my = e->button.y;

    if (mx > 800) {
        int relX = mx - 800;
        int index = relX / 80;
        if (index >= 0 && index < (int)ctx->sprites.size()) {
            ctx->selectedSpriteIndex = index;
        }

        if (index == ctx->sprites.size()) {
            AddNewSprite(r, ctx);
        }
        }
}
#endif
