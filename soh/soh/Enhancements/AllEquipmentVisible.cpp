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

// Bottle tracking structure
typedef struct {
    s8 equippedButton; // Which button index this bottle is equipped to (-1 if not equipped)
    s8 inventorySlot;  // Which inventory slot this bottle belongs to (0-3 for SLOT_BOTTLE_1 through SLOT_BOTTLE_4)
    u8 itemId;         // What item is in this slot (ITEM_NONE, ITEM_BOTTLE, ITEM_POTION_RED, etc.)
} BottleInfo;

// Track info for all 4 bottle inventory slots
static BottleInfo sBottles[4] = {
    {-1, 0, ITEM_NONE}, // Bottle slot 1
    {-1, 1, ITEM_NONE}, // Bottle slot 2
    {-1, 2, ITEM_NONE}, // Bottle slot 3
    {-1, 3, ITEM_NONE}  // Bottle slot 4
};

// Track which bottle slot is currently being used (in action)
static s8 sActiveBottleSlot = -1;

void RegisterBottleOnWaist() {
    // Hook to track when a bottle action is active
    COND_HOOK(OnPlayerUpdate, CVAR_VISIBLEEQUIPMENT_SET, []() {
        if (!GameInteractor::IsSaveLoaded()) {
            return;
        }
        
        Player* player = GET_PLAYER(gPlayState);
        
        // Check if a bottle action is happening
        if (player->itemAction >= PLAYER_IA_BOTTLE && player->itemAction <= PLAYER_IA_BOTTLE_FAIRY) {
            if (sActiveBottleSlot < 0) {
                // Bottle action just started - find which bottle slot is equipped to this button
                s8 buttonPressed = player->heldItemButton;
                for (int i = 0; i < 4; i++) {
                    if (sBottles[i].equippedButton == buttonPressed) {
                        sActiveBottleSlot = i;
                        break;
                    }
                }
            }
        } else {
            // No bottle action, reset and update the bottle that was just used
            if (sActiveBottleSlot >= 0) {
                // Update the bottle's item from inventory
                sBottles[sActiveBottleSlot].itemId = gSaveContext.inventory.items[SLOT_BOTTLE_1 + sActiveBottleSlot];
                sActiveBottleSlot = -1;
            }
        }
    });
    
    // Hook to initialize bottle data when save loads
    COND_HOOK(OnLoadGame, CVAR_VISIBLEEQUIPMENT_SET, [](int16_t fileNum) {
        // Update bottle inventory items from save data
        for (int i = 0; i < 4; i++) {
            sBottles[i].itemId = gSaveContext.inventory.items[SLOT_BOTTLE_1 + i];
            sBottles[i].equippedButton = -1; // Reset equipped button
        }
        
        // Check C-button and D-pad slots (buttons 1-7, cButtonSlots is 0-indexed for these)
        for (int buttonIndex = 1; buttonIndex < 8; buttonIndex++) {
            u8 inventorySlot = gSaveContext.equips.cButtonSlots[buttonIndex - 1];
            
            // Check if this slot is a bottle slot
            if (inventorySlot >= SLOT_BOTTLE_1 && inventorySlot <= SLOT_BOTTLE_4) {
                int bottleIndex = inventorySlot - SLOT_BOTTLE_1;
                sBottles[bottleIndex].equippedButton = buttonIndex;
            }
        }
    });
    
    // Hook to update bottle contents when bottle is used/changed
     COND_HOOK(OnPlayerBottleUpdate, CVAR_VISIBLEEQUIPMENT_SET, [](int16_t contents) {
        // Update all bottle items from inventory
        for (int i = 0; i < 4; i++) {
            u8 newItemId = gSaveContext.inventory.items[SLOT_BOTTLE_1 + i];
            if (sBottles[i].itemId != newItemId) {
                sBottles[i].itemId = newItemId;
            }
        }
    });
    
    // Hook to reload bottles when purchasing potions/bottle items
    COND_HOOK(OnSaleEnd, CVAR_VISIBLEEQUIPMENT_SET, [](GetItemEntry itemEntry) {
        // Reload all bottle contents from inventory after any purchase
        // (in randomizer, any item could potentially be a bottle)
        for (int i = 0; i < 4; i++) {
            u8 newItemId = gSaveContext.inventory.items[SLOT_BOTTLE_1 + i];
            if (sBottles[i].itemId != newItemId) {
                sBottles[i].itemId = newItemId;
            }
        }
    });
    
    // Hook to track when bottles are equipped to buttons
    COND_HOOK(OnItemEquip, CVAR_VISIBLEEQUIPMENT_SET, [](int16_t buttonIndex, int16_t inventorySlot, uint16_t itemId) {
        // Check if a bottle was previously equipped to this button, and unequip it
        for (int i = 0; i < 4; i++) {
            if (sBottles[i].equippedButton == buttonIndex) {
                sBottles[i].equippedButton = -1;
                break;
            }
        }
        
        // Check if this is a bottle item
        bool isBottle = (itemId >= ITEM_BOTTLE && itemId <= ITEM_LETTER_RUTO) ||
                        (itemId >= ITEM_POTION_RED && itemId <= ITEM_FAIRY);
        
        if (isBottle && inventorySlot >= SLOT_BOTTLE_1 && inventorySlot <= SLOT_BOTTLE_4) {
            int bottleIndex = inventorySlot - SLOT_BOTTLE_1;
            sBottles[bottleIndex].equippedButton = buttonIndex;
        }
    });
    
    COND_ID_HOOK(OnPlayerPostLimbDraw, PLAYER_LIMB_TORSO, CVAR_VISIBLEEQUIPMENT_SET, [](Player* player, s32 limbIndex) {
        if (!GameInteractor::IsSaveLoaded()) {
            return;
        }

        if (gSaveContext.linkAge != LINK_AGE_ADULT) {
            return;
        }

        PlayState* play = gPlayState;
        OPEN_DISPS(play->state.gfxCtx);

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

        // Bottle 1
        u8 bottle1Item = sBottles[0].itemId;
        if (bottle1Item != ITEM_NONE && sActiveBottleSlot != 0) {
            s32 actionParam = getBottleActionParam(bottle1Item);
            if (actionParam >= 0) {
                Color_RGB8* bottleColor = &sBottleColors[actionParam];
                Matrix_Push();
                Matrix_RotateZYX(0, 9331, 0, MTXMODE_APPLY);
                Matrix_Translate(248.5f, -93.2f, 1118.0f, MTXMODE_APPLY);
                Matrix_Scale(0.5f, 0.5f, 0.5f, MTXMODE_APPLY);
                gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gDPSetEnvColor(POLY_OPA_DISP++, bottleColor->r, bottleColor->g, bottleColor->b, 255);
                gSPDisplayList(POLY_OPA_DISP++, ResourceMgr_LoadGfxByName(gLinkAdultBottleDL));
                Matrix_Pop();
            }
        }

        // Bottle 2
        u8 bottle2Item = sBottles[1].itemId;
        if (bottle2Item != ITEM_NONE && sActiveBottleSlot != 1) {
            s32 actionParam = getBottleActionParam(bottle2Item);
            if (actionParam >= 0) {
                Color_RGB8* bottleColor = &sBottleColors[actionParam];
                Matrix_Push();
                Matrix_RotateZYX(0, 8548, 0, MTXMODE_APPLY);
                Matrix_Translate(310.5f, 93.2f, 745.0f, MTXMODE_APPLY);
                Matrix_Scale(0.5f, 0.5f, 0.5f, MTXMODE_APPLY);
                gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gDPSetEnvColor(POLY_OPA_DISP++, bottleColor->r, bottleColor->g, bottleColor->b, 255);
                gSPDisplayList(POLY_OPA_DISP++, ResourceMgr_LoadGfxByName(gLinkAdultBottleDL));
                Matrix_Pop();
            }
        }

        // Bottle 3
        u8 bottle3Item = sBottles[2].itemId;
        if (bottle3Item != ITEM_NONE && sActiveBottleSlot != 2) {
            s32 actionParam = getBottleActionParam(bottle3Item);
            if (actionParam >= 0) {
                Color_RGB8* bottleColor = &sBottleColors[actionParam];
                Matrix_Push();
                Matrix_RotateZYX(0, 6919, 0, MTXMODE_APPLY);
                Matrix_Translate(403.7f, 186.3f, 312.6f, MTXMODE_APPLY);
                Matrix_Scale(0.5f, 0.5f, 0.5f, MTXMODE_APPLY);
                gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gDPSetEnvColor(POLY_OPA_DISP++, bottleColor->r, bottleColor->g, bottleColor->b, 255);
                gSPDisplayList(POLY_OPA_DISP++, ResourceMgr_LoadGfxByName(gLinkAdultBottleDL));
                Matrix_Pop();
            }
        }

        // Bottle 4
        u8 bottle4Item = sBottles[3].itemId;
        if (bottle4Item != ITEM_NONE && sActiveBottleSlot != 3) {
            s32 actionParam = getBottleActionParam(bottle4Item);
            if (actionParam >= 0) {
                Color_RGB8* bottleColor = &sBottleColors[actionParam];
                Matrix_Push();
                Matrix_RotateZYX(0, 4774, 0, MTXMODE_APPLY);
                Matrix_Translate(430.5f, 165.5f, -198.7f, MTXMODE_APPLY);
                Matrix_Scale(0.5f, 0.5f, 0.5f, MTXMODE_APPLY);
                gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gDPSetEnvColor(POLY_OPA_DISP++, bottleColor->r, bottleColor->g, bottleColor->b, 255);
                gSPDisplayList(POLY_OPA_DISP++, ResourceMgr_LoadGfxByName(gLinkAdultBottleDL));
                Matrix_Pop();
            }
        }

        CLOSE_DISPS(play->state.gfxCtx);
    });
}

void RegisterAllEquipmentVisible() {
    RegisterBottleOnWaist();
}

static RegisterShipInitFunc initFunc(RegisterAllEquipmentVisible, { CVAR_VISIBLEEQUIPMENT_NAME });
