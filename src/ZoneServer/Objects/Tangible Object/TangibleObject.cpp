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
#include "ZoneServer/Objects/Tangible Object/TangibleObject.h"
#include "ZoneServer\Objects\Creature Object\CreatureObject.h"
#include "ZoneServer/WorldManager.h"
#include "DatabaseManager/Database.h"
#include "ZoneServer\Objects\Tangible Object\tangible_message_builder.h"

#include <anh\app\swganh_kernel.h>

using namespace swganh::messages;

//=============================================================================

TangibleObject::TangibleObject()
    : Object()
    , mComplexity(1.0f)
    , mLastTimerUpdate(0)
    , mTimer(0)
    , mDamage(0)
    , mMaxCondition(100)
    , mTimerInterval(1000)
{
    mType				= ObjType_Tangible;
    mName				= "";
    mNameFile			= "";
    mDetailFile			= "";
    mColorStr			= "";
    mUnknownStr1		= "";
    mUnknownStr2		= "";
    mCustomizationStr	= "";
	

    //uint64 l = 0;
    for(uint16 i=0; i<256; i++)
        mCustomization[i] = 0;
		
}

//=============================================================================

TangibleObject::TangibleObject(uint64 id,uint64 parentId,std::string model,TangibleGroup tanGroup,TangibleType tanType,BString name,BString nameFile,BString detailFile)
    : Object(id,parentId,model,ObjType_Tangible)
    ,mDetailFile(detailFile)
    ,mTanGroup(tanGroup)
    ,mTanType(tanType)
	
{
    mColorStr			= "";
    mUnknownStr1		= "";
    mUnknownStr2		= "";
    
    mCustomizationStr	= "";
	
	for(uint16 i = 1;i<256;i++)
        mCustomization[i]=0;
}

//=============================================================================

TangibleObject::~TangibleObject()
{
}

//=============================================================================

void TangibleObject::initTimer(int32 count,int32 interval,uint64 startTime)
{
    mTimer				= count;
    mTimerInterval		= interval;
    mLastTimerUpdate	= startTime;
}

//=============================================================================

bool TangibleObject::updateTimer(uint64 callTime)
{
    if(callTime - mLastTimerUpdate >= mTimerInterval)
    {
        mTimer -= (int32)(mTimerInterval / 1000);

        if(mTimer < 0)
            mTimer = 0;

        if(mTimer <= 0)
        {
            mTimer				= 0;
            mLastTimerUpdate	= 0;
            mTimerInterval		= 1000;

            return(false);
        }

        mLastTimerUpdate = callTime;
    }

    return(true);
}

//=============================================================================
//build the customization string based on the rawdata
void TangibleObject::buildTanoCustomization(uint8 len)
{
    uint8 theCustomization[512];
	for(uint32 i = 0; i < 512; i++)	{
		theCustomization[i] = 0;
	}

    uint16 byteCount = 4; // 2 byte header + 2 byte footer
    uint8 elementCount = 0;

    // get absolute bytecount(1 byte index + value)
    for(uint8 i = 1; i < len; i++)
    {
        if(mCustomization[i] != 0)
        {
            if(mCustomization[i] == 0)
                mCustomization[i] = 511;

            if(mCustomization[i] == 255)
                mCustomization[i] = 767;

            if(mCustomization[i] < 255)
                byteCount += 2;
            else
                byteCount += 3;

            elementCount++;
        }
    }

    if(!elementCount)
    {
		
        setCustomizationStr(theCustomization);
        return;
    }

    // elements count
    theCustomization[0] = 0x01;
    theCustomization[1] = elementCount;

    uint16 j = 2;

    // fill our string
    for(uint8 i = 1; i < len; i++)
    {
        if(mCustomization[i] != 0)
        {
            theCustomization[j] = i;

            if(mCustomization[i] < 255)
            {
                theCustomization[j+1] = (uint8)(mCustomization[i] & 0xff);
                j += 2;
            }
            else
            {
                theCustomization[j+1] = (uint8)(mCustomization[i] & 0xff);
                theCustomization[j+2] = (uint8)((mCustomization[i] >> 8) & 0xff);
                j += 3;
            }
        }
    }

    // footer
    theCustomization[j] = 0xff;
    theCustomization[j+1] = 0x03;
    theCustomization[j+2] = '\0';

    setCustomizationStr(theCustomization);

	//if(getId() == 4831838212)	{
	//	LOG(info) << "TangibleObject::buildTanoCustomization :: " << getCustomizationStr().getLength() << " : " << getCustomizationStr().getAnsi();
	//}
  
}

//=============================================================================
// assign the item a new custom name
//
void TangibleObject::setCustomNameIncDB(const std::string name)
{
	custom_name_ = std::u16string(name.begin(), name.end());
    int8 sql[1024],restStr[128],*sqlPointer;
    sprintf(sql,"UPDATE %s.items SET customName='",gWorldManager->getKernel()->GetDatabase()->galaxy());
    sqlPointer = sql + strlen(sql);
    sqlPointer += gWorldManager->getKernel()->GetDatabase()->escapeString(sqlPointer,name.c_str(),name.length());
    sprintf(restStr,"' WHERE id=%"PRIu64" ",this->getId());

    strcat(sql,restStr);
    gWorldManager->getKernel()->GetDatabase()->executeSqlAsync(0,0,sql);
}

//=============================================================================
// assign the item a new parentid
//

void TangibleObject::setParentIdIncDB(uint64 parentId)
{
    mParentId = parentId;
    gWorldManager->getKernel()->GetDatabase()->executeSqlAsync(0,0,"UPDATE %s.items SET parent_id=%"PRIu64" WHERE id=%"PRIu64"",gWorldManager->getKernel()->GetDatabase()->galaxy(),mParentId,this->getId());
    
}


void TangibleObject::prepareCustomRadialMenuInCell(CreatureObject* creatureObject, uint8 itemCount)
{
    RadialMenu* radial	= new RadialMenu();
    uint8 i = 1;
    uint8 u = 1;

    // any object with callbacks needs to handle those (received with menuselect messages) !
	if(this->getHeadCount())
        radial->addItem(i++,0,radId_itemOpen,radAction_Default,"");

    radial->addItem(i++,0,radId_examine,radAction_Default,"");

    radial->addItem(i++,0,radId_itemPickup,radAction_Default,"");

    u = i;
    radial->addItem(i++,0,radId_itemMove,radAction_Default, "");
    radial->addItem(i++,u,radId_itemMoveForward,radAction_Default, "");//radAction_ObjCallback
    radial->addItem(i++,u,radId_ItemMoveBack,radAction_Default, "");
    radial->addItem(i++,u,radId_itemMoveUp,radAction_Default, "");
    radial->addItem(i++,u,radId_itemMoveDown,radAction_Default, "");

    u = i;
    radial->addItem(i++,0,radId_itemRotate,radAction_Default, "");
    radial->addItem(i++,u,radId_itemRotateRight,radAction_Default, "");
    radial->addItem(i++,u,radId_itemRotateLeft,radAction_Default, "");


    RadialMenuPtr radialPtr(radial);
    mRadialMenu = radialPtr;


}

uint64 TangibleObject::getNearestAttackingDefender(void)
{
    uint64 defenederId = 0;
    float minLenght = FLT_MAX;

	auto defenderList = GetDefender();
    auto it = defenderList.begin();

    while(it != defenderList.end())
    {
        if((*it) != 0)
        {
            CreatureObject* defender = dynamic_cast<CreatureObject*>(gWorldManager->getObjectById((*it)));
            if (defender && !defender->isDead() && !defender->isIncapacitated())
            {
                if ((defender->getCreoGroup() == CreoGroup_Player) || (defender->getCreoGroup() == CreoGroup_Creature))
                {
                    float len = glm::distance(this->mPosition, defender->mPosition);
                    if (len < minLenght)
                    {
                        minLenght = len;
                        defenederId = (*it);
                    }
                }
            }
        }
        ++it;
    }

    return defenederId;
}

void TangibleObject::AddDefender(uint64 defenderId)
{
    auto lock = AcquireLock();
    AddDefender(defenderId, lock);
}

void TangibleObject::AddDefender(uint64 defenderId, boost::unique_lock<boost::mutex>& lock)
{
    defender_list_.add(defenderId);
   
	lock.unlock();
	
	auto dispatcher = GetEventDispatcher();
	dispatcher->Dispatch(std::make_shared<TangibleObjectEvent>("TangibleObject::DefenderList", this));

}

uint32 TangibleObject::GetDefenderCounter()
{
    return defender_list_.get_counter();
}


void TangibleObject::RemoveDefender(uint64 defenderId)
{
    auto lock = AcquireLock();
    RemoveDefender(defenderId, lock);
}

void TangibleObject::RemoveDefender(uint64 defenderId, boost::unique_lock<boost::mutex>& lock)
{
    auto iter = std::find_if(defender_list_.begin(), defender_list_.end(), [=](uint64_t id)->bool
    {
        return (defenderId == id);
    });

    if(iter == defender_list_.end())
    {
        return;
    }
    defender_list_.remove(iter);

	lock.unlock();

	auto dispatcher = GetEventDispatcher();
	dispatcher->Dispatch(std::make_shared<TangibleObjectEvent>("TangibleObject::DefenderList", this));
}

std::vector<uint64> TangibleObject::GetDefender(void)
{
    auto lock = AcquireLock();
    return GetDefender(lock);
}

std::vector<uint64> TangibleObject::GetDefender(boost::unique_lock<boost::mutex>& lock)
{
    return defender_list_.raw();
}

bool TangibleObject::SerializeDefender(swganh::messages::BaseSwgMessage* message)
{
    auto lock = AcquireLock();
    return	SerializeDefender(message, lock);
}

bool TangibleObject::SerializeDefender(swganh::messages::BaseSwgMessage* message, boost::unique_lock<boost::mutex>& lock)
{
	return	defender_list_.Serialize(message);
}

void	TangibleObject::ClearDefender()
{
	auto defenderList = GetDefender();
	auto it = defenderList.begin();

    while(it != defenderList.end())
    {
        RemoveDefender(*it);
        ++it;
    }
}

bool TangibleObject::checkDefenderList(uint64 defenderId)
{
	auto defenderList = GetDefender();
    auto it = defenderList.begin();

    while(it != defenderList.end())
    {
        if((*it) == defenderId)
        {
            return(true);
        }

        ++it;
    }

    return(false);
}