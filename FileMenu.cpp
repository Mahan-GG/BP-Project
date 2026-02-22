#include "FileMenu.h"
#include "BlockSystem.h"
#include "SpriteSystem.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>
#include <algorithm>

void InitBlockSystem(BlockSystemContext* ctx, int w, int h);

void AddNewSprite(SDL_Renderer *pRenderer, SpriteContext *pContext);

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

// کدهای SaveProject و LoadProject و HandleFileMenuEvents دقیقاً مثل قبل
void SaveProject(std::string name, BlockSystemContext* blockCtx, SpriteContext* spriteCtx) {
    UpdateProjectIndex(name);
    std::string filename = name + ".txt";
    std::ofstream file(filename);
    if (!file.is_open()) return;

    file << "SCRATCH_CLONE_SAVE_V1\n";
    file << "[SPRITES]\n" << spriteCtx->sprites.size() << "\n";
    for (const auto& s : spriteCtx->sprites) {
        file << "SPRITE_START\n" << s.name << "\n" << s.x << " " << s.y << " " << s.direction << " " << s.scale << " " << s.isVisible << "\nSPRITE_END\n";
    }

    file << "[BLOCKS]\n";
    int blockCount = 0;
    for(const auto& b : blockCtx->blocks) { if(b.rect.x > blockCtx->paletteArea.w) blockCount++; }
    file << blockCount << "\n";

    for (const auto& b : blockCtx->blocks) {
        if (b.rect.x <= blockCtx->paletteArea.w) continue;
        file << "BLOCK_DATA " << b.id << " " << b.opCode << " " << b.rect.x << " " << b.rect.y << " " << b.param1 << " " << b.param2 << " ";
        int nextID = (b.next) ? b.next->id : -1;
        file << nextID << "\n";
    }
    file.close();
    std::cout << "Saved: " << filename << std::endl;
}

void LoadProject(SDL_Renderer* renderer, std::string name, BlockSystemContext* blockCtx, SpriteContext* spriteCtx) {
    std::string filename = name + ".txt";
    std::ifstream file(filename);
    if (!file.is_open()) return;

    std::string header; file >> header;
    if (header != "SCRATCH_CLONE_SAVE_V1") return;

    spriteCtx->sprites.clear(); blockCtx->blocks.clear();
    InitBlockSystem(blockCtx, 1280, 720);

    std::string line;
    while (file >> line) {
        if (line == "[SPRITES]") {
            int count; file >> count;
            for (int i = 0; i < count; i++) {
                std::string tag, sName; float x, y, dir, sc; bool vis;
                file >> tag >> sName >> x >> y >> dir >> sc >> vis >> tag;
                AddNewSprite(renderer, spriteCtx);
                Sprite& s = spriteCtx->sprites.back();
                s.name = sName; s.x = x; s.y = y; s.direction = dir; s.scale = sc; s.isVisible = vis;
            }
        }
        else if (line == "[BLOCKS]") {
            int count; file >> count;
            struct Temp { Block b; int nID; };
            std::vector<Temp> temps; std::map<int, Block*> idMap;

            for(int i=0; i<count; i++) {
                std::string t; int id, op, bx, by, nid; float p1, p2;
                file >> t >> id >> op >> bx >> by >> p1 >> p2 >> nid;
                // *** فیکس مهم: الان CreateBlock هفت تا پارامتر می‌گیره ***// *** فیکس شد: حالا 6 پارامتر می‌فرستیم، و opCode رو بعدش دستی تنظیم می‌کنیم ***
                Block b = CreateBlock(id, bx, by, "Loaded Block", CAT_EVENTS, SHAPE_STACK);
                b.opCode = (BlockOpCode)op;
                b.param1 = p1;
                b.param2 = p2;


                if(b.opCode == OP_FLAG_CLICKED) { b.text="When Green Flag Clicked"; b.shape=SHAPE_HAT; b.category=CAT_EVENTS;}
                else if(b.opCode == OP_MOVE_STEPS) { b.text="Move "+std::to_string((int)p1)+" Steps"; b.category=CAT_MOTION;}
                else if(b.opCode == OP_TURN_RIGHT) b.text="Turn Right "+std::to_string((int)p1);
                else if(b.opCode == OP_TURN_LEFT) b.text="Turn Left "+std::to_string((int)p1);
                else if(b.opCode == OP_WAIT_SEC) { b.text="Wait 1 Secs"; b.category=CAT_CONTROL;}

                temps.push_back({b, nid});
            }
            for(auto& tmp : temps) { blockCtx->blocks.push_back(tmp.b); idMap[tmp.b.id] = &blockCtx->blocks.back(); }
            for(int i=0; i<temps.size(); i++) {
                if(temps[i].nID != -1) { for(auto& rb : blockCtx->blocks) if(rb.id == temps[i].b.id) rb.next = idMap[temps[i].nID]; }
            }
        }
    }
    std::cout << "Loaded: " << filename << std::endl;
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

}
