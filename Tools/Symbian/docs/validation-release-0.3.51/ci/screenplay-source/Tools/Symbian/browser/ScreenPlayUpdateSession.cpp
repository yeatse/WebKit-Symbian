// Copyright (c) 2006-2009 Nokia Corporation and/or its subsidiary(-ies).
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
// Browser-local adaptation of graphicscomposition/surfaceupdate/src/surfaceupdateclient.cpp
// Upstream revision: ff133bc50e6158bfb08cc093b0f0055321dcde99
// Upstream SHA-256: 3802ef161726fcd7d84e4912958bc53749759df9859703584cf6bacfb845aede
// Mechanical changes are recorded in ScreenPlayUpdateSession.manifest.json.
// SPDX-License-Identifier: EPL-1.0


/**
 @file
*/

#include "ScreenPlayUpdateSession.h"
#include <e32debug.h>
#include "ScreenPlayUpdateProtocol.h"

inline void ScreenPlayUpdateAssert(TInt aCond)	{__ASSERT_ALWAYS(aCond, User::Panic(_L("ScreenPlayUpdateSession.cpp; assertion failed"), __LINE__));}

RScreenPlayUpdateSession::RScreenPlayUpdateSession() :
	RSessionBase(), 
	iStatusAvailable(NULL), iStatusDisplayed(NULL), iStatusDisplayedXTimes(NULL), iTimeStamp(NULL), iCount(0)
	{
	}

TInt RScreenPlayUpdateSession::Connect(TInt aMessageSlots)
	{
	if(Handle())
		{
		return KErrAlreadyExists; 
		}
	iStatusAvailable = NULL;
	iStatusDisplayed = NULL;
	iStatusDisplayedXTimes = NULL;
	
	//establish connection to the server
	return CreateSession(KScreenPlayUpdateServerName, Version(), aMessageSlots);
	}

void RScreenPlayUpdateSession::Close()	
	{
	if(Handle())
		{
		RHandleBase::Close();
		}
	iStatusAvailable = NULL;
	iStatusDisplayed = NULL;
	iStatusDisplayedXTimes = NULL;
	}

TInt RScreenPlayUpdateSession::SubmitUpdate(TInt aScreen, const TSurfaceId& aSurfaceId, 
									TInt aBuffer, const TRegion* aDirtyRegion)
	{
	ScreenPlayUpdateAssert(Handle() != KNullHandle);

#if defined _DEBUG
	if (aScreen != KAllScreens)
		RDebug::Printf("You should use KAllScreens as the argument for the screen: (other values OK for test)");
#endif

	if(aScreen < 0 || aBuffer < 0 || aSurfaceId.IsNull())
		{
		IssueRequestComplete(KErrArgument);
		return KErrArgument;
		}
	if(iStatusAvailable)
		{
		SendReceive(EScreenPlayUpdateServNotifyWhenAvailable, *iStatusAvailable);
		iStatusAvailable= NULL;
		}
	
	if(iStatusDisplayed)
		{
		TIpcArgs args(iTimeStamp);
		SendReceive(EScreenPlayUpdateServNotifyWhenDisplayed, args, *iStatusDisplayed);
		iStatusDisplayed=NULL;
		}
		
	if(iStatusDisplayedXTimes)
		{
		if(iCount <= 0)
			{
			User::RequestComplete(iStatusDisplayedXTimes, KErrArgument);
			}
		else
			{
			TIpcArgs args(iCount);
			SendReceive(EScreenPlayUpdateServNotifyWhenDisplayedXTimes, 
                     args, *iStatusDisplayedXTimes);
			}	
		iStatusDisplayedXTimes=NULL;
		}

	TPckgC<TSurfaceId> pckgSurfaceId(aSurfaceId);
	TIpcArgs theArgs(aScreen, &pckgSurfaceId, aBuffer);
	// Keep the descriptor alive until the synchronous SubmitUpdate IPC returns.
	TPtrC8 ptrRect(NULL, 0);

	if(aDirtyRegion)
		{
		const TInt regionCount = aDirtyRegion->Count();
		if(regionCount > 0)
			{
			ptrRect.Set(reinterpret_cast<const TUint8*> (aDirtyRegion->RectangleList()), regionCount * sizeof(TRect));
			theArgs.Set(3, &ptrRect);
			}
		}

	//final request for composition 
	return SendReceive(EScreenPlayUpdateServSubmitUpdate, theArgs);
	}

void RScreenPlayUpdateSession::NotifyWhenAvailable(TRequestStatus& aStatus)
	{
	ScreenPlayUpdateAssert(Handle() != KNullHandle);
	iStatusAvailable = &aStatus;
	}
	
void RScreenPlayUpdateSession::NotifyWhenDisplayed(TRequestStatus& aStatus, TScreenPlayUpdateTimeStamp& aTimeStamp)
	{
	ScreenPlayUpdateAssert(Handle() != KNullHandle);
	iStatusDisplayed = &aStatus;
	iTimeStamp = &aTimeStamp;
	}

void RScreenPlayUpdateSession::NotifyWhenDisplayedXTimes(TInt aCount, TRequestStatus &aStatus)
	{
	ScreenPlayUpdateAssert(Handle() != KNullHandle);
	iStatusDisplayedXTimes = &aStatus;
	//we will check aCount validity at submission time (in SubmitUpdate)
	iCount = aCount;
	}

void RScreenPlayUpdateSession::CancelAllUpdateNotifications()
	{
	ScreenPlayUpdateAssert(Handle() != KNullHandle);
	SendReceive(EScreenPlayUpdateServCancelAllNotifications);
	//Signal calling client's TRequestStatus object if server didn't do that (SubmitUpdate is not called)
	//if SubmitUpdate is called, server will take care of signaling status object and following line does nothing
	IssueRequestComplete(KErrCancel);
	}
	
TVersion RScreenPlayUpdateSession::Version() const
	{
	return (TVersion(KScreenPlayUpdateServMajorVersionNumber, KScreenPlayUpdateServMinorVersionNumber, KScreenPlayUpdateServBuildVersionNumber));
	}

/*
 Helper function to complete all outstanding requests. 
 
 param - aErr Error code for completion.
 */
void RScreenPlayUpdateSession::IssueRequestComplete(TInt aErr)
	{
	if(iStatusAvailable)
		{
		User::RequestComplete(iStatusAvailable, aErr);
		iStatusAvailable= NULL;
		}
	
	if(iStatusDisplayed)
		{
		User::RequestComplete(iStatusDisplayed, aErr);
		iStatusDisplayed=NULL;
		}
		
	if(iStatusDisplayedXTimes)
		{
		User::RequestComplete(iStatusDisplayedXTimes, aErr);
		iStatusDisplayedXTimes=NULL;
		}
	}
