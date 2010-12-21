
#include "precompiled.h"
#include "Spell.h"
#include "pe_structs.h"

// note: this script is best triggered with spell 46673

// + : good effect for caster
// - : bad effect for caster
// ? : wtf effect for caster
static uint32 spellList[] =
{
    34109, // +      whirlwind knockback (some damage)
    46360, // ++     ice spear knockback (little damage)
    35788, // +++    arakkoa egg debuff (poison damage)
    14535, // +++++  dredge sickness, -25% to all stats
    36810, // --     rotting putrescence (sometimes stun + hp loss
    30898, // +++++  SW: Pain to every hostile unit in 100y
    54417, // ++++   Ray of Suffering, 8% hp loss to hostile target per sec for 5 sec
    52723, // ++++   Vampiric touch, heal for 50% of dmg
    70449, // ++++   Vampire Rush, ~4k AOE dmg + AOE knockback
    32839, // ???    Beam (Red) - useless, but looks cool
    40647, // +++    Shadow prison (30s stun to everyone near, but also immune to dmg)
    45922, // ----   Shadow prison (2 mins stun to self, not immune) -> HACK to last only 30s
    64970, // ++++   Fuse Lightning (damage to target + nearby, wears off after 1 effect [BUG])
    64889, // +++    Lightning charged (3x ~3k dmg to nearby targets on hit)
    36914, // ----   Magic damage increased by 100%
    36657, // ++++   Death Count (to all targets in range, 5k dmg after 15 secs)
    23127, // ---    Death's Door (disease, 5% hp loss every 3 sec, inf duration)
    //62269, // +++++  Rune of Death (AOE, ~5k dps to hostile)
    //28433, // +-+-+  Explode (caster dies, but with 30k damage in 5y aoe range)
    42696, // +++??  Drunken master, weird AOE stun
    67468, // -??    Drunken vomit, self stun for 2 sec
    8554,  // ?      Get really drunk
    29040, // +      +6 mana every 5 sec
    38208, // --     Burning poison, low periodic damage for 30 secs
    54714, // ++     Acid Volley, 250 dmg/sec to enemies, stackable
    45862, // ++     Blink
    16319, // ++++   Touch of Vaelastrasz
    16332, // +++++  Divine Touch of Vaelastrasz
    6614,  // -      Cowardly Flight (run like maniac, but can be cancelled by player)
    24684, // +++    Chain burn (burn ~1500 mana + 50% of mana burned as damage)
    5024,  // -      Flee (some damage + chicken effect)
    23965, // +++++  Instant heal (full)
    14539, // +++    Putrid Enzyme (+50% shadow/nature damage, AOE)
    23126, // ??     World Enlarger
    38441, // ++++   Cataclymic Bolt (-50% of total hp to enemy)
    20547, // +++    Fun bomb (damage friends of target, can be casted on hostile+friendly)
    16711, // +?     Grow (size increase + slight run speed increase)
    24207, // ??     Hellfire cast Visual (knocks down all allies)
    36354, // ??     Kael Explodes (screen rumble)
    55529, // ??     Boom! Does nothing, but looks cool
    55086, // ??     Looks cool, some flame aura kinda
    8892,  // ++     Goblin Rocket Boots (+speed)
    30452, // ++-?   Ludicrous speed!!! + Spinning
    13120, // ++     Net-O-Matic projector



    // terminating the list with malfunction
    55529,
    55529,
    55529
};

static uint32 modelList[] =
{
    666,   // wolf
    5555,  // kitten
    18718, // mediv
    21304, // golden pig
    23732, // fire demon
    11331, // satyr
    11355, // peasant with wood
    25041, // troll pirate
    19881, // NE druid
    19607, // big evil cat
    19517, // haris pilton
    19193, // BE in drees
    19147, // mech draenei
    18515, // cat
    18261, // tiger guy
    8889,  // orc mofa
    12345, // white imp
    25050, // pirate orc f
    11239, // troll f
    25051, // tauren pirate f
    19339, // xmas goblin f
    19342, // xmas goblin m
    20366, // crate/box
    21290, // goblin mage
    23498, // undead sw guard
    21175, // xmas belf f
    21176, // xmas belf m
    21177, // xmas draenei m
    21178, // xmas draenei f
    18785, // xmas belf f (green)
    18793, // all xmas start ....
    18794,
    18795,
    18796,
    18797,
    18798,
    18799,
    18800,
    18801,
    18802,
    18803,
    18804,
    18805,
    18806,
    18807,
    18808,
    18809,
    18810,
    18811  // .... all xmas end
};


void ItemUse_pe_item_xmas_doMorph(Player *pPlayer, SpellCastTargets const& scTargets)
{
    uint32 count = sizeof(modelList) / sizeof(uint32);
    uint32 model;

    do 
        model = modelList[urand(0, count-1)];
    while(model == pPlayer->GetDisplayId());

    pPlayer->SetDisplayId(model);

    pPlayer->CastSpell(pPlayer, 41232, true); // teleport visual
    pPlayer->CastSpell(pPlayer, 19484, true); // red flash visual (1 sec later)
}

void ItemUse_pe_item_xmas_doCastSpell(Player *pPlayer, SpellCastTargets const& scTargets)
{
    Unit *target = scTargets.getUnitTarget();
    if(!target)
    {
        ObjectGuid og = pPlayer->GetTargetGuid();
        target = pPlayer->GetMap()->GetUnit(og);
        if(!target)
            target = pPlayer;
    }

    uint32 count = sizeof(spellList) / sizeof(uint32);

select_spell:

    uint32 id = spellList[urand(0, count-1)];

    // HACK: cataclysmic bolt halves HP, limit to targets with lower health to make it not imba
    if(id == 38441 && (target->GetMaxHealth() > 80000 || target->GetHealthPercent() < 55.0))
        goto select_spell;

    pPlayer->CastSpell(target, id, true);

    // spell-specific HACKS
    switch(id)
    {
        case 45922:
        {
            Aura *aura = pPlayer->GetAura(id, EFFECT_INDEX_0);
            if(aura)
            {
                aura->SetAuraDuration(aura->GetAuraDuration() / 4); // reduce 2 mins to 30 secs
                aura->SetAuraMaxDuration(aura->GetAuraMaxDuration() / 4); // reduce 2 mins to 30 secs
                aura->GetHolder()->SendAuraUpdate(false);
            }
            break;
        }

    }
    // end HACKS
}


bool ItemUse_pe_item_xmas(Player* pPlayer, Item* pItem, SpellCastTargets const& scTargets)
{
    uint32 r = URandomize(0, 100);
    if(pPlayer->GetSelectionGuid().IsEmpty())
        ItemUse_pe_item_xmas_doMorph(pPlayer, scTargets);
    else
        ItemUse_pe_item_xmas_doCastSpell(pPlayer, scTargets);

    return false; // process as usual (set cooldown, etc)
}

void AddSC_pe_item_xmas()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "pe_item_xmas";
    pNewScript->pItemUse = &ItemUse_pe_item_xmas;
    pNewScript->RegisterSelf();
}
