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

#include "Settings.h"
#include "Log.h"

Settings::Settings(Player* pPlayer)
{
    m_player = pPlayer;
}

void Settings::LoadSettings()
{
    QueryResult* result = CharacterDatabase.PQuery("SELECT DataTypeId, FloatSetting, IntSetting, UintSetting, StringSetting, SettingNumber FROM player_settings WHERE guid = %u", m_player->GetGUIDLow());
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            DataTypeId rDataTypeId = DataTypeId(fields[0].GetUInt32());
            uint32 SettingNumber = fields[5].GetUInt32();

            switch (rDataTypeId)
            {
            case SETTING_FLOAT:
                m_FloatContainer.insert(std::make_pair(FloatSettings(SettingNumber), fields[rDataTypeId].GetFloat()));
                break;
            case SETTING_INT:
                m_IntContainer.insert(std::make_pair(IntSettings(SettingNumber), fields[rDataTypeId].GetInt32()));
                break;
            case SETTING_UINT:
                m_UintContainer.insert(std::make_pair(UintSettings(SettingNumber), fields[rDataTypeId].GetUInt32()));
                break;
            case SETTING_STRING:
                m_StringContainer.insert(std::make_pair(StringSettings(SettingNumber), fields[rDataTypeId].GetCppString()));
                break;
            default:
                sLog.outError("Invalid DataTypeId %u for player %u", uint32(rDataTypeId), m_player->GetGUIDLow());
                break;
            }
        }
        while (result->NextRow());

        delete result;
    }
}

void Settings::SaveSettings()
{
    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("DELETE FROM player_settings WHERE guid = %u", m_player->GetGUIDLow());

    for (auto& itr : m_FloatContainer)
        CharacterDatabase.PExecute("INSERT INTO player_settings (guid, SettingNumber, DataTypeID, FloatSetting) VALUES (%u, %u, %u, %f)", m_player->GetGUIDLow(), itr.first, SETTING_FLOAT, itr.second);

    for (auto& itr : m_IntContainer)
        CharacterDatabase.PExecute("INSERT INTO player_settings (guid, SettingNumber, DataTypeID, IntSetting) VALUES (%u, %u, %u, %i)", m_player->GetGUIDLow(), itr.first, SETTING_INT, itr.second);

    for (auto& itr : m_UintContainer)
        CharacterDatabase.PExecute("INSERT INTO player_settings (guid, SettingNumber, DataTypeID, UintSetting) VALUES (%u, %u, %u, %u)", m_player->GetGUIDLow(), itr.first, SETTING_UINT, itr.second);

    for (auto& itr : m_StringContainer)
        CharacterDatabase.PExecute("INSERT INTO player_settings (guid, SettingNumber, DataTypeID, StringSetting) VALUES (%u, %u, %u, '%s')", m_player->GetGUIDLow(), itr.first, SETTING_STRING, itr.second.c_str());

    CharacterDatabase.CommitTransaction();
}