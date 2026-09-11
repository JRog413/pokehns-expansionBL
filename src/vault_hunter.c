#include "global.h"
#include "vault_hunter.h"
#include "string_util.h"
#include "strings.h"
#include "field_message_box.h"
#include "script.h"
#include "event_data.h"
#include "battle.h"

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

// The one check every battle-effect hook needs: is this specific battler both (a)
// the player's own side (only the player has a Vault Hunter -- this never applies
// to wild Pokemon or trainer opponents) and (b) currently benefiting from this
// specific passive being the one active slot. All 6 passive effects in
// battle_util.c/battle_main.c/etc. go through this single function rather than
// each reimplementing the same two checks.
bool32 IsVaultHunterPassiveActiveForBattler(u8 battler, u8 slot)
{
    return IsOnPlayerSide(battler) && IsVaultHunterPassiveActive(slot);
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

// --- Ruin (Maya): status-triggered secondary effect system. ---
//
// Each secondary effect is a simple apply/clear function pair. This is the part
// of the design meant to stay expandable: adding a new secondary effect later
// means writing one apply function, one clear function, and adding one line to
// sRuinSecondaryEffects below -- nothing else in this file needs to change to
// support it.
//
// "clear" exists specifically so a later Ruin activation can cleanly remove
// whichever effect it previously applied before applying a new one (per the spec:
// max one secondary effect at a time, and a new one replaces the old). It only
// touches state Ruin itself tracks having set (see ruinSecondaryEffect on
// BattlerState) -- it never clears a volatile that arose from an unrelated,
// ordinary move, since that's not Ruin's to touch.

static void RuinApplyConfusion(u8 attacker, u8 target)
{
    if (!gBattleMons[target].volatiles.confusionTurns)
        gBattleMons[target].volatiles.confusionTurns = RandomUniform(RNG_CONFUSION_TURNS, 2, B_CONFUSION_TURNS);
}
static void RuinClearConfusion(u8 target)
{
    gBattleMons[target].volatiles.confusionTurns = 0;
}

static void RuinApplyInfatuation(u8 attacker, u8 target)
{
    if (!gBattleMons[target].volatiles.infatuation)
        gBattleMons[target].volatiles.infatuation = INFATUATED_WITH(attacker);
}
static void RuinClearInfatuation(u8 target)
{
    gBattleMons[target].volatiles.infatuation = 0;
}

static void RuinApplyDisable(u8 attacker, u8 target)
{
    u32 i;
    if (gBattleMons[target].volatiles.disabledMove != MOVE_NONE)
        return;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (gBattleMons[target].moves[i] == gLastMoves[target])
            break;
    }
    // If the target's last move isn't found (e.g. it hasn't moved yet this
    // battle) or is already out of PP, there's nothing valid to disable --
    // Ruin's activation still happened (the status landed), it simply has
    // nothing to attach this particular secondary effect to this time.
    if (i == MAX_MON_MOVES || gBattleMons[target].pp[i] == 0)
        return;
    gBattleMons[target].volatiles.disabledMove = gBattleMons[target].moves[i];
    gBattleMons[target].volatiles.disableTimer = B_DISABLE_TIMER;
}
static void RuinClearDisable(u8 target)
{
    gBattleMons[target].volatiles.disabledMove = MOVE_NONE;
    gBattleMons[target].volatiles.disableTimer = 0;
}

static void RuinApplyTrap(u8 attacker, u8 target)
{
    if (!gBattleMons[target].volatiles.wrapped)
    {
        gBattleMons[target].volatiles.wrapped = TRUE;
        gBattleMons[target].volatiles.wrappedMove = MOVE_NONE; // not tied to a specific move
        gBattleMons[target].volatiles.wrappedBy = attacker;
    }
}
static void RuinClearTrap(u8 target)
{
    gBattleMons[target].volatiles.wrapped = FALSE;
}

static void RuinApplyLeechSeed(u8 attacker, u8 target)
{
    if (!gBattleMons[target].volatiles.leechSeed && !IS_BATTLER_OF_TYPE(target, TYPE_GRASS))
        gBattleMons[target].volatiles.leechSeed = LEECHSEEDED_BY(attacker);
}
static void RuinClearLeechSeed(u8 target)
{
    gBattleMons[target].volatiles.leechSeed = 0;
}

static void RuinApplyEncore(u8 attacker, u8 target)
{
    u32 i;
    if (gBattleMons[target].volatiles.encoreTimer)
        return;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (gBattleMons[target].moves[i] == gLastMoves[target])
            break;
    }
    if (i == MAX_MON_MOVES || gBattleMons[target].pp[i] == 0)
        return; // nothing valid to Encore into this time -- see RuinApplyDisable
    gBattleMons[target].volatiles.encoredMove = gBattleMons[target].moves[i];
    gBattleMons[target].volatiles.encoredMovePos = i;
    gBattleMons[target].volatiles.encoreTimer = B_ENCORE_TIMER;
}
static void RuinClearEncore(u8 target)
{
    gBattleMons[target].volatiles.encoredMove = MOVE_NONE;
    gBattleMons[target].volatiles.encoreTimer = 0;
}

static void RuinApplyTaunt(u8 attacker, u8 target)
{
    if (!gBattleMons[target].volatiles.tauntTimer)
        gBattleMons[target].volatiles.tauntTimer = 3;
}
static void RuinClearTaunt(u8 target)
{
    gBattleMons[target].volatiles.tauntTimer = 0;
}

struct RuinSecondaryEffect
{
    void (*apply)(u8 attacker, u8 target);
    void (*clear)(u8 target);
};

// Index 0 is deliberately unused (BattlerState.ruinSecondaryEffect == 0 means "no
// effect active"), so real effects start at index 1 -- see RuinTryActivate below,
// which picks a random index in [1, ARRAY_COUNT) rather than [0, ARRAY_COUNT).
static const struct RuinSecondaryEffect sRuinSecondaryEffects[] =
{
    [0] = { NULL, NULL },
    { RuinApplyConfusion, RuinClearConfusion },
    { RuinApplyInfatuation, RuinClearInfatuation },
    { RuinApplyDisable, RuinClearDisable },
    { RuinApplyTrap, RuinClearTrap },
    { RuinApplyLeechSeed, RuinClearLeechSeed },
    { RuinApplyEncore, RuinClearEncore },
    { RuinApplyTaunt, RuinClearTaunt },
};

// Called from SetNonVolatileStatus right after a primary status has actually been
// applied to `target`. `targetHadStatusBefore` must reflect the target's status
// immediately before this infliction (the tiered activation chance depends on it),
// so the caller captures that before calling SetNonVolatileStatus's status-setting
// switch, not after.
// Applies (or replaces) Ruin's secondary effect on `target`. Shared by both
// activation entry points below.
static void ApplyRuinSecondaryEffect(u8 attacker, u8 target)
{
    u8 previousEffect = gBattleStruct->battlerState[target].ruinSecondaryEffect;
    if (previousEffect != 0)
        sRuinSecondaryEffects[previousEffect].clear(target);

    u8 newEffect = 1 + (Random() % (ARRAY_COUNT(sRuinSecondaryEffects) - 1));
    sRuinSecondaryEffects[newEffect].apply(attacker, target);
    gBattleStruct->battlerState[target].ruinSecondaryEffect = newEffect;
}

// The tiered activation chance itself (10/15/20% with no existing status,
// 15/20/25% with one), shared by both entry points so the two percentages only
// ever live in one place. Returns 0 if Ruin isn't active for `attacker` or
// nothing is unlocked yet, which the callers below both treat as "never rolls".
static u32 GetRuinActivationChance(u8 attacker, bool32 targetHadStatusBefore)
{
    if (!IsVaultHunterPassiveActiveForBattler(attacker, MAYA_PASSIVE_RUIN))
        return 0;

    switch (GetVaultHunterPassiveTier())
    {
    case 1:
        return targetHadStatusBefore ? 15 : 10;
    case 2:
        return targetHadStatusBefore ? 20 : 15;
    case 3:
        return targetHadStatusBefore ? 25 : 20;
    default:
        return 0;
    }
}

// Called from CanSetNonVolatileStatus's "target already has a primary status"
// check -- the only place that can actually decide whether this status is
// allowed to override the existing one, since without this, that check fails
// the move before SetNonVolatileStatus (and TryActivateVaultHunterRuin below)
// ever runs at all, making the spec's "can replace/override" branch permanently
// unreachable. This IS the roll for that branch -- on success, the secondary
// effect is applied immediately, since this is the only point that will ever
// get to decide this particular activation.
bool32 TryVaultHunterRuinStatusOverride(u8 attacker, u8 target)
{
    u32 chance = GetRuinActivationChance(attacker, TRUE);
    if (chance == 0 || !RandomPercentage(RNG_NONE, chance))
        return FALSE;

    ApplyRuinSecondaryEffect(attacker, target);
    return TRUE;
}

// Called from SetNonVolatileStatus right after a primary status has actually been
// applied to `target`. `targetHadStatusBefore` must reflect the target's status
// immediately before this infliction, so the caller captures that before calling
// SetNonVolatileStatus's status-setting switch, not after.
//
// Deliberately does NOT roll again when targetHadStatusBefore is TRUE: the only
// way execution reaches here with a prior status is if
// TryVaultHunterRuinStatusOverride already rolled and succeeded -- otherwise
// CanSetNonVolatileStatus would have blocked the move before this function was
// ever called. Rolling a second time here would silently square the real
// activation probability instead of applying the spec's actual percentages.
void TryActivateVaultHunterRuin(u8 attacker, u8 target, bool32 targetHadStatusBefore)
{
    if (targetHadStatusBefore)
        return; // already rolled and applied by TryVaultHunterRuinStatusOverride

    u32 chance = GetRuinActivationChance(attacker, FALSE);
    if (chance == 0 || !RandomPercentage(RNG_NONE, chance))
        return;

    ApplyRuinSecondaryEffect(attacker, target);
}
