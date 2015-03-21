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

#ifndef ANH_ZONESERVER_UILISTBOX_H
#define ANH_ZONESERVER_UILISTBOX_H

#include "UIWindow.h"


//================================================================================

class UIListBox : public UIWindow
{
public:
	
	UIListBox(UICallback* callback,uint32 id,uint8 windowType,const int8* eventStr,BString caption,BString prompt,const StringVector dataItems,PlayerObject* playerObject,uint8 lbType = SUI_LB_OK, float distance = 0, uint64 object = 0, std::shared_ptr<WindowAsyncContainerCommand> container = nullptr);
    virtual ~UIListBox();

    StringVector*	getDataItems() {
        return &mDataItems;
    }

    void			addDataItem(std::string item) {
        mDataItems.push_back(item);
    }

    virtual void	handleEvent(Message* message);
    void			sendCreate();

protected:

    void		_initChildren();

    BString			mCaption;

    BString			mPrompt;
    uint8			mLbType;
    StringVector	mDataItems;
    float			mDistance;
    uint64			mObjectID;
};

//================================================================================

#endif




