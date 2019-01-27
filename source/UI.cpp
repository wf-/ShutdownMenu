/*
* ReiNX Toolkit
* Copyright (C) 2018  Team ReiSwitched
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <switch.h>
#include "UI.hpp"
#include "Power.hpp"

#include "Services/reboot_to_payload.h"

#define WIN_WIDTH 1280
#define WIN_HEIGHT 720

static u32 currSel = 0;
static u32 vol = 64;
static u32 titleX = 130;
static u32 titleY = 40;
static u32 menuX = 55, menuY = 115;

static bool deinited = false;

static string title;
static string version;

vector<MenuOption> mainMenu;

Mix_Music *menuSel;
Mix_Music *menuConfirm;
Mix_Music *menuBack;

UI * UI::mInstance = 0;

/* ---------------------------------------------------------------------------------------
* Menu functions
*/
void UI::optReboot() {
    UI::deinit();
    Power::Reboot();
}
void UI::optShutdown() {
    UI::deinit();
    Power::Shutdown();
}
void UI::optRebootRcm() {
    UI::atmoReboot(TYPE_RCM);
}
void UI::optRebootToPayload() {
    UI::atmoReboot(TYPE_PAYLOAD);
}
void UI::atmoReboot(RebootType type) {
    if (MessageBox("Warning!",
            "This may corrupt your exFAT file system!\nUse at your own risk on exFAT formatted\nSD Card.\n\nDo you want to continue?",
            TYPE_YES_NO)) {
        Result rc = splInitialize();
        if (R_FAILED(rc)) {
            MessageBox("Error",
                    "spl initialization failed\n\nAre you using Atmosphere?",
                    TYPE_OK);
        }
        else {
            switch (type) {
                case TYPE_RCM: {
                    deinit();
                    rc = splSetConfig ((SplConfigItem) 65001, 1);
                    if (R_FAILED(rc)) {
                        splExit();
                        exitApp();
                    }
                }
                break;
                case TYPE_PAYLOAD: {
                    FILE *f = fopen("sdmc:/atmosphere/reboot_payload.bin", "rb");
                    if (f == NULL) {
                        MessageBox("Error",
                                "Failed to open\natmosphere/reboot_payload.bin",
                                TYPE_OK);
                    }
                    else {
                        read_payload(f);
                        deinit();
                        rc = reboot_to_payload();
                        if (R_FAILED(rc)) {
                            splExit();
                            exitApp();
                        }
                    }
                }
                break;
                default:
                break;
            }
        }
    }
}

/* ---------------------------------------------------------------------------------------
* UI class functions
*/
UI::UI(string Title, string Version) {
    romfsInit();
    fsdevMountSdmc();
    socketInitializeDefault();
#ifdef DEBUG
    nxlinkStdio();
    printf("printf output now goes to nxlink server\n");
#endif
    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_CreateWindowAndRenderer(WIN_WIDTH, WIN_HEIGHT, 0, &mRender._window, &mRender._renderer);
    mRender._surface = SDL_GetWindowSurface(mRender._window);
    SDL_SetRenderDrawBlendMode(mRender._renderer, SDL_BLENDMODE_BLEND);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    TTF_Init();
    SDL_SetRenderDrawColor(mRender._renderer, 255, 255, 255, 255);
    Mix_Init(MIX_INIT_FLAC | MIX_INIT_MOD | MIX_INIT_MP3 | MIX_INIT_OGG);
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 4096);
    Mix_VolumeMusic(vol);
    mThemes = Themes::instance();
    mThemes->Init(mRender);

    title = Title;
    version = Version;
    
    menuSel = Mix_LoadMUS("romfs:/Sounds/menu_select.mp3");
    menuConfirm = Mix_LoadMUS("romfs:/Sounds/menu_confirm.mp3");
    menuBack = Mix_LoadMUS("romfs:/Sounds/menu_back.mp3");
    
    //Main pages
    mainMenu.push_back(MenuOption("Power off", bind(&UI::optShutdown, this)));
    mainMenu.push_back(MenuOption("Restart", bind(&UI::optReboot, this)));
    mainMenu.push_back(MenuOption("Restart to atmosphere/reboot_payload.bin \ue151", bind(&UI::optRebootToPayload, this)));
    mainMenu.push_back(MenuOption("Restart to RCM \ue151", bind(&UI::optRebootRcm, this)));
}

void UI::setInstance(UI ui) {
    mInstance = &ui;
}

UI *UI::getInstance() {
    if(!mInstance)
        return NULL;
    return mInstance;
}

void UI::deinit() {
    if (deinited) return;

    TTF_Quit();
    IMG_Quit();
    Mix_CloseAudio();
    Mix_Quit();
    SDL_DestroyRenderer(mRender._renderer);
    SDL_FreeSurface(mRender._surface);
    SDL_DestroyWindow(mRender._window);
    SDL_Quit();
    romfsExit();
    socketExit();
    fsdevUnmountAll();

    deinited = true;
}

void UI::exitApp() {
    deinit();
    appletEndBlockingHomeButton(); // make sure we don't screw up hbmenu
    ::exit(0);
}

/*
* UI draw functions
*/
void UI::drawText(int x, int y, SDL_Color scolor, string text, TTF_Font *font) {
	SDL_Surface *surface = TTF_RenderUTF8_Blended_Wrapped(font, text.c_str(), scolor, 1280);

	if (!surface) {
		return;
	}

	SDL_SetSurfaceAlphaMod(surface, 255);
	SDL_Texture *texture = SDL_CreateTextureFromSurface(mRender._renderer, surface);

	if (texture) {
		SDL_Rect position;
		position.x = x;
		position.y = y;
		position.w = surface->w;
		position.h = surface->h;

		SDL_RenderCopy(mRender._renderer, texture, NULL, &position);
		SDL_DestroyTexture(texture);
	}
	SDL_FreeSurface(surface);
}


void UI::drawRect(int x, int y, int w, int h, SDL_Color scolor, unsigned border, SDL_Color bcolor) {
    drawRect(x-border, y-border, w+(2*border), h+(2*border), bcolor);
    drawRect(x, y, w, h, scolor);
}

void UI::drawRect(int x, int y, int w, int h, SDL_Color scolor) {
    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    SDL_SetRenderDrawColor(mRender._renderer, scolor.r, scolor.g, scolor.b, scolor.a);
    SDL_RenderFillRect(mRender._renderer, &rect);
}

void UI::drawBackXY(SDL_Surface *surf, SDL_Texture *tex, int x, int y) {
    SDL_Rect position;
    position.x = x;
    position.y = y;
    position.w = surf->w;
    position.h = surf->h;
    SDL_RenderCopy(mRender._renderer, tex, NULL, &position);
}

/*
* UI Pop-up stuff
*/
void UI::CreatePopupBox(u32 x, u32 y, u32 w, u32 h, string header) {
    drawRect(x-5, y-5, w+10, h+70, {0, 0, 0, 0xFF}); //BG box
    drawRect(x, y, w, 57, mThemes->popCol1); //Header
    drawRect(x, y+60, w, h, mThemes->popCol2); //Message
    drawText(x+15, y+15, mThemes->txtcolor, header, mThemes->fntMedium);
}

bool UI::MessageBox(string header, string message, MessageType type) {
    bool ret = false;
    u32 poph = 300, popw = 450;
    u32 buth = 50, butw = 100;
    u32 startx = (WIN_WIDTH/2)-(popw/2), starty = (WIN_HEIGHT/2)-(poph/2);
    
    renderMenu(true);
    CreatePopupBox(startx, starty, popw, poph, header);
    drawText(startx+15, starty+75, mThemes->txtcolor, message, mThemes->fntMedium);
    
    switch(type){
        case TYPE_YES_NO:
            drawRect(startx+popw-butw-10, starty+poph, butw, buth, mThemes->popCol1, 3, {0, 0, 0, 0xFF}); //YES
            drawText(startx+popw-butw+12, starty+poph+12, mThemes->txtcolor, "\ue0e0 Yes", mThemes->fntMedium);
            drawRect(startx+popw-(2*(butw+5))-10, starty+poph, butw, buth, mThemes->popCol1, 3, {0, 0, 0, 0xFF}); //NO
            drawText(startx+popw-(2*(butw+5))+12, starty+poph+12, mThemes->txtcolor, "\ue0e1 No", mThemes->fntMedium);
            break;
        default:
        case TYPE_OK:
            drawRect(startx+popw-butw-10, starty+poph, butw, buth, mThemes->popCol1, 3, {0, 0, 0, 0xFF}); //OK
            drawText(startx+popw-butw+12, starty+poph+12, mThemes->txtcolor, "\ue0e0 OK", mThemes->fntMedium);
            break;
    }
    SDL_RenderPresent(mRender._renderer);
    while(1){
        hidScanInput();
        u64 kDown = hidKeysDown(CONTROLLER_P1_AUTO);
        if(kDown & KEY_A) {
            ret = true;
            Mix_PlayMusic(menuConfirm, 1);
            break;
        }
        else if((kDown & KEY_B) && type == TYPE_YES_NO) {
            ret = false;
            Mix_PlayMusic(menuBack, 1);
            break;
        }
    }
    return ret;
}

/*
* Render function
*/
void UI::renderMenu(bool dontUpdateScreen) {
    SDL_RenderClear(mRender._renderer);
    drawBackXY(mThemes->bgs, mThemes->bgt, 0, 0);
    //Mainmenu  text
    drawText(titleX, titleY, mThemes->txtcolor, title, mThemes->fntLarge);
    int oy = menuY;
    if(!mainMenu.empty()) for(unsigned int i = 0; i < mainMenu.size(); i++){
        //Mainmenu buttons
        if(i == currSel) {
            drawRect(menuX-10, oy-10, 1190, 25+20, mThemes->butCol, 5, mThemes->butBorderCol);
            drawText(menuX, oy, mThemes->selcolor, mainMenu[i].getName(), mThemes->fntMedium);
        } else {
            drawText(menuX, oy, mThemes->txtcolor, mainMenu[i].getName(), mThemes->fntMedium);
        }
        oy += 50;
    }
    if (!dontUpdateScreen) SDL_RenderPresent(mRender._renderer);
}

/*
* Menu nav functions
*/
void UI::MenuUp() {
    Mix_PlayMusic(menuSel, 1);
    if(currSel > 0) currSel--;
    else currSel = mainMenu.size() - 1;
}

void UI::MenuDown() {
    Mix_PlayMusic(menuSel, 1);
    if((unsigned int) currSel < mainMenu.size() - 1) currSel++;
    else currSel = 0;
}

void UI::MenuSel() {
    Mix_PlayMusic(menuConfirm, 1);
    mainMenu[currSel].callFunc();
}

void UI::MenuBack() {
    exitApp();
}
