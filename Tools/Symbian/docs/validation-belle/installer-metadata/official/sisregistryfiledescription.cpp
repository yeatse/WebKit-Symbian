/*
* Copyright (c) 2004-2009 Nokia Corporation and/or its subsidiary(-ies).
* All rights reserved.
* This component and the accompanying materials are made available
* under the terms of the License "Eclipse Public License v1.0"
* which accompanies this distribution, and is available
* at the URL "http://www.eclipse.org/legal/epl-v10.html".
*
* Initial Contributors:
* Nokia Corporation - initial contribution.
*
* Contributors:
*
* Description: 
* CSisRegistryFileDescription - file description implementation
*
*/


/**
 @file 
 @released
 @internalComponent
*/

#include<s32strm.h>
#include "sisfiledescription.h"
#include "sishash.h"
#include "sisstring.h"
#include "hashcontainer.h"
#include "sisregistryfiledescription.h"
#include "siscapabilities.h"
#include "securitypolicy.h"

#ifdef SYMBIAN_UNIVERSAL_INSTALL_FRAMEWORK
#include "scrhelperutil.h"
#endif

using namespace Swi;


EXPORT_C CSisRegistryFileDescription* CSisRegistryFileDescription::NewL(const Sis::CFileDescription& aFileDescription, TChar aDrive, TBool aIsStub)
	{
	CSisRegistryFileDescription* self = NewLC(aFileDescription, aDrive, aIsStub);
	CleanupStack::Pop(self);
	return self;
	}

EXPORT_C CSisRegistryFileDescription* CSisRegistryFileDescription::NewLC(const Sis::CFileDescription& aFileDescription, TChar aDrive, TBool aIsStub)
	{
	CSisRegistryFileDescription* self = new(ELeave) CSisRegistryFileDescription();
	CleanupStack::PushL(self);
	self->ConstructL(aFileDescription, aDrive, aIsStub);
	return self;
	}

EXPORT_C CSisRegistryFileDescription* CSisRegistryFileDescription::NewL(const CSisRegistryFileDescription& aFileDescription)
	{
	CSisRegistryFileDescription* self = NewLC(aFileDescription);
	CleanupStack::Pop(self);
	return self;
	}

EXPORT_C CSisRegistryFileDescription* CSisRegistryFileDescription::NewLC(const CSisRegistryFileDescription& aFileDescription)
	{
	CSisRegistryFileDescription* self = new(ELeave) CSisRegistryFileDescription();
	CleanupStack::PushL(self);
	self->ConstructL(aFileDescription);
	return self;
	}

EXPORT_C CSisRegistryFileDescription* CSisRegistryFileDescription::NewL(RReadStream& aStream)
	{
	CSisRegistryFileDescription* self = NewLC(aStream);
	CleanupStack::Pop(self);
	return self;
	}

EXPORT_C CSisRegistryFileDescription* CSisRegistryFileDescription::NewLC(RReadStream& aStream)
	{
	CSisRegistryFileDescription* self = new(ELeave) CSisRegistryFileDescription();
	CleanupStack::PushL(self);
	self->ConstructL(aStream);
	return self;
	}
	
#ifdef SYMBIAN_UNIVERSAL_INSTALL_FRAMEWORK
EXPORT_C CSisRegistryFileDescription* CSisRegistryFileDescription::NewL()
	{
	CSisRegistryFileDescription* self = NewLC();
	CleanupStack::Pop(self);
	return self;
	}

EXPORT_C CSisRegistryFileDescription* CSisRegistryFileDescription::NewLC()
	{
	CSisRegistryFileDescription* self = new(ELeave) CSisRegistryFileDescription();
	CleanupStack::PushL(self);	
	return self;
	}
#endif

CSisRegistryFileDescription::CSisRegistryFileDescription(): iSid(KNullUid)
	{
	}

EXPORT_C CSisRegistryFileDescription::~CSisRegistryFileDescription()
	{
	delete iTarget;
	delete iMimeType;
	delete iHash;
	delete iCapabilitiesData;
	}

void CSisRegistryFileDescription::ConstructL(const Sis::CFileDescription& aFileDescription, TChar aDrive, TBool aIsStub)
	{	
	CSecurityPolicy* securityPolicy=CSecurityPolicy::GetSecurityPolicyL();
	const TDesC& mmpPath = aFileDescription.Target().Data();
	iTarget = securityPolicy->ResolveTargetFileNameL(mmpPath, aDrive);
	
	if (aIsStub)
		{
		// rewrite the target so it uses the selected drive.
		TPtr targetPtr(iTarget->Des());
		if ( targetPtr.Length() )
			targetPtr[0] = aDrive;

  		}
  	
  	if (aDrive == 'z')
  		{			
		// Makesis doesn't process the actual files when creating ROM stubs
		// so much of this data can be ignored.
		iMimeType = KNullDesC().AllocL();
		// iHash and iOperation are initialised to default values, so that: (a) the object would be valid, (b) when adding the file description to the SCR, we'll skip adding these fields (see scrhelperutil.cpp)
		iHash = CHashContainer::NewL(CMessageDigest::ESHA1, KNullDesC8);
		iOperation = Swi::Sis::EOpInstall;
		}
	else
		{
		iHash = CHashContainer::NewL(aFileDescription.Hash());
		iMimeType = aFileDescription.MimeType().Data().AllocL();
		iOperation = aFileDescription.Operation();
		iOperationOptions = aFileDescription.OperationOptions();
		iUncompressedLength = aFileDescription.UncompressedLength();
		iIndex = aFileDescription.Index();
		if (aFileDescription.Capabilities())
			{
			iCapabilitiesData = aFileDescription.Capabilities()->Data().AllocL();
			}
		}
	}

void CSisRegistryFileDescription::ConstructL(const CSisRegistryFileDescription& aFileDescription)
	{
	iHash = CHashContainer::NewL(aFileDescription.Hash().Algorithm(), aFileDescription.Hash().Data());
	iTarget = aFileDescription.Target().AllocL();
	iMimeType = aFileDescription.MimeType().AllocL();
	iOperation = aFileDescription.Operation();
	iOperationOptions = aFileDescription.OperationOptions();
	iUncompressedLength = aFileDescription.UncompressedLength();
	iIndex = aFileDescription.Index();
	iSid = aFileDescription.Sid();
	if (aFileDescription.iCapabilitiesData)
		{
		iCapabilitiesData = aFileDescription.iCapabilitiesData->AllocL();
		}
	}

void CSisRegistryFileDescription::ConstructL(RReadStream& aStream)
	{
	InternalizeL(aStream);
	}

EXPORT_C void CSisRegistryFileDescription::InternalizeL(RReadStream& aStream)
	{
	iTarget = HBufC::NewL(aStream, KMaxFileName);
	iMimeType = HBufC::NewL(aStream, KMaxTInt);
	iOperation = static_cast<Sis::TSISFileOperation>(aStream.ReadInt32L());
	iOperationOptions = static_cast<Sis::TSISFileOperationOptions>(aStream.ReadInt32L());
	iHash = CHashContainer::NewL(aStream);
	TInt32 low = aStream.ReadInt32L();
	TInt32 high = aStream.ReadInt32L();
	iUncompressedLength = MAKE_TINT64(high, low);
	iIndex = aStream.ReadUint32L();
	iSid.iUid = aStream.ReadInt32L();
	}

EXPORT_C void CSisRegistryFileDescription::ExternalizeL(RWriteStream& aStream) const
	{
	// If the internalization protocol changes, the ::SerializeSize() function would need to be updated
	aStream << *iTarget;
	aStream << *iMimeType;
	aStream.WriteInt32L(static_cast<TInt32>(iOperation));
	aStream.WriteInt32L(static_cast<TInt32>(iOperationOptions));
	iHash->ExternalizeL(aStream);
	aStream.WriteInt32L(I64LOW(iUncompressedLength));
	aStream.WriteInt32L(I64HIGH(iUncompressedLength));
	aStream.WriteInt32L(iIndex);
	aStream.WriteInt32L(iSid.iUid);
	}

EXPORT_C TUint32 CSisRegistryFileDescription::SerializedSize() const	
	{	
	return iTarget->Size() + iMimeType->Size() + sizeof(iOperation) + sizeof(iOperationOptions) + sizeof(iUncompressedLength) + sizeof(iIndex) +
		   sizeof(iSid.iUid) + iHash->SerializedSize();
	}

EXPORT_C CSisRegistryFileDescription* CSisRegistryFileDescription::NewL(CHashContainer& aHash,
														const TDesC& aTarget,
														const TDesC& aMimeType,
														const Sis::TSISFileOperation aOperation,
														const Sis::TSISFileOperationOptions aOperationOptions,
														const TInt64 aUncompressedLength,
														const TUint32 aIndex,
														const TUid aSid )
	{
	CSisRegistryFileDescription* self = new(ELeave) CSisRegistryFileDescription();
	CleanupStack::PushL(self);
	self->ConstructL(aHash, aTarget, aMimeType, aOperation, aOperationOptions, aUncompressedLength, aIndex, aSid);
	CleanupStack::Pop(self);
	return self;
	}

void CSisRegistryFileDescription::ConstructL(CHashContainer& aHash,
												const TDesC& aTarget,
												const TDesC& aMimeType,
												const Sis::TSISFileOperation aOperation,
												const Sis::TSISFileOperationOptions aOperationOptions,
												const TInt64 aUncompressedLength,
												const TUint32 aIndex,
												const TUid aSid)
	{
	iHash = CHashContainer::NewL(aHash.Algorithm(), aHash.Data());
	iTarget = aTarget.AllocL();
	iMimeType = aMimeType.AllocL();
	iOperation = aOperation;
	iOperationOptions = aOperationOptions;
	iUncompressedLength = aUncompressedLength;
	iIndex = aIndex;
	iSid = aSid;
	}
