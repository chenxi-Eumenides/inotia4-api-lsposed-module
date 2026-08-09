#!/usr/bin/env python3
"""校验 libgame.so 符号 VMA 与 gamebridge.cpp 硬编码常量是否一致。

换游戏版本后运行：检测哪些符号地址变了，输出更新后的常量。
用法：uv run python scripts/analyze/check_symbols.py [libgame.so 路径]
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_SO = ROOT / "apk" / "decoded" / "lib" / "arm64-v8a" / "libgame.so"
HEADER = ROOT / "module" / "app" / "src" / "main" / "cpp" / "game_symbols.h"
READELF = (
    ROOT / "tools" / "ndk" / "android-ndk-r26d" / "toolchains" / "llvm"
    / "prebuilt" / "linux-x86_64" / "bin" / "llvm-readelf"
)

SYMBOL_TO_MACRO = {
    "INVEN_nMoney": "G_MONEY_VMA",
    "MAP_nBaseInfo": "G_MAP_ID_VMA",
    "PARTY_pChar": "G_PARTY_VMA",
    "QUESTSYSTEM_nActiveQuest": "G_ACTIVE_QUEST_VMA",
    "SAVE_nMainMercenarySlot": "G_MAIN_MERC_SLOT_VMA",
    "CHARSYSTEM_pPool": "G_CHAR_POOL_VMA",
    "STATE_nState": "G_STATE_VMA",
    "STATE_nPrevState": "G_PREV_STATE_VMA",
    "GAMESTATE_nState": "G_GAMESTATE_VMA",
    "INITSTATE_nState": "G_INITSTATE_VMA",
    "UIPopupMsg_bOn": "G_POPUP_ON_VMA",
    "UIMainMenu_bDrawFull": "G_MAINMENU_DRAW_VMA",
    "g_arrPopupStack": "G_POPUP_STACK_VMA",
    "INVEN_GetMoney": "F_GET_MONEY_VMA",
    "PARTY_GetMember": "F_GET_MEMBER_VMA",
    "PARTY_GetSize": "F_GET_PARTY_SIZE_VMA",
    "CHAR_GetAttr": "F_GET_ATTR_VMA",
    "CHAR_GetEquipItem": "F_GET_EQUIP_VMA",
    "CHAR_GetExperience": "F_GET_EXP_VMA",
    "CHAR_GetNextExperience": "F_GET_NEXT_EXP_VMA",
    "ITEMSYSTEM_GetRarity": "F_GET_RARITY_VMA",
    "INVEN_GetBagSize": "F_GET_BAG_SIZE_VMA",
    "UTIL_GetBitValue": "F_GET_BIT_VMA",
    "ITEM_GetDamage": "F_GET_DAMAGE_VMA",
    "ITEM_GetDefense": "F_GET_DEFENSE_VMA",
    "CHAR_GetStat": "F_GET_STAT_VMA",
    "CHAR_GetStatusPoint": "F_GET_STATUS_POINT_VMA",
    "CHAR_GetName": "F_GET_NAME_VMA",
    "CHARSYSTEM_FindAsMercenarySlot": "F_FIND_MERC_SLOT_VMA",
    "CHAR_SearchPath": "F_SEARCH_PATH_VMA",
    "INVEN_SetMoney": "F_SET_MONEY_VMA",
    "INVEN_AddMoney": "F_ADD_MONEY_VMA",
    "INVEN_MinusMoney": "F_MINUS_MONEY_VMA",
    "INVEN_RemoveItem": "F_REMOVE_ITEM_VMA",
    "ITEM_GetPrice": "F_ITEM_GET_PRICE_VMA",
    "INVEN_MoveItem": "F_INVEN_MOVE_ITEM_VMA",
    "CHAR_SetExperience": "F_SET_EXP_VMA",
    "CHAR_AddExperience": "F_ADD_EXP_VMA",
    "CHAR_SetStatusPoint": "F_SET_STATUS_POINT_VMA",
    "CHAR_GetStatMain": "F_GET_STAT_MAIN_VMA",
    "CHAR_SetStatMain": "F_SET_STAT_MAIN_VMA",
    "CHAR_InitializeStatus": "F_CHAR_INITIALIZE_STATUS_VMA",
    "CHAR_InitializeSkill": "F_CHAR_INITIALIZE_SKILL_VMA",
    "CHAR_SetActionID": "F_CHAR_SET_ACTION_ID_VMA",
    "CHAR_GetEnemyTarget": "F_CHAR_GET_ENEMY_TARGET_VMA",
    "ITEM_GetBuyPrice": "F_ITEM_GET_BUY_PRICE_VMA",
    "INVEN_FindSaveSlot": "F_INVEN_FIND_SAVE_SLOT_VMA",
    "INVEN_SaveItem": "F_INVEN_SAVE_ITEM_VMA",
    "DEALSYSTEM_FindSaleByID": "F_DEALSYSTEM_FIND_SALE_BY_ID_VMA",
    "UINpc_InitNPC": "F_UINPC_INIT_VMA",
    "UINpc_ExeCurrentNpcTask": "F_UINPC_EXE_CURRENT_TASK_VMA",
    "NPCTASKLIST_MakeDlg": "F_NPCTASKLIST_MAKE_DLG_VMA",
    "PLAYER_DoCheckNearNPC": "F_PLAYER_DO_CHECK_NEAR_NPC_VMA",
    "CHAR_GetSkillUsage": "F_CHAR_GET_SKILL_USAGE_VMA",
    "CHAR_SetSkillUsage": "F_CHAR_SET_SKILL_USAGE_VMA",
    "ITEMSYSTEM_PutJewel": "F_PUT_JEWEL_VMA",
    "ITEMSYSTEM_IsJewel": "F_IS_JEWEL_VMA",
    "CHAR_SetAutoAttack": "F_SET_AUTO_ATTACK_VMA",
    "CHAR_EquipItem": "F_EQUIP_ITEM_VMA",
    "CHAR_UnequipItemToInven": "F_UNEQUIP_VMA",
    "CHAR_CanEquipItem": "F_CAN_EQUIP_VMA",
    "CHAR_FindEquipSlot": "F_FIND_EQUIP_SLOT_VMA",
    "CHAR_GetEquipItem": "F_GET_EQUIP_ITEM_VMA",
    "CHAR_IsSpecialNPC": "F_IS_SPECIAL_NPC_VMA",
    "CHAR_LearnAction": "F_LEARN_ACTION_VMA",
    "PARTY_SetActivePlayer": "F_SET_ACTIVE_PLAYER_VMA",
    "PARTY_Swap": "F_PARTY_SWAP_VMA",
    "CharSetPosition": "F_SET_POSITION_VMA",
    "MAPSYSTEM_ChangeMap": "F_CHANGE_MAP_VMA",
    "CHAR_MoveAsPath": "F_MOVE_AS_PATH_VMA",
    "CHAR_Move": "F_CHAR_MOVE_VMA",
    "CHAR_RemovePath": "F_CHAR_REMOVE_PATH_VMA",
    "CHAR_SetTarget": "F_CHAR_SET_TARGET_VMA",
    "CHAR_MakeDefaultAttack": "F_CHAR_MAKE_DEFAULT_ATTACK_VMA",
    "CHAR_StopCombat": "F_CHAR_STOP_COMBAT_VMA",
    "INVEN_ConsumeItem": "F_CONSUME_ITEM_VMA",
    "INVEN_RemoveItemDirect": "F_REMOVE_ITEM_DIRECT_VMA",
    "MERCENARYSYSTEM_IncludeParty": "F_INCLUDE_PARTY_VMA",
    "MERCENARYSYSTEM_ExcludeParty": "F_EXCLUDE_PARTY_VMA",
    "MERCENARYSYSTEM_Release": "F_MERCENARY_RELEASE_VMA",
    "ITEMDATABASE_IsUse": "F_ITEMDATA_IS_USE_VMA",
}


def read_symbols(so: Path) -> dict[str, int]:
    out = subprocess.run([str(READELF), "-s", str(so)], capture_output=True, text=True).stdout
    syms: dict[str, int] = {}
    for line in out.splitlines():
        m = re.match(r"\s*\d+:\s+([0-9a-fA-F]+)\s+\d+\s+\S+\s+GLOBAL\s+\S+\s+\d+\s+(.+)$", line)
        if m:
            syms[m.group(2).strip()] = int(m.group(1), 16)
    return syms


def read_cpp_constants(header: Path) -> dict[str, int]:
    text = header.read_text()
    consts: dict[str, int] = {}
    for m in re.finditer(r"constexpr uintptr_t ([A-Z0-9_]+)_VMA = (0x[0-9a-fA-F]+)", text):
        consts[m.group(1) + "_VMA"] = int(m.group(2), 16)
    return consts


def main() -> None:
    so = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_SO
    if not so.exists():
        print(f"libgame.so 不存在: {so}")
        sys.exit(1)
    syms = read_symbols(so)
    consts = read_cpp_constants(HEADER)
    print(f"libgame.so: {so}（符号表 {len(syms)} 个）")
    print(f"{'符号':28s} {'cpp 当前':>12s} {'新版本':>12s} {'状态'}")
    changed = 0
    for symbol, macro in SYMBOL_TO_MACRO.items():
        new_addr = syms.get(symbol)
        cur_addr = consts.get(macro)
        if new_addr is None:
            print(f"{symbol:28s} {'缺失':>12s} ❌ 符号不存在")
            continue
        if cur_addr != new_addr:
            changed += 1
            print(f"{symbol:28s} {'0x%x' % cur_addr:>12s} {'0x%x' % new_addr:>12s} ⚠️ 需更新")
        else:
            print(f"{symbol:28s} {'0x%x' % cur_addr:>12s} {'0x%x' % new_addr:>12s} ✅ 一致")
    if changed:
        print(f"\n⚠️ {changed} 个符号地址变化：用 sed 更新 game_symbols.h 中对应 _VMA 常量后重新构建")


if __name__ == "__main__":
    main()
