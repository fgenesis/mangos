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

struct MANGOS_DLL_DECL pe_npc_xmasAI : public ScriptedAI
{
    pe_npc_xmasAI(Creature *c) : ScriptedAI(c) {Reset();}
    uint32 action_timer;

    void Reset()
    {
        action_timer = 2000;
        
        //m_creature->RemoveAllAuras();
        //m_creature->DeleteThreatList();
        //m_creature->CombatStop();
        //DoGoHome();
    }

    void Aggro(Unit *who)
    {
    }

    void UnAnim(void)
    {
        m_creature->SetUInt32Value(UNIT_NPC_EMOTESTATE,0);
        action_timer = 2000;
    }

    // set emote anim state
    void DoAnim(uint32 id)
    {

        m_creature->SetUInt32Value(UNIT_NPC_EMOTESTATE,id);
    }

    //*** HANDLED FUNCTION *** 
    //Move in line of sight is called whenever any unit moves within our sight radius (something like 50 yards)
    void MoveInLineOfSight(Unit *who)
    {
        if (!who)
            return;

        if (who->GetTypeId() == TYPEID_PLAYER && m_creature->IsFriendlyTo(who))
        {
            if(!who->HasAura(SPELL_BUFF,EFFECT_INDEX_0))
            {
                UnAnim();
                DoCast(who, SPELL_BUFF);
                return;
            }
            if(!who->HasAura(SPELL_BUFF2,EFFECT_INDEX_0) && who->getLevel() < 70)
            {
                UnAnim();
                who->CastSpell(who,SPELL_BUFF2,false);
                //DoCast(who, SPELL_BUFF2);
                return;
            }
            if(!who->HasAura(SPELL_BUFF3,EFFECT_INDEX_0) && URandomize(0,99) < 3)
            {
                UnAnim();
                DoCast(who, SPELL_BUFF3);
                return;
            }
            /*if(!who->HasAura(SPELL_BUFF_EXP,0))
            {
                UnAnim();
                SpellEntry const *spellInfo = GetSpellStore()->LookupEntry( SPELL_BUFF_EXP );
                if(spellInfo)
                {
                    for(uint32 i = 0;i<3;i++)
                    {
                        uint8 eff = spellInfo->Effect[i];
                        if (eff>=TOTAL_SPELL_EFFECTS)
                            continue;

                        if (eff == SPELL_EFFECT_APPLY_AURA || eff == SPELL_EFFECT_PERSISTENT_AREA_AURA)
                        {
                            Aura *Aur = new Aura(spellInfo, i, NULL, who, who);
                            Aur->SetRemoveOnDeath(false);
                            Aur->SetPositive();
                            who->AddAura(Aur);
                        }
                    }
                }
                return;
            }*/
        }
    }

    void UpdateAI(const uint32 diff)
    {
        if (action_timer < diff)
        {
            uint32 r = rand()%100;
            if(r > 40 && r < 60)
            {
                DoAnim(EMOTE_STATE_DANCE);
                action_timer = 5000;
            }
            else if(r <= 60 && r > 70)
            {
                DoAnim(EMOTE_STATE_USESTANDING);
                action_timer = 5000;
            }
            else if(r >= 70 && r < 80)
            {
                DoAnim(EMOTE_STATE_FISHING);
                action_timer = 7000;
            }
            else if(r >= 80 && r < 89)
            {
                DoAnim(EMOTE_STATE_WORK);
                action_timer = 5000;
            }
            else if(r >=89 && r < 97)
            {
                DoAnim(EMOTE_STATE_WHIRLWIND);
                action_timer = 3000;
            }
            else if( r >= 97 )
            {
                DoAnim(EMOTE_STATE_DROWNED);
                action_timer = 10000;
            }
            else
            {
                UnAnim();
                action_timer = URandomize(2000,5500);
            }
        }
        else
            action_timer -= diff;
    }
}; 

CreatureAI* GetAI_pe_npc_xmas(Creature *_Creature)
{
    return new pe_npc_xmasAI (_Creature);
}

bool ReceiveEmote_pe_npc_xmas(Player *player, Creature *_Creature, uint32 emote)
{
    //_Creature->HandleEmoteCommand(emote);

    if (emote == EMOTE_ONESHOT_QUESTION)
        _Creature->MonsterSay("PE Xmas NPC Script, (C) FG, Dec 2007",LANG_UNIVERSAL,NULL);


    return true;
}

void AddSC_pe_npc_xmas()
{
    Script *newscript;
    newscript = new Script;
    newscript->Name="pe_npc_xmas";
    newscript->GetAI = GetAI_pe_npc_xmas;
    //newscript->pReceiveEmote = &ReceiveEmote_pe_npc_xmas;
    newscript->RegisterSelf();
}
