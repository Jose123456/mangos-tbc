#include "AntiCheat_waterwalking.h"
#include "CPlayer.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellMgr.h"

AntiCheat_waterwalking::AntiCheat_waterwalking(CPlayer* player) : AntiCheat(player)
{
}

bool AntiCheat_waterwalking::HandleMovement(MovementInfo& moveInfo, Opcodes opcode)
{
    m_MoveInfo[0] = moveInfo; // moveInfo shouldn't be used anymore then assigning it in the beginning.

    if (!Initialized())
    {
        m_MoveInfo[1] = m_MoveInfo[0];
        return false;
    }

    if (!m_Player->HasAuraType(SPELL_AURA_WATER_WALK) && !m_Player->HasAuraType(SPELL_AURA_GHOST) && m_MoveInfo[0].HasMovementFlag(MOVEFLAG_WATERWALKING))
    {
        // Idk how to make emohacker stop waterwalking without adding and removing the aura.
        m_Player->AddAura(546);
        m_Player->RemoveAurasDueToSpell(546);

        m_Player->BoxChat << "WATERWALK CHEAT" << "\n";
    }

    return false;
}
