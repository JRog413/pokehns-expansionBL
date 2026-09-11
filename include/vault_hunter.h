#ifndef GUARD_VAULT_HUNTER_H
#define GUARD_VAULT_HUNTER_H

// There are exactly two playable Vault Hunters. VAULT_HUNTER_ZERO maps to the
// existing MALE gender choice, VAULT_HUNTER_MAYA to FEMALE -- the existing
// gender-selection screen doubles as Vault Hunter selection (see the
// gSaveBlock2Ptr->vaultHunterId assignments in oak_speech_hns.c), so no new
// selection screen was needed.
enum VaultHunterId
{
    VAULT_HUNTER_ZERO,
    VAULT_HUNTER_MAYA,
};

// Each Vault Hunter has exactly 3 passives. Slot meaning depends on which Vault
// Hunter is active (e.g. slot 0 is "One Shot, One Kill" for Zer0, "Siphon" for
// Maya) -- see the sVaultHunterPassives data table in vault_hunter.c.
enum VaultHunterPassiveSlot
{
    VH_PASSIVE_SLOT_0,
    VH_PASSIVE_SLOT_1,
    VH_PASSIVE_SLOT_2,
    VAULT_HUNTER_PASSIVE_COUNT,
};
#define PASSIVE_SLOT_NONE 0xFF

// Named per-passive slot constants -- "slot 0" is ambiguous across two different
// Vault Hunters, so battle-effect code should reference these instead.
#define ZERO_PASSIVE_ONE_SHOT_ONE_KILL VH_PASSIVE_SLOT_0 // crit chance/damage
#define ZERO_PASSIVE_OUTMANEUVER       VH_PASSIVE_SLOT_1 // evasion, move-first-on-dodge
#define ZERO_PASSIVE_OVERKILL          VH_PASSIVE_SLOT_2 // damage vs low-HP targets
#define MAYA_PASSIVE_SOUL_HARVEST      VH_PASSIVE_SLOT_0 // KO-triggered healing
#define MAYA_PASSIVE_RUIN              VH_PASSIVE_SLOT_1 // status-triggered secondary effect
#define MAYA_PASSIVE_PHASE_SHIELD      VH_PASSIVE_SLOT_2 // damage reduction above 50% HP

#define VAULT_HUNTER_PASSIVE_TIER_MAX 3

// Public API
enum VaultHunterId GetVaultHunterId(void);
bool32 IsVaultHunterPassiveUnlocked(u8 slot);
u8 GetVaultHunterPassiveTier(void); // 0 = nothing unlocked yet, else the current global tier
u8 GetActiveVaultHunterPassiveSlot(void); // PASSIVE_SLOT_NONE if nothing active yet
bool32 SetActiveVaultHunterPassiveSlot(u8 slot); // fails (returns FALSE) if that slot isn't unlocked
bool32 IsVaultHunterPassiveActive(u8 slot); // convenience: unlocked AND currently selected
bool32 IsVaultHunterPassiveActiveForBattler(u8 battler, u8 slot);

// Ruin (Maya): called from SetNonVolatileStatus right after a primary status is
// actually applied. See its own doc comment in vault_hunter.c for the exact
// contract around targetHadStatusBefore.
bool32 TryVaultHunterRuinStatusOverride(u8 attacker, u8 target);
void TryActivateVaultHunterRuin(u8 attacker, u8 target, bool32 targetHadStatusBefore);
void UnlockVaultHunterPassive(u8 slot);
void SetVaultHunterPassiveTier(u8 tier);

// Badge 1 and Badge 3 owe the player a passive-unlock choice (which of the locked
// slots to unlock) rather than unlocking one automatically; this tracks how many
// such choices are still owed but not yet made (0, 1, or 2). Badge 5's unlock isn't
// tracked here since only one locked slot remains by then -- no real choice to make.
u8 GetPendingVaultHunterPassiveUnlocks(void);
// Called once the player has picked which locked slot to unlock (e.g. at a Pokemon
// Center); unlocks that slot and reduces the pending count by one. Does nothing if
// there wasn't actually a pending choice, or if the requested slot is already
// unlocked.
void ChooseVaultHunterPassiveUnlock(u8 slot);

// Badge-progression hook. Called after a badge is obtained; internally checks
// whether that badge number matters for passive progression (1, 3, 4, 5, 8) and
// does nothing otherwise.
void TryAdvanceVaultHunterPassivesForBadge(u8 badgeNumber);

// Script-facing (callnative) helpers for the Pokemon Center flow. All read the
// slot argument from VAR_0x8005 (not VAR_0x8004 -- that one's already used by the
// nurse script's own Gold Card logic), set by the calling script beforehand.
struct ScriptContext;
void Script_HasVaultHunterPassiveBusiness(struct ScriptContext *ctx);
void Script_HasPendingVaultHunterPassiveChoice(struct ScriptContext *ctx);
void Script_IsVaultHunterPassiveSlotLocked(struct ScriptContext *ctx);
void Script_IsVaultHunterPassiveSlotUnlocked(struct ScriptContext *ctx);
void Script_IsVaultHunterPassiveSlotActive(struct ScriptContext *ctx);
void Script_BufferVaultHunterPassiveName(struct ScriptContext *ctx);
void Script_ChooseVaultHunterPassiveUnlock(struct ScriptContext *ctx);
void Script_SetActiveVaultHunterPassiveSlot(struct ScriptContext *ctx);

#endif // GUARD_VAULT_HUNTER_H
