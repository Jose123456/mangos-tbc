/*
* See AUTHORS file for Copyright information
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "CFBG.h"
#include "World.h"
#include "Custom.h"
#include "ObjectMgr.h"
#include "DBCStores.h"
#include "Player.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundMgr.h"

CFBG::CFBG(Player* pPlayer)
{
    m_player = pPlayer;

    m_fRace = 0;
    m_oRace = 0;
    m_fFaction = 0;
    m_oFaction = 0;
    m_oPlayerBytes = 0;
    m_oPlayerBytes2 = 0;
    m_fPlayerBytes = 0;
    m_fPlayerBytes2 = 0;
    m_FakeOnNextTick = 0;
}

bool CFBG::NativeTeam() const
{
    return m_player->GetTeam() == m_player->GetOTeam();
}

void CFBG::SetFakeValues()
{
    m_oRace = m_player->GetByteValue(UNIT_FIELD_BYTES_0, 0);
    m_oFaction = m_player->GetUInt32Value(UNIT_FIELD_FACTIONTEMPLATE);

    m_fRace = sCustom.PickFakeRace(m_player->getClass(), m_player->GetOTeam());
    m_fFaction = Player::getFactionForRace(m_fRace);

    m_oPlayerBytes = m_player->GetUInt32Value(PLAYER_BYTES);
    m_oPlayerBytes2 = m_player->GetUInt32Value(PLAYER_BYTES_2);
    m_fPlayerBytes = sCustom.GetFakePlayerBytes(m_fRace, m_player->getGender());
    m_fPlayerBytes2 = sCustom.GetFakePlayerBytes2(m_fRace, m_player->getGender());

    if (!m_fPlayerBytes)
        m_fPlayerBytes = m_oPlayerBytes;

    if (!m_fPlayerBytes2)
        m_fPlayerBytes2 = m_oPlayerBytes2;
}


void CFBG::RecachePlayersFromBG()
{
    BattleGround* bg = m_player->GetBattleGround();

    if (!bg)
        return;

    for (auto& itr : bg->GetPlayers())
    {
        if (Player* player = sObjectMgr.GetPlayer(itr.first))
        {
            if (!player->GetCFBG()->NativeTeam())
            {
                // Erase bg players from source player cache
                WorldPacket data(SMSG_INVALIDATE_PLAYER, 8);
                data << player->GetObjectGuid();
                m_player->GetSession()->SendPacket(&data);
                // Send bg player data to source player
                data = player->GetCFBG()->BuildNameQuery();
                m_player->GetSession()->SendPacket(&data);
            }

            if (!NativeTeam())
            {
                // Erase source player from bg players cache
                WorldPacket data(SMSG_INVALIDATE_PLAYER, 8);
                data << m_player->GetObjectGuid();
                player->GetSession()->SendPacket(&data);
                // Send new source data to bg players
                data = BuildNameQuery();
                player->GetSession()->SendPacket(&data);
            }
        }
        else
        {
            // Couldn't find bgplayer, recache him for source player in case he logs in again.
            WorldPacket data(SMSG_INVALIDATE_PLAYER, 8);
            data << itr.first;
            m_player->GetSession()->SendPacket(&data);
        }
    }
}

void CFBG::RecachePlayersFromList()
{
    for (auto& itr : m_FakedPlayers)
    {
        WorldPacket data(SMSG_INVALIDATE_PLAYER, 8);
        data << itr;
        m_player->GetSession()->SendPacket(&data);

        if (Player* player = sObjectMgr.GetPlayer(itr))
        {
            WorldPacket data = player->GetCFBG()->BuildNameQuery();
            m_player->GetSession()->SendPacket(&data);
        }
    }

    m_FakedPlayers.clear();
}

WorldPacket CFBG::BuildNameQuery()
{
    WorldPacket data(SMSG_NAME_QUERY_RESPONSE, (8 + 1 + 4 + 4 + 4 + 10));
    data << m_player->GetObjectGuid();      // player guid
    data << m_player->GetName();            // player name
    data << uint8(0);                       // realm name
    data << uint32(m_player->getRace());    // player race
    data << uint32(m_player->getGender());  // player gender
    data << uint32(m_player->getClass());   // player class
    data << uint8(0);                       // is not declined

    return data;
}

void CFBG::FakeDisplayID()
{
    if (NativeTeam())
        return;

    PlayerInfo const* info = sObjectMgr.GetPlayerInfo(m_player->getRace(), m_player->getClass());
    if (!info)
    {
        for (auto i = 0; i < MAX_CLASSES; ++i)
        {
            info = sObjectMgr.GetPlayerInfo(m_player->getRace(), i);
            if (info)
                break;
        }
    }

    if (!info)
    {
        sLog.outError("Player %u has incorrect race/class pair. Can't init display ids.", m_player->GetGUIDLow());
        return;
    }

    // reset scale before reapply auras
    m_player->SetObjectScale(DEFAULT_OBJECT_SCALE);

    uint8 gender = m_player->getGender();
    switch (gender)
    {
    case GENDER_FEMALE:
        m_player->SetDisplayId(info->displayId_f);
        m_player->SetNativeDisplayId(info->displayId_f);
        break;
    case GENDER_MALE:
        m_player->SetDisplayId(info->displayId_m);
        m_player->SetNativeDisplayId(info->displayId_m);
        break;
    default:
        sLog.outError("Invalid gender %u for player", gender);
        return;
    }

    m_player->SetUInt32Value(PLAYER_BYTES, getFPlayerBytes());
    m_player->SetUInt32Value(PLAYER_BYTES_2, getFPlayerBytes2());
}

void CFBG::JoinBattleGround(BattleGround* bg)
{
    if (bg->isArena())
        return;

    if (!NativeTeam())
    {
        m_FakedPlayers.push_back(m_player->GetObjectGuid());
        m_player->SetByteValue(UNIT_FIELD_BYTES_0, 0, getFRace());
        m_player->setFaction(getFFaction());
    }

    SetRecache();
    FakeDisplayID();
}

void CFBG::LeaveBattleGround(BattleGround* bg)
{
    if (bg->isArena())
        return;

    m_player->SetByteValue(UNIT_FIELD_BYTES_0, 0, getORace());
    m_player->setFaction(getOFaction());
    m_player->InitDisplayIds();

    SetFakedPlayers(m_FakedPlayers);
    SetRecache();

    m_player->SetUInt32Value(PLAYER_BYTES, getOPlayerBytes());
    m_player->SetUInt32Value(PLAYER_BYTES_2, getOPlayerBytes2());
}

bool CFBG::SendBattleGroundChat(ChatMsg msgtype, std::string message)
{
    // Select distance to broadcast to.
    float distance = sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_SAY);

    if (msgtype == CHAT_MSG_YELL)
        sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_YELL);
    else if (msgtype == CHAT_MSG_EMOTE)
        sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE);

    BattleGround* pBattleGround = m_player->GetBattleGround();

    if (!pBattleGround || pBattleGround->isArena()) // Only fake chat in BG's. CFBG should not interfere with arenas.
        return false;

    for (auto& itr : pBattleGround->GetPlayers())
    {
        if (Player* pPlayer = sObjectMgr.GetPlayer(itr.first))
        {
            if (m_player->GetDistance2d(pPlayer->GetPositionX(), pPlayer->GetPositionY()) <= distance)
            {
                WorldPacket data(SMSG_MESSAGECHAT, 200);


                if (m_player->GetTeam() == pPlayer->GetTeam())
                    ChatHandler::BuildChatPacket(data, msgtype, message.c_str(), LANG_UNIVERSAL, m_player->GetChatTag(), m_player->GetObjectGuid(), m_player->GetName());
                else if (msgtype != CHAT_MSG_EMOTE)
                    ChatHandler::BuildChatPacket(data, msgtype, message.c_str(), pPlayer->GetOTeam() == ALLIANCE ? LANG_ORCISH : LANG_COMMON, m_player->GetChatTag(), m_player->GetObjectGuid(), m_player->GetName());

                pPlayer->GetSession()->SendPacket(&data);
            }
        }
    }
    return true;
}

void CFBG::RewardReputationToXBGTeam(BattleGround* pBG, uint32 faction_ally, uint32 faction_horde, uint32 gain, Team teamId)
{
    FactionEntry const* a_factionEntry = sFactionStore.LookupEntry(faction_ally);
    FactionEntry const* h_factionEntry = sFactionStore.LookupEntry(faction_horde);

    if (!a_factionEntry || !h_factionEntry)
        return;

    for (auto& itr : pBG->GetPlayers())
    {
        if (itr.second.OfflineRemoveTime)
            continue;

        Player* plr = sObjectMgr.GetPlayer(itr.first);

        if (!plr)
        {
            sLog.outError("BattleGround:RewardReputationToTeam: %s not found!", itr.first.GetString().c_str());
            continue;
        }

        if (plr->GetTeam() == teamId) // Check if player is playing in the team that capped and then reward by original team.
            plr->GetReputationMgr().ModifyReputation(plr->GetOTeam() == ALLIANCE ? a_factionEntry : h_factionEntry, gain);
    }
}

bool BattleGroundQueue::CheckMixedMatch(BattleGround* bg_template, BattleGroundBracketId bracket_id, uint32 minPlayers, uint32 maxPlayers)
{
    if (!sWorld.getConfig(CONFIG_BOOL_CFBG_ENABLED) || !bg_template->isBattleGround())
        return false;

    // Empty selection pool, we do not want old data.
    m_SelectionPools[BG_TEAM_ALLIANCE].Init();
    m_SelectionPools[BG_TEAM_HORDE].Init();

    uint32 addedally = 0;
    uint32 addedhorde = 0;

    for (auto& itr : m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_ALLIANCE])
    {
        GroupQueueInfo* ginfo = itr;
        if (!ginfo->IsInvitedToBGInstanceGUID)
        {
            bool makeally = addedally < addedhorde;

            if (addedally == addedhorde)
                makeally = urand(0, 1);

            ginfo->GroupTeam = makeally ? ALLIANCE : HORDE;

            if (m_SelectionPools[makeally ? BG_TEAM_ALLIANCE : BG_TEAM_HORDE].AddGroup(itr, maxPlayers))
                makeally ? addedally += ginfo->Players.size() : addedhorde += ginfo->Players.size();
            else
                break;

            if (m_SelectionPools[BG_TEAM_ALLIANCE].GetPlayerCount() >= minPlayers &&
                m_SelectionPools[BG_TEAM_HORDE].GetPlayerCount() >= minPlayers)
                return true;
        }
    }

    if (sBattleGroundMgr.isTesting() &&
        (m_SelectionPools[BG_TEAM_ALLIANCE].GetPlayerCount() ||
        m_SelectionPools[BG_TEAM_HORDE].GetPlayerCount()))
        return true;

    return false;
}

bool BattleGroundQueue::MixPlayersToBG(BattleGround* bg, BattleGroundBracketId bracket_id)
{
    if (!sWorld.getConfig(CONFIG_BOOL_CFBG_ENABLED) || bg->isArena())
        return false;

    int32 allyFree = bg->GetFreeSlotsForTeam(ALLIANCE);
    int32 hordeFree = bg->GetFreeSlotsForTeam(HORDE);

    uint32 addedally = bg->GetMaxPlayersPerTeam() - bg->GetFreeSlotsForTeam(ALLIANCE);
    uint32 addedhorde = bg->GetMaxPlayersPerTeam() - bg->GetFreeSlotsForTeam(HORDE);

    for (auto& itr : m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_ALLIANCE])
    {
        GroupQueueInfo* ginfo = itr;
        if (!ginfo->IsInvitedToBGInstanceGUID)
        {
            bool makeally = addedally < addedhorde;

            if (addedally == addedhorde)
                makeally = urand(0, 1);

            makeally ? ++addedally : ++addedhorde;

            ginfo->GroupTeam = makeally ? ALLIANCE : HORDE;

            if (m_SelectionPools[makeally ? BG_TEAM_ALLIANCE : BG_TEAM_HORDE].AddGroup(ginfo, makeally ? allyFree : hordeFree))
                makeally ? addedally += ginfo->Players.size() : addedhorde += ginfo->Players.size();
            else
                break;
        }
    }

    return true;
}