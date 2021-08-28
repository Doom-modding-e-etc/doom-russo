//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2016-2021 Julian Nechaevsky
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	Handling interactions (i.e., collisions).
//

#include "include/doomdef.h"
#include "include/rd_lang.h"
#include "include/sounds.h"
#include "include/deh_main.h"
#include "include/deh_misc.h"
#include "include/doomstat.h"
#include "include/m_random.h"
#include "include/i_system.h"
#include "include/am_map.h"
#include "include/p_local.h"
#include "include/s_sound.h"
#include "include/crispy.h"
#include "include/jn.h"


#define BONUSADD	6


// a weapon is found with two clip loads,
// a big item has five clip loads
int	maxammo[NUMAMMO] = {200, 50, 300, 50};
int	clipammo[NUMAMMO] = {10, 4, 20, 1};


// =============================================================================
// GET STUFF
// =============================================================================

// -----------------------------------------------------------------------------
// P_GiveAmmo
// Num is the number of clip loads, not the individual count (0= 1/2 clip).
// Returns false if the ammo can't be picked up at all.
// -----------------------------------------------------------------------------
boolean P_GiveAmmo (player_t *player, ammotype_t ammo, int num)
{
    int oldammo;

    if (ammo == am_noammo)
    {
        return false;
    }

    if (ammo > NUMAMMO)
    {
        I_Error (english_language ?
                 "P_GiveAmmo: bad type %i" :
                 "P_GiveAmmo: некорректный тип %i", ammo);
    }

    if (player->ammo[ammo] == player->maxammo[ammo])
    {
        return false;
    }

    if (num)
    {
        num *= clipammo[ammo];
    }
    else
    {
        num = clipammo[ammo]/2;
    }

    if (gameskill == sk_baby || gameskill == sk_nightmare)
    {
        // give double ammo in trainer mode,
        // you'll need in nightmare
        num <<= 1;
    }

    oldammo = player->ammo[ammo];
    player->ammo[ammo] += num;

    if (player->ammo[ammo] > player->maxammo[ammo])
    {
        player->ammo[ammo] = player->maxammo[ammo];
    }

    // If non zero ammo, don't change up weapons, player was lower on purpose.
    if (oldammo)
    {
        return true;
    }

    // We were down to zero, so select a new weapon.
    // Preferences are not user selectable.
    switch (ammo)
    {
        case am_clip:
        {
            if (player->readyweapon == wp_fist)
            {
                if (player->weaponowned[wp_chaingun])
                player->pendingweapon = wp_chaingun;
                else
                player->pendingweapon = wp_pistol;
            }
        }
        break;

        case am_shell:
        {
            if (player->readyweapon == wp_fist
            ||  player->readyweapon == wp_pistol)
            {
                if (player->weaponowned[wp_shotgun])
                player->pendingweapon = wp_shotgun;
            }
        }
        break;

        case am_cell:
        {
            if (player->readyweapon == wp_fist
            ||  player->readyweapon == wp_pistol)
            {
                if (player->weaponowned[wp_plasma])
                player->pendingweapon = wp_plasma;
            }
        }
        break;

        case am_misl:
        {
            if (player->readyweapon == wp_fist)
            {
                if (player->weaponowned[wp_missile])
                player->pendingweapon = wp_missile;
            }
        }
        break;

        default:
        break;
    }

    return true;
}

// -----------------------------------------------------------------------------
// P_GiveWeapon
// The weapon name may have a MF_DROPPED flag ored in.
// -----------------------------------------------------------------------------

boolean P_GiveWeapon (player_t *player, weapontype_t weapon, boolean dropped)
{
    boolean	gaveammo;
    boolean	gaveweapon;

    if (netgame && (deathmatch != 2) && !dropped)
    {
        // leave placed weapons forever on net games
        if (player->weaponowned[weapon])
        {
            return false;
        }

        player->bonuscount += BONUSADD;
        player->weaponowned[weapon] = true;

        P_GiveAmmo (player, weaponinfo[weapon].ammo, deathmatch ? 5 : 2);

        player->pendingweapon = weapon;

        if (player == &players[consoleplayer])
        {
            S_StartSound (NULL, sfx_wpnup);
        }

        return false;
    }

    if (weaponinfo[weapon].ammo != am_noammo)
    {
        // give one clip with a dropped weapon,
        // two clips with a found weapon
        gaveammo = P_GiveAmmo (player, weaponinfo[weapon].ammo, dropped ? 1 : 2);
    }
    else
    {
        gaveammo = false;
    }

    if (player->weaponowned[weapon])
    {
        gaveweapon = false;
    }
    else
    {
        gaveweapon = true;
        player->weaponowned[weapon] = true;
        player->pendingweapon = weapon;
    }

    return (gaveweapon || gaveammo);
}

// -----------------------------------------------------------------------------
// P_GiveBody
// Returns false if the body isn't needed at all
// -----------------------------------------------------------------------------

boolean P_GiveBody (player_t *player, int num)
{
    if (player->health >= MAXHEALTH)
    {
        return false;
    }

    player->health += num;

    if (player->health > MAXHEALTH)
    {
        player->health = MAXHEALTH;
    }

    player->mo->health = player->health;

    return true;
}

// -----------------------------------------------------------------------------
// P_GiveArmor
// Returns false if the armor is worse than the current armor.
// -----------------------------------------------------------------------------

boolean P_GiveArmor (player_t *player, int armortype)
{
    int hits;

    hits = armortype * 100;

    if (player->armorpoints >= hits)
    {
        return false;  // don't pick up
    }

    player->armortype = armortype;
    player->armorpoints = hits;

    return true;
}

// -----------------------------------------------------------------------------
// P_GiveCard
// -----------------------------------------------------------------------------

void P_GiveCard (player_t *player, card_t card)
{
    if (player->cards[card])
    {
        return;
    }

    if (vanillaparm)
    {
        // [JN] Keep this bug in vanilla mode
        player->bonuscount = BONUSADD;
    }
    else
    {
        // [crispy] Fix "Key pickup resets palette"
        player->bonuscount += netgame ? BONUSADD : 0;
    }

    player->cards[card] = 1;
}

// -----------------------------------------------------------------------------
// P_GivePower
// -----------------------------------------------------------------------------

boolean P_GivePower (player_t *player, int power)
{
    if (power == pw_invulnerability)
    {
        player->powers[power] = INVULNTICS;
        return true;
    }

    if (power == pw_invisibility)
    {
        player->powers[power] = INVISTICS;
        player->mo->flags |= MF_SHADOW;
        return true;
    }

    if (power == pw_infrared)
    {
        player->powers[power] = INFRATICS;
        return true;
    }

    if (power == pw_ironfeet)
    {
        player->powers[power] = IRONTICS;
        return true;
    }

    if (power == pw_strength)
    {
        P_GiveBody (player, 100);
        player->powers[power] = 1;
        return true;
    }

    if (player->powers[power])
    {
        return false;  // already got it
    }

    player->powers[power] = 1;

    return true;
}

// -----------------------------------------------------------------------------
// P_TouchSpecialThing
// -----------------------------------------------------------------------------

void P_TouchSpecialThing (mobj_t *special, mobj_t *toucher)
{
    int        sound = sfx_itemup;
    player_t  *player = toucher->player;
    fixed_t    delta = special->z - toucher->z;

    if (delta > toucher->height || delta < -8 * FRACUNIT)
    {
        // out of reach
        return;
    }

    // Dead thing touching. Can happen with a sliding player corpse.
    if (toucher->health <= 0)
    {
        return;
    }

    // Identify by sprite.
    switch (special->sprite)
    {
        // armor
        case SPR_ARM1:
        {
            if (!P_GiveArmor (player, deh_green_armor_class))
            return;
            player->message = DEH_String(gotarmor);
        }
        break;

        case SPR_ARM2:
        {
            if (!P_GiveArmor (player, deh_blue_armor_class))
            return;
            player->message = DEH_String(gotmega);
        }
        break;

        // bonus items
        case SPR_BON1:
        {
            // [JN] No health bonus in Press Beta
            if (gamemode != pressbeta)
            {
                // [JN] Jaguar Doom: doubled bonus (can go over 100%)
                player->health += (gamemission == jaguar ? 2 : 1);
                if (player->health > deh_max_health)
                {
                    player->health = deh_max_health;
                }
                player->mo->health = player->health;
            }
            else
            {
                artifactcount++;
            }
            player->message = DEH_String(goththbonus);
        }
        break;

        case SPR_BON2:
        {
            // [JN] No armor bonus in Press Beta
            if (gamemode != pressbeta)
            {
                // [JN] Jaguar Doom: doubled bonus (can go over 100%)
                player->armorpoints += (gamemission == jaguar ? 2 : 1);
                if (player->armorpoints > deh_max_armor)
                {
                    player->armorpoints = deh_max_armor;
                    // deh_green_armor_class only applies to the green armor shirt;
                    // for the armor helmets, armortype 1 is always used.
                }
                if (!player->armortype)
                {
                    player->armortype = 1;
                }
            }
            else
            {
                artifactcount++;
            }
            player->message = DEH_String(gotarmbonus);
        }
        break;

        case SPR_BON3:  // [JN] Evil Sceptre
        {
            artifactcount++;
            player->message = DEH_String(gotsceptre);
        }
        break;

        case SPR_BON4:  // [JN] Unholy bible
        {
            artifactcount++;
            player->message = DEH_String(gotbible);
        }
        break;

        case SPR_SOUL:
        {
            if (gamemode == pressbeta)  // [JN] Press Beta: add Extra Life!
            lifecount++;

            player->health += deh_soulsphere_health;
            if (player->health > deh_max_soulsphere)
            {
                player->health = deh_max_soulsphere;
            }
            player->mo->health = player->health;
            player->message = DEH_String(gotsuper);
            sound = sfx_getpow;
        }
        break;

        case SPR_MEGA:
        {
            if (gamemode != commercial)
            return;

            player->health = deh_megasphere_health;
            player->mo->health = player->health;
            // We always give armor type 2 for the megasphere; dehacked only 
            // affects the MegaArmor.
            P_GiveArmor (player, 2);
            player->message = DEH_String(gotmsphere);
            sound = sfx_getpow;
        }
        break;

        // cards
        // leave cards for everyone
        case SPR_BKEY:
        {
            player->message = DEH_String(gotbluecard);
            P_GiveCard (player, it_bluecard);
            if (!netgame)
            break;
        }
        return;

        case SPR_YKEY:
        {
            player->message = DEH_String(gotyelwcard);
            P_GiveCard (player, it_yellowcard);
            if (!netgame)
            break;
        }
        return;

        case SPR_RKEY:
        {
            player->message = DEH_String(gotredcard);
            P_GiveCard (player, it_redcard);
            if (!netgame)
            break;
        }
        return;

        case SPR_BSKU:
        {
            player->message = DEH_String(gotblueskul);
            P_GiveCard (player, it_blueskull);
            if (!netgame)
            break;
        }
        return;

        case SPR_YSKU:
        {
            player->message = DEH_String(gotyelwskul);
            P_GiveCard (player, it_yellowskull);
            if (!netgame)
            break;
        }
        return;

        case SPR_RSKU:
        {
            player->message = DEH_String(gotredskull);
            P_GiveCard (player, it_redskull);
            if (!netgame)
            break;
        }
        return;

        // medikits, heals
        case SPR_STIM:
        {
            if (!P_GiveBody (player, 10))
            return;
            player->message = DEH_String(gotstim);
        }
        break;

        case SPR_MEDI:
        {
            if (player->health >= MAXHEALTH)
            return;
            // [JN] Fix for "Medikit that you really needed!"
            player->message = player->health < 25 ? gotmedineed : gotmedikit;
            P_GiveBody (player, 25);
        }
        break;

        // power ups
        case SPR_PINV:
        {
            if (!P_GivePower (player, pw_invulnerability))
            return;
            player->message = DEH_String(gotinvul);
            sound = sfx_getpow;
        }
        break;

        case SPR_PSTR:
        {
            if (!P_GivePower (player, pw_strength))
            return;
            player->message = DEH_String(gotberserk);
            if (player->readyweapon != wp_fist)
            {
                player->pendingweapon = wp_fist;
            }
            sound = sfx_getpow;
        }
        break;

        case SPR_PINS:
        {
            if (!P_GivePower (player, pw_invisibility))
            return;
            player->message = DEH_String(gotinvis);
            sound = sfx_getpow;
        }
        break;

        case SPR_SUIT:
        {
            if (!P_GivePower (player, pw_ironfeet))
            return;
            player->message = DEH_String(gotsuit);
            sound = sfx_getpow;
        }
        break;

        case SPR_PMAP:
        {
            // [JN] Allow to pickup automap if player already have it,
            // to make 100% items possible on levels with few automaps. 
            // Notable example: MAP27 of Hell on Earth.
            if (singleplayer && !vanillaparm)
            {
                P_GivePower (player, pw_allmap);
            }
            else
            {
                if (!P_GivePower (player, pw_allmap))
                return;
            }
            player->message = DEH_String(gotmap);
            sound = sfx_getpow;
        }
        break;

        case SPR_PVIS:
        {
            if (!P_GivePower (player, pw_infrared))
            return;
            player->message = DEH_String(gotvisor);
            sound = sfx_getpow;
        }
        break;

        // ammo
        case SPR_CLIP:
        {
            if (!P_GiveAmmo (player, am_clip, special->flags & MF_DROPPED ? 0 : 1))
            return;
            player->message = DEH_String(gotclip);
        }
        break;

        case SPR_AMMO:
        {
            if (!P_GiveAmmo (player, am_clip,5))
            return;
            player->message = DEH_String(gotclipbox);
        }
        break;

        case SPR_ROCK:
        {
            if (!P_GiveAmmo (player, am_misl,1))
            return;

            // [JN] Print "Picked up a two rockets".
            player->message = DEH_String(gameskill == sk_baby ||
                                         gameskill == sk_nightmare ?
                                         gotrocket2 : gotrocket);
        }
        break;

        case SPR_BROK:
        {
            if (!P_GiveAmmo (player, am_misl,5))
            return;
            player->message = DEH_String(gotrockbox);
        }
        break;

        case SPR_CELL:
        {
            if (!P_GiveAmmo (player, am_cell,1))
            return;
            player->message = DEH_String(gotcell);
        }
        break;

        case SPR_CELP:
        {
            if (!P_GiveAmmo (player, am_cell,5))
            return;
            player->message = DEH_String(gotcellbox);
        }
        break;

        case SPR_SHEL:
        {
            if (!P_GiveAmmo (player, am_shell,1))
            return;
            player->message = DEH_String(gameskill == sk_baby ||
                                         gameskill == sk_nightmare ?
                                         gotshells8 : gotshells);
        }
        break;

        case SPR_SBOX:
        {
            if (!P_GiveAmmo (player, am_shell,5))
            return;
            player->message = DEH_String(gotshellbox);
        }
        break;

        case SPR_BPAK:
        {
            int i;

            if (!player->backpack)
            {
                for (i = 0 ; i < NUMAMMO ; i++)
                player->maxammo[i] *= 2;
                player->backpack = true;
            }
            for (i = 0 ; i < NUMAMMO ; i++)
            {
                P_GiveAmmo (player, i, 1);
            }
            player->message = DEH_String(gotbackpack);
        }
        break;

        // weapons
        case SPR_BFUG:
        {
            if (!P_GiveWeapon (player, wp_bfg, false) )
            return;
            player->message = DEH_String(gotbfg9000);
            sound = sfx_wpnup;
        }
        break;

        case SPR_MGUN:
        {
            if (!P_GiveWeapon(player, wp_chaingun, (special->flags & MF_DROPPED) != 0))
            return;
            player->message = DEH_String(gotchaingun);
            sound = sfx_wpnup;	
        }
        break;

        case SPR_CSAW:
        {
            if (!P_GiveWeapon (player, wp_chainsaw, false))
            return;
            player->message = DEH_String(gotchainsaw);
            sound = sfx_wpnup;
        }
        break;

        case SPR_LAUN:
        {
            if (!P_GiveWeapon (player, wp_missile, false))
            return;
            player->message = DEH_String(gotlauncher);
            sound = sfx_wpnup;
        }
        break;

        case SPR_PLAS:
        {
            if (!P_GiveWeapon (player, wp_plasma, false))
            return;
            player->message = DEH_String(gotplasma);
            sound = sfx_wpnup;
        }
        break;

        case SPR_SHOT:
        {
            if (!P_GiveWeapon(player, wp_shotgun, (special->flags & MF_DROPPED) != 0))
            return;
            player->message = DEH_String(gotshotgun);
            sound = sfx_wpnup;
        }
        break;

        case SPR_SGN2:
        {
            if (!P_GiveWeapon(player, wp_supershotgun, (special->flags & MF_DROPPED) != 0))
            return;
            player->message = DEH_String(gotshotgun2);
            sound = sfx_wpnup;
        }
        break;

        default:
        {
            I_Error (english_language ?
                     "P_SpecialThing: Unknown gettable thing" :
                     "P_SpecialThing: получен неизвестный предмет");
        }
    }

    if (special->flags & MF_COUNTITEM)
    {
        player->itemcount++;
    }

    P_RemoveMobj (special);

    // [JN] Limit bonus palette duration to 4 seconds
    if (player->bonuscount >= 4 * TICRATE)
    {
        player->bonuscount = 4 * TICRATE;
    }

    player->bonuscount += BONUSADD;

    if (player == &players[consoleplayer])
    {
        S_StartSound (NULL, sound);
    }
}

// -----------------------------------------------------------------------------
// KillMobj
// -----------------------------------------------------------------------------

void P_KillMobj (mobj_t *source, mobj_t *target)
{
    mobjtype_t  item;
    mobj_t     *mo;

    target->flags &= ~(MF_SHOOTABLE|MF_FLOAT|MF_SKULLFLY);

    if (target->type != MT_SKULL)
    {
        target->flags &= ~MF_NOGRAVITY;
    }

    target->flags |= MF_CORPSE|MF_DROPOFF;
    target->height >>= 2;
    target->geartics = 5 * TICRATE;  // [JN] Limit torque simulation for 5 seconds.

    if (source && source->player)
    {
        // count for intermission
        if (target->flags & MF_COUNTKILL)
        {
            source->player->killcount++;
        }

        if (target->player)
        {
            source->player->frags[target->player-players]++;
        }
    }
    else if (!netgame && (target->flags & MF_COUNTKILL))
    {
        // count all monster deaths,
        // even those caused by other monsters
        players[0].killcount++;
    }

    if (target->player)
    {
        // count environment kills against you
        if (!source)
        {
            target->player->frags[target->player-players]++;
        }
        target->flags &= ~MF_SOLID;
        target->player->playerstate = PST_DEAD;

        // [JN] & [crispy] Remove bonus pallete after death.
        target->player->bonuscount = 0;
        // [JN] & [crispy] Remove invulnerability colormap after death.
        target->player->fixedcolormap = target->player->powers[pw_infrared] ? (infragreen_visor ? 33 : 1) : 0;

        P_DropWeapon (target->player);

        if (target->player == &players[consoleplayer] && automapactive)
        {
            // don't die in auto map,
            // switch view prior to dying
            AM_Stop ();
        }
    }

    if (target->health < -target->info->spawnhealth 
    &&  target->info->xdeathstate && gamemode != pressbeta) // [JN] Press Beta don't have xdeath states
    {
        P_SetMobjState (target, target->info->xdeathstate);
    }
    else
    {
        P_SetMobjState (target, target->info->deathstate);
    }

    target->tics -= P_Random()&3;

    // [crispy] randomize corpse health
    if (singleplayer)
    {
        target->health -= target->tics & 1;
    }
 
    if (target->tics < 1)
    {
        target->tics = 1;
    }

    // In Chex Quest, monsters don't drop items.
    if (gameversion == exe_chex)
    {
        return;
    }

    // Drop stuff.
    // This determines the kind of object spawned
    // during the death frame of a thing.
    switch (target->type)
    {
        case MT_WOLFSS:
        case MT_POSSESSED:
        item = MT_CLIP;
        break;

        case MT_SHOTGUY:
        item = MT_SHOTGUN;
        break;

        case MT_CHAINGUY:
        item = MT_CHAINGUN;
        break;

        default:
        return;
    }

    // [JN] Dropped items tossing feature (from Doom Retro).
    if (toss_drop && singleplayer && !vanillaparm)
    {
        mo = P_SpawnMobj(target->x ,target->y, target->floorz
                                             + target->height
                                             * 3 / 2 - 3 * FRACUNIT, item);
        mo->momx = (target->momx >> 1) + (Crispy_Random() << 8);
        mo->momy = (target->momy >> 1) + (Crispy_Random() << 8);
        mo->momz = 2 * FRACUNIT + (M_Random() << 9);
    }
    else
    {
        mo = P_SpawnMobj (target->x,target->y,ONFLOORZ, item);
    }
    mo->flags |= MF_DROPPED;	// special versions of items
}

// -----------------------------------------------------------------------------
// P_DamageMobj
// Damages both enemies and players "inflictor" is the thing that caused the
// damage creature or missile, can be NULL (slime, etc) "source" is the thing
// to target after taking damage creature or NULL.
// Source and inflictor are the same for melee attacks.
// Source can be NULL for slime, barrel explosions and other environmental stuff.
// -----------------------------------------------------------------------------

void P_DamageMobj (mobj_t *target, mobj_t *inflictor, mobj_t *source, int damage)
{
    player_t  *player;
    fixed_t    thrust;

    if (!(target->flags & MF_SHOOTABLE))
    {
        return;  // shouldn't happen...
    }

    if (target->health <= 0)
    {
        return;
    }

    if ( target->flags & MF_SKULLFLY )
    {
        target->momx = target->momy = target->momz = 0;
    }

    player = target->player;
    
    if (player && gameskill == sk_baby)
    {
        damage >>= 1; 	// take half damage in trainer mode
    }

    // Some close combat weapons should not
    // inflict thrust and push the victim out of reach,
    // thus kick away unless using the chainsaw.
    if (inflictor && !(target->flags & MF_NOCLIP) && (!source
    || !source->player || source->player->readyweapon != wp_chainsaw))
    {
        unsigned ang = R_PointToAngle2 (inflictor->x,
                                        inflictor->y,
                                        target->x,
                                        target->y);

        thrust = damage*(FRACUNIT>>3)*100/target->info->mass;

        // make fall forwards sometimes
        if (damage < 40 && damage > target->health 
        &&  target->z - inflictor->z > 64*FRACUNIT && (P_Random ()&1) )
        {
            ang += ANG180;
            thrust *= 4;
        }

        ang >>= ANGLETOFINESHIFT;
        target->momx += FixedMul (thrust, finecosine[ang]);
        target->momy += FixedMul (thrust, finesine[ang]);
    }

    // player specific
    if (player)
    {
        int temp = damage < 100 ? damage : 100;

        // end of game hell hack
        if (target->subsector->sector->special == 11
        &&  damage >= target->health)
        {
            damage = target->health - 1;
        }

        // Below certain threshold,
        // ignore damage in GOD mode, or with INVUL power.
        if (damage < 1000 && ((player->cheats & CF_GODMODE)
        ||  player->powers[pw_invulnerability]))
        {
            return;
        }

        if (player->armortype)
        {
            int saved;

            if (player->armortype == 1)
            {
                saved = damage/3;
            }
            else
            {
                saved = damage/2;
            }

            if (player->armorpoints <= saved)
            {
                // armor is used up
                saved = player->armorpoints;
                player->armortype = 0;
            }

            player->armorpoints -= saved;
            damage -= saved;
        }

        player->health -= damage;             // mirror mobj health here for Dave
        player->health_neg = player->health;  // [JN] Set negative health value

        if (player->health < 0)
        {
            player->health = 0;
        }

        // [crispy] & [JN] Negative player health
        if (player->health_neg < -99)
        {
            player->health_neg = -99;
        }

        player->attacker = source;
        player->damagecount += damage;	// add damage after armor / invuln

        // [JN] Fix 1% damage bug (https://doomwiki.org/wiki/1%25_damage_bug)
        if (damage > 0 && player->damagecount < 2)
        {
            player->damagecount = 2;
        }

        if (player->damagecount > 100)
        {
            player->damagecount = 100;  // teleport stomp does 10k points...
        }

        if (player == &players[consoleplayer])
        {
            I_Tactile (40,10,40+temp*2);
        }
    }

    // do the damage	
    target->health -= damage;	

    if (target->health <= 0)
    {
        // [crispy] the lethal pellet of a point-blank SSG blast
        // gets an extra damage boost for the occasional gib chance
        if (ssg_blast_enemies && !vanillaparm)
        {
            extern boolean P_CheckMeleeRange (mobj_t *actor);

            if (singleplayer && source && source->player
            && source->player->readyweapon == wp_supershotgun
            && target->info->xdeathstate && P_CheckMeleeRange(target) && damage >= 10)
            {
                target->health -= target->info->spawnhealth;
            }
        }

        P_KillMobj (source, target);
        return;
    }

    // [JN] Fix bug: https://doomwiki.org/wiki/Lost_soul_charging_backwards
    // Thanks AXDOOMER for this fix!
    if (singleplayer && agressive_lost_souls && !vanillaparm)
    {
        if (P_Random () < target->info->painchance)
        {
            if (target->flags & MF_SKULLFLY)
            {
                target->flags &= ~MF_SKULLFLY;
            }

            target->flags |= MF_JUSTHIT;	// fight back!
            P_SetMobjState (target, target->info->painstate);
        }
    }
    else
    {
        if ((P_Random () < target->info->painchance) && !(target->flags & MF_SKULLFLY))
        {
            target->flags |= MF_JUSTHIT;	// fight back!
            P_SetMobjState (target, target->info->painstate);
        }        
    }

    target->reactiontime = 0;   // we're awake now...	

    if ( (!target->threshold || target->type == MT_VILE)
    && source && source != target && source->type != MT_VILE)
    {
        // if not intent on another player,
        // chase after this one
        target->target = source;
        target->threshold = BASETHRESHOLD;

        if (target->state == &states[target->info->spawnstate]
        && target->info->seestate != S_NULL)
        {
            P_SetMobjState (target, target->info->seestate);
        }
    }
}
