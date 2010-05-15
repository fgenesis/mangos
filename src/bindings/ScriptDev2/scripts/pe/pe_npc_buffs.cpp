#include "precompiled.h"
#include "pe_base.h"
#include "pe_structs.h"

// **** This script is designed as an example for others to build on ****
// **** Please modify whatever you'd like to as this script is only for developement ****

// **** Script Info ****
// This script is written in a way that it can be used for both friendly and hostile monsters
// Its primary purpose is to show just how much you can really do with scripts
// I recommend trying it out on both an agressive NPC and on friendly npc

// **** Quick Info ****
// Functions with Handled Function marked above them are functions that are called automatically by the core
// Functions that are marked Custom Function are functions I've created to simplify code

#define SPELL_BUFF 15366
#define SPELL_BUFF2 26393
#define SPELL_BUFF3 16609
#define SPELL_FULLHEAL 7393
#define SPELL_BUFF_EXP 42138

struct MANGOS_DLL_DECL pe_npc_buffsAI : public ScriptedAI
{
    pe_npc_buffsAI(Creature *c) : ScriptedAI(c) {Reset();}

    void Reset()
    {
        //m_creature->RemoveAllAuras();
        //m_creature->DeleteThreatList();
        //m_creature->CombatStop();
        //DoGoHome();
    }

    void Aggro(Unit *who)
    {
    }

    //*** HANDLED FUNCTION *** 
    //Move in line of sight is called whenever any unit moves within our sight radius (something like 50 yards)
    void MoveInLineOfSight(Unit *who)
    {
        if (!who)
            return;

        if (who->GetTypeId() == TYPEID_PLAYER && m_creature->IsFriendlyTo(who) && !((Player*)who)->isGameMaster())
        {
            if(who->getLevel() >= 80)
                return;

            if(!who->HasAura(SPELL_BUFF,EFFECT_INDEX_0))
            {
                DoCast(who, SPELL_BUFF);
                return;
            }
            if(!who->HasAura(SPELL_BUFF2,EFFECT_INDEX_0) && who->getLevel() < 70)
            {
                who->CastSpell(who,SPELL_BUFF2,false);
                return;
            }
            if(!who->HasAura(SPELL_BUFF3,EFFECT_INDEX_0) && URandomize(0,99) < 3)
            {
                DoCast(who, SPELL_BUFF3);
                return;
            }
        }
    }
}; 

CreatureAI* GetAI_pe_npc_buffs(Creature *_Creature)
{
    return new pe_npc_buffsAI (_Creature);
}

bool ReceiveEmote_pe_npc_buffs(Player *player, Creature *_Creature, uint32 emote)
{
    if (emote == EMOTE_ONESHOT_QUESTION)
        _Creature->MonsterSay("PE Buffs NPC Script, (C) PE",LANG_UNIVERSAL,NULL);

    return true;
}

void AddSC_pe_npc_buffs()
{
    Script *newscript;
    newscript = new Script;
    newscript->Name="pe_npc_buffs";
    newscript->GetAI = GetAI_pe_npc_buffs;
    //newscript->pReceiveEmote = &ReceiveEmote_pe_npc_buffs;
    newscript->RegisterSelf();
}
