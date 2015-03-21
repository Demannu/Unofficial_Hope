/*
---------------------------------------------------------------------------------------
This source file is part of SWG:ANH (Star Wars Galaxies - A New Hope - Server Emulator)

For more information, visit http://www.swganh.com

Copyright (c) 2006 - 2014 The SWG:ANH Team
---------------------------------------------------------------------------------------
Use of this source code is governed by the GPL v3 license that can be found
in the COPYING file or at http://www.gnu.org/licenses/gpl-3.0.html

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
---------------------------------------------------------------------------------------
*/

#include "ZoneServer/Objects/Creature Object/CreatureObject.h"
#include "Zoneserver/Objects/AttackableCreature.h"

#include "ZoneServer\Objects\Creature Object\equipment_item.h"

#include "Zoneserver/GameSystemManagers/NPC Manager/AttackableStaticNpc.h"
#include "Zoneserver/GameSystemManagers/Buff Manager/Buff.h"
#include "Zoneserver/GameSystemManagers/Structure Manager/BuildingObject.h"
#include "ZoneServer\GameSystemManagers\Skill Manager\skill_mod.h"

#include "ZoneServer/ProfessionManagers/Entertainer Manager/EntertainerManager.h"

#include "ZoneServer/Objects/Player Object/PlayerObject.h"
#include "ZoneServer/GameSystemManagers/State Manager/StateManager.h"
#include "ZoneServer/GameSystemManagers/Spawn Manager/SpawnPoint.h"
#include "ZoneServer/GameSystemManagers/UI Manager/UIManager.h"
#include "ZoneServer/Objects/VehicleController.h"
#include "ZoneServer/WorldManager.h"
#include "ZoneServer/GameSystemManagers/Spatial Index Manager/SpatialIndexManager.h"
#include "ZoneServer/WorldConfig.h"
#include "ZoneServer/Tutorial.h"
#include "MessageLib/MessageLib.h"

// events
#include "Zoneserver/GameSystemManagers/Event Manager/IncapRecoveryEvent.h"
#include "Zoneserver/GameSystemManagers/Event Manager/PostureEvent.h"
#include "Zoneserver/GameSystemManagers/Event Manager/ActionStateEvent.h"
#include "Zoneserver/GameSystemManagers/Event Manager/LocomotionStateEvent.h"
#include "Common/EventDispatcher.h"

#include <anh\app\swganh_kernel.h>
#include "anh/app/swganh_app.h"
#include <ZoneServer\Services\scene_events.h>

#include "anh/Utils/clock.h"

#include "anh/Utils/rand.h"
#include <cfloat>
#include <memory>

using ::common::IEventPtr;
using ::common::EventDispatcher;
using ::common::EventType;
using ::common::EventListener;
using ::common::EventListenerType;

using swganh::containers::NetworkVector;
using swganh::containers::DefaultSerializer;

using namespace swganh::messages;
using namespace swganh::containers;

//=============================================================================

CreatureObject::CreatureObject()
: MovingObject()
, mTargetId(0)
, mDefenderUpdateCounter(0)
, mSkillModUpdateCounter(0)
, mCurrentAnimation("")
, mCustomizationStr("")
, mFaction("")
, mSpecies("")
, mSpeciesGroup("")
, mPerformance(NULL)
, mPvPStatus(CreaturePvPStatus_None)
, mPendingPerform(PlayerPerformance_None)
, mCurrentIncapTime(0)
, mEntertainerListenToId(0)
, mEntertainerPauseId(0)
, mEntertainerTaskId(0)
, mEntertainerWatchToId(0)
, mFirstIncapTime(0)
, mGroupId(0)
, mLastEntertainerXP(0)
, mScale(1.0)
, mLastMoveTick(0)
, mPerformanceCounter(0)
, mPerformanceId(0)
, mRaceGenderMask(0)
, mSkillUpdateCounter(0)
, mCL(1)
, mFactionRank(0)
, mIncapCount(0)
, mMoodId(0)
, mReady(false)
{
    mType = ObjType_Creature;
	object_type_ = SWG_CREATURE;

    mSkillMods.reserve(50);
//    mSkillCommands.reserve(50);
    mFactionList.reserve(50);

    for(uint16 i = 1;i<256;i++)
        mCustomization[i]=0;
    
    // initialize state struct
    states.action          = 0;
	this->SetPosture(0);
    states.locomotion      = 0;
    
    states.blockAction     = false;
    states.blockLocomotion = false;
    states.blockPosture    = false;

    // register event functions
    registerEventFunction(this,&CreatureObject::onIncapRecovery);

    // register new event listeners
}

//=============================================================================

CreatureObject::~CreatureObject()
{
    mBuffList.clear();

	// update defender lists
	auto defenderList = GetDefender();
    auto defenderIt = defenderList.begin();

    while (defenderIt != defenderList.end())
    {
        if (CreatureObject* defenderCreature = dynamic_cast<CreatureObject*>(gWorldManager->getObjectById((*defenderIt))))
        {
            defenderCreature->RemoveDefender(mId);

            if(PlayerObject* defenderPlayer = dynamic_cast<PlayerObject*>(defenderCreature))
            {
                gMessageLib->sendUpdatePvpStatus(this,defenderPlayer);
            }

            // if no more defenders, clear combat state
            if(!defenderCreature->GetDefender().size())
            {
                // TODO: replace
                gStateManager.removeActionState(this, CreatureState_Combat);

                gMessageLib->sendStateUpdate(defenderCreature);
            }
        }

        ++defenderIt;
    }

}

//=============================================================================

void CreatureObject::prepareSkillMods()
{
    mSkillMods.clear();

    auto skillIt		= skills_.begin();

    while(skillIt != skills_.end())
    {
		Skill* skill = gSkillManager->getSkillByName((*skillIt).c_str());

        SkillModsList::iterator smIt = skill->mSkillMods.begin();
        SkillModsList::iterator localSmIt;

        while(smIt != skill->mSkillMods.end())
        {
            localSmIt = findSkillMod((*smIt).first);

            if(localSmIt != mSkillMods.end())
                (*localSmIt).second += (*smIt).second;
            else
                mSkillMods.push_back((*smIt));

            ++smIt;
        }
        ++skillIt;
    }
}

//=============================================================================



//=============================================================================

SkillModsList::iterator CreatureObject::findSkillMod(uint32 modId)
{
    SkillModsList::iterator it = mSkillMods.begin();

    while(it != mSkillMods.end())
    {
        if(modId == (*it).first)
            return(it);

        ++it;
    }
    return(mSkillMods.end());
}

//=============================================================================

int32 CreatureObject::getSkillModValue(uint32 modId)
{
    SkillModsList::iterator it = mSkillMods.begin();

    while(it != mSkillMods.end())
    {
        if(modId == (*it).first)
            return((*it).second);

        ++it;
    }

    return(-1000);
}

//=============================================================================

bool CreatureObject::setSkillModValue(uint32 modId,int32 value)
{
    SkillModsList::iterator it = mSkillMods.begin();

    while(it != mSkillMods.end())
    {
        if(modId == (*it).first)
        {
            (*it).second = value;
            return(true);
        }

        ++it;
    }

    return(false);
}
bool CreatureObject::modifySkillModValue(uint32 modId,int32 modifier)
{
    SkillModsList::iterator it = mSkillMods.begin();

    while(it != mSkillMods.end())
    {
        if(modId == (*it).first)
        {
            (*it).second += modifier;

            //If we have a player, send mod updates
            PlayerObject* temp = dynamic_cast<PlayerObject*>(this);
            if(temp)
            {
                SkillModsList modlist;
                modlist.push_back(*it);
                //TODO: Skillmods should use a Delta, not a Baseline
                gMessageLib->sendBaselinesCREO_4(temp);
                //gMessageLib->sendSkillModUpdateCreo4(temp);
                //gMessageLib->sendSkillModDeltasCREO_4(modlist, 0, temp, temp);
            }
            return(true);
        }

        ++it;
    }



    return(false);
}

//=============================================================================

int32 CreatureObject::getFactionPointsByFactionId(uint32 id)
{
    FactionList::iterator it = mFactionList.begin();

    while(it != mFactionList.end())
    {
        if((*it).first == id)
            return((*it).second);

        ++it;
    }

    return(-10000);
}

//=============================================================================

bool CreatureObject::updateFactionPoints(uint32 factionId,int32 value)
{
    FactionList::iterator it = mFactionList.begin();

    while(it != mFactionList.end())
    {
        if((*it).first == factionId)
        {
            (*it).second += value;
            return(true);
        }

        ++it;
    }

    return(false);
}

//=============================================================================

bool CreatureObject::checkSkill(uint32 skillId)
{
    auto skillIt		= skills_.begin();

    while(skillIt != skills_.end())
    {
		Skill* skill = gSkillManager->getSkillByName((*skillIt).c_str());

        if(skill->mId == skillId)
            return(true);
        ++skillIt;
    }

    return(false);
}

//=============================================================================


//=============================================================================

uint32 CreatureObject::getSkillPointsLeft()
{
    auto skillIt		= skills_.begin();
    uint32 skillPointsLeft			= 250;

    while(skillIt != skills_.end())
    {
		Skill* skill = gSkillManager->getSkillByName((*skillIt).c_str());
        skillPointsLeft -= skill->mSkillPointsRequired;
        ++skillIt;
    }

    return(skillPointsLeft);
}

//=============================================================================

bool CreatureObject::handlePerformanceTick(uint64 time,void* ref)
{
    return(gEntertainerManager->handlePerformanceTick(this));
}

bool CreatureObject::handleImagedesignerTimeOut(uint64 time,void* ref)
{
    return(gEntertainerManager->handleImagedesignTimeOut(this));
}

//=============================================================================
//
// update current movement properties
//

void CreatureObject::updateMovementProperties()
{
	switch(this->GetPosture())
    {
        case CreaturePosture_KnockedDown:
        case CreaturePosture_Incapacitated:
        case CreaturePosture_Dead:
        case CreaturePosture_Sitting:
        {
            mCurrentRunSpeedLimit		= 0.0f;
            mCurrentAcceleration		= 0.0f;
            mCurrentTurnRate			= 0.0f;
            mCurrentTerrainNegotiation	= 0.0f;
        }
        break;

        case CreaturePosture_Upright:
        {
            mCurrentRunSpeedLimit		= mBaseRunSpeedLimit;
            mCurrentAcceleration		= mBaseAcceleration;
            mCurrentTurnRate			= mBaseTurnRate;
            mCurrentTerrainNegotiation	= mBaseTerrainNegotiation;
        }
        break;

        case CreaturePosture_Prone:
        {
            mCurrentRunSpeedLimit		= 1.0f;
            mCurrentAcceleration		= 0.25f;
            mCurrentTurnRate			= mBaseTurnRate;
            mCurrentTerrainNegotiation	= mBaseTerrainNegotiation;
        }
        break;

        case CreaturePosture_Crouched:
        {
            mCurrentRunSpeedLimit		= 0.0f;
            mCurrentAcceleration		= 0.0f;
            mCurrentTurnRate			= mBaseTurnRate;
            mCurrentTerrainNegotiation	= mBaseTerrainNegotiation;
        }
        break;

        default:
        {
            mCurrentRunSpeedLimit		= mBaseRunSpeedLimit;
            mCurrentAcceleration		= mBaseAcceleration;
            mCurrentTurnRate			= mBaseTurnRate;
            mCurrentTerrainNegotiation	= mBaseTerrainNegotiation;
        }
        break;
    }
}

//=============================================================================
Buff* CreatureObject::GetBuff(uint32 BuffIcon)
{
    //Cycle through all buffs for the creature
    BuffList::iterator it = mBuffList.begin();
    //Check if the Icon CRCs Match (ie Duplication)
    while(it != mBuffList.end())
    {
        Buff* temp = *it;
        if(temp->GetIcon() == BuffIcon)
        {
            return temp;
        }
        it++;
    }
    return 0;
}
bool CreatureObject::GetBuffExists(uint32 BuffIcon)
{
    //Cycle through all buffs for the creature
    BuffList::iterator it = mBuffList.begin();
    while(it != mBuffList.end())
    {
        //Check if the Icon CRCs Match (ie Duplication)
        Buff* temp = *it;
        if(temp->GetIcon() == BuffIcon)
        {
            //Check if the duplicate isn't marked for deletion
            if(!temp->GetIsMarkedForDeletion())
            {
                return true;
            }
        }
        it++;
    }
    return false;
}
//=============================================================================
void CreatureObject::AddBuff(Buff* buff,  bool stackable, bool overwrite)
{
    if(!buff) return;

    if(buff->GetRemainingTime(gWorldManager->GetCurrentGlobalTick()) <= 0)    {
        SAFE_DELETE(buff);
        return;
    }

    //Use this opportunity to clean up any dead buffs in BuffList
    this->CleanUpBuffs();

    //Check we don't have a null ptr
    if(!buff)
        return;

    buff->SetInit(true);
    //If we already have a buff with this CRC Icon   we want to decide whether we can stack this buff
    //stacking is for example important for filling and food buffs

    //the next thing to be aware of, is that a type of foodbuff can have several effects (plus burstrun minus ham)
    //but has only one icon
    //in this case every effect we send will have the icon associated with it
    if(GetBuffExists(buff->GetIcon()) && !stackable )
    {
        if(overwrite)
        {
            //find old buff

            //apply changes

            //exit

            //for now do nothing
        }
        PlayerObject* player = dynamic_cast<PlayerObject*>(this);
        if(player != 0)
        {
            //gMessageLib->sendSystemMessage(player, "You appear to have attempted to stack Buffs. The server has prevented this");
            DLOG(info) << "Attempt to duplicate buffs prevented.";
        }
        SAFE_DELETE(buff);
        return;
        //TODO Currently, no overwrites - need to make it so certain buffs replace older ones
        //which will be important for doctor buffs
    }

    //Set the target to this creature, in case it isn't already
    buff->mTarget = this;

    //Add to Buff List
    mBuffList.push_back(buff);

    //Perform Inital Buff Changes
    buff->InitialChanges();

    //Add Buff to Scheduler
    gWorldManager->addBuffToProcess(buff);
}


int CreatureObject::GetNoOfBuffs()
{
    /*int No =0;
    BuffList::iterator it = mBuffList.begin();
    while(*it != *mBuffList.end())
    {
    	No++;it++;
    }
    return No;*/
    return mBuffList.size();
    /*if(*mBuffList.end())
    {
    	return 1 + ((*mBuffList.end()) - (*mBuffList.begin()));
    } else {
    	return 0;
    }*/
}
void CreatureObject::RemoveBuff(Buff* buff)
{
    if(!buff)
        return;

    //Cancel the buff so the next Event doesn't do anything
    buff->mCancelled = true;

    //Remove from the Process Queue
    gWorldManager->removeBuffToProcess(buff->GetID());
    buff->SetID(0);

    //Perform any Final changes
    buff->FinalChanges();
    buff->MarkForDeletion();
}

//================================================
//
void CreatureObject::ClearAllBuffs()
{
    BuffList::iterator it = mBuffList.begin();
    while(it != mBuffList.end())
    {
        (*it)->FinalChanges();
        gWorldManager->removeBuffToProcess((*it)->GetID());
        (*it)->MarkForDeletion();
        ++it;
    }
    CleanUpBuffs();
}
void CreatureObject::CleanUpBuffs()
{
    BuffList::iterator it = mBuffList.begin();
    while(it != mBuffList.end())
    {
        if((*it)->GetIsMarkedForDeletion())
        {
            SAFE_DELETE(*it);
            mBuffList.erase(it++);
        }
        else
        {
            it++;
        }
    }
}

//=============================================================================

void CreatureObject::updateRaceGenderMask(bool female)
{
    // set race flag
    switch(mRaceId)
    {
    case 0:
        mRaceGenderMask |= 0x4;
        break;
    case 1:
        mRaceGenderMask |= 0x8;
        break;
    case 2:
        mRaceGenderMask |= 0x10;
        break;
    case 3:
        mRaceGenderMask |= 0x20;
        break;
    case 4:
        mRaceGenderMask |= 0x40;
        break;
    case 5:
        mRaceGenderMask |= 0x80;
        break;
    case 6:
        mRaceGenderMask |= 0x100;
        break;
    case 7:
        mRaceGenderMask |= 0x200;
        break;
    case 33:
        mRaceGenderMask |= 0x400;
        break;
    case 49:
        mRaceGenderMask |= 0x800;
        break;

    default:
        break;
    }

    // set gender flag
    if(female)
    {
        mRaceGenderMask |= 0x1;
    }
    else
    {
        mRaceGenderMask |= 0x2;
    }

    // set jedi flag
    if(PlayerObject* player = dynamic_cast<PlayerObject*>(this))
    {
        if(player->getJediState())
        {
            mRaceGenderMask |= 0x1000;
        }
    }
}
//gets called by the active Object of the statemanager
void CreatureObject::creatureActionStateUpdate()
{
	gMessageLib->sendPostureAndStateUpdate(this);   
}
void CreatureObject::creaturePostureUpdate()
{
    this->updateMovementProperties();
}

void CreatureObject::creatureLocomotionUpdate()
{
    this->updateMovementProperties();
}

//=============================================================================
//
// incap
//
void CreatureObject::incap()
{
    // sanity check
    if (isIncapacitated() || isDead())
    {
        return;
    }

    if (this->getType() == ObjType_Player)
    {
        // first incap, update the initial time
        uint64 localTime = Anh_Utils::Clock::getSingleton()->getLocalTime();
        if(!mIncapCount)
        {
            mFirstIncapTime = localTime;
        }
        // reset the counter if the reset time has passed
        else if(mIncapCount != 0 && (localTime - mFirstIncapTime) >= gWorldConfig->getIncapResetTime() * 1000)
        {
            mIncapCount = 0;
            mFirstIncapTime = localTime;
        }
        /*
        if (mIncapCount != 0)
        {
        }
        */

        PlayerObject* player = dynamic_cast<PlayerObject*>(this);
        if (player)
        {
            player->disableAutoAttack();
        }
        //See if our player is mounted -- if so dismount him
        if(player->checkIfMounted())
        {
            //Get the player's mount
            if(VehicleController* vehicle = dynamic_cast<VehicleController*>(gWorldManager->getObjectById(player->getMount()->controller())))
            {
                //Now dismount
                vehicle->DismountPlayer();
            }

        }

        // advance incaps counter
        uint32 configIncapCount = gWorldConfig->getConfiguration<uint32>("Player_Incapacitation",3);
        if(++mIncapCount < (uint8)configIncapCount)
        {
            // update the posture and locomotion
            gStateManager.setCurrentPostureState(this, CreaturePosture_Incapacitated);
            gStateManager.setCurrentActionState(this, CreatureState_ClearState);

            // send timer updates
            mCurrentIncapTime = gWorldConfig->getBaseIncapTime() * 1000;
            gMessageLib->sendIncapTimerUpdate(this);

            // schedule recovery event
            mObjectController.addEvent(new IncapRecoveryEvent(),mCurrentIncapTime);

            // reset states
            mState = 0;

            //updateMovementProperties();

            //gMessageLib->sendPostureAndStateUpdate(this);

            //if(PlayerObject* player = dynamic_cast<PlayerObject*>(this))
            //{
            //    gMessageLib->sendUpdateMovementProperties(player);
            //    gMessageLib->sendSelfPostureUpdate(player);
            //}
        }
        // we hit the max -> death
        else
        {
            die();
        }
    }
    else if (this->getType() == ObjType_Creature)	// A Creature.
    {
        die();
    }
    else
    {
        LOG(info) << "CreatureObject::incap Incapped unsupported type " <<  this->getType();
    }

}

//=============================================================================

void CreatureObject::die()
{
    mIncapCount			= 0;
    mCurrentIncapTime	= 0;
    mFirstIncapTime		= 0;

    gMessageLib->sendIncapTimerUpdate(this);

    if(PlayerObject* player = dynamic_cast<PlayerObject*>(this))
    {
        gMessageLib->SendSystemMessage(::common::OutOfBand("base_player", "victim_dead"), player);
        player->disableAutoAttack();
    }

    updateMovementProperties();

    gStateManager.setCurrentPostureState(this, CreaturePosture_Dead);
    // clear states
    gStateManager.setCurrentActionState(this, CreatureState_ClearState);

    //gMessageLib->sendPostureAndStateUpdate(this);

    if(PlayerObject* player = dynamic_cast<PlayerObject*>(this))
    {
        /*gMessageLib->sendUpdateMovementProperties(player);
        gMessageLib->sendSelfPostureUpdate(player);*/

        // update duel lists
        player->clearDuelList();

        // update defender lists
		auto defenderList = GetDefender();
        auto defenderIt = defenderList.begin();
        while (defenderIt != defenderList.end())
        {
            if (CreatureObject* defenderCreature = dynamic_cast<CreatureObject*>(gWorldManager->getObjectById((*defenderIt))))
            {
                defenderCreature->RemoveDefender(this->getId());

                if (PlayerObject* defenderPlayer = dynamic_cast<PlayerObject*>(defenderCreature))
                {
                    gMessageLib->sendUpdatePvpStatus(this,defenderPlayer);
                    // gMessageLib->sendDefenderUpdate(defenderPlayer,0,0,this->getId());
                }

                // Defender not hostile to me any more.
                gMessageLib->sendUpdatePvpStatus(defenderCreature, player);

                // if no more defenders, clear combat state
                if (!defenderCreature->GetDefender().size())
                {
                    gStateManager.setCurrentActionState(defenderCreature, CreatureState_Peace);
                }
            }
            // If we remove self from all defenders, then we should remove all defenders from self. Remember, we are dead.
            RemoveDefender(*defenderIt);
        }

        // bring up the clone selection window
        ObjectSet inRangeBuildings;
        StringVector buildingNames;
        std::vector<BuildingObject*> buildings;
        BuildingObject*	nearestBuilding = NULL;
        BuildingObject* preDesignatedBuilding = NULL;

        // Search for cloning facilities.
        gSpatialIndexManager->getObjectsInRange(this,&inRangeBuildings,ObjType_Building,8192, false);

        ObjectSet::iterator buildingIt = inRangeBuildings.begin();

        while (buildingIt != inRangeBuildings.end())
        {
            BuildingObject* building = dynamic_cast<BuildingObject*>(*buildingIt);
            if (building)
            {
                // Do we have any personal clone location?
                if (building->getId() == player->getPreDesignatedCloningFacilityId())
                {
                    preDesignatedBuilding = building;
                }

                if (building->getBuildingFamily() == BuildingFamily_Cloning_Facility)
                {
                    // TODO: This code is not working as intended if player dies inside, since buildings use world coordinates and players inside have cell coordinates.
                    // Tranformation is needed before the correct distance can be calculated.
                    if(!nearestBuilding	||
                        (nearestBuilding != building && (glm::distance(mPosition, building->mPosition) < glm::distance(mPosition, nearestBuilding->mPosition))))
                    {
                        nearestBuilding = building;
                    }
                }
            }
            ++buildingIt;
        }

        if (nearestBuilding)
        {
            if (nearestBuilding->getSpawnPoints()->size())
            {
                // buildingNames.push_back(nearestBuilding->getRandomSpawnPoint()->mName.getAnsi());
                // buildingNames.push_back("Closest Cloning Facility");
                buildingNames.push_back("@base_player:revive_closest");
                buildings.push_back(nearestBuilding);

                // Save nearest building in case we need to clone when reviev times out.
                player->saveNearestCloningFacility(nearestBuilding);
            }
        }

        if (preDesignatedBuilding)
        {
            if (preDesignatedBuilding->getSpawnPoints()->size())
            {
                // buildingNames.push_back("Pre-Designated Facility");
                buildingNames.push_back("@base_player:revive_bind");
                buildings.push_back(preDesignatedBuilding);
            }
        }

        if (buildings.size())
        {
            // Set up revive timer in case of user don't clone.
            gWorldManager->addPlayerObjectForTimedCloning(player->getId(), 10 * 60 * 1000);	// TODO: Don't use hardcoded value.
            gUIManager->createNewCloneSelectListBox(player,"cloneSelect","Select Clone Destination","Select clone destination",buildingNames,buildings,player);
        }
        else
        {
            DLOG(info) << "No cloning facility available ";
        }
    }
    else // if(CreatureObject* creature = dynamic_cast<CreatureObject*>(this))
    {
        // gMessageLib->sendUpdateMovementProperties(player);
        // gMessageLib->sendSelfPostureUpdate(player);

        // Who killed me
        // this->prepareCustomRadialMenu(this,0);

        //
        // update defender lists
		auto defenderList = GetDefender();
        auto defenderIt = defenderList.begin();

        while(defenderIt != defenderList.end())
        {
            // This may be optimized, but rigth now THIS code does the job.
            this->makePeaceWithDefender(*defenderIt);
            defenderIt++;//were using a copy
        }

        // I'm dead. De-spawn after "loot-timer" has expired, if any.
        // 3-5 min.
        uint64 timeBeforeDeletion = (180 + (gRandom->getRand() % (int32)120)) * 1000;
        if (this->getCreoGroup() == CreoGroup_AttackableObject)
        {
            // timeBeforeDeletion = 2 * 1000;
            // Almost NOW!
            timeBeforeDeletion = 500;

            // Play the death-animation, if any.
            if (AttackableStaticNpc* staticNpc = dynamic_cast<AttackableStaticNpc*>(this))
            {
                staticNpc->playDeathAnimation();
            }
        }
        else if (this->getCreoGroup() == CreoGroup_Creature)
        {
            if (AttackableCreature* creature = dynamic_cast<AttackableCreature*>(this))
            {
                creature->unequipWeapon();

                // Give XP to the one(s) attacking me.
                // creature->updateAttackersXp();
            }
        }

        if (NPCObject* npc = dynamic_cast<NPCObject*>(this))
        {
            // Give XP to the one(s) attacking me.
            npc->updateAttackersXp();
        }

        // Put this creature in the pool of delayed destruction.
        gWorldManager->addCreatureObjectForTimedDeletion(this->getId(), timeBeforeDeletion);
        // this->killEvent();
    }
}


//=============================================================================

std::queue<std::pair<uint8_t, std::string>> CreatureObject::GetSkillsSyncQueue()
{
    auto lock = AcquireLock();
    return GetSkillsSyncQueue(lock);
}

std::queue<std::pair<uint8_t, std::string>> CreatureObject::GetSkillsSyncQueue(boost::unique_lock<boost::mutex>& lock)
{
    return skills_sync_queue_;
}


//=============================================================================

uint64 CreatureObject::getNearestDefender(void)
{
    uint64 defenederId = 0;
    float minLenght = FLT_MAX;

	auto defenderList = GetDefender();
    auto it = defenderList.begin();

    while(it != defenderList.end())
    {
		CreatureObject* defender = dynamic_cast<CreatureObject*>(gWorldManager->getObjectById((*it)));
                    
        if (defender && !defender->isDead() && !defender->isIncapacitated())
        {
            float len = glm::distance(this->mPosition, defender->mPosition);
            if (len < minLenght)
            {
                minLenght = len;
                defenederId = (*it);
            }
        }
        ++it;
    }

    return defenederId;
}



//=============================================================================

void CreatureObject::buildCustomization(uint16 customization[])
{
    uint8 len = 0x73;

    uint8* playerCustomization = new uint8[512];

    uint16 byteCount = 4; // 2 byte header + footer
    uint8 elementCount = 0;

    // get absolute bytecount(1 byte index + value)
    for(uint8 i = 1; i < len; i++)
    {
        if((customization[i] != 0))
        {
            if(customization[i] < 255)
                byteCount += 2;
            else
                byteCount += 3;

            elementCount++;
        }
    }
    //get additional bytes for female chest
    bool			female = false;
    PlayerObject*	player = dynamic_cast<PlayerObject*>(this);

    if(player)
        female = player->getGender();

    if(female)
    {
        //please note its 1 index with 2 attribute values!!!!
        byteCount += 1;
        elementCount++;

        for(uint8 i = 171; i < 173; i++)
        {
            if(customization[i] == 0)
                customization[i] = 511;

            if(customization[i] == 255)
                customization[i] = 767;

            if((customization[i] < 255)&&(customization[i] > 0))
                byteCount += 1;
            else
                byteCount += 2;
        }
    }

    // elements count
    playerCustomization[0] = 0x01;
    playerCustomization[1] = elementCount;

    uint16 j = 2;
    //get additional bytes for female chest
    if(female)
    {
        playerCustomization[j] = 171;
        j += 1;

        for(uint8 i = 171; i < 173; i++)
        {
            if((customization[i] < 255)&(customization[i] > 0))
            {
                playerCustomization[j] = (uint8)(customization[i] & 0xff);
                j += 1;
            }
            else
            {
                playerCustomization[j] = (uint8)(customization[i] & 0xff);
                playerCustomization[j+1] = (uint8)((customization[i] >> 8) & 0xff);
                j += 2;
            }

        }
    }



    // fill our string
    for(uint8 i = 1; i < len; i++)
    {
        if((customization[i] != 0))
        {
            playerCustomization[j] = i;

            if(customization[i] < 255)
            {
                playerCustomization[j+1] = (uint8)(customization[i] & 0xff);
                j += 2;
            }
            else
            {
                playerCustomization[j+1] = (uint8)(customization[i] & 0xff);
                playerCustomization[j+2] = (uint8)((customization[i] >> 8) & 0xff);
                j += 3;
            }
        }
    }

    // footer
    playerCustomization[j] = 0xff;
    playerCustomization[j+1] = 0x03;
    playerCustomization[j+2] = '\0';

    setCustomizationStr((int8*)playerCustomization);
}


//=============================================================================
//
//	Make peace with a creature or player.
//
//	This is the prefered method for creatures to handle peace.
//	Remove both themself and defender from each others defender list and eventually clear combat state and pvp status.
//

void CreatureObject::makePeaceWithDefender(uint64 defenderId)
{
    // Atempting a forced peace is no good.

    PlayerObject* attackerPlayer = dynamic_cast<PlayerObject*>(this);

    CreatureObject* defenderCreature = dynamic_cast<CreatureObject*>(gWorldManager->getObjectById(defenderId));
    // assert(defenderCreature)
    PlayerObject* defenderPlayer = NULL;
    if (defenderCreature)
    {
        defenderPlayer = dynamic_cast<PlayerObject*>(defenderCreature);
    }

    if (defenderPlayer)
    {
        if (attackerPlayer)
        {
            // Players do not make peace with other players.
            return;
        }
    }
    else
    {
        return;
    }

    // Remove defender from my list.
    if (defenderCreature)
    {
		RemoveDefender(defenderCreature->getId());
    }

    if (defenderPlayer)
    {
        // Update defender about attacker pvp status.
        gMessageLib->sendUpdatePvpStatus(this, defenderPlayer);
    }

    if (this->GetDefender().size() == 0)
    {
        // I have no more defenders.
        if (attackerPlayer)
        {
            // Update player (my self) with the new status.
            gMessageLib->sendUpdatePvpStatus(this,attackerPlayer);
        }
        else
        {
            // We are a npc, and we have no more defenders (for whatever reason).
            // Inform npc about this event.
            this->inPeace();
        }
        gStateManager.setCurrentActionState(this, CreatureState_Peace);
    }
    else
    {
        // gMessageLib->sendNewDefenderList(this);
    }
    // gMessageLib->sendNewDefenderList(this);


    if (defenderCreature)
    {
        // Remove us from defenders list.
        defenderCreature->RemoveDefender(this->getId());

        if (attackerPlayer)
        {
            // Update attacker about defender pvp status.
            gMessageLib->sendUpdatePvpStatus(defenderCreature, attackerPlayer);
        }

        if (defenderCreature->GetDefender().size() == 0)
        {
            // He have no more defenders.

            if (defenderPlayer)
            {
                gMessageLib->sendUpdatePvpStatus(defenderCreature,defenderPlayer);
            }
            else
            {
                // We are a npc, and we have no more defenders (for whatever reason).
                // Inform npc about this event.
                defenderCreature->inPeace();
            }
            gStateManager.setCurrentActionState(defenderCreature, CreatureState_Peace);
        }
        else
        {
            //gMessageLib->sendNewDefenderList(defenderCreature);
        }
        // gMessageLib->sendNewDefenderList(defenderCreature);
    }
}




uint32	CreatureObject::UpdatePerformanceCounter()
{
    return (uint32) gWorldManager->GetCurrentGlobalTick();
}

Object* CreatureObject::getTarget() const
{
    return gWorldManager->getObjectById(mTargetId);
}


//=============================================================================
//handles building custom radials
void CreatureObject::prepareCustomRadialMenu(CreatureObject* creatureObject, uint8 itemCount)
{

    return;
}

//=============================================================================
//handles the radial selection

void CreatureObject::handleObjectMenuSelect(uint8 messageType,Object* srcObject)
{

    if(PlayerObject* player = dynamic_cast<PlayerObject*>(srcObject))
    {
            
    }
}


bool CreatureObject::SerializeCurrentStats(swganh::messages::BaseSwgMessage* message)
{
    auto lock = AcquireLock();
    return SerializeCurrentStats(message, lock);
}

bool CreatureObject::SerializeCurrentStats(swganh::messages::BaseSwgMessage* message, boost::unique_lock<boost::mutex>& lock)
{
    return stat_current_list_.Serialize(message);
}

bool CreatureObject::SerializeMaxStats(swganh::messages::BaseSwgMessage* message)
{
    auto lock = AcquireLock();
    return SerializeMaxStats(message, lock);
}

bool CreatureObject::SerializeMaxStats(swganh::messages::BaseSwgMessage* message, boost::unique_lock<boost::mutex>& lock)
{
    return stat_max_list_.Serialize(message);
}

void CreatureObject::setFirstName(std::string name)	{ 
	auto lock = AcquireLock(); 
	setFirstName(lock, name); 
}

void CreatureObject::setFirstName(boost::unique_lock<boost::mutex>& lock, std::string name)	{ 
	first_name = name; 
	std::stringstream stream;
	stream << getFirstName() << " " << getLastName();
	std::string name_ansi = stream.str();
	std::u16string name_u16(name_ansi.begin(), name_ansi.end());
	lock.unlock();
	setCustomName(name_u16);
}

void CreatureObject::setLastName(std::string name)
{ 
	auto lock = AcquireLock(); 
	setLastName(lock, name); 
}

void CreatureObject::setLastName(boost::unique_lock<boost::mutex>& lock, std::string name)
{ 
	last_name = name; 
	std::stringstream stream;
	stream << getFirstName() << " " << getLastName();
	std::string name_ansi = stream.str();
	std::u16string name_u16(name_ansi.begin(), name_ansi.end());
	lock.unlock();
	setCustomName(name_u16);
}

void CreatureObject::SetStatCurrent(uint16_t stat_index, int32_t value)
{
    auto lock = AcquireLock();
    SetStatCurrent(stat_index, value, lock);
}

void CreatureObject::SetStatCurrent(uint16_t stat_index, int32_t value, boost::unique_lock<boost::mutex>& lock)
{
    stat_current_list_.update(stat_index, value);

	auto dispatcher = GetEventDispatcher();
	dispatcher->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::StatCurrent", this));
	//dispatcher->DispatchMainThread(std::make_shared<CreatureObjectEvent>("CreatureObject::PersistStatCurrent", (this)));
}

void CreatureObject::InitStatCurrent( int32_t value)
{
    auto lock = AcquireLock();
    InitStatCurrent(value, lock);
}

void CreatureObject::InitStatCurrent( int32_t value, boost::unique_lock<boost::mutex>& lock)
{
    stat_current_list_.add_initialize( value);
}

void CreatureObject::InitStatCurrent(uint16_t stat_index, int32_t value)
{
    auto lock = AcquireLock();
    InitStatCurrent(stat_index, value, lock);
}

void CreatureObject::InitStatCurrent(uint16_t stat_index, int32_t value, boost::unique_lock<boost::mutex>& lock)
{
    stat_current_list_.update(stat_index, value, false);
}

void CreatureObject::AddStatCurrent(uint16_t stat_index, int32_t value)
{
    auto lock = AcquireLock();
    AddStatCurrent(stat_index, value, lock);
}

void CreatureObject::AddStatCurrent(uint16_t stat_index, int32_t value, boost::unique_lock<boost::mutex>& lock)
{
    int32_t new_value = stat_current_list_[stat_index] + value;
    stat_current_list_.update(stat_index, new_value);
	lock.unlock();

	auto dispatcher = GetEventDispatcher();
	dispatcher->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::StatCurrent", this));
	//dispatcher->DispatchMainThread(std::make_shared<CreatureObjectEvent>("CreatureObject::PersistStatCurrent", (this)));
}

void CreatureObject::DeductStatCurrent(uint16_t stat_index, int32_t value)
{
    auto lock = AcquireLock();
    lock.unlock();

	auto dispatcher = GetEventDispatcher();
	dispatcher->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::StatCurrent", this));
	//dispatcher->DispatchMainThread(std::make_shared<CreatureObjectEvent>("CreatureObject::PersistStatCurrent", (this)));
}

void CreatureObject::DeductStatCurrent(uint16_t stat_index, int32_t value, boost::unique_lock<boost::mutex>& lock)
{
    int32_t current = stat_current_list_[stat_index];
    stat_current_list_.update(stat_index, (current > value) ? (current - value) : 0);
    lock.unlock();

	auto dispatcher = GetEventDispatcher();
	dispatcher->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::StatCurrent", this));
	//dispatcher->DispatchMainThread(std::make_shared<CreatureObjectEvent>("CreatureObject::PersistStatCurrent", (this)));
}

std::vector<int32_t> CreatureObject::GetCurrentStats(void)
{
    auto lock = AcquireLock();
    return GetCurrentStats(lock);
}

std::vector<int32_t> CreatureObject::GetCurrentStats(boost::unique_lock<boost::mutex>& lock)
{
    return stat_current_list_.raw();
}

int32_t CreatureObject::GetStatCurrent(const uint16_t stat_index)
{
    auto lock = AcquireLock();
    return GetStatCurrent(stat_index, lock);
}

int32_t CreatureObject::GetStatCurrent(const uint16_t stat_index, boost::unique_lock<boost::mutex>& lock)
{
    return stat_current_list_[stat_index];
}

void CreatureObject::SetStatMax(uint16_t stat_index, int32_t value)
{
    auto lock = AcquireLock();
    SetStatMax(stat_index, value, lock);
}

void CreatureObject::SetStatMax(uint16_t stat_index, int32_t value, boost::unique_lock<boost::mutex>& lock)
{
    stat_max_list_.update(stat_index, value);
	auto dispatcher = GetEventDispatcher();
	lock.unlock();
	dispatcher->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::StatMax", this));
	//DISPATCH(Creature, StatMax);
	

}

void CreatureObject::InitStatMax( int32_t value)
{
    auto lock = AcquireLock();
    InitStatMax(value, lock);
}

void CreatureObject::InitStatMax( int32_t value, boost::unique_lock<boost::mutex>& lock)
{
    stat_max_list_.add_initialize( value);
}

void CreatureObject::InitStatMax(uint16_t stat_index, int32_t value)
{
    auto lock = AcquireLock();
    InitStatMax(stat_index, value, lock);
}

void CreatureObject::InitStatMax(uint16_t stat_index, int32_t value, boost::unique_lock<boost::mutex>& lock)
{
    stat_max_list_.update(stat_index, value, false);
}

void CreatureObject::AddStatMax(uint16_t stat_index, int32_t value)
{
    auto lock = AcquireLock();
    AddStatMax(stat_index, value, lock);
}

void CreatureObject::AddStatMax(uint16_t stat_index, int32_t value, boost::unique_lock<boost::mutex>& lock)
{
    stat_max_list_.update(stat_index, stat_max_list_[stat_index] + value);
	auto dispatcher = GetEventDispatcher();
	lock.unlock();
	dispatcher->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::StatMax", this));
    //DISPATCH(Creature, StatMax);
}

void CreatureObject::DeductStatMax(uint16_t stat_index, int32_t value)
{
    auto lock = AcquireLock();
    DeductStatMax(stat_index, value, lock);
}

void CreatureObject::DeductStatMax(uint16_t stat_index, int32_t value, boost::unique_lock<boost::mutex>& lock)
{
    int32_t current = stat_max_list_[stat_index];
    stat_max_list_.update(stat_index, (current > value) ? (current - value) : 0);
    
	auto dispatcher = GetEventDispatcher();
	lock.unlock();
	dispatcher->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::StatMax", this));
}

std::vector<int32_t> CreatureObject::GetMaxStats(void)
{
    auto lock = AcquireLock();
    return GetMaxStats(lock);
}

std::vector<int32_t> CreatureObject::GetMaxStats(boost::unique_lock<boost::mutex>& lock)
{
    return stat_max_list_.raw();
}

int32_t CreatureObject::GetStatMax(uint16_t stat_index)
{
    auto lock = AcquireLock();
    return GetStatMax(stat_index, lock);
}

int32_t CreatureObject::GetStatMax(uint16_t stat_index, boost::unique_lock<boost::mutex>& lock)
{
    return stat_max_list_[stat_index];
}

void CreatureObject::InitStatBase( int32_t value)
{
    auto lock = AcquireLock();
    InitStatBase(value, lock);
}

void CreatureObject::InitStatBase( int32_t value, boost::unique_lock<boost::mutex>& lock)
{
    stat_base_list_.add_initialize( value);
}

void CreatureObject::InitStatBase(uint16_t stat_index, int32_t value)
{
    auto lock = AcquireLock();
    InitStatBase(stat_index, value, lock);
}

void CreatureObject::InitStatBase(uint16_t stat_index, int32_t value, boost::unique_lock<boost::mutex>& lock)
{
    stat_base_list_.update(stat_index, value, false);
}

void CreatureObject::SetStatBase(uint16_t stat_index, int32_t value)
{
    auto lock = AcquireLock();
    SetStatBase(stat_index, value, lock);
}

void CreatureObject::SetStatBase(uint16_t stat_index, int32_t value, boost::unique_lock<boost::mutex>& lock)
{
    stat_base_list_.update(stat_index, value);

	auto dispatcher = GetEventDispatcher();
	lock.unlock();
	dispatcher->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::StatBase", this));
//    DISPATCH(Creature, StatBase);
}

void CreatureObject::AddStatBase(uint16_t stat_index, int32_t value)
{
    auto lock = AcquireLock();
    AddStatBase(stat_index, value, lock);
}

void CreatureObject::AddStatBase(uint16_t stat_index, int32_t value, boost::unique_lock<boost::mutex>& lock)
{
    uint32_t new_stat = stat_base_list_[stat_index] + value;
    stat_base_list_.update(stat_index, new_stat);
	auto dispatcher = GetEventDispatcher();
	lock.unlock();
	dispatcher->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::StatBase", this));
}

void CreatureObject::DeductStatBase(uint16_t stat_index, int32_t value)
{
    auto lock = AcquireLock();
    DeductStatBase(stat_index, value, lock);
}

void CreatureObject::DeductStatBase(uint16_t stat_index, int32_t value, boost::unique_lock<boost::mutex>& lock)
{
    int32_t current = stat_base_list_[stat_index];
    stat_base_list_.update(stat_index, (current > value) ? current - value : 0);
	
	auto dispatcher = GetEventDispatcher();
	lock.unlock();
	dispatcher->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::StatBase", this));
}

std::vector<int32_t> CreatureObject::GetBaseStats()
{
    auto lock = AcquireLock();
    return GetBaseStats(lock);
}

std::vector<int32_t> CreatureObject::GetBaseStats(boost::unique_lock<boost::mutex>& lock)
{
    return stat_base_list_.raw();
}

int32_t CreatureObject::GetStatBase(uint16_t stat_index)
{
    auto lock = AcquireLock();
    return GetStatBase(stat_index, lock);
}

int32_t CreatureObject::GetStatBase(uint16_t stat_index, boost::unique_lock<boost::mutex>& lock)
{
    return stat_base_list_[stat_index];
}

bool  CreatureObject::SerializeBaseStats(swganh::messages::BaseSwgMessage* message)
{
    auto lock = AcquireLock();
    return SerializeBaseStats(message, lock);
}

bool  CreatureObject::SerializeBaseStats(swganh::messages::BaseSwgMessage* message, boost::unique_lock<boost::mutex>& lock)
{
    return stat_base_list_.Serialize(message);
}

void CreatureObject::InitStatWound( int32_t value)
{
    auto lock = AcquireLock();
    InitStatWound(value, lock);
}

void CreatureObject::InitStatWound( int32_t value, boost::unique_lock<boost::mutex>& lock)
{
    stat_wound_list_.add_initialize( value);
}

void CreatureObject::InitStatWound(uint16_t stat_index, int32_t value)
{
    auto lock = AcquireLock();
    InitStatWound(stat_index, value, lock);
}

void CreatureObject::InitStatWound(uint16_t stat_index, int32_t value, boost::unique_lock<boost::mutex>& lock)
{
    stat_wound_list_.update(stat_index, value, false);
}


void CreatureObject::SetStatWound(uint16_t stat_index, int32_t value)
{
    auto lock = AcquireLock();
    SetStatWound(stat_index, value, lock);
}

void CreatureObject::SetStatWound(uint16_t stat_index, int32_t value, boost::unique_lock<boost::mutex>& lock)
{
    stat_wound_list_.update(stat_index, value);
    
	auto dispatcher = GetEventDispatcher();
	lock.unlock();
	dispatcher->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::StatWound", this));
	
}

void CreatureObject::AddStatWound(uint16_t stat_index, int32_t value)
{
    auto lock = AcquireLock();
    AddStatWound(stat_index, value, lock);
}

void CreatureObject::AddStatWound(uint16_t stat_index, int32_t value, boost::unique_lock<boost::mutex>& lock)
{
    stat_wound_list_.update(stat_index, stat_wound_list_[stat_index] + value);
	
	auto dispatcher = GetEventDispatcher();
	lock.unlock();
	dispatcher->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::StatWound", this));
	
}

void CreatureObject::DeductStatWound(uint16_t stat_index, int32_t value)
{
    auto lock = AcquireLock();
    DeductStatWound(stat_index, value, lock);
}

void CreatureObject::DeductStatWound(uint16_t stat_index, int32_t value, boost::unique_lock<boost::mutex>& lock)
{
    int32_t current = stat_wound_list_[stat_index];
    stat_wound_list_.update(stat_index, (current > value) ? current - value : 0);
	
	auto dispatcher = GetEventDispatcher();
	lock.unlock();
	dispatcher->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::StatWound", this));
}

std::vector<int32_t> CreatureObject::GetStatWounds()
{
    auto lock = AcquireLock();
    return GetStatWounds(lock);
}

std::vector<int32_t> CreatureObject::GetStatWounds(boost::unique_lock<boost::mutex>& lock)
{
    return stat_wound_list_.raw();
}

int32_t CreatureObject::GetStatWound(uint16_t stat_index)
{
    auto lock = AcquireLock();
    return GetStatWound(stat_index, lock);
}

int32_t CreatureObject::GetStatWound(uint16_t stat_index, boost::unique_lock<boost::mutex>& lock)
{
    return stat_wound_list_[stat_index];
}

void CreatureObject::InitStatEncumberance( int32_t value)
{
    auto lock = AcquireLock();
    InitStatEncumberance(value, lock);
}

void CreatureObject::InitStatEncumberance( int32_t value, boost::unique_lock<boost::mutex>& lock)
{
    stat_encumberance_list_.add_initialize( value);
}


void CreatureObject::SetStatEncumberance(uint16_t stat_index, int32_t value)
{
    auto lock = AcquireLock();
    SetStatEncumberance(stat_index, value, lock);
}

void CreatureObject::SetStatEncumberance(uint16_t stat_index, int32_t value, boost::unique_lock<boost::mutex>& lock)
{
	stat_encumberance_list_.update(stat_index, value);
	
	auto dispatcher = GetEventDispatcher();
	lock.unlock();
	dispatcher->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::StatEncumberance", this));
	
}

void CreatureObject::AddStatEncumberance(uint16_t stat_index, int32_t value)
{
    auto lock = AcquireLock();
    AddStatEncumberance(stat_index, value, lock);
}

void CreatureObject::AddStatEncumberance(uint16_t stat_index, int32_t value, boost::unique_lock<boost::mutex>& lock)
{
    int32_t new_stat = stat_encumberance_list_[stat_index] + value;
    stat_encumberance_list_.update(stat_index, new_stat);
    
	auto dispatcher = GetEventDispatcher();
	lock.unlock();
	dispatcher->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::StatEncumberance", this));
}

void CreatureObject::DeductStatEncumberance(uint16_t stat_index, int32_t value)
{
    auto lock = AcquireLock();
    DeductStatEncumberance(stat_index, value, lock);
}

void CreatureObject::DeductStatEncumberance(uint16_t stat_index, int32_t value, boost::unique_lock<boost::mutex>& lock)
{
    int32_t current = stat_encumberance_list_[stat_index];
    stat_encumberance_list_.update(stat_index, (current > value) ? (current - value) : 0);
    auto dispatcher = GetEventDispatcher();
	lock.unlock();
	dispatcher->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::StatEncumberance", this));
}

std::vector<int32_t> CreatureObject::GetStatEncumberances()
{
    auto lock = AcquireLock();
    return GetStatEncumberances(lock);
}

std::vector<int32_t> CreatureObject::GetStatEncumberances(boost::unique_lock<boost::mutex>& lock)
{
    return stat_encumberance_list_.raw();
}

int32_t CreatureObject::GetStatEncumberance(uint16_t stat_index)
{
    auto lock = AcquireLock();
    return GetStatEncumberance(stat_index, lock);
}

int32_t CreatureObject::GetStatEncumberance(uint16_t stat_index, boost::unique_lock<boost::mutex>& lock)
{
    return stat_encumberance_list_[stat_index];
}

bool CreatureObject::SerializeStatWounds(swganh::messages::BaseSwgMessage* message)
{
    auto lock = AcquireLock();
    return SerializeStatWounds(message, lock);
}

bool CreatureObject::SerializeStatWounds(swganh::messages::BaseSwgMessage* message, boost::unique_lock<boost::mutex>& lock)
{
    return stat_wound_list_.Serialize(message);
}

bool CreatureObject::SerializeStatEncumberances(swganh::messages::BaseSwgMessage* message)
{
    auto lock = AcquireLock();
    return SerializeStatEncumberances(message, lock);
}

bool CreatureObject::SerializeStatEncumberances(swganh::messages::BaseSwgMessage* message, boost::unique_lock<boost::mutex>& lock)
{
    return stat_encumberance_list_.Serialize(message);
}

void CreatureObject::SetBattleFatigue(uint32_t battle_fatigue)
{
    auto lock = AcquireLock();
    SetBattleFatigue(battle_fatigue, lock);
}

void CreatureObject::SetBattleFatigue(uint32_t battle_fatigue, boost::unique_lock<boost::mutex>& lock)
{
    battle_fatigue_ = battle_fatigue;
	auto dispatcher = GetEventDispatcher();
	dispatcher->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::BattleFatigue", this));
}

void CreatureObject::AddBattleFatigue(uint32_t battle_fatigue)
{
    auto lock = AcquireLock();
    AddBattleFatigue(battle_fatigue, lock);
}

void CreatureObject::AddBattleFatigue(uint32_t battle_fatigue, boost::unique_lock<boost::mutex>& lock)
{
    battle_fatigue += battle_fatigue;
    auto dispatcher = GetEventDispatcher();
	dispatcher->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::BattleFatigue", this));
}

void CreatureObject::DeductBattleFatigue(uint32_t battle_fatigue)
{
    auto lock = AcquireLock();
    DeductBattleFatigue(battle_fatigue, lock);
}

void CreatureObject::DeductBattleFatigue(uint32_t battle_fatigue, boost::unique_lock<boost::mutex>& lock)
{
    battle_fatigue -= battle_fatigue;
    auto dispatcher = GetEventDispatcher();
	dispatcher->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::BattleFatigue", this));
}

uint32_t CreatureObject::GetBattleFatigue()
{
    auto lock = AcquireLock();
    return GetBattleFatigue(lock);
}

uint32_t CreatureObject::GetBattleFatigue(boost::unique_lock<boost::mutex>& lock)
{
    return battle_fatigue_;
}

void CreatureObject::InitializeEquipmentItem(swganh::object::EquipmentItem item)
{
	auto lock = AcquireLock();
    InitializeEquipmentItem(item, lock);
}

void CreatureObject::InitializeEquipmentItem(swganh::object::EquipmentItem item, boost::unique_lock<boost::mutex>& lock)
{
	equipment_list_.add(item);
}

void CreatureObject::AddEquipmentItem(swganh::object::EquipmentItem item)
{
    auto lock = AcquireLock();
    AddEquipmentItem(item, lock);
}

void CreatureObject::AddEquipmentItem(swganh::object::EquipmentItem item, boost::unique_lock<boost::mutex>& lock)
{
    equipment_list_.add(item);
	GetEventDispatcher()->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::EquipmentItem", this));
}

void CreatureObject::RemoveEquipmentItem(uint64_t object_id)
{
    auto lock = AcquireLock();
    RemoveEquipmentItem(object_id, lock);
}

void CreatureObject::RemoveEquipmentItem(uint64_t object_id, boost::unique_lock<boost::mutex>& lock)
{
    auto iter = std::find_if(equipment_list_.begin(), equipment_list_.end(), [=](swganh::object::EquipmentItem item)->bool
    {
        return (object_id == item.object_id);
    });

    if(iter != equipment_list_.end())    {
		equipment_list_.remove(iter);
		GetEventDispatcher()->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::EquipmentItem", this));
    }
	
	return;
    

    
}

void CreatureObject::UpdateEquipmentItem(swganh::object::EquipmentItem item)
{
    auto lock = AcquireLock();
    UpdateEquipmentItem(item, lock);
}

void CreatureObject::UpdateEquipmentItem(swganh::object::EquipmentItem item, boost::unique_lock<boost::mutex>& lock)
{
    uint64_t object_id = item.object_id;
    equipment_list_.update(std::find_if(equipment_list_.begin(), equipment_list_.end(), [&](swganh::object::EquipmentItem item)->bool
    {
        return (object_id == item.object_id);
    }));

    GetEventDispatcher()->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::EquipmentItem", this));
}

std::vector<swganh::object::EquipmentItem> CreatureObject::GetEquipment(void)
{
    auto lock = AcquireLock();
    return GetEquipment(lock);
}

std::vector<swganh::object::EquipmentItem> CreatureObject::GetEquipment(boost::unique_lock<boost::mutex>& lock)
{
    return equipment_list_.raw();
}

swganh::object::EquipmentItem CreatureObject::GetEquipmentItem(uint64_t object_id)
{
    auto lock = AcquireLock();
    return GetEquipmentItem(object_id, lock);
}

swganh::object::EquipmentItem CreatureObject::GetEquipmentItem(uint64_t object_id, boost::unique_lock<boost::mutex>& lock)
{
    auto iter = std::find_if(equipment_list_.begin(), equipment_list_.end(), [&](swganh::object::EquipmentItem object)
    {
		return object.object_id == object_id;
    });

    if(iter != equipment_list_.end())
    {
        return *iter;
    }

	LOG(error) << "CreatureObject::GetEquipmentItem : " << object_id << " not found";
	
	swganh::object::EquipmentItem a;

	return a;
    //throw EquipmentNotFound();
}

bool CreatureObject::SerializeEquipment(swganh::messages::BaseSwgMessage* message)
{
    auto lock = AcquireLock();
    return SerializeEquipment(message, lock);
}

bool CreatureObject::SerializeEquipment(swganh::messages::BaseSwgMessage* message, boost::unique_lock<boost::mutex>& lock)
{
    return equipment_list_.Serialize(message);
}

void CreatureObject::SetWeaponId(uint64_t weapon_id)
{
    auto lock = AcquireLock();
    SetWeaponId(weapon_id, lock);
}

void CreatureObject::SetWeaponId(uint64_t weapon_id, boost::unique_lock<boost::mutex>& lock)
{
    weapon_id_ = weapon_id;
    GetEventDispatcher()->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::WeaponId", this));
}

uint64_t CreatureObject::GetWeaponId()
{
    auto lock = AcquireLock();
    return GetWeaponId(lock);
}

uint64_t CreatureObject::GetWeaponId(boost::unique_lock<boost::mutex>& lock)
{
    return weapon_id_;
}

void CreatureObject::AddSkill(std::string skill)
{
    auto lock = AcquireLock();
    AddSkill(skill, lock);
}

void CreatureObject::AddSkill(std::string skill, boost::unique_lock<boost::mutex>& lock)
{
    skills_.add(skill);
	skills_sync_queue_.push(std::pair<uint8_t, std::string>(1, skill));
	
	GetEventDispatcher()->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::Skill", this));
}

void CreatureObject::InitializeSkill(std::string skill)
{
    auto lock = AcquireLock();
    InitializeSkill(skill, lock);
}

void CreatureObject::InitializeSkill(std::string skill, boost::unique_lock<boost::mutex>& lock)
{
    skills_.add(skill, false);
}

void CreatureObject::RemoveSkill(std::string skill)
{
    auto lock = AcquireLock();
    RemoveSkill(skill, lock);
}

void CreatureObject::RemoveSkill(std::string skill, boost::unique_lock<boost::mutex>& lock)
{
    skills_.remove(skill);
	skills_sync_queue_.push(std::pair<uint8_t, std::string>(0, skill));
    GetEventDispatcher()->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::Skill", this));
}

std::set<std::string> CreatureObject::GetSkills()
{
    auto lock = AcquireLock();
    return GetSkills(lock);
}

std::set<std::string> CreatureObject::GetSkills(boost::unique_lock<boost::mutex>& lock)
{
    return skills_.raw();
}

bool CreatureObject::HasSkill(std::string skill)
{
    auto lock = AcquireLock();
    return HasSkill(skill, lock);
}

bool CreatureObject::HasSkill(std::string skill, boost::unique_lock<boost::mutex>& lock)
{
    return skills_.contains(skill);
}

bool CreatureObject::SerializeSkills(swganh::messages::BaseSwgMessage* message)
{
    auto lock = AcquireLock();
    return SerializeSkills(message, lock);
}

bool CreatureObject::SerializeSkills(swganh::messages::BaseSwgMessage* message, boost::unique_lock<boost::mutex>& lock)
{
    return skills_.Serialize(message);
}

bool CreatureObject::SerializeSkillMods(swganh::messages::BaseSwgMessage* message)
{
    auto lock = AcquireLock();
    return SerializeSkillMods(message, lock);
}

bool CreatureObject::SerializeSkillMods(swganh::messages::BaseSwgMessage* message, boost::unique_lock<boost::mutex>& lock, bool baseline)
{
	//dont build deltas when we're empty lol
	if((skill_mod_list_.update_size() > 0) || baseline)	{
		skill_mod_list_.Serialize(message);
		return true;
	}
	return false;
}

PlayerObject*	CreatureObject::GetGhost()
{
	auto lock = AcquireLock();
	return GetGhost(lock);
}

PlayerObject*	CreatureObject::GetGhost(boost::unique_lock<boost::mutex>& lock)
{
	return ghost_;
}

void CreatureObject::SetPosture(uint32 posture)
{
    auto lock = AcquireLock();
    SetPosture(posture, lock);
}

void CreatureObject::SetPosture(uint32 posture, boost::unique_lock<boost::mutex>& lock)
{
    states.posture_ = posture;
	GetEventDispatcher()->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::Posture", this));
}

uint32_t	CreatureObject::GetPosture()
{
    auto lock = AcquireLock();
    return GetPosture(lock);
}

uint32_t CreatureObject::GetPosture(boost::unique_lock<boost::mutex>& lock)
{
    return states.posture_;
    //GetEventDispatcher()->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::Posture", this));
}

bool CreatureObject::HasSkillMod(std::string identifier)
{
    auto lock = AcquireLock();
    return HasSkillMod(identifier, lock);
}

bool CreatureObject::HasSkillMod(std::string identifier, boost::unique_lock<boost::mutex>& lock)
{

	auto it = skill_mod_list_.begin();
	while (it != skill_mod_list_.end())	{
		if(it->second.identifier == identifier)	{
			return true;
		}
		it++;
	}

    
	return false;
  
}

void CreatureObject::AddSkillMod(SkillModStruct mod)
{
    auto lock = AcquireLock();
    AddSkillMod(mod, lock);
}

void CreatureObject::AddSkillMod(SkillModStruct mod, boost::unique_lock<boost::mutex>& lock)
{
    skill_mod_list_.add(mod.identifier, mod);
	GetEventDispatcher()->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::SkillMod", this));
	
	
}

void CreatureObject::RemoveSkillMod(std::string identifier)
{
    auto lock = AcquireLock();
    RemoveSkillMod(identifier, lock);
}

void CreatureObject::RemoveSkillMod(std::string identifier, boost::unique_lock<boost::mutex>& lock)
{
	auto it = skill_mod_list_.begin();
	while (it != skill_mod_list_.end())	{
		if(it->second.identifier == identifier)	{
			skill_mod_list_.remove(it);
			GetEventDispatcher()->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::SkillMod", this));
			return;
		}
		it++;
	}    
}

void CreatureObject::SetSkillMod(SkillModStruct mod)
{
    auto lock = AcquireLock();
    SetSkillMod(mod, lock);
}

void CreatureObject::SetSkillMod(SkillModStruct mod, boost::unique_lock<boost::mutex>& lock)
{
    skill_mod_list_.update(mod.identifier);
	GetEventDispatcher()->Dispatch(std::make_shared<CreatureObjectEvent>("CreatureObject::SkillMod", this));
	
}

std::map<std::string, SkillModStruct> CreatureObject::GetSkillMods()
{
    auto lock = AcquireLock();
    return GetSkillMods(lock);
}

std::map<std::string, SkillModStruct> CreatureObject::GetSkillMods(boost::unique_lock<boost::mutex>& lock)
{
    return skill_mod_list_.raw();
}

SkillModStruct CreatureObject::GetSkillMod(std::string identifier)
{
    auto lock = AcquireLock();
    return GetSkillMod(identifier, lock);
}

SkillModStruct CreatureObject::GetSkillMod(std::string identifier, boost::unique_lock<boost::mutex>& lock)
{
	auto it = skill_mod_list_.begin();
	while (it != skill_mod_list_.end())	{
		if(it->second.identifier == identifier)	{
			return it->second;
		}
		it++;
	}

	SkillModStruct a;
	return a;
	/*
    auto iter = std::find_if(begin(skill_mod_list_), end(skill_mod_list_), [=](std::pair<std::string, SkillModStruct> pair)->bool
    {
        return (pair.second.identifier == identifier);
    });

    if(iter != end(skill_mod_list_))
    {
        return iter->second;
    }
	*/
}

//=============================================================================
// maps the incoming posture
//void CreatureObject::setLocomotionByPosture(uint32 posture)
//{
//    switch (posture)
//    {
//        case CreaturePosture_Upright: mLocomotion = CreatureLocomotion_Standing; break;
//        // not even sure if this is used.
//        case CreaturePosture_Crouched:
//        {
//            if(this->getCurrentSpeed() < this->getBaseAcceleration())
//                mLocomotion = CreatureLocomotion_CrouchWalking;
//            else
//                mLocomotion = CreatureLocomotion_CrouchSneaking;
//            break;
//        }
//        case CreaturePosture_Prone: mLocomotion = CreatureLocomotion_Prone; break;
//        case CreaturePosture_Sneaking: mLocomotion = CreatureLocomotion_Sneaking; break;
//        // is this used?
//        case CreaturePosture_Blocking: mLocomotion = CreatureLocomotion_Blocking; break;
//        // is this used?
//        case CreaturePosture_Climbing:
//        {
//                if(this->getCurrentSpeed() >0)
//                    mLocomotion = CreatureLocomotion_Climbing;
//                else
//                    mLocomotion = CreatureLocomotion_ClimbingStationary;
//                break;
//        }
//        case CreaturePosture_Flying: mLocomotion = CreatureLocomotion_Flying; break;
//        case CreaturePosture_LyingDown:	mLocomotion = CreatureLocomotion_LyingDown; break;
//        case CreaturePosture_Sitting: mLocomotion = CreatureLocomotion_Sitting; break;
//        case CreaturePosture_SkillAnimating: mLocomotion = CreatureLocomotion_SkillAnimating; break;
//        case CreaturePosture_DrivingVehicle: mLocomotion = CreatureLocomotion_DrivingVehicle; break;
//        case CreaturePosture_RidingCreature: mLocomotion = CreatureLocomotion_RidingCreature; break;
//        case CreaturePosture_KnockedDown: mLocomotion = CreatureLocomotion_KnockedDown; break;
//        case CreaturePosture_Incapacitated: mLocomotion = CreatureLocomotion_Incapacitated; break;
//        case CreaturePosture_Dead: mLocomotion = CreatureLocomotion_Dead; break;
//    }
//}
