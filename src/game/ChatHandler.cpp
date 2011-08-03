/*
 * Copyright (C) 2005-2011 MaNGOS <http://getmangos.com/>
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

#include "Common.h"
#include "Log.h"
#include "ChatLog.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "Opcodes.h"
#include "ObjectMgr.h"
#include "Chat.h"
#include "Database/DatabaseEnv.h"
#include "ChannelMgr.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Player.h"
#include "SpellAuras.h"
#include "Language.h"
#include "Util.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"

// FG
#include "VirtualPlayerMgr.h"

// Playerbot mod
#include "playerbot/PlayerbotAI.h"


bool WorldSession::processChatmessageFurtherAfterSecurityChecks(std::string& msg, uint32 lang)
{
    if (lang != LANG_ADDON)
    {
        // strip invisible characters for non-addon messages
        if(sWorld.getConfig(CONFIG_BOOL_CHAT_FAKE_MESSAGE_PREVENTING))
            stripLineInvisibleChars(msg);

        if (sWorld.getConfig(CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_SEVERITY) && GetSecurity() < SEC_MODERATOR
                && !ChatHandler(this).isValidChatMessage(msg.c_str()))
        {
            sLog.outError("Player %s (GUID: %u) sent a chatmessage with an invalid link: %s", GetPlayer()->GetName(),
                    GetPlayer()->GetGUIDLow(), msg.c_str());
            if (sWorld.getConfig(CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_KICK))
                KickPlayer();
            return false;
        }
    }

    return true;
}

void WorldSession::HandleMessagechatOpcode( WorldPacket & recv_data )
{
    uint32 type;
    uint32 lang;

    recv_data >> type;
    recv_data >> lang;

    if(type >= MAX_CHAT_MSG_TYPE)
    {
        sLog.outError("CHAT: Wrong message type received: %u", type);
        return;
    }

    DEBUG_LOG("CHAT: packet received. type %u, lang %u", type, lang );

    // prevent talking at unknown language (cheating)
    LanguageDesc const* langDesc = GetLanguageDescByID(lang);
    if(!langDesc)
    {
        SendNotification(LANG_UNKNOWN_LANGUAGE);
        return;
    }
    if(langDesc->skill_id != 0 && !_player->HasSkill(langDesc->skill_id))
    {
        // also check SPELL_AURA_COMPREHEND_LANGUAGE (client offers option to speak in that language)
        Unit::AuraList const& langAuras = _player->GetAurasByType(SPELL_AURA_COMPREHEND_LANGUAGE);
        bool foundAura = false;
        for(Unit::AuraList::const_iterator i = langAuras.begin(); i != langAuras.end(); ++i)
        {
            if((*i)->GetModifier()->m_miscvalue == int32(lang))
            {
                foundAura = true;
                break;
            }
        }
        if(!foundAura)
        {
            SendNotification(LANG_NOT_LEARNED_LANGUAGE);
            return;
        }
    }

    if(lang == LANG_ADDON)
    {
        // Disabled addon channel?
        if(!sWorld.getConfig(CONFIG_BOOL_ADDON_CHANNEL))
            return;
    }
    // LANG_ADDON should not be changed nor be affected by flood control
    else
    {
        // send in universal language if player in .gmon mode (ignore spell effects)
        if (_player->isGameMaster())
            lang = LANG_UNIVERSAL;
        else
        {
            // send in universal language in two side iteration allowed mode
            if (sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT))
                lang = LANG_UNIVERSAL;
            else
            {
                switch(type)
                {
                    case CHAT_MSG_PARTY:
                    case CHAT_MSG_RAID:
                    case CHAT_MSG_RAID_LEADER:
                    case CHAT_MSG_RAID_WARNING:
                        // allow two side chat at group channel if two side group allowed
                        if(sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GROUP))
                            lang = LANG_UNIVERSAL;
                        break;
                    case CHAT_MSG_GUILD:
                    case CHAT_MSG_OFFICER:
                        // allow two side chat at guild channel if two side guild allowed
                        if(sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD))
                            lang = LANG_UNIVERSAL;
                        break;
                }
            }

            // but overwrite it by SPELL_AURA_MOD_LANGUAGE auras (only single case used)
            Unit::AuraList const& ModLangAuras = _player->GetAurasByType(SPELL_AURA_MOD_LANGUAGE);
            if(!ModLangAuras.empty())
                lang = ModLangAuras.front()->GetModifier()->m_miscvalue;
        }

        if (type != CHAT_MSG_AFK && type != CHAT_MSG_DND)
        {
            if (!_player->CanSpeak())
            {
                std::string timeStr = secsToTimeString(m_muteTime - time(NULL));
                SendNotification(GetMangosString(LANG_WAIT_BEFORE_SPEAKING), timeStr.c_str());
                return;
            }

            GetPlayer()->UpdateSpeakTime();
        }
    }

    // FG: universal languages for some chat types
    if(lang != LANG_ADDON)
    {
        switch(type)
        {
        case CHAT_MSG_PARTY:
        case CHAT_MSG_RAID:
        case CHAT_MSG_RAID_LEADER:
        case CHAT_MSG_RAID_WARNING:
            lang = 0;
        }
    }



    switch(type)
    {
        case CHAT_MSG_SAY:
        case CHAT_MSG_EMOTE:
        case CHAT_MSG_YELL:
        {
            std::string msg;
            recv_data >> msg;

            if(msg.empty())
                break;

            if (ChatHandler(this).ParseCommands(msg.c_str()))
                break;

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
                return;

            if(msg.empty())
                break;

            if(type == CHAT_MSG_SAY)
            {
                sChatLog.ChatMsg(GetPlayer(), msg, type);
                GetPlayer()->Say(msg, lang);
            }
            else if(type == CHAT_MSG_EMOTE)
            {
                sChatLog.ChatMsg(GetPlayer(), msg, type);
                GetPlayer()->TextEmote(msg);
            }
            else if(type == CHAT_MSG_YELL)
            {
                sChatLog.ChatMsg(GetPlayer(), msg, type);
                GetPlayer()->Yell(msg, lang);
            }
        } break;

        case CHAT_MSG_WHISPER:
        {
            std::string to, msg;
            recv_data >> to;
            recv_data >> msg;

            if (ChatHandler(this).ParseCommands(msg.c_str()))
                break;

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
                return;

            if(msg.empty())
                break;

            sChatLog.WhisperMsg(GetPlayer(), to, msg);

            if(!normalizePlayerName(to))
            {
                SendPlayerNotFoundNotice(to);
                break;
            }

            // FG: virtual players can be whispered to, but wont answer
            if(VirtualPlayer *vp = sVPlayerMgr.GetChar(to))
            {
                uint32 t = Player::TeamForRace(vp->race);
                if(!vp->online || (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT) && GetPlayer()->GetTeam() != t) )
                {
                    WorldPacket data(SMSG_CHAT_PLAYER_NOT_FOUND, (to.size()+1));
                    data<<to;
                    SendPacket(&data);
                    return;
                }

                // simulate received whisper. (To: <Name>: blah blah   at sender client)
                WorldPacket rdata(SMSG_MESSAGECHAT, 200);
                rdata << (uint8)CHAT_MSG_WHISPER_INFORM;
                rdata << (uint32)lang;
                rdata << (uint64)vp->guid.GetRawValue();
                rdata << (uint32)lang;
                rdata << (uint64)vp->guid.GetRawValue();
                rdata << (uint32)(msg.length()+1);
                rdata << msg;
                rdata << (uint8)0;
                SendPacket(&rdata);
                return;
            }

            Player *player = sObjectMgr.GetPlayer(to.c_str());
            if (!player)
            {
                SendPlayerNotFoundNotice(to);
                return;
            }

            uint32 mySecurity = GetSecurity();
            uint32 pSecurity = player->GetSession()->GetSecurity();
            if (!player->isAcceptWhispers() && mySecurity == SEC_PLAYER)
            {
                SendPlayerNotFoundNotice(to);
                return;
            }

            if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT) && mySecurity < SEC_MODERATOR && pSecurity < SEC_MODERATOR )
            {
                if (GetPlayer()->GetTeam() != player->GetTeam())
                {
                    SendWrongFactionNotice();
                    return;
                }
            }

            // Playerbot mod: handle whispered command to bot
            if (player->GetPlayerbotAI())
            {
                player->GetPlayerbotAI()->HandleCommand(msg, *GetPlayer());
                GetPlayer()->m_speakTime = 0;
                GetPlayer()->m_speakCount = 0;
            }
            else
                GetPlayer()->Whisper(msg, lang, player->GetObjectGuid());
        } break;

        case CHAT_MSG_PARTY:
        case CHAT_MSG_PARTY_LEADER:
        {
            std::string msg;
            recv_data >> msg;

            if(msg.empty())
                break;

            if (ChatHandler(this).ParseCommands(msg.c_str()))
                break;

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
                return;

            if(msg.empty())
                break;

            sChatLog.PartyMsg(GetPlayer(), msg);

            // if player is in battleground, he cannot say to battleground members by /p
            Group *group = GetPlayer()->GetOriginalGroup();
            if(!group)
            {
                group = _player->GetGroup();
                if(!group || group->isBGGroup())
                    return;
            }

            if ((type == CHAT_MSG_PARTY_LEADER) && !group->IsLeader(_player->GetObjectGuid()))
                return;

            // Playerbot mod: broadcast message to bot members
            for(GroupReference* itr = group->GetFirstMember(); itr != NULL; itr=itr->next())
            {
                Player* player = itr->getSource();
                if (player && player->GetPlayerbotAI())
                {
                    player->GetPlayerbotAI()->HandleCommand(msg, *GetPlayer());
                    GetPlayer()->m_speakTime = 0;
                    GetPlayer()->m_speakCount = 0;
                }
            }
            // END Playerbot mod

            WorldPacket data;
            ChatHandler::FillMessageData(&data, this, type, lang, msg.c_str());
            group->BroadcastPacket(&data, false, group->GetMemberGroup(GetPlayer()->GetObjectGuid()));
        } break;

        case CHAT_MSG_GUILD:
        {
            std::string msg;
            recv_data >> msg;

            if(msg.empty())
                break;

            if (ChatHandler(this).ParseCommands(msg.c_str()))
                break;

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
                return;

            if(msg.empty())
                break;

            sChatLog.GuildMsg(GetPlayer(), msg, false);

            if (GetPlayer()->GetGuildId())
                if (Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId()))
                    guild->BroadcastToGuild(this, msg, lang == LANG_ADDON ? LANG_ADDON : LANG_UNIVERSAL);
        } break;

        case CHAT_MSG_OFFICER:
        {
            std::string msg;
            recv_data >> msg;

            if(msg.empty())
                break;

            if (ChatHandler(this).ParseCommands(msg.c_str()))
                break;

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
                return;

            if(msg.empty())
                break;

            sChatLog.GuildMsg(GetPlayer(), msg, true);

            if (GetPlayer()->GetGuildId())
                if (Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId()))
                    guild->BroadcastToOfficers(this, msg, lang == LANG_ADDON ? LANG_ADDON : LANG_UNIVERSAL);
        } break;

        case CHAT_MSG_RAID:
        {
            std::string msg;
            recv_data >> msg;

            if(msg.empty())
                break;

            if (ChatHandler(this).ParseCommands(msg.c_str()))
                break;

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
                return;

            if(msg.empty())
                break;

            sChatLog.RaidMsg(GetPlayer(), msg, type);

            // if player is in battleground, he cannot say to battleground members by /ra
            Group *group = GetPlayer()->GetOriginalGroup();
            if(!group)
            {
                group = GetPlayer()->GetGroup();
                if(!group || group->isBGGroup() || !group->isRaidGroup())
                    return;
            }

            WorldPacket data;
            ChatHandler::FillMessageData(&data, this, CHAT_MSG_RAID, lang, msg.c_str());
            group->BroadcastPacket(&data, false);
        } break;
        case CHAT_MSG_RAID_LEADER:
        {
            std::string msg;
            recv_data >> msg;

            if(msg.empty())
                break;

            if (ChatHandler(this).ParseCommands(msg.c_str()))
                break;

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
                return;

            if(msg.empty())
                break;

            sChatLog.RaidMsg(GetPlayer(), msg, type);

            // if player is in battleground, he cannot say to battleground members by /ra
            Group *group = GetPlayer()->GetOriginalGroup();
            if (!group)
            {
                group = GetPlayer()->GetGroup();
                if (!group || group->isBGGroup() || !group->isRaidGroup() || !group->IsLeader(_player->GetObjectGuid()))
                    return;
            }

            WorldPacket data;
            ChatHandler::FillMessageData(&data, this, CHAT_MSG_RAID_LEADER, lang, msg.c_str());
            group->BroadcastPacket(&data, false);
        } break;

        case CHAT_MSG_RAID_WARNING:
        {
            std::string msg;
            recv_data >> msg;

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
                return;

            if(msg.empty())
                break;

            sChatLog.RaidMsg(GetPlayer(), msg, type);

            Group *group = GetPlayer()->GetGroup();
            if (!group || !group->isRaidGroup() ||
                !(group->IsLeader(GetPlayer()->GetObjectGuid()) || group->IsAssistant(GetPlayer()->GetObjectGuid())))
                return;

            WorldPacket data;
            //in battleground, raid warning is sent only to players in battleground - code is ok
            ChatHandler::FillMessageData(&data, this, CHAT_MSG_RAID_WARNING, lang, msg.c_str());
            group->BroadcastPacket(&data, false);
        } break;

        case CHAT_MSG_BATTLEGROUND:
        {
            std::string msg;
            recv_data >> msg;

            if (ChatHandler(this).ParseCommands(msg.c_str()))
                break;

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
                return;

            if(msg.empty())
                break;

            sChatLog.BattleGroundMsg(GetPlayer(), msg, type);

            // battleground raid is always in Player->GetGroup(), never in GetOriginalGroup()
            Group *group = GetPlayer()->GetGroup();
            if(!group || !group->isBGGroup())
                return;

            WorldPacket data;
            ChatHandler::FillMessageData(&data, this, CHAT_MSG_BATTLEGROUND, lang, msg.c_str());
            group->BroadcastPacket(&data, false);
        } break;

        case CHAT_MSG_BATTLEGROUND_LEADER:
        {
            std::string msg;
            recv_data >> msg;

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
                return;

            if(msg.empty())
                break;

            sChatLog.BattleGroundMsg(GetPlayer(), msg, type);

            // battleground raid is always in Player->GetGroup(), never in GetOriginalGroup()
            Group *group = GetPlayer()->GetGroup();
            if (!group || !group->isBGGroup() || !group->IsLeader(GetPlayer()->GetObjectGuid()))
                return;

            WorldPacket data;
            ChatHandler::FillMessageData(&data, this, CHAT_MSG_BATTLEGROUND_LEADER, lang, msg.c_str());
            group->BroadcastPacket(&data, false);
        } break;

        case CHAT_MSG_CHANNEL:
        {
            std::string channel, msg;
            recv_data >> channel;
            recv_data >> msg;

            if (ChatHandler(this).ParseCommands(msg.c_str()))
                break;

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
                return;

            if(msg.empty())
                break;

            sChatLog.ChannelMsg(GetPlayer(), channel, msg);

            if(ChannelMgr* cMgr = channelMgr(_player->GetTeam()))
                if(Channel *chn = cMgr->GetChannel(channel, _player))
                {
                    // FG: check for channel mute addition
                    if((chn->IsGeneral() || chn->IsSpecial()) && !_player->CanSpeakInGlobalChannel())
                    {
                        std::string timeStr = secsToTimeString(m_channelMuteTime - time(NULL));
                        SendNotification(GetMangosString(LANG_WAIT_BEFORE_SPEAKING_IN_CHANNEL), timeStr.c_str());
                        return;
                    }

                    chn->Say(_player->GetObjectGuid(), msg.c_str(), lang);
                }
        } break;

        case CHAT_MSG_AFK:
        {
            std::string msg;
            recv_data >> msg;

            if (!_player->isInCombat())
            {
                if (!msg.empty() || !_player->isAFK())
                {
                    if (msg.empty())
                        _player->afkMsg = GetMangosString(LANG_PLAYER_AFK_DEFAULT);
                    else
                        _player->afkMsg = msg;
                }
                if (msg.empty() || !_player->isAFK())
                {
                    _player->ToggleAFK();
                    if (_player->isAFK() && _player->isDND())
                        _player->ToggleDND();
                }
            }
        } break;

        case CHAT_MSG_DND:
        {
            std::string msg;
            recv_data >> msg;

            if (!msg.empty() || !_player->isDND())
            {
                if (msg.empty())
                    _player->dndMsg = GetMangosString(LANG_PLAYER_DND_DEFAULT);
                else
                    _player->dndMsg = msg;
            }
            if (msg.empty() || !_player->isDND())
            {
                _player->ToggleDND();
                if (_player->isDND() && _player->isAFK())
                    _player->ToggleAFK();
            }
        } break;

        default:
            sLog.outError("CHAT: unknown message type %u, lang: %u", type, lang);
            break;
    }
}

void WorldSession::HandleEmoteOpcode( WorldPacket & recv_data )
{
    if(!GetPlayer()->isAlive() || GetPlayer()->hasUnitState(UNIT_STAT_DIED))
        return;

    uint32 emote;
    recv_data >> emote;
    GetPlayer()->HandleEmoteCommand(emote);
}

namespace MaNGOS
{
    class EmoteChatBuilder
    {
        public:
            EmoteChatBuilder(Player const& pl, uint32 text_emote, uint32 emote_num, Unit const* target)
                : i_player(pl), i_text_emote(text_emote), i_emote_num(emote_num), i_target(target) {}

            void operator()(WorldPacket& data, int32 loc_idx)
            {
                char const* nam = i_target ? i_target->GetNameForLocaleIdx(loc_idx) : NULL;
                uint32 namlen = (nam ? strlen(nam) : 0) + 1;

                data.Initialize(SMSG_TEXT_EMOTE, (20+namlen));
                data << ObjectGuid(i_player.GetObjectGuid());
                data << uint32(i_text_emote);
                data << uint32(i_emote_num);
                data << uint32(namlen);
                if (namlen > 1)
                    data.append(nam, namlen);
                else
                    data << uint8(0x00);
            }

        private:
            Player const& i_player;
            uint32        i_text_emote;
            uint32        i_emote_num;
            Unit const*   i_target;
    };
}                                                           // namespace MaNGOS

void WorldSession::HandleTextEmoteOpcode( WorldPacket & recv_data )
{
    if(!GetPlayer()->isAlive())
        return;

    if (!GetPlayer()->CanSpeak())
    {
        std::string timeStr = secsToTimeString(m_muteTime - time(NULL));
        SendNotification(GetMangosString(LANG_WAIT_BEFORE_SPEAKING), timeStr.c_str());
        return;
    }

    uint32 text_emote, emoteNum;
    ObjectGuid guid;

    recv_data >> text_emote;
    recv_data >> emoteNum;
    recv_data >> guid;

    EmotesTextEntry const *em = sEmotesTextStore.LookupEntry(text_emote);
    if (!em)
        return;

    uint32 emote_id = em->textid;

    switch(emote_id)
    {
        case EMOTE_STATE_SLEEP:
        case EMOTE_STATE_SIT:
        case EMOTE_STATE_KNEEL:
        case EMOTE_ONESHOT_NONE:
            break;
        default:
        {
            // in feign death state allowed only text emotes.
            if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
                break;

            GetPlayer()->HandleEmoteCommand(emote_id);
            break;
        }
    }

    Unit* unit = GetPlayer()->GetMap(true)->GetUnit(guid);

    MaNGOS::EmoteChatBuilder emote_builder(*GetPlayer(), text_emote, emoteNum, unit);
    MaNGOS::LocalizedPacketDo<MaNGOS::EmoteChatBuilder > emote_do(emote_builder);
    MaNGOS::CameraDistWorker<MaNGOS::LocalizedPacketDo<MaNGOS::EmoteChatBuilder > > emote_worker(GetPlayer(), sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE), emote_do);
    Cell::VisitWorldObjects(GetPlayer(), emote_worker,  sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE));

    GetPlayer()->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_DO_EMOTE, text_emote, 0, unit);

    //Send scripted event call
    if (unit && unit->GetTypeId() == TYPEID_UNIT && ((Creature*)unit)->AI())
        ((Creature*)unit)->AI()->ReceiveEmote(GetPlayer(), text_emote);
}

void WorldSession::HandleChatIgnoredOpcode(WorldPacket& recv_data )
{
    ObjectGuid iguid;
    uint8 unk;
    //DEBUG_LOG("WORLD: Received CMSG_CHAT_IGNORED");

    recv_data >> iguid;
    recv_data >> unk;                                       // probably related to spam reporting

    Player *player = sObjectMgr.GetPlayer(iguid);
    if(!player || !player->GetSession())
        return;

    WorldPacket data;
    ChatHandler::FillMessageData(&data, this, CHAT_MSG_IGNORED, LANG_UNIVERSAL, NULL, GetPlayer()->GetObjectGuid(), GetPlayer()->GetName(), NULL);
    player->GetSession()->SendPacket(&data);
}

void WorldSession::SendPlayerNotFoundNotice(std::string name)
{
    WorldPacket data(SMSG_CHAT_PLAYER_NOT_FOUND, name.size()+1);
    data << name;
    SendPacket(&data);
}

void WorldSession::SendPlayerAmbiguousNotice(std::string name)
{
    WorldPacket data(SMSG_CHAT_PLAYER_AMBIGUOUS, name.size()+1);
    data << name;
    SendPacket(&data);
}

void WorldSession::SendWrongFactionNotice()
{
    WorldPacket data(SMSG_CHAT_WRONG_FACTION, 0);
    SendPacket(&data);
}

void WorldSession::SendChatRestrictedNotice(ChatRestrictionType restriction)
{
    WorldPacket data(SMSG_CHAT_RESTRICTED, 1);
    data << uint8(restriction);
    SendPacket(&data);
}
