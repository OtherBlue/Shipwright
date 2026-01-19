#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
#include "soh/ResourceManagerHelpers.h"
#include "soh/frame_interpolation.h"
#include "objects/object_link_boy/object_link_boy.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
extern Color_RGB8 sBottleColors[];
extern PlayState* gPlayState;
}

#define CVAR_VISIBLEEQUIPMENT_NAME CVAR_ENHANCEMENT("VisibleEquipment")
#define CVAR_VISIBLEEQUIPMENT_DEFAULT 0
#define CVAR_VISIBLEEQUIPMENT_VALUE CVarGetInteger(CVAR_VISIBLEEQUIPMENT_NAME, CVAR_VISIBLEEQUIPMENT_DEFAULT)
#define CVAR_VISIBLEEQUIPMENT_SET (CVAR_VISIBLEEQUIPMENT_VALUE != CVAR_VISIBLEEQUIPMENT_DEFAULT)

void RenderBottlesOnTorso() {
    // Position and rotation data for each bottle slot
    static const struct {
        s16 rotY;
        f32 transX, transY, transZ;
    } bottlePositions[4] = {
        { 9331, 248.5f, -93.2f, 1118.0f },  // Bottle 1
        { 8548, 310.5f, 93.2f, 745.0f },     // Bottle 2
        { 6919, 403.7f, 186.3f, 312.6f },    // Bottle 3
        { 4774, 430.5f, 165.5f, -198.7f }    // Bottle 4
    };
    
    COND_ID_HOOK(OnPlayerPostLimbDraw, PLAYER_LIMB_TORSO, CVAR_VISIBLEEQUIPMENT_SET, [](Player* player, s32 limbIndex) {
        if (!GameInteractor::IsSaveLoaded()) {
            return;
        }

        if (gSaveContext.linkAge != LINK_AGE_ADULT) {
            return;
        }

        PlayState* play = gPlayState;
        OPEN_DISPS(play->state.gfxCtx);

        // Determine which bottle slot is currently in use (if any)
        s8 activeBottleSlot = -1;
        if (player->itemAction >= PLAYER_IA_BOTTLE && player->itemAction <= PLAYER_IA_BOTTLE_FAIRY) {
            s8 buttonPressed = player->heldItemButton;
            if (buttonPressed > 0 && buttonPressed < 8) {
                u8 inventorySlot = gSaveContext.equips.cButtonSlots[buttonPressed - 1];
                if (inventorySlot >= SLOT_BOTTLE_1 && inventorySlot <= SLOT_BOTTLE_4) {
                    activeBottleSlot = inventorySlot - SLOT_BOTTLE_1;
                }
            }
        }

        // Helper function to get bottle action param from item ID
        auto getBottleActionParam = [](u8 bottleItem) -> s32 {
            switch (bottleItem) {
                case ITEM_BOTTLE: return 0;
                case ITEM_FISH: return 1;
                case ITEM_BLUE_FIRE: return 2;
                case ITEM_BUG: return 3;
                case ITEM_POE: return 4;
                case ITEM_BIG_POE: return 5;
                case ITEM_LETTER_RUTO: return 6;
                case ITEM_POTION_RED: return 7;
                case ITEM_POTION_BLUE: return 8;
                case ITEM_POTION_GREEN: return 9;
                case ITEM_MILK_BOTTLE: return 10;
                case ITEM_MILK_HALF: return 11;
                case ITEM_FAIRY: return 12;
                default: return -1;
            }
        };

        // Draw each bottle from inventory if it's not currently being used
        for (int i = 0; i < 4; i++) {
            if (activeBottleSlot == i) {
                continue; // Skip the bottle currently in use
            }
            
            u8 bottleItem = gSaveContext.inventory.items[SLOT_BOTTLE_1 + i];
            if (bottleItem == ITEM_NONE) {
                continue; // No bottle in this slot
            }
            
            s32 actionParam = getBottleActionParam(bottleItem);
            if (actionParam < 0) {
                continue; // Invalid bottle type
            }
            
            Color_RGB8* bottleColor = &sBottleColors[actionParam];
            
            Matrix_Push();
            Matrix_RotateZYX(0, bottlePositions[i].rotY, 0, MTXMODE_APPLY);
            Matrix_Translate(bottlePositions[i].transX, bottlePositions[i].transY, 
                           bottlePositions[i].transZ, MTXMODE_APPLY);
            Matrix_Scale(0.5f, 0.5f, 0.5f, MTXMODE_APPLY);
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), 
                     G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gDPSetEnvColor(POLY_OPA_DISP++, bottleColor->r, bottleColor->g, bottleColor->b, 255);
            gSPDisplayList(POLY_OPA_DISP++, ResourceMgr_LoadGfxByName(gLinkAdultBottleDL));
            Matrix_Pop();
        }

        CLOSE_DISPS(play->state.gfxCtx);
    });
}

void RegisterAllEquipmentVisible() {
    RenderBottlesOnTorso();
}

static RegisterShipInitFunc initFunc(RegisterAllEquipmentVisible, { CVAR_VISIBLEEQUIPMENT_NAME });