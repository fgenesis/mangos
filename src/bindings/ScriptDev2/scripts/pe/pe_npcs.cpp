#include "precompiled.h"

/*####
## npc_snake_trap_serpents - Summoned snake id are 19921 and 19833
## thx Loockas for this NPC script, original author unknown
####*/

#define SPELL_MIND_NUMBING_POISON    25810   //Viper
#define SPELL_CRIPPLING_POISON       30981    //Viper
#define SPELL_DEADLY_POISON          34655   //Venomous Snake

#define MOB_VIPER 19921

struct MANGOS_DLL_DECL npc_snake_trap_serpentsAI : public ScriptedAI
{
    npc_snake_trap_serpentsAI(Creature *c) : ScriptedAI(c) {Reset();}

    uint32 SpellTimer;
    bool IsViper;

    void Reset()
    {
        SpellTimer = 500;

        Unit *Owner = m_creature->GetOwner();
        if (!Owner)
            return;

        CreatureInfo const *Info = m_creature->GetCreatureInfo();

        IsViper = Info->Entry == MOB_VIPER;
    }

    void UpdateAI(const uint32 diff)
    {
        Unit *Owner = m_creature->GetOwner();

        if (!Owner) return;

        if (!m_creature->getVictim())
        {
            if (m_creature->isInCombat())
                DoStopAttack();

            if (Owner->getAttackerForHelper())
                AttackStart(Owner->getAttackerForHelper());
        }

        if (SpellTimer <= diff)
        {
            if (IsViper) //Viper - 19921
            {
                if (urand(0,2) == 0) //33% chance to cast
                {
                    uint32 spell;
                    if (urand(0,1) == 0)
                        spell = SPELL_MIND_NUMBING_POISON;
                    else
                        spell = SPELL_CRIPPLING_POISON;

                    m_creature->CastSpell(m_creature->getVictim(), spell, true);
                }

                SpellTimer = urand(3000, 5000);
            }
            else //Venomous Snake - 19833
            {
                if (urand(0,1) == 0) //50% chance to cast
                    m_creature->CastSpell(m_creature->getVictim(), SPELL_DEADLY_POISON, true);
                SpellTimer = urand(2500, 4500);
            }
        } else SpellTimer -= diff;
        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_npc_snake_trap_serpents(Creature* pCreature)
{
    return new npc_snake_trap_serpentsAI(pCreature);
}


void AddSC_pe_npcs()
{
    Script *newscript;

    newscript = new Script;
    newscript->Name = "npc_snake_trap_serpents";
    newscript->GetAI = &GetAI_npc_snake_trap_serpents;
    newscript->RegisterSelf();
}
