#include "global.h"
#include "vault_hunter.h"
#include "string_util.h"
#include "strings.h"
#include "field_message_box.h"
#include "script.h"
#include "event_data.h"

// Final names, confirmed with the user. Slot order for each Vault Hunter matches
// the order they're described in the original spec:
// Zer0:  0 = crit specialization, 1 = evasion/speed, 2 = execute/finisher damage
// Maya:  0 = kill-triggered healing, 1 = status/control, 2 = defensive
static const u8 *const sVaultHunterPassiveNames[2][VAULT_HUNTER_PASSIVE_COUNT] =
{
    [VAULT_HUNTER_ZERO] = {
        COMPOUND_STRING("0ne Sh0t, 0ne Kill"),
        COMPOUND_STRING("0utmane0uver"),
        COMPOUND_STRING("0verkill"),
    },
    [VAULT_HUNTER_MAYA] = {
        COMPOUND_STRING("Soul Harvest"),
        COMPOUND_STRING("Ruin"),
        COMPOUND_STRING("Phase Shield"),
    },
};

enum VaultHunterId GetVaultHunterId(void)
{
    return gSaveBlock2Ptr->vaultHunterId;
}

bool32 IsVaultHunterPassiveUnlocked(u8 slot)
{
    if (slot >= VAULT_HUNTER_PASSIVE_COUNT)
        return FALSE;
    return (gSaveBlock2Ptr->unlockedPassives >> slot) & 1;
}

u8 GetVaultHunterPassiveTier(void)
{
    return gSaveBlock2Ptr->passiveTier;
}

u8 GetActiveVaultHunterPassiveSlot(void)
{
    return gSaveBlock2Ptr->activePassiveSlot;
}

bool32 IsVaultHunterPassiveActive(u8 slot)
{
    return IsVaultHunterPassiveUnlocked(slot) && GetActiveVaultHunterPassiveSlot() == slot;
}

// Fails (does nothing, returns FALSE) if the requested slot isn't unlocked yet --
// the player can only ever activate a passive they've actually earned.
bool32 SetActiveVaultHunterPassiveSlot(u8 slot)
{
    if (!IsVaultHunterPassiveUnlocked(slot))
        return FALSE;
    gSaveBlock2Ptr->activePassiveSlot = slot;
    return TRUE;
}

void UnlockVaultHunterPassive(u8 slot)
{
    if (slot >= VAULT_HUNTER_PASSIVE_COUNT)
        return;
    gSaveBlock2Ptr->unlockedPassives |= (1 << slot);
    // Changing the active passive should never happen automatically -- per the
    // spec, unlocking a new passive (or a tier increase) must not switch what's
    // currently active. If this is the very first passive ever unlocked, though,
    // there is no "currently active" one to preserve, so it's set as active by
    // default purely so the player has something active immediately rather than
    // needing to visit a Pokemon Center first just to turn their first passive on.
    if (GetActiveVaultHunterPassiveSlot() == PASSIVE_SLOT_NONE)
        gSaveBlock2Ptr->activePassiveSlot = slot;
}

void SetVaultHunterPassiveTier(u8 tier)
{
    if (tier > VAULT_HUNTER_PASSIVE_TIER_MAX)
        tier = VAULT_HUNTER_PASSIVE_TIER_MAX;
    gSaveBlock2Ptr->passiveTier = tier;
}

u8 GetPendingVaultHunterPassiveUnlocks(void)
{
    return gSaveBlock2Ptr->pendingPassiveUnlocks;
}

void ChooseVaultHunterPassiveUnlock(u8 slot)
{
    if (gSaveBlock2Ptr->pendingPassiveUnlocks == 0)
        return;
    if (IsVaultHunterPassiveUnlocked(slot))
        return;
    UnlockVaultHunterPassive(slot);
    gSaveBlock2Ptr->pendingPassiveUnlocks--;
}

// --- Script-facing (callnative) helpers for the Pokemon Center flow. ---
// Slot argument is passed via VAR_0x8004, set by the calling script beforehand.

void Script_HasVaultHunterPassiveBusiness(struct ScriptContext *ctx)
{
    bool32 hasBusiness = (GetPendingVaultHunterPassiveUnlocks() > 0)
                       || (gSaveBlock2Ptr->unlockedPassives != 0);
    gSpecialVar_Result = hasBusiness;
}

void Script_HasPendingVaultHunterPassiveChoice(struct ScriptContext *ctx)
{
    gSpecialVar_Result = (GetPendingVaultHunterPassiveUnlocks() > 0);
}

void Script_IsVaultHunterPassiveSlotLocked(struct ScriptContext *ctx)
{
    u8 slot = VarGet(VAR_0x8005);
    gSpecialVar_Result = !IsVaultHunterPassiveUnlocked(slot);
}

void Script_IsVaultHunterPassiveSlotUnlocked(struct ScriptContext *ctx)
{
    u8 slot = VarGet(VAR_0x8005);
    gSpecialVar_Result = IsVaultHunterPassiveUnlocked(slot);
}

void Script_IsVaultHunterPassiveSlotActive(struct ScriptContext *ctx)
{
    u8 slot = VarGet(VAR_0x8005);
    gSpecialVar_Result = IsVaultHunterPassiveActive(slot);
}

void Script_BufferVaultHunterPassiveName(struct ScriptContext *ctx)
{
    u8 slot = VarGet(VAR_0x8005);
    if (slot < VAULT_HUNTER_PASSIVE_COUNT)
        StringCopy(gStringVar1, sVaultHunterPassiveNames[GetVaultHunterId()][slot]);
}

void Script_ChooseVaultHunterPassiveUnlock(struct ScriptContext *ctx)
{
    ChooseVaultHunterPassiveUnlock(VarGet(VAR_0x8005));
}

void Script_SetActiveVaultHunterPassiveSlot(struct ScriptContext *ctx)
{
    SetActiveVaultHunterPassiveSlot(VarGet(VAR_0x8005));
}

// TODO: Badge 5's unlock is still automatic, which is correct (only one locked
// slot remains by then, so there's no real choice to make). Badges 1 and 3 now
// correctly owe the player a choice (see GetPendingVaultHunterPassiveUnlocks /
// ChooseVaultHunterPassiveUnlock) rather than silently auto-picking one -- the
// Pokemon Center is where that choice actually gets presented and made.
static u8 FindFirstLockedPassiveSlot(void)
{
    u8 slot;
    for (slot = 0; slot < VAULT_HUNTER_PASSIVE_COUNT; slot++)
    {
        if (!IsVaultHunterPassiveUnlocked(slot))
            return slot;
    }
    return PASSIVE_SLOT_NONE;
}

void TryAdvanceVaultHunterPassivesForBadge(u8 badgeNumber)
{
    switch (badgeNumber)
    {
    case 1:
        // First passive unlock is owed to the player as a choice. The global tier
        // needs to reach 1 immediately regardless of which slot they eventually
        // pick, since Tier 1 is what that first unlock (whenever chosen) should
        // begin at.
        SetVaultHunterPassiveTier(1);
        gSaveBlock2Ptr->pendingPassiveUnlocks++;
        break;
    case 3:
        // Second passive unlock, also owed as a choice between the two remaining
        // locked slots. Tier is still 1 at this point (Badge 4 raises it to 2), so
        // whichever slot gets chosen will correctly start at Tier 1.
        gSaveBlock2Ptr->pendingPassiveUnlocks++;
        break;
    case 4:
        // All currently-unlocked passives move from Tier 1 to Tier 2.
        SetVaultHunterPassiveTier(2);
        break;
    case 5:
        // Third and final passive unlocks automatically -- only one locked slot
        // remains by this point, so there's no real choice left to make. Tier is
        // already 2, so it correctly begins at Tier 2 rather than Tier 1.
        UnlockVaultHunterPassive(FindFirstLockedPassiveSlot());
        break;
    case 8:
        // All three passives move from Tier 2 to Tier 3.
        SetVaultHunterPassiveTier(3);
        break;
    default:
        break; // badges 2, 6, 7 (and anything outside 1-8) don't affect passives
    }
}
