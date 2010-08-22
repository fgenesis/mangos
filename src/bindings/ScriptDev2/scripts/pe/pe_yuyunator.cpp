// displayid 21514

#include "precompiled.h"
#include "Spell.h"

#include "pe_structs.h"
#include "pe_base.h"

// arcane spells
#define CAST_ARCANEBARRIER 36481 // looks cool, immune to everything
#define CAST_ARCANESHOCK_DISARM 33175 // 5 sec disarm + ~700 dmg
#define CAST_ARCANESHOCK_DOT 44319 // 2400 dmg + 4x 250 dmg // -- RND
#define CAST_ARCANEVOLLEY 40424 //2-3k dmg, instant, 100y //OLD: 29960 // 20 sec, per sec ~180 aoe dmg
#define CAST_ARCANEBLAST 22920 // 1k dmg + knockback // -- RND
#define CAST_MAGEARMOR 27125 // resistance++, manareg++

// frost spells
#define CAST_FROSTARMOR 31256 // +3k armor and slow
#define CAST_FROSTBREATH 44799 // slow in cone in front + 2-3k dmg // -- RND
#define CAST_FROSTBURN 23189 // attacktime +400% // -- RND
#define CAST_FROSTHOLD 10017 // freez for 10 sec
#define CAST_FROSTSHOCK 43524 // some 2k dmg + 6 sec slow
#define CAST_FROSTBOLT 38238 // 2k dmg, slow
#define CAST_FROSTAURA 28531 // 600 dmg per sec for 5 sec. 100y aoe -- RND
#define CAST_ICEARMOR 27124

// fire spells
#define CAST_PYROBLAST 31263 // 5k dmg + 3x 500 more // -- RND
#define CAST_PYROGENICS 45230 // +35% more fire dmg
#define CAST_PYROCLAST_BARRAGE 19641 // 1500 dmg + stun. cone. // -- RND
#define CAST_BLASTWAVE 25049 // 1500 dmg + daze + knockback // -- RND
#define CAST_DRAGONSBREATH_BURN 29964 // 6k dmg + 3x 2500 + 1k for allies nearby. single target..? // -- RND
#define CAST_DRAGONSBREATH 37289 // 2400 dmg + disorient. cone. // -- RND
#define CAST_AURAOFFLAMES 22436 // 250 dmg to attackers
#define CAST_AMPLIFY_FLAMES 9482 // buff: +100 fire dmg taken
#define CAST_CAUTERIZINGFLAMES 19366 // aoe: -200 fire res
#define CAST_COMBUSTION 11129 // crit chance++

// chaos spells
#define CAST_CHAOSBLAST 37675 // +1.6k fire dmg
#define CAST_RAINOFCHAOS 31340 // like blizzard, some 3k dmg per sec -- to cast 10 secs after chaos blast

// misc spells
#define CAST_COUNTERSPELL 15122 // 15 sec -- to be used against priest heal ;]
#define CAST_DARKSHELL 38759 // 1500 dmg for attackers + spell reflect -- use shortly before death, in short intervals
#define CAST_DRAININGPOISON 64152 // 21k hp/mana over 18s
#define CAST_MAGNETICPULL 29661 // let player flyyy, but use only at shorter range (~20)
#define CAST_CORRUPTEDHEALING 23401 // dmg instead of heal
#define CAST_MADNESS_VISUAL 32839 // red circle around npc
#define CAST_DEATH_CINEMATIC_BEAM 64580 // o_O'
#define CAST_DEATH_CINEMATIC_VISUAL 57772 // beam of light

// minion spells
#define CAST_EXPLODE 568 // better, this one also has knockback + slow
#define CAST_EXPLODE2 64875 // 10k dmg, looks really cool
#define CAST_EXPLODE_VISUAL 55529 // mechanical boom, cool
#define CAST_HEALINGREDUCE 17820 // veil of shadow, -75% healing received
#define CAST_SHADOWPRISON 45922 // 1.5 mins stun
#define CAST_BEAM_1 45537 // perm. chain lightning visual
#define CAST_BEAM_2 36295 // red beam, cloudy  // OLD: 30888 // star beam (pink & cloudy)
#define CAST_MARK 64328 // hunters mark visual

// priest minion spells
#define CAST_GREATERHEAL 38580 // 50k heal
#define CAST_MINDWARP 38047 // mana cost +200%
#define CAST_RAYOFPAIN 59525 // 2k dmg, -15% heal
#define CAST_RAYOFSUFFERING 54417
#define CAST_SHADOWWORDPAIN 30898 // 1300 dmg/3s for 18s
#define CAST_PSYCHICSCREAM 10890
#define CAST_SHADOWFORM 55086 // looks cool
#define CAST_SMITE 20695 // 1300-1600 holy dmg
#define CAST_DRAINMANA 58770 // 5k mana leech

// rogue minion spells
#define CAST_THROW_DAGGER 41152 // 5400-6600 dmg
//#define CAST_VANISH 24699
#define CAST_DISARM 13534
#define CAST_FRENZY 54123
#define CAST_FANOFKNIVES 52874
#define CAST_BACKSTAB 63754
#define CAST_SHADOWSTEP 63793
#define CAST_KIDNEYSHOT 40864
#define CAST_SPRINT 56354


// these spells dont have mana cost, need to simulate them
#define MANACOST_PRIEST_HEAL 672
#define MANACOST_PRIEST_SMITE 339


#define NPC_SELF 100096
#define NPC_FIRECASTER 100099
#define NPC_ICECASTER 100098
#define NPC_ARCANECASTER 100097
#define NPC_ENERGYLOCK1 100095
#define NPC_ENERGYLOCK2 100094
#define NPC_SPAWN_PRIEST 100093
#define NPC_SPAWN_ROGUE 100092

enum
{
    SAY_COMBATSTART    = -1990000,
    SAY_DEATH          = -1990001,
    SAY_WIPE           = -1990002,
    SAY_KILL1          = -1990003,
    SAY_KILL2          = -1990004,
    SAY_KILL3          = -1990005,
    SAY_KILL4          = -1990006,
    SAY_KILL5          = -1990007,
    SAY_KILL6          = -1990008,
    SAY_KILL7          = -1990009,
    SAY_KILL8          = -1990010,
    SAY_MADNESS        = -1990011,
    SAY_ENRAGE         = -1990012,
    SAY_WIPE_ENRAGED   = -1990013,
};


enum Phases
{
    // random
    ELEMENT_ICE = 0,
    ELEMENT_FIRE = 1,
    ELEMENT_ARCANE = 2,
    // definitely last phase
    ELEMENT_MADNESS = 3
};

uint32 priestModels[] =
{
    11216, // transparent NE in red robe
    11208, // a ne f
    11210, // a ne f
    11244, // a ne f
    11243, // a ne m
    16200, // a dr f
    16202, // b dr f
    //16207, // c be m
    16223, // b dr f
    16225, // a dr m
    16249, // b be f
    16443, // b ud f
    16864  // a ne f
};

uint32 rogueModels[] = 
{
    11183, // b tr m
    11184, // b tr f
    11185, // b tr f
    11203, // a tr f
    11204, // a tr f
    //16286, // b be m
    16429, // a ne f
    16436, // a or f
    16644, // b or f
    16689, // a be f
    //16767, // b be m
    16782, // a ne f
    16783, // a ne m
    16796, // a be f
    //16805, // c dr f
    16818, // a be m
    16857, // a dr f
    //17026, // b dr f
    17115, // a be m
    17198, // a be m
    25028  // a dr f
};

// some helpers
bool IsAlliedUnit(Unit *who)
{
    if(!who)
        return false;
    switch(who->GetEntry())
    {
    case NPC_ARCANECASTER:
    case NPC_FIRECASTER:
    case NPC_ICECASTER:
    case NPC_ENERGYLOCK1:
    case NPC_ENERGYLOCK2:
    case NPC_SPAWN_PRIEST:
    case NPC_SPAWN_ROGUE:
    case NPC_SELF:
        return true;
    }
    return false;
}

void SelfKill(Unit *who)
{
    if(!who)
        return;
    who->DealDamage(who, who->GetMaxHealth(), NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
}


class MANGOS_DLL_DECL pe_andromedaAI : public ScriptedAI
{    
    typedef std::list<Unit*> UnitList;
    typedef std::map<uint64, UnitList> UnitListMap; // player guid as index, spawn ptrs as elements

public:
    pe_andromedaAI(Creature *c) : ScriptedAI(c)
    {
        SetCombatMovement(false);
        FillSpells();
        ClearEmitters();
        Reset();
    }

    void Reset(void);
    void JustRespawned(void);
    void JustDied(Unit*);
    void KilledUnit(Unit*);
    void EnterCombat(Unit*);
    void MoveInLineOfSight(Unit*);
    void UpdateAI(const uint32);
    void DamageTaken(Unit *done_by, uint32 &damage);
    void SpellHit(Unit *who, const SpellEntry *entry);

    void UpdateEmitters(uint32 diff);
    void ClearEmitters(void);
    void ResetTimers(void);
    void HandleSpellcasts(uint32);
    void FillSpells(void);
    void ChangePhase(float);
    Unit *GetRandomTarget(void);

    void PauseCombat(uint32);
    void ContinueCombatIfNecessary(uint32);
    void ApplyEnergyLocksIfNecessary(uint32);
    void UpdateEnergyLocks(uint32);
    void UnsummonAllEnergyLocks(void);

    // grid searchers
    std::list<Unit*> SelectNearUnits(float distance = 100.0f);
    std::list<Unit*> SelectNearPlayers(float distance = 100.0f);
    std::list<Unit*> SelectNearUnitsWithEntry(uint32 entry, float distance = 100.0f);

    uint32 element,phase;
    uint32 killed;
    std::list<Emitter*> emitters;

    UnitListMap energyLocks; // stores

    std::vector<uint32> FireSpells, IceSpells, ArcSpells;

    // timers
    uint32 timer_combat_pause;
    uint32 timer_element;
    uint32 timer_emit1, timer_emit2; // 1 = weak emits, 2 = hard emits
    uint32 counter_emit1;
    uint32 timer_energylock;
    uint32 timer_death_cinematic;

    uint32 timer_rndcast;

    uint32 timer_pyrogenics;
    uint32 timer_amplify_flames;
    uint32 timer_ct_flames;
    uint32 timer_combustion;

    uint32 timer_frosthold;
    uint32 timer_frostshock;
    uint32 timer_frostbolt;

    uint32 timer_arc_disarm;
    uint32 timer_arc_barrier;
    uint32 timer_arc_volley;

    uint32 timer_counterspell;
    uint32 timer_darkshell;
    uint32 timer_drainingpoison, counter_drainingpoison;
    uint32 timer_pull;
    uint32 timer_corr_heal;

    uint32 timer_chaosblast;
    uint32 timer_rainofchaos;

    // misc timers
    uint32 timer_say_kill;

};

struct MANGOS_DLL_DECL pe_andromeda_minionGenericAI : public ScriptedAI
{
    pe_andromeda_minionGenericAI(Creature *c) : ScriptedAI(c) {}

    Unit *SelectNearestUnitEntry(uint32 entry, float fRange)
    {
        CellPair p(MaNGOS::ComputeCellPair(m_creature->GetPositionX(), m_creature->GetPositionY()));
        Cell cell(p);
        cell.data.Part.reserved = ALL_DISTRICT;
        cell.SetNoCreate();

        Unit *pUnit = NULL;

        AllCreaturesOfEntryInRange u_check(m_creature, entry, fRange);
        MaNGOS::UnitLastSearcher<AllCreaturesOfEntryInRange> searcher(m_creature, pUnit, u_check);

        /*
        typedef TYPELIST_4(GameObject, Creature*except pets*, DynamicObject, Corpse*Bones*) AllGridObjectTypes;
        This means that if we only search grid then we cannot possibly return pets or players so this is safe
        */
        TypeContainerVisitor<MaNGOS::UnitLastSearcher<AllCreaturesOfEntryInRange>, GridTypeMapContainer >  grid_unit_searcher(searcher);

        cell.Visit(p, grid_unit_searcher, *(m_creature->GetMap()));

        return pUnit;
    }

    virtual void KilledUnit(Unit* Victim)
    {
        if(Victim == m_creature)
            return;

        Unit *owner = m_creature->GetOwner();

        if(!owner)
        {
            owner = SelectNearestUnitEntry(NPC_SELF, 50.0f);
        }

        if(owner && owner->GetTypeId() == TYPEID_UNIT && owner->GetEntry() == NPC_SELF)
        {
            Creature *cr = (Creature*)owner;
            ((pe_andromedaAI*)(cr->AI()))->KilledUnit(Victim);
        }
    }
};

struct MANGOS_DLL_DECL pe_energylockGenericAI : public pe_andromeda_minionGenericAI
{
    bool isDone : 1;
    uint32 timerSuicide;
    bool beamCasted : 1;
    uint32 timer_damage;
    uint64 target_guid;

    pe_energylockGenericAI(Creature *c) : pe_andromeda_minionGenericAI(c)
    {
        Reset();
        SetCombatMovement(false);
    }

    void Reset()
    {
        timerSuicide = 0;
        beamCasted = false;
        isDone = false;
        timer_damage = 1;
        target_guid = 0;
    }

    void DamageTaken(Unit* pDoneBy, uint32& uiDamage)
    {
        if(IsAlliedUnit(pDoneBy))
        {
            uiDamage = 0;
            return;
        }

        if(uiDamage >= m_creature->GetHealth())
        {

            if(!timerSuicide)
            {
                DoCast(m_creature, CAST_EXPLODE_VISUAL, true);
                timerSuicide = 300;
            }
            if(timerSuicide < 100000)
            {
                uiDamage = 0;
            }
        }
    }
};

void pe_andromedaAI::PauseCombat(uint32 t)
{
    m_creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
    timer_combat_pause = t;
}

void pe_andromedaAI::ContinueCombatIfNecessary(uint32 diff)
{
    if(!timer_combat_pause)
        return;

    if(timer_combat_pause <= diff)
    {
        m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
        timer_combat_pause = 0;
    }
    else timer_combat_pause -= diff;
}

std::list<Unit*> pe_andromedaAI::SelectNearUnits(float distance /* = 100.0f */)
{
    CellPair p(MaNGOS::ComputeCellPair(m_creature->GetPositionX(), m_creature->GetPositionY()));
    Cell cell(p);
    cell.data.Part.reserved = ALL_DISTRICT;
    cell.SetNoCreate();

    std::list<Unit*> pList;

    MaNGOS::AnyUnitInObjectRangeCheck u_check(m_creature, distance);
    MaNGOS::UnitListSearcher<MaNGOS::AnyUnitInObjectRangeCheck> searcher(m_creature, pList, u_check);

    TypeContainerVisitor<MaNGOS::UnitListSearcher<MaNGOS::AnyUnitInObjectRangeCheck>, GridTypeMapContainer >  grid_unit_searcher(searcher);

    cell.Visit(p, grid_unit_searcher, *(m_creature->GetMap()));

    return pList;
}

// HACK: the core doesnt provide a PlayerListSearcher, so have to search for all units and drop all that are not players
std::list<Unit*> pe_andromedaAI::SelectNearPlayers(float distance /* = 100.0f */)
{
    Map *map = m_creature->GetMap();
    Map::PlayerList const &PlayerList = map->GetPlayers();

    std::list<Unit*> pList;
    if (PlayerList.isEmpty())
        return pList;

    for(Map::PlayerList::const_iterator i = PlayerList.begin(); i != PlayerList.end(); ++i)
        if(m_creature->IsWithinDistInMap(i->getSource(), distance) && i->getSource()->isAlive())
            pList.push_back(i->getSource());


    // grid search method - doesnt work
    /*
    std::list<Unit*> pList = SelectNearUnits(distance);
    std::list<Unit*>::iterator it;
    for(it = pList.begin(); pList.size() && it != pList.end(); )
    {
        if((*it)->GetTypeId() != TYPEID_PLAYER)
            it = pList.erase(it);
        else
            it++;
    }
    */

    return pList;
}

std::list<Unit*> pe_andromedaAI::SelectNearUnitsWithEntry(uint32 entry, float fRange)
{
    CellPair p(MaNGOS::ComputeCellPair(m_creature->GetPositionX(), m_creature->GetPositionY()));
    Cell cell(p);
    cell.data.Part.reserved = ALL_DISTRICT;
    cell.SetNoCreate();

    std::list<Unit*> uList;

    AllCreaturesOfEntryInRange u_check(m_creature, entry, fRange);
    MaNGOS::UnitListSearcher<AllCreaturesOfEntryInRange> searcher(m_creature, uList, u_check);

    /*
    typedef TYPELIST_4(GameObject, Creature*except pets*, DynamicObject, Corpse*Bones*) AllGridObjectTypes;
    This means that if we only search grid then we cannot possibly return pets or players so this is safe
    */
    TypeContainerVisitor<MaNGOS::UnitListSearcher<AllCreaturesOfEntryInRange>, GridTypeMapContainer >  grid_unit_searcher(searcher);

    cell.Visit(p, grid_unit_searcher, *(m_creature->GetMap()));

    return uList;
}

void pe_andromedaAI::HandleSpellcasts(uint32 diff)
{
    // --- random periodic spellcasts ---
    if(timer_rndcast < diff)
    {
        std::vector<uint32> *sl = NULL;

        // good for some finetuning
        switch(element)
        {
        case ELEMENT_FIRE: timer_rndcast = urand(4000,6500); sl = &FireSpells; break;
        case ELEMENT_ICE: timer_rndcast = urand(3700,6200); sl = &IceSpells; break;
        case ELEMENT_ARCANE: timer_rndcast = urand(3500,6200); sl = &ArcSpells; break;
        case ELEMENT_MADNESS:
            timer_rndcast = urand(4000,7000);
            uint32 rsl = urand(0,2);
            if(rsl == 0) sl = &FireSpells;
            else if(rsl == 1) sl = &IceSpells;
            else sl = &ArcSpells;
            break;
        }

        if(sl)
        {
            // select random spell from spell list
            uint32 pos = urand(0, sl->size() - 1);
            uint32 spell = (*sl)[pos];

            Unit *target = GetRandomTarget();
            if(target)
                DoCast(target,spell,false);
        }

    }
    else timer_rndcast -= diff;

    // --- all other spells ---

    if(element == ELEMENT_FIRE || element == ELEMENT_MADNESS)
    {
        if(timer_pyrogenics < diff)
        {
            DoCast(m_creature,CAST_PYROGENICS);
            timer_pyrogenics = urand(18000,30000);
        } else timer_pyrogenics -= diff;

        if(timer_amplify_flames < diff)
        {
            Unit *target = GetRandomTarget();
            DoCast(target,CAST_AMPLIFY_FLAMES);
            timer_amplify_flames = urand(3000,8000);
        } else timer_amplify_flames -= diff;

        if(timer_ct_flames < diff)
        {
            Unit *target = GetRandomTarget();
            DoCast(m_creature,CAST_CAUTERIZINGFLAMES);
            timer_ct_flames = urand(65000,115000);
        } else timer_ct_flames -= diff;

        if(timer_combustion < diff)
        {
            Unit *target = GetRandomTarget();
            DoCast(m_creature,CAST_COMBUSTION);
            timer_combustion = urand(20000,30000);
        } else timer_combustion -= diff;
    }

    if(element == ELEMENT_ICE || element == ELEMENT_MADNESS)
    {
        if(timer_frosthold < diff)
        {
            DoCast(m_creature,CAST_FROSTHOLD);
            timer_frosthold = urand(15000,25000);
        } else timer_frosthold -= diff;

        if(timer_frostshock < diff)
        {
            Unit *target = GetRandomTarget();
            if(urand(0,100) < 50)
                target = m_creature->getVictim();
            if(target)
                DoCast(target,CAST_FROSTSHOCK);
            timer_frostshock = urand(3300,5500);
        } else timer_frostshock -= diff;

        if(timer_frostbolt < diff)
        {
            Unit *target = GetRandomTarget();
            if(target)
                DoCast(target,CAST_FROSTBOLT);
            timer_frostbolt = urand(4000,7000);
        } else timer_frostbolt -= diff;
    }

    if(element == ELEMENT_ARCANE || element == ELEMENT_MADNESS)
    {
        if(timer_arc_barrier < diff)
        {
            DoCast(m_creature,CAST_ARCANEBARRIER);
            timer_arc_barrier = phase < 2 ? urand(15000,60000) : urand(15000,30000);
        } else timer_arc_barrier -= diff;

        if(timer_arc_disarm < diff)
        {
            Unit *target = m_creature->getVictim();
            if(target)
                DoCast(m_creature,CAST_ARCANESHOCK_DISARM);
            timer_arc_disarm = phase < 2 ? urand(8000,15000) : urand(3000,8000);
        } else timer_arc_disarm -= diff;
    }

    if(timer_arc_volley < diff)
    {
        DoCast(m_creature->getVictim(),CAST_ARCANEVOLLEY,true);
        timer_arc_volley = (element == ELEMENT_ARCANE) ? urand(6000, 10000) : urand(14000, 22500);
        if(phase == 4)
            timer_arc_volley = uint32(timer_arc_volley * 0.7f);
    }
    else timer_arc_volley -= diff;

    if(timer_counterspell < diff)
    {
        bool exitloop = false;
        std::list<Unit*> plist = SelectNearPlayers(40.0f);
        for(std::list<Unit*>::iterator it = plist.begin(); it != plist.end(); it++)
        {
            Player *p = (Player*)*it;
            // ...check all spells casted by player...
            for(uint32 i = 0; i < CURRENT_MAX_SPELL; i++)
            {
                Spell *spell = p->GetCurrentSpell(CurrentSpellTypes(i));
                // ...and interrupt long cast time spells, with priority on heal spells
                if(spell && (spell->GetCastTime() > 3000 || (spell->GetCastTime() > 600 && spell->m_spellInfo->Effect[0] == SPELL_EFFECT_HEAL)) )
                {
                    DoCast(p,CAST_COUNTERSPELL,true);
                    exitloop = true;
                    break;
                }
                if(exitloop)
                    break;
            }
        }
        timer_counterspell = urand(4000,9000);            
    } else timer_counterspell -= diff;

    if(timer_drainingpoison < diff)
    {
        Unit *target = GetRandomTarget();
        if(target)
            DoCast(target,CAST_DRAININGPOISON,true);

        ++counter_drainingpoison;
        counter_drainingpoison %= (phase >= 3 ? 6 : 3);
        if(!counter_drainingpoison)
            timer_drainingpoison = urand(17000,35000);
        else
            timer_drainingpoison = 1500;
    } else timer_drainingpoison -= diff;

    if(timer_darkshell < diff)
    {
        DoCast(m_creature,CAST_DARKSHELL,true);
        timer_darkshell = (phase >= 3 ? urand(30000,60000) : urand(25000, 35000));
    } else timer_darkshell -= diff;

    /*
    if(timer_corr_heal >= 0)
    {
        if(timer_corr_heal < diff)
        {
            timer_corr_heal = (phase == 4) ? urand(4000, 7000) : urand(10000, 18000);
            std::list<HostilReference*>& tlist = m_creature->getThreatManager().getThreatList();
            for(std::list<HostilReference*>::iterator it = tlist.begin(); it != tlist.end(); it++)
            {
                HostilReference *ref = *it;
                Unit *u =  Unit::GetUnit(*m_creature,ref->getUnitGuid());
                // ...check if target is player...
                if(u && u->GetTypeId() == TYPEID_PLAYER)
                {
                    Player *p = (Player*)u;
                    if(p->getClass() == CLASS_SHAMAN || p->getClass() == CLASS_PRIEST || p->getClass() == CLASS_DRUID || p->getClass() == CLASS_PALADIN)
                    {
                        DoCast(u,CAST_CORRUPTEDHEALING);
                        break;
                    }
                }
            }
        } else timer_corr_heal -= diff;
    }
    */

    if(timer_pull < diff)
    {
        timer_pull = urand(8000,20000);
        ThreatList const& tlist = m_creature->getThreatManager().getThreatList();
        for(ThreatList::const_iterator it = tlist.begin(); it != tlist.end(); it++)
        {
            HostileReference *ref = *it;
            Unit *u = m_creature->GetMap()->GetUnit(ref->getUnitGuid());
            // ...check if target is player...
            if(u && u->GetTypeId() == TYPEID_PLAYER)
            {
                float dist = m_creature->GetDistance(u);
                if(dist < 20)
                {
                    DoCast(u,CAST_MAGNETICPULL);
                    break;
                }
            }
        }
    } else timer_pull -= diff;

    if(timer_chaosblast < diff)
    {
        Unit *target = GetRandomTarget();
        if(target)
            DoCast(target,CAST_CHAOSBLAST);
        timer_chaosblast = urand(4000,8000);
    } else timer_chaosblast -= diff;

    // this will be casted some mins after reaching phase 4
    if(timer_rainofchaos < diff)
    {
        Unit *target = m_creature->getVictim();
        if(target)
            DoCast(target,CAST_RAINOFCHAOS);
        timer_rainofchaos = urand(18000, 25000);
    }
    else timer_rainofchaos -= diff;


}

void pe_andromedaAI::FillSpells(void)
{
    if(FireSpells.empty())
    {
        FireSpells.push_back(CAST_PYROBLAST);
        FireSpells.push_back(CAST_BLASTWAVE);
        FireSpells.push_back(CAST_PYROCLAST_BARRAGE);
        FireSpells.push_back(CAST_DRAGONSBREATH_BURN);
        FireSpells.push_back(CAST_DRAGONSBREATH);
    }
    if(IceSpells.empty())
    {
        IceSpells.push_back(CAST_FROSTBREATH);
        IceSpells.push_back(CAST_FROSTBURN);
    }
    if(ArcSpells.empty())
    {
        ArcSpells.push_back(CAST_ARCANEBLAST);
        ArcSpells.push_back(CAST_ARCANESHOCK_DOT);
    }
}

void pe_andromedaAI::ChangePhase(float perc)
{
    if(perc < 90 && phase == 0)
    {
        timer_counterspell = 6000;
        phase = 1;
        timer_energylock = 10000;
    }
    else if(perc < 75 && phase <= 1)
    {
        phase = 2;
    }
    else if(perc < 40 && phase <= 2)
    {
        timer_corr_heal = 2000;
        timer_darkshell = 6000;
        phase = 3;
    }
    else if(perc < 20 && phase <= 3)
    {
        timer_chaosblast = 2000;
        timer_rainofchaos = 120000; // after 2 mins start raining fire
        phase = 4;

        element = ELEMENT_MADNESS;

        DoCast(m_creature, CAST_MADNESS_VISUAL, true);

        DoScriptText(SAY_MADNESS, m_creature, NULL);

        RandomRadiusOrbitingEmitter *emit;

        // madness indicator emitter, 10 secs
        emit = new RandomRadiusOrbitingEmitter(m_creature,NPC_FIRECASTER,3000.0f,200,0,10000,25,666);
        emit->SetSource(m_creature->GetPositionX(),m_creature->GetPositionY(),m_creature->GetPositionZ(),m_creature->GetOrientation());
        emitters.push_back(emit);

        // "endless" circling fire emitter, max. 10 mins
        emit = new RandomRadiusOrbitingEmitter(m_creature,NPC_FIRECASTER,1500.0f,300,20,600000,15,1833);
        emit->SetSource(m_creature->GetPositionX(),m_creature->GetPositionY(),m_creature->GetPositionZ(),m_creature->GetOrientation());
        emitters.push_back(emit);
    }
}




void pe_andromedaAI::ResetTimers(void)
{
    timer_element = 60000;
    timer_emit1 = urand(6000,10000);
    timer_emit2 = urand(16000,25000);

    timer_energylock = -1;
    timer_death_cinematic = 0;

    timer_rndcast = 3000;

    timer_pyrogenics = 5000;
    timer_amplify_flames = 6500;
    timer_ct_flames = 10000;
    timer_combustion = 12000;

    timer_frosthold = 2000;
    timer_frostshock = 3000;
    timer_frostbolt = 4000;

    timer_arc_disarm = 3000;
    timer_arc_barrier = 4500;
    timer_arc_volley = 20000;

    timer_counterspell = -1;
    timer_darkshell = -1;
    timer_drainingpoison = 40000, counter_drainingpoison = 0;
    timer_pull = 37000;
    timer_corr_heal = -1;

    timer_chaosblast = -1;
    timer_rainofchaos = -1;

    element = urand(ELEMENT_ICE, ELEMENT_ARCANE);

    timer_say_kill = 0;
}

Unit *pe_andromedaAI::GetRandomTarget(void)
{
    Unit *target = NULL;
    // somewhat hacklike to make it select random units better
    uint32 i = 0;
    do
    {
        target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM,0);
        i++;
    }
    while(i < 10 && target && !(target->GetTypeId() == TYPEID_PLAYER || (GUID_HIPART(target->GetGUID()) & HIGHGUID_PET)));
    if(!target)
        target = m_creature->getVictim();
    return target;
}

void pe_andromedaAI::ClearEmitters(void)
{
    for(std::list<Emitter*>::iterator it = emitters.begin(); it != emitters.end(); it++)
    {
        delete *it;
    }
    emitters.clear();    
}

void pe_andromedaAI::UpdateEmitters(uint32 diff)
{
    if(emitters.empty())
        return;

    std::list<Emitter*>::iterator it = emitters.begin();

    do
    {
        (*it)->Update(diff);

        if((*it)->IsDeletable())
        {
            Emitter *e = *it;
            it = emitters.erase(it);
            delete e;
        }
        else
        {
            it++;
        }
    }
    while(emitters.size() && it != emitters.end());
}

void pe_andromedaAI::Reset(void)
{
    phase = 0;
    killed = 0;
    ClearEmitters();
    ResetTimers();
    UnsummonAllEnergyLocks();
    m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
    //--FillSpells();
}

void pe_andromedaAI::JustDied(Unit *killer)
{
    ClearEmitters();
    UnsummonAllEnergyLocks();
    DoScriptText(SAY_DEATH, m_creature, killer);
    timer_combat_pause = 1;
    ContinueCombatIfNecessary(100); // this will remove untargetable flags
}

void pe_andromedaAI::KilledUnit(Unit* Victim)
{
    if(Victim->GetTypeId()==TYPEID_PLAYER)
    {
        //m_creature->SetHealth(m_creature->GetHealth()+2500);
        //m_creature->SetPower(POWER_MANA,m_creature->GetPower(POWER_MANA)+2500);

        killed++;

        std::list<Unit*> PlayerList = SelectNearPlayers(60.0f);
        bool wiped = true;

        for(std::list<Unit*>::iterator it = PlayerList.begin(); it != PlayerList.end(); it++)
        {
            if ((*it)->isAlive() && !((Player*)(*it))->isGameMaster())
            {
                wiped = false;
                break;
            }
        }

        if(wiped)
        {
            if(element == ELEMENT_MADNESS)
                DoScriptText(SAY_WIPE_ENRAGED, m_creature, Victim);
            else
                DoScriptText(SAY_WIPE, m_creature, Victim);

            return;
        }


        if(killed==6)
        {
            DoScriptText(SAY_KILL3,m_creature, Victim);
        }
        else
        {
            if(!timer_say_kill)
            {
                timer_say_kill = 12000; // min 12 secs between kill messages
                uint32 rnd = urand(0,7);
                switch(rnd)
                {
                case 0: DoScriptText(SAY_KILL1,m_creature, Victim); break;
                case 1: DoScriptText(SAY_KILL2,m_creature, Victim); break;
                case 2: DoScriptText(SAY_KILL3,m_creature, Victim); break;
                case 3: DoScriptText(SAY_KILL4,m_creature, Victim); break;
                case 4: DoScriptText(SAY_KILL5,m_creature, Victim); break;
                case 5: DoScriptText(SAY_KILL6,m_creature, Victim); break;
                case 6: DoScriptText(SAY_KILL7,m_creature, Victim); break;
                case 7: DoScriptText(SAY_KILL8,m_creature, Victim); break;
                }
            }
        }
    }

}

void pe_andromedaAI::JustRespawned()
{
    Reset();
}

void pe_andromedaAI::DamageTaken(Unit *done_by, uint32 &damage)
{
    if(m_creature->GetHealth() <= damage)
    {
        if(!timer_death_cinematic)
        {
            timer_death_cinematic = 4000;
            PauseCombat(timer_death_cinematic);

            m_creature->ApplySpellAura(CAST_DEATH_CINEMATIC_VISUAL);

            // spawn stuff and animate
            /*for(uint32 i = 0; i < 6; ++i)
            {
                float angle = ((2 * M_PI) / 6.0f) * i;
                //float hangle = rand_norm() * 2 * M_PI;
                float radius = (rand_norm() + 1.5f) * 12.0f;
                float xoffs = sin(angle) * radius;
                float yoffs = cos(angle) * radius;
                //float zoffs = fabs(sin(hangle) * radius);
                Creature *npc = m_creature->SummonCreatureCustom(NPC_ARCANECASTER, m_creature->GetPositionX() + xoffs, m_creature->GetPositionY() + yoffs,
                    m_creature->GetPositionZ(), 0, TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, timer_death_cinematic);
                if(npc)
                {
                    //npc->SetDisplayId(11686); // invisible
                    npc->setFaction(35); // make friendly
                    npc->CastSpell(m_creature, CAST_DEATH_CINEMATIC_BEAM, true); // TODO: fixme!
                }
            }*/
        }
        if(timer_death_cinematic < 100000) // when we should really die, the timer will be set to a very high value to indicate our time is over
        {
            m_creature->SetHealth(1);
            damage = 0;
        }
    }
}

void pe_andromedaAI::SpellHit(Unit *who, const SpellEntry *entry)
{
}

//*** HANDLED FUNCTION *** 
//Move in line of sight is called whenever any unit moves within our sight radius (something like 50 yards)
void pe_andromedaAI::MoveInLineOfSight(Unit *who)
{
}

void pe_andromedaAI::EnterCombat(Unit *target)
{
    DoScriptText(SAY_COMBATSTART,m_creature,target);

}

//*** HANDLED FUNCTION *** 
//Update AI is called Every single map update (roughly once every 100ms if a player is within the grid)
void pe_andromedaAI::UpdateAI(const uint32 diff)
{
    if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        return;

    if(timer_death_cinematic)
    {
        if(timer_death_cinematic <= diff)
        {
            timer_death_cinematic = uint32(-1); // end the fight, finally we die
            SelfKill(m_creature);
        }
        else timer_death_cinematic -= diff;
    }
    UpdateEmitters(diff);
    UpdateEnergyLocks(diff);
    ContinueCombatIfNecessary(diff);

    // if we are supposed to be paused, stop updating AI here
    if(timer_combat_pause)
        return;

    float HealthPCT = (float(m_creature->GetHealth()) / float(m_creature->GetMaxHealth())) * 100;

    ChangePhase(HealthPCT);

    // handle element switching
    if(element != ELEMENT_MADNESS)
    {
        if(timer_element < diff)
        {

            if(element == ELEMENT_FIRE)
            {
                element = roll_chance_f(33.3f) ? ELEMENT_ICE : ELEMENT_ARCANE;
            }
            else if(element == ELEMENT_ICE)
            {
                element = roll_chance_f(33.3f) ? ELEMENT_ARCANE : ELEMENT_FIRE;
            }
            else if(element == ELEMENT_ARCANE)
            {
                element = roll_chance_f(33.3f) ? ELEMENT_ICE : ELEMENT_FIRE;
            }
            //element = ELEMENT_FIRE; // PERFORMANCE: always use fire phase for cpu-hungry fire spawns


            // cast appropriate element shield
            switch(element)
            {
            case ELEMENT_FIRE:
                DoCast(m_creature,CAST_AURAOFFLAMES,true);
                break;
            case ELEMENT_ICE:
                DoCast(m_creature,CAST_ICEARMOR,true);
                DoCast(m_creature,CAST_FROSTAURA,true); // cast frost aura on all nearby enemies
                break;
            case ELEMENT_ARCANE:
                DoCast(m_creature,CAST_MAGEARMOR,true);
            }

            timer_element = 60000;
        }
        else timer_element -= diff;
    }

    // when leaving ice phase, remove Frost Aura from all nearby units
    if(element != ELEMENT_ICE)
    {
        std::list<Unit*> ulist = SelectNearUnits();
        for(std::list<Unit*>::iterator it = ulist.begin(); it != ulist.end(); it++)
            (*it)->RemoveAurasDueToSpell(CAST_FROSTAURA);
        ulist = SelectNearPlayers();
        for(std::list<Unit*>::iterator it = ulist.begin(); it != ulist.end(); it++)
            (*it)->RemoveAurasDueToSpell(CAST_FROSTAURA);

    }

    uint32 spawn;

    switch(element)
    {
        case ELEMENT_FIRE: spawn = NPC_FIRECASTER; break;
        case ELEMENT_ICE: spawn = NPC_ICECASTER; break;
        case ELEMENT_ARCANE: spawn = NPC_ARCANECASTER; break;
        default:
        {
            uint32 tmpelem = urand(ELEMENT_ICE, ELEMENT_ARCANE);
            switch(tmpelem)
            {
                case ELEMENT_FIRE: spawn = NPC_FIRECASTER; break;
                case ELEMENT_ICE: spawn = NPC_ICECASTER; break;
                case ELEMENT_ARCANE: spawn = NPC_ARCANECASTER; break;
                default: spawn = NPC_FIRECASTER;
            }
        }

    }

    // ... and here we can select only players or pets
    Unit *pVictim = m_creature->getVictim();
    Unit *pTarget = GetRandomTarget();
    Unit *pRnd = NULL;
    if(!pVictim)
        pRnd = pTarget;
    else if(!pTarget)
        pRnd = pVictim;
    else
        pRnd = ((urand(0,100) < 60) ? pVictim : pTarget);

    if(!pTarget)
        pTarget = pVictim;

    if(pRnd && pRnd->GetTypeId() != TYPEID_PLAYER && !(GUID_HIPART(pRnd->GetGUID()) & HIGHGUID_PET))
        pRnd = pTarget;

    if(pTarget && pRnd)
    {
        float speed = 0.77f + (urand(0,300) / 100.0f);

        if(timer_emit1 < diff)
        {
            uint32 rnd_emit1 = urand(0, 100);
            if(element == ELEMENT_ICE || element == ELEMENT_FIRE || (element == ELEMENT_MADNESS && rnd_emit1 < 33) )
            {
                BeamEmitter *emit = new BeamEmitter(m_creature,spawn,speed,20,40,4000, TEMPSUMMON_CORPSE_DESPAWN);
                emit->SetTarget(pRnd->GetPositionX(), pRnd->GetPositionY());
                emit->SetSource(m_creature->GetPositionX(),m_creature->GetPositionY(),m_creature->GetPositionZ(),m_creature->GetOrientation());
                emitters.push_back(emit);
            }
            else if(element == ELEMENT_ARCANE || (element == ELEMENT_MADNESS && rnd_emit1 >= 33 && rnd_emit1 < 66 && counter_emit1 == 0) )
            {
                RandomEmitter *emit = new RandomEmitter(m_creature,spawn,10,10,30,10000, TEMPSUMMON_CORPSE_DESPAWN);
                emit->SetSource(m_creature->GetPositionX(),m_creature->GetPositionY(),m_creature->GetPositionZ(),m_creature->GetOrientation());
                emitters.push_back(emit);
                counter_emit1 = 0; // dont cast this again in short time
            }
            else if(spawn == NPC_FIRECASTER || spawn == NPC_ICECASTER) // only in madness and with 33% chance. DONT spawn arcane balls here, it sucks
            {
                OutwardCircleEmitter *emit = new OutwardCircleEmitter(m_creature, spawn, 8, 35, 35, 10000, TEMPSUMMON_CORPSE_DESPAWN);
                emit->CreateChildBeams(1, 1.6f, false);
                emit->SetSource(m_creature->GetPositionX(),m_creature->GetPositionY(),m_creature->GetPositionZ(), float(rand_norm() * 2*M_PI) );
                emit->sourceo = float(rand_norm() * 2*M_PI);
                emitters.push_back(emit);
            }

            if(!counter_emit1)
            {
                counter_emit1 = phase;
                timer_emit1 = urand(6000,12000);
            }
            else
            {
                --counter_emit1;
                timer_emit1 = urand(1000,1800);
            }
        } else timer_emit1 -= diff;

        if(timer_emit2 < diff)
        {
            Emitter *emit = NULL;
            if(element == ELEMENT_FIRE)
            {
                if(urand(0,100) < 50)
                {
                    float spread = 0.2f + (urand(0,1000) / 2500.0f);
                    emit = new TripleBeamEmitter(m_creature,spawn,speed,15,40,4000, TEMPSUMMON_CORPSE_DESPAWN);
                    ((TripleBeamEmitter*)emit)->SetTarget(pTarget->GetPositionX(), pTarget->GetPositionY());
                    ((TripleBeamEmitter*)emit)->CreateChildBeams(0.35f);
                }
                else
                {
                    uint32 beams = 2;
                    if(HealthPCT < 90) beams++;
                    if(HealthPCT < 80) beams++;
                    if(HealthPCT < 45) beams++;
                    if(HealthPCT < 20) beams++; // <-- if we switched to madness before, this will never be called...
                    float circlefactor = 0.25f + ( urand(0,10000) / 8500.0f );
                    emit = new OutwardCircleEmitter(m_creature,spawn,speed,15,40,4000, TEMPSUMMON_CORPSE_DESPAWN);
                    ((OutwardCircleEmitter*)emit)->CreateChildBeams(beams,circlefactor,false);
                }
            }
            else if(element == ELEMENT_ICE)
            {
                emit = new RandomEmitter(m_creature,spawn,speed,25,40,4000, TEMPSUMMON_CORPSE_DESPAWN);
            }
            else if(element == ELEMENT_ARCANE)
            {
                emit = new RandomEmitter(m_creature,spawn,speed*1.35f,55,40,10000, TEMPSUMMON_CORPSE_DESPAWN);
            }
            else if(element = ELEMENT_MADNESS)
            {
                if( (spawn == NPC_ICECASTER || spawn == NPC_FIRECASTER) && urand(0,100) < 50)
                {
                    emit = new OutwardCircleEmitter(m_creature,spawn,speed,10,30,10000,TEMPSUMMON_CORPSE_DESPAWN);
                    ((OutwardCircleEmitter*)emit)->CreateChildBeams(4,0.75f,true); // random circling start position
                }
                else
                {
                    emit = new RandomEmitter(m_creature,spawn,speed*8.5f,45,40,10000, TEMPSUMMON_CORPSE_DESPAWN);
                }
            }


            if(emit)
            {
                emit->SetSource(m_creature->GetPositionX(),m_creature->GetPositionY(),m_creature->GetPositionZ(),m_creature->GetOrientation());
                emitters.push_back(emit);
            }
            timer_emit2 = urand(13000,20000);
            if(phase == 4)
                timer_emit2 += 4000;
        }
        else timer_emit2 -= diff;
    }

    if(timer_say_kill < diff)
        timer_say_kill = 0; // is processed in KilledUnit()
    else
        timer_say_kill -= diff;


    HandleSpellcasts(diff);
    ApplyEnergyLocksIfNecessary(diff);

}

void pe_andromedaAI::ApplyEnergyLocksIfNecessary(uint32 diff)
{
    if(phase < 1)
        return;

    if(timer_energylock < diff)
    {
        timer_energylock = urand(80000, 120000);
        PauseCombat(180000); // 3 mins timeout for everything (energylocks + ad spawns)

        std::list<Unit*> playerlist = SelectNearPlayers(50.0f);
        if(playerlist.empty())
            return;

        uint32 totalplayers = playerlist.size() / 5;
        totalplayers = std::max(totalplayers, phase + 1);
        totalplayers = std::min(totalplayers, uint32(playerlist.size()));
        if(!totalplayers)
            return;

        // how many locks per player
        uint32 locks;
        switch(phase)
        {
            case 1: locks = 3; break;
            case 2: locks = 4; break;
            default: locks = 5;
        }

        //totalplayers = 1;

        
        // clear selected player list from dead players and players who are still energylocked (this shouldnt happen anyway!)
        for(UnitListMap::iterator ulmit = energyLocks.begin(); ulmit != energyLocks.end(); ulmit++)
        {
            for(std::list<Unit*>::iterator plit = playerlist.begin(); plit != playerlist.end(); )
            {
                if(!(*plit)->isAlive() || energyLocks.find((*plit)->GetGUID()) != energyLocks.end() || ((Player*)(*plit))->isGameMaster())
                    plit = playerlist.erase(plit);
                else
                    plit++;
            }
        }

        for(uint32 p = 0; p < totalplayers && playerlist.size(); ++p)
        {
            std::list<Unit*>::iterator iChosen = playerlist.begin();
            advance(iChosen, urand(0, playerlist.size() - 1)); // choose random player
            Player *pChosen = (Player*)(*iChosen);
            playerlist.erase(iChosen); // remove that player from the list that it will not be chosen again
            uint32 lockentry = (urand(0, 100) < 75) ? NPC_ENERGYLOCK1 : NPC_ENERGYLOCK2;
            float factor = float((2*M_PI) / (float)locks);

            for(uint32 l = 0; l < locks; ++l)
            {
                UnitList& locklist = energyLocks[pChosen->GetGUID()];
                float angle = l * factor;
                float xoffs = sin(angle) * 12;
                float yoffs = cos(angle) * 12;
                Creature *summon = m_creature->SummonCreatureCustom(lockentry, pChosen->GetPositionX() + xoffs, pChosen->GetPositionY() + yoffs, pChosen->GetPositionZ(), angle, TEMPSUMMON_MANUAL_DESPAWN, 0);
                if(summon)
                {
                    // DO NOT set owner or beam spells of energy locks will not work!!
                    summon->FallGround();
                    ((pe_energylockGenericAI*)summon->AI())->target_guid = pChosen->GetGUID();
                    locklist.push_back(summon);
                }
            }

            pChosen->ApplySpellAura(CAST_SHADOWPRISON);
        }

        // dont make things too easy, spawn some ads
        if(phase && phase < 4)
        {
            for(uint32 i = 0; i < phase * 2; ++i)
            {
                float xoffs = 35.0f * (rand_norm() - 0.5f);
                float yoffs = 35.0f * (rand_norm() - 0.5f);

                Unit *spw = m_creature->SummonCreatureCustom(NPC_SPAWN_PRIEST, m_creature->GetPositionX() + xoffs, m_creature->GetPositionY() + yoffs, m_creature->GetPositionZ(), rand_norm() * 2 * M_PI, TEMPSUMMON_CORPSE_DESPAWN, 0);
                if(spw)
                {
                    spw->SetOwnerGUID(m_creature->GetGUID());
                    spw->Attack(m_creature->getVictim(), false); 
                }
            }

            for(uint32 i = 0; i < phase * 2; ++i)
            {
                float xoffs = 35.0f * (rand_norm() - 0.5f);
                float yoffs = 35.0f * (rand_norm() - 0.5f);

                Unit *spw = m_creature->SummonCreatureCustom(NPC_SPAWN_ROGUE, m_creature->GetPositionX() + xoffs, m_creature->GetPositionY() + yoffs, m_creature->GetPositionZ(), rand_norm() * 2 * M_PI, TEMPSUMMON_CORPSE_DESPAWN, 0);
                if(spw)
                {
                    spw->SetOwnerGUID(m_creature->GetGUID());
                    spw->Attack(GetRandomTarget(), false); 
                }
            }
        }


    }
    else timer_energylock -= diff;

}

void pe_andromedaAI::UpdateEnergyLocks(uint32 diff)
{
    if(energyLocks.empty() )
    {
        std::list<Unit*> priestList;
        std::list<Unit*> rogueList; // TODO: should rogue ever be implemented, uncomment this
        priestList = SelectNearUnitsWithEntry(NPC_SPAWN_PRIEST, 60.0f);
        rogueList = SelectNearUnitsWithEntry(NPC_SPAWN_ROGUE, 60.0f);
        if(priestList.empty() && rogueList.empty())
        {
            timer_combat_pause = 1; // DO NOT set to 0 here!!
            return;
        }
    }

    for(UnitListMap::iterator ulmit = energyLocks.begin(); ulmit != energyLocks.end(); )
    {
        UnitList& locklist = ulmit->second;
        bool allDone = true;

        // check if all locks have isDone == true, which means players have won and all locks for that player can be despawned
        for(UnitList::iterator ulit = locklist.begin(); ulit != locklist.end(); ulit++)
        {
            if( (*ulit)->isAlive() && !((pe_energylockGenericAI*)((Creature*)(*ulit))->AI())->isDone )
            {
                allDone = false;
                break;
            }
        }
        // can safely despawn now
        if(allDone)
        {
            Player *victim;
            for(UnitList::iterator ulit = locklist.begin(); ulit != locklist.end(); ulit++)
            {
                // free player from buffs (this is called multiple times, shouldnt be too bad)
                victim = m_creature->GetMap()->GetPlayer(((pe_energylockGenericAI*)((Creature*)(*ulit))->AI())->target_guid);
                if(victim)
                {
                    victim->RemoveAurasDueToSpell(CAST_HEALINGREDUCE);
                    victim->RemoveAurasDueToSpell(CAST_SHADOWPRISON);
                }
                // SummonCreatureCustom() returns Creature*, but works internally with TemporarySummon*, so the following typecast is safe:
                ((TemporarySummon*)(*ulit))->UnSummon();
            }
            if(victim)
            {
                energyLocks.erase(victim->GetGUID());
                ulmit = energyLocks.begin();
            }

        }
        else
        {
            ulmit++;
        }
    }
}

void pe_andromedaAI::UnsummonAllEnergyLocks(void)
{
    for(UnitListMap::iterator ulmit = energyLocks.begin(); ulmit != energyLocks.end(); ulmit++)
    {
        UnitList& locklist = ulmit->second;
        for(UnitList::iterator ulit = locklist.begin(); ulit != locklist.end(); ulit++)
        {
            ((TemporarySummon*)(*ulit))->UnSummon();
    
            Player *victim = m_creature->GetMap()->GetPlayer(((pe_energylockGenericAI*)((Creature*)(*ulit))->AI())->target_guid);
            if(victim)
            {
                victim->RemoveAurasDueToSpell(CAST_HEALINGREDUCE);
                victim->RemoveAurasDueToSpell(CAST_SHADOWPRISON);
            }
        }
    }
    energyLocks.clear();
}

//This is the GetAI method used by all scripts that involve AI
//It is called every time a new creature using this script is created
CreatureAI* GetAI_pe_andromeda(Creature *_Creature)
{
    return new pe_andromedaAI (_Creature);
}

struct MANGOS_DLL_DECL pe_firecasterAI : public pe_andromeda_minionGenericAI
{
    pe_firecasterAI(Creature *c) : pe_andromeda_minionGenericAI(c)
    {
        Reset();
        SetCombatMovement(false);
    }
    uint32 timer, dietimer, castcount;
    bool spawnvisual_casted;

    void Reset()
    {
        spawnvisual_casted = false;
        timer = 200;
        dietimer = 2100;
        castcount = 0;
    }

    void UpdateAI(const uint32 diff)
    {
        if(!spawnvisual_casted)
        {
            spawnvisual_casted = true;
            DoCast(m_creature, 19484, true);
        }

        if(timer < diff)
        { 
            DoCast(m_creature,5113); // Living Flames
            ++castcount;
            timer = 500;
        } else timer -= diff;

        if(dietimer < diff)
        {
            // Suicide (hack because despawn of creatures in combat doesnt work...)
            SelfKill(m_creature);
            dietimer = 1000; // o_O
        } else dietimer -= diff;

        if(castcount >= 2)
        {
            dietimer = std::min(dietimer, uint32(150));
        }
    }

    void Aggro(Unit *who)
    {
    }

    void MoveInLineOfSight(Unit *who)
    {
    }

    void AttackStart(Unit *who)
    {
    }
};

CreatureAI* GetAI_pe_firecaster(Creature *_Creature)
{
    return new pe_firecasterAI (_Creature);
}

struct MANGOS_DLL_DECL pe_icecasterAI : public pe_andromeda_minionGenericAI
{
    pe_icecasterAI(Creature *c) : pe_andromeda_minionGenericAI(c)
    {
        SetCombatMovement(false);
        Reset();
    }
    uint32 timer, dietimer;

    void Reset()
    {
        timer = 3500;
        dietimer = 4200;
    }

    void UpdateAI(const uint32 diff)
    {
        if(timer < diff)
        {
            DoCast(m_creature, 16803 /*37478*/); // Flash freeze
            timer = 4000;
        } else timer -= diff;

        if(dietimer < diff)
        {
            // Suicide (hack because despawn of creatures in combat doesnt work...)
           SelfKill(m_creature);
            dietimer = 1000; // o_O
        } else dietimer -= diff;
    }

    void SpellHit(Unit *who, const SpellEntry *spell)
    {
    }

    void DamageTaken(Unit *done_by, uint32 &damage)
    {
        if(IsAlliedUnit(done_by))
        {
            damage = 0;
            return;
        }
    }

    void Aggro(Unit *who)
    {
    }

    void MoveInLineOfSight(Unit *who)
    {
    }

    void AttackStart(Unit *who)
    {
    }
};

CreatureAI* GetAI_pe_icecaster(Creature *_Creature)
{
    return new pe_icecasterAI (_Creature);
}

struct MANGOS_DLL_DECL pe_arcanecasterAI : public pe_andromeda_minionGenericAI
{
    pe_arcanecasterAI(Creature *c) : pe_andromeda_minionGenericAI(c)
    {
        Reset();
        SetCombatMovement(false);
    }
    uint32 timer;
    int32 explode_timer;
    uint32 unsummon_timer;
    bool exploded;
    bool unsummon_after_cast;
    void Reset(void)
    {
        explode_timer = -1;
        timer = 50;
        unsummon_timer = 10000;
        unsummon_after_cast = false;
        exploded = false;
    }

    void UpdateAI(const uint32 diff)
    {
        if(timer < diff)
        {
            //DoCast(m_creature,25938); // Viscidus Explode Trigger // TODO: keep disabled for weaker machines?
            timer = -1; // disable
        } else timer -= diff;

        if(explode_timer >= 0)
        {
            if(explode_timer < (int32)diff)
            {
                ExplodeCast2();
                timer = -1; // disable
                explode_timer = -1;
            } else explode_timer -= diff;
        }

        // die if time is over
        if(unsummon_timer < diff)
        {
            if(!unsummon_after_cast)
            {
                exploded = true;
                SelfKill(m_creature);
            }
        } else unsummon_timer -= diff;

        if(unsummon_after_cast && !m_creature->FindCurrentSpellBySpellId(CAST_EXPLODE2))
        {
            unsummon_after_cast = false;
            unsummon_timer = 150;
        }

    }

    void Aggro(Unit *who)
    {
    }

    void MoveInLineOfSight(Unit *who)
    {
    }

    void AttackStart(Unit *who)
    {
    }

    void DamageTaken(Unit *who, uint32& damage)
    {
        if(IsAlliedUnit(who) && who != m_creature)
        {
            damage = 0;
            return;
        }
        /*if(explode_timer < 0 || !exploded)
        {
            damage = 0;
        }*/
        
        Explode();
    }

    void SpellHit(Unit *who, const SpellEntry *spell)
    {
        if(spell->Id == CAST_ARCANEVOLLEY && explode_timer < 0)
        {
            explode_timer = 5000; // + 5 sec cast time = 10 secs total
            unsummon_timer = 13000;
            DoCast(m_creature, 22518, true); // red glow
            return;
        }

        if(IsAlliedUnit(who)) // hack not to explode from related units
            return;

        Explode();
    }

    void Explode()
    {
        if(exploded || explode_timer >= 0)
            return;
        exploded = true;
        DoCast(m_creature,CAST_EXPLODE);
        unsummon_timer = 150;
    }

    void ExplodeCast2(bool trig = false)
    {
        if(exploded)
            return;
        exploded = true;
        unsummon_after_cast = true;
        DoCast(m_creature, CAST_EXPLODE2, trig);
    }
};

CreatureAI* GetAI_pe_arcanecaster(Creature *_Creature)
{
    return new pe_arcanecasterAI (_Creature);
}


struct MANGOS_DLL_DECL pe_energylock1AI : public pe_energylockGenericAI
{

    pe_energylock1AI(Creature *c) : pe_energylockGenericAI(c)
    {
    }

    void UpdateAI(const uint32 diff)
    {
        if(timerSuicide)
        {
            if(timerSuicide <= diff)
            {
                timerSuicide = uint32(-1);
                SelfKill(m_creature);
            }
            else timerSuicide -= diff;
        }

        if(isDone)
            return;

        // player died... we are no longer needed
        Unit *victim = m_creature->GetMap()->GetUnit(target_guid);
        if(victim)
        {
            if(!victim->isAlive())
            {
                isDone = true;
                // KilledUnit(victim); // will be called in DealDamage()
                return;
            }
        }
        else
            return;

        // apply or refresh healing reduce buff
        if(!victim->HasAura(CAST_HEALINGREDUCE))
            victim->ApplySpellAura(CAST_HEALINGREDUCE);

        // refresh stun if necessary
        if(!victim->HasAura(CAST_SHADOWPRISON))
            victim->ApplySpellAura(CAST_SHADOWPRISON);

        if(!beamCasted)
        {
            DoCast(victim, CAST_BEAM_1, true);
            beamCasted = true;
        }

        // periodically deal damage to victim player
        if(timer_damage < diff)
        {
            m_creature->DealDamage(victim, m_creature->GetMaxHealth() * 0.01f, NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
            m_creature->SetPower(POWER_MANA,m_creature->GetPower(POWER_MANA) * 0.97f);
            timer_damage = 1000;
        }
        else
            timer_damage -= diff;
    }


    void JustDied(Unit *killer)
    {
        isDone = true;
    }


};

CreatureAI* GetAI_pe_energylock1(Creature *_Creature)
{
    return new pe_energylock1AI (_Creature);
}



struct MANGOS_DLL_DECL pe_energylock2AI : public pe_energylockGenericAI
{
    pe_energylock2AI(Creature *c) : pe_energylockGenericAI(c)
    {
        c->SetHealth(uint32(c->GetMaxHealth() * 0.75f));
        //c->setFaction(1770);
        //c->SetFlag(UNIT_FIELD_BYTES_2, UNIT_BYTE2_FLAG_SANCTUARY << 8);
        c->SetSpoofSamePlayerFaction(true);
        c->ForceValuesUpdateAtIndex(UNIT_FIELD_BYTES_2);
        c->ForceValuesUpdateAtIndex(UNIT_FIELD_FACTIONTEMPLATE);
    }

    void UpdateAI(const uint32 diff)
    {
        if(timerSuicide)
        {
            if(timerSuicide <= diff)
            {
                timerSuicide = uint32(-1);
                SelfKill(m_creature);
            }
            else timerSuicide -= diff;
        }

        if(isDone)
            return;

        // if we were healed up, player has won and will be freed by parent if all other same npcs were also despawned
        if(m_creature->GetHealth() > m_creature->GetMaxHealth() * 0.97f)
        {
            isDone = true;
            SelfKill(m_creature);
            return;
        }

        // player died... we are no longer needed
        Unit *victim = m_creature->GetMap()->GetUnit(target_guid);
        if(victim)
        {
            if(!victim->isAlive())
            {
                isDone = true;
                return;
            }
        }
        else
            return;

        // maybe THIS fixes the andromeda-heal-up bug in madness phase....
        if(IsAlliedUnit(victim))
            return;

        // refresh stun if necessary
        if(!victim->HasAura(CAST_SHADOWPRISON))
            victim->ApplySpellAura(CAST_SHADOWPRISON);

        if(!beamCasted)
        {
            DoCast(victim, CAST_BEAM_2, true);
            beamCasted = true;
        }

        // periodically deal damage to self (other player must heal us, else victim player dies)
        // nothing speaks against healing victim player during that time
        if(timer_damage < diff)
        {
            timer_damage = 1000;
            // HACK: for some reason DealDamage is NOT called in the line below; the following if manages explosion show and is called when players didnt heal up
            if(m_creature->GetHealth() <= m_creature->GetMaxHealth() * 0.03f)
            {
                DoCast(m_creature, CAST_EXPLODE_VISUAL, true);
                timerSuicide = 300;
                return; // do not deal damage, wait for suicide
            }


            m_creature->DealDamage(m_creature, m_creature->GetMaxHealth() * 0.0233f, NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
            if(isDone)
            {
                // this part of code is called when players did NOT manage to heal up in time
                return;
            }
            m_creature->DealHeal(victim, victim->GetMaxHealth() * 0.01f, NULL, false);
            m_creature->SetPower(POWER_MANA,m_creature->GetPower(POWER_MANA) * 1.02f);

        }
        else
            timer_damage -= diff;

        float HealthPCT = (float(m_creature->GetHealth()) / float(m_creature->GetMaxHealth())) * 100;
        if(HealthPCT < 20.0f)
        {
            if(!m_creature->HasAura(CAST_MARK))
                m_creature->ApplySpellAura(CAST_MARK);
        }
        else if(HealthPCT > 33.0f)
        {
            m_creature->RemoveAurasDueToSpell(CAST_MARK);
        }
    }

    void JustDied(Unit *killer)
    {
        if(isDone)
            return;

        // if we die, kill victim player too
        if(Unit *victim = m_creature->GetMap()->GetUnit(target_guid))
        {
            if(victim && victim->isAlive())
            {
                isDone = true;
                m_creature->DealDamage(victim, victim->GetHealth(), NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
                // KilledUnit(victim); // will be called in DealDamage()
            }
        }
    }
};

CreatureAI* GetAI_pe_energylock2(Creature *_Creature)
{
    return new pe_energylock2AI (_Creature);
}


struct MANGOS_DLL_DECL pe_andromeda_priestAI : public pe_andromeda_minionGenericAI
{
    // Unit searcher - list of healable units in range
    std::list<Unit*> DoSelectMissingHpFriendlyList(float fRange, uint32 uiMinHPDiff)
    {
        CellPair p(MaNGOS::ComputeCellPair(m_creature->GetPositionX(), m_creature->GetPositionY()));
        Cell cell(p);
        cell.data.Part.reserved = ALL_DISTRICT;
        cell.SetNoCreate();

        std::list<Unit*> uList;

        MaNGOS::MostHPMissingInRange u_check(m_creature, fRange, uiMinHPDiff);
        MaNGOS::UnitListSearcher<MaNGOS::MostHPMissingInRange> searcher(m_creature, uList, u_check);

        /*
        typedef TYPELIST_4(GameObject, Creature*except pets*, DynamicObject, Corpse*Bones*) AllGridObjectTypes;
        This means that if we only search grid then we cannot possibly return pets or players so this is safe
        */
        TypeContainerVisitor<MaNGOS::UnitListSearcher<MaNGOS::MostHPMissingInRange>, GridTypeMapContainer >  grid_unit_searcher(searcher);


        cell.Visit(p, grid_unit_searcher, *(m_creature->GetMap()));

        return uList;
    }

    bool shadowPhase;
    uint64 psychicScreamTarget;
    uint32 timer_globalcd; // global cooldown timer
    uint32 timer_heal;
    uint32 timer_mindwarp;
    uint32 timer_rayofpain;
    uint32 timer_psychicscream;
    uint32 timer_shadowwordpain;
    uint32 timer_melee;
    uint32 timer_drainmana;



    pe_andromeda_priestAI(Creature *c) : pe_andromeda_minionGenericAI(c)
    {
        c->SetDisplayId(priestModels[urand(0, (sizeof(priestModels) / sizeof(uint32)) - 1)]);
        Reset();
    }

    void Reset()
    {
        SetCombatMovement(false);
        shadowPhase = false;
        psychicScreamTarget = 0;
        timer_globalcd = 2000;
        timer_heal = 1000;
        timer_mindwarp = 500;
        timer_rayofpain = 2000;
        timer_psychicscream = 4000;
        timer_shadowwordpain = 6200;
        timer_melee = 1000;
        timer_drainmana = 5000;
    }

    void MoveInLineOfSight(Unit *who)
    {
        //if(shadowPhase)
            ScriptedAI::MoveInLineOfSight(who);
    }

    void AttackStart(Unit *who)
    {
        //if(shadowPhase)
            ScriptedAI::AttackStart(who);
    }

    void UpdateAI(const uint32 diff)
    {
        // decrease timers here, since we use the global cooldown and cast timers would not be decremented otherwise
        if(shadowPhase)
        {
            if(timer_mindwarp > diff)
                timer_mindwarp -= diff;
            else
                timer_mindwarp = 0;

            if(timer_rayofpain > diff)
                timer_rayofpain -= diff;
            else
                timer_rayofpain = 0;

            if(timer_shadowwordpain > diff)
                timer_shadowwordpain -= diff;
            else
                timer_shadowwordpain = 0;
        }
        else
        {
            if(timer_heal > diff)
                timer_heal -= diff;
            else
                timer_heal = 0;

            if( float(m_creature->GetPower(POWER_MANA)) / float(m_creature->GetMaxPower(POWER_MANA)) < 0.3f )
            {
                if(timer_drainmana > diff)
                    timer_drainmana -= diff;
                else
                    timer_drainmana = 0;
            }
        }

        float HealthPCT = (float(m_creature->GetHealth()) / float(m_creature->GetMaxHealth())) * 100;
        if(HealthPCT < 50.0f && !shadowPhase)
        {
            shadowPhase = true;
            SetCombatMovement(true);
            m_creature->GetMotionMaster()->MoveChase(m_creature->getVictim());
            m_creature->AddThreat(m_creature->getVictim(), 20.0f);
            m_creature->ApplySpellAura(CAST_SHADOWFORM);
            return; // return once here
        }

        if(shadowPhase)
        {
            //Return since we have no target
            if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
                return;

            // hack to get the movement right after point movement expired... not possible to set this is MovementExpired()
            if(m_creature->GetMotionMaster()->GetCurrentMovementGeneratorType() == IDLE_MOTION_TYPE)
            {
                m_creature->GetMotionMaster()->MoveChase(m_creature->getVictim());
            }

            // hacky melee combat, as spellcasting would override it otherwise
            if(timer_melee < diff)
            {
                //If we are within range melee the target
                if (m_creature->IsWithinDistInMap(m_creature->getVictim(), ATTACK_DISTANCE))
                {
                    m_creature->AttackerStateUpdate(m_creature->getVictim());
                    timer_melee = m_creature->GetAttackTime(BASE_ATTACK);
                }
            }
            else timer_melee -= diff;

            if(timer_psychicscream < diff)
            {
                timer_psychicscream = urand(15000, 23000);
                psychicScreamTarget = 0;
                uint32 listsize = m_creature->getThreatManager().getThreatList().size();
                uint32 listpos = urand(0, std::min(listsize - 1, uint32(4))); // select up to 4 positions down from top aggro
                Unit *target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_TOPAGGRO, listpos);
                if(target)
                {
                    // hacky, move to absolute player point; this can be done more nicely but this is safe at least
                    //m_creature->GetMotionMaster()->MovePoint(1, target->GetPositionX(), target->GetPositionY(), target->GetPositionZ());
                    // cast will be done in MovementInform

                    // better to solve it with aggro reset
                    psychicScreamTarget = target->GetGUID();
                    DoResetThreat();
                    m_creature->AddThreat(target, 400.0f);
                    m_creature->GetMotionMaster()->Clear();
                    m_creature->GetMotionMaster()->MoveChase(target);
                }
            }
            else timer_psychicscream -= diff;

            // if we have a target, check if we are near enough to cast psychic scream
            if(psychicScreamTarget)
            {
                Unit *target = m_creature->GetMap()->GetUnit(psychicScreamTarget);
                if(target)
                {
                    if(m_creature->GetDistance2d(target) <= 5.0f)
                    {
                        timer_psychicscream = urand(25000, 32000);
                        psychicScreamTarget = 0;
                        DoCast(m_creature, CAST_PSYCHICSCREAM);
                        m_creature->GetMotionMaster()->Clear();
                        m_creature->GetMotionMaster()->MoveChase(m_creature->getVictim());
                    }
                }
                else
                {
                    timer_psychicscream = urand(5000, 8000);
                    psychicScreamTarget = 0;
                }
            }

        }


        if(timer_globalcd < diff)
        {
            timer_globalcd = 300; // limit creature searching to once each 300ms

            if(shadowPhase)
            {
                //Return since we have no target
                if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
                    return;

                if(timer_mindwarp < diff)
                {
                    timer_mindwarp = urand(5000, 10000);
                    Unit *target = NULL;
                    for(uint32 i = 0; i < 5; i++) // 5 tries to select correct target
                    {
                        target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0);
                        if(!target)
                            break; // no unit could be selected, useless to try again
                        if(IsAlliedUnit(target))
                        {
                            target = NULL;
                            continue; // this shouldnt happen....
                        }
                        if(target->GetTypeId() == TYPEID_PLAYER && (
                               ((Player*)target)->getClass() == CLASS_WARRIOR
                            || ((Player*)target)->getClass() == CLASS_ROGUE
                            || ((Player*)target)->getClass() == CLASS_DEATH_KNIGHT
                            ))
                        {
                            target = NULL;
                            continue; // player isnt mana user, spell isnt needed for this class
                        }
                        if(!target)
                            target = m_creature->getVictim();

                        DoCast(target, CAST_MINDWARP);
                        timer_globalcd = 2000;
                        break;
                    }
                }

                if(timer_rayofpain < diff)
                {
                    timer_rayofpain = 2200;
                    Unit *target = m_creature->getVictim();
                    if(m_creature->getThreatManager().getThreatList().size() >= 2)
                    {
                        uint32 pos = (urand(0,100) < 33) ? 1 : 0;
                        target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_TOPAGGRO, pos);
                    }
                    uint32 spell = (urand(0,100) < 93) ? CAST_RAYOFPAIN : CAST_RAYOFSUFFERING;
                    DoCast(target, spell);
                }

                if(timer_shadowwordpain < diff)
                {
                    timer_shadowwordpain = urand(5000, 8000);
                   Unit *target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0);
                   if(target)
                       DoCast(target, CAST_SHADOWWORDPAIN);
                }

            }
            else
            {
                if(timer_heal < diff)
                {
                    Unit *healTarget = NULL;
                    if(roll_chance_i(20))
                    {
                        healTarget = m_creature->GetOwner();
                    }
                    if(!healTarget)
                    {
                        std::list<Unit*> ulist = DoSelectMissingHpFriendlyList(40.0f, 8000);
                        if(ulist.size())
                        {
                            for(std::list<Unit*>::iterator it = ulist.begin(); it != ulist.end(); )
                            {
                                // remove all unneeded units from list
                                Unit *u = *it;
                                switch(u->GetEntry())
                                {
                                    case NPC_SPAWN_PRIEST:
                                    case NPC_SPAWN_ROGUE:
                                        it++;
                                        break;
                                    default:
                                        it = ulist.erase(it);
                                }
                            }
                        }
                        if(ulist.size())
                        {
                            std::list<Unit*>::iterator selUnit = ulist.begin();
                            advance(selUnit, urand(0, ulist.size() - 1));
                            healTarget = *selUnit;
                        }
                    }
                    if(healTarget)
                    {
                        if(m_creature->GetPower(POWER_MANA) >= MANACOST_PRIEST_HEAL)
                        {
                            m_creature->SetPower(POWER_MANA, m_creature->GetPower(POWER_MANA) - MANACOST_PRIEST_HEAL);
                            DoCast(healTarget, CAST_GREATERHEAL);
                            timer_heal = urand(3500, 4500);
                            timer_globalcd = 2000;
                        }
                    }
                    else // nothing healable found, cause some damage instead
                    {
                        if(m_creature->GetPower(POWER_MANA) >= MANACOST_PRIEST_SMITE)
                        {
                            m_creature->SetPower(POWER_MANA, m_creature->GetPower(POWER_MANA) - MANACOST_PRIEST_SMITE);
                            DoCast(m_creature->getVictim(), CAST_SMITE);
                            timer_globalcd = 2000;
                            timer_heal = urand(2800,3800);
                        }
                    }
                }
                if(timer_drainmana < diff)
                {
                    timer_drainmana = urand(4000, 6000);
                    Unit *target = NULL;
                    for(uint32 i = 0; i < 50; i++) // 50 tries to select correct target
                    {
                        target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0);
                        if(!target)
                            break; // no unit could be selected, useless to try again
                        if(IsAlliedUnit(target))
                        {
                            target = NULL;
                            continue; // this shouldnt happen....
                        }
                        if(target->GetTypeId() == TYPEID_PLAYER && (
                            ((Player*)target)->getClass() == CLASS_WARRIOR
                            || ((Player*)target)->getClass() == CLASS_ROGUE
                            || ((Player*)target)->getClass() == CLASS_DEATH_KNIGHT
                            ))
                        {
                            target = NULL;
                            continue; // player isnt mana user, spell isnt needed for this class
                        }
                        if(target)
                        {
                            if(target->GetPower(POWER_MANA) < 500)
                            {
                                target = NULL;
                                continue;
                            }
                            m_creature->InterruptSpell(CURRENT_GENERIC_SPELL);
                            DoCast(target, CAST_DRAINMANA, true);
                            timer_globalcd = 5500; // need 5 secs here, since this spell is channeled
                            break;
                        }
                    }
                    if(!target && m_creature->GetPower(POWER_MANA) < 1000) // if we really didnt find anything useful, go to shadow form... nothing to do in holy form
                    {
                        shadowPhase = true;
                        SetCombatMovement(true);
                        m_creature->GetMotionMaster()->MoveChase(m_creature->getVictim());
                        m_creature->AddThreat(m_creature->getVictim(), 20.0f);
                        m_creature->ApplySpellAura(CAST_SHADOWFORM);
                    }
                }
            }
        }
        else timer_globalcd -= diff;    
    }

    void DamageTaken(Unit* pDoneBy, uint32& uiDamage)
    {
        if(IsAlliedUnit(pDoneBy))
            uiDamage = 0;
    }
};

CreatureAI* GetAI_pe_andromeda_priest(Creature *_Creature)
{
    return new pe_andromeda_priestAI (_Creature);
}


struct MANGOS_DLL_DECL pe_andromeda_rogueAI : public pe_andromeda_minionGenericAI
{
    pe_andromeda_rogueAI(Creature *c) : pe_andromeda_minionGenericAI(c)
    {
        c->SetDisplayId(rogueModels[urand(0, (sizeof(rogueModels) / sizeof(uint32)) - 1)]);
        Reset();
    }

    uint32 timer_throw;
    uint32 timer_disarm;
    uint32 timer_frenzy;
    uint32 counter_changetarget;
    uint32 timer_backstab;
    uint32 timer_shadowstep;
    uint32 timer_kidneyshot;
    uint32 timer_sprint;
    uint32 timer_changetarget;

    void Reset()
    {
        SetCombatMovement(true);
        timer_throw = 5000;
        timer_disarm = 3000;
        timer_frenzy = 60000;
        counter_changetarget = urand(3,8);
        timer_backstab = -1;
        timer_shadowstep = 7000;
        timer_kidneyshot = 4000;
        timer_sprint = -1;
        timer_changetarget = 12000;
    }

    void MoveInLineOfSight(Unit *who)
    {
        ScriptedAI::MoveInLineOfSight(who);
    }

    void AttackStart(Unit *who)
    {
        ScriptedAI::AttackStart(who);
    }

    void UpdateAI(const uint32 diff)
    {
        //Return since we have no target
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if(timer_frenzy < diff)
        {
            timer_frenzy = 10000;
            if(!m_creature->HasAura(CAST_FRENZY))
                DoCast(m_creature, CAST_FRENZY, true);
        }
        timer_frenzy -= diff;

        Unit *newTarget = NULL;


        if(timer_changetarget < diff)
        {
            newTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0);
            if(counter_changetarget)
            {
                timer_changetarget = urand(4000, 9000);
                --counter_changetarget;
            }
            else
            {
                timer_changetarget = urand(15000, 30000);
                counter_changetarget = urand(3,8);
            }
        }
        else
            timer_changetarget -= diff;

        if(newTarget)
        {
            bool method = (urand(0, 100) < 43); // true: shadowstep, false: sprint
            if(!method)
            {
                DoCast(m_creature->getVictim(), CAST_KIDNEYSHOT, true);
            }
            DoResetThreat();
            m_creature->AddThreat(newTarget, 10000.0f); // need 10k dmg to change target, or some taunt
            m_creature->SelectHostileTarget();
            if(method)
            {
                // shadowstep method
                DoCast(m_creature, CAST_SHADOWSTEP, true);
                timer_kidneyshot = 200;
                timer_backstab = 500;
                timer_disarm = 2500;
            }
            else
            {
                // sprint method
                DoCast(m_creature, CAST_SPRINT, true);
                timer_kidneyshot = 1000;
                timer_disarm = 2500;
            }
        }

        if(timer_disarm < diff)
        {
            if(m_creature->GetDistance(m_creature->getVictim()) <= 5.0f)
            {
                DoCast(m_creature->getVictim(), CAST_DISARM, false);
                timer_disarm = urand(7000, 17000);
            }
            else
                timer_disarm = 0;
        }
        else
            timer_disarm -= diff;

        if(timer_kidneyshot < diff)
        {
            if(m_creature->GetDistance(m_creature->getVictim()) <= 5.0f)
            {
                DoCast(m_creature->getVictim(), CAST_KIDNEYSHOT, false);
                timer_kidneyshot = urand(5000, 8000);
            }
            else
                timer_kidneyshot = 0;
        }
        else
            timer_kidneyshot -= diff;

        if(timer_throw < diff)
        {
            timer_throw = urand(8000, 12000);
            Unit *target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0);
            if(target)
                DoCast(target, CAST_THROW_DAGGER, false);
        }
        else
            timer_throw -= diff;


        DoMeleeAttackIfReady();
    }




    void DamageTaken(Unit* pDoneBy, uint32& uiDamage)
    {
        if(IsAlliedUnit(pDoneBy))
            uiDamage = 0;
    }
};

CreatureAI* GetAI_pe_andromeda_rogue(Creature *_Creature)
{
    return new pe_andromeda_rogueAI (_Creature);
}

//This is the actual function called only once during InitScripts()
//It must define all handled functions that are to be run in this script
//For example if you want this Script to handle Emotes you must include
//newscript->ReciveEmote = My_Emote_Function;
void AddSC_pe_andromeda()
{
    Script *newscript;
    newscript = new Script;
    newscript->Name="pe_andromeda";
    newscript->GetAI = GetAI_pe_andromeda;
    //newscript->pReceiveEmote = &ReciveEmote_pe_andromeda;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name="pe_firecaster";
    newscript->GetAI = GetAI_pe_firecaster;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name="pe_icecaster";
    newscript->GetAI = GetAI_pe_icecaster;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name="pe_arcanecaster";
    newscript->GetAI = GetAI_pe_arcanecaster;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name="pe_energylock1";
    newscript->GetAI = GetAI_pe_energylock1;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name="pe_energylock2";
    newscript->GetAI = GetAI_pe_energylock2;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name="pe_andromeda_priest";
    newscript->GetAI = GetAI_pe_andromeda_priest;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name="pe_andromeda_rogue";
    newscript->GetAI = GetAI_pe_andromeda_rogue;
    newscript->RegisterSelf();
}
