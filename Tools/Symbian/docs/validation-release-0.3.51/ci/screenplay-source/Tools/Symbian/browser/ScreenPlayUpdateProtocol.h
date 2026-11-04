// Copyright (c) 2006-2010 Nokia Corporation and/or its subsidiary(-ies).
// All rights reserved.
// This component and the accompanying materials are made available
// under the terms of "Eclipse Public License v1.0"
// which accompanies this distribution, and is available
// at the URL "http://www.eclipse.org/legal/epl-v10.html".
//
// Initial Contributors:
// Nokia Corporation - initial contribution.
//
// Contributors:
//
// Description:
//
// Browser-local adaptation of graphicscomposition/surfaceupdate/inc/surfaceupdate.h
// Upstream revision: ff133bc50e6158bfb08cc093b0f0055321dcde99
// Upstream SHA-256: fb115fa08deb0c21263e68b0c53ab34cd09d63c2fcd3d1e69edf9e73a49bc244
// Mechanical changes are recorded in ScreenPlayUpdateSession.manifest.json.
// SPDX-License-Identifier: EPL-1.0


#ifndef SCREENPLAY_UPDATE_PROTOCOL_H
#define SCREENPLAY_UPDATE_PROTOCOL_H


#include <graphics/surface.h>


const TUint KScreenPlayUpdateServMajorVersionNumber = 1;
const TUint KScreenPlayUpdateServMinorVersionNumber = 1;
const TUint KScreenPlayUpdateServBuildVersionNumber = 3;


enum TScreenPlayUpdateServRqst
	{
	EScreenPlayUpdateServCreate = 1,
	EScreenPlayUpdateServClose,
	EScreenPlayUpdateServNotifyWhenAvailable,
	EScreenPlayUpdateServNotifyWhenDisplayed,
	EScreenPlayUpdateServNotifyWhenDisplayedXTimes,
	EScreenPlayUpdateServSubmitUpdate,
	EScreenPlayUpdateServCancelAllNotifications,
	EScreenPlayUpdateServLast = 1000,
	};

_LIT(KScreenPlayUpdateServerName, "SURFACE_UPDATE_SERVER");


#endif
