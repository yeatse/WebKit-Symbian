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
* Note: This file may contain code to generate corrupt files for test purposes.
* Such code is excluded from production builds by use of compiler defines;
* it is recommended that such code should be removed if this code is ever published publicly.
* As specified in SGL.GT0188.251
*
*/


/**
 @file 
 @publishedPartner
 @released
*/

#ifndef __SISFILEDESCRIPTION_H__
#define __SISFILEDESCRIPTION_H__

#include "sisstring.h"
#include "sishash.h"
#include "siscapabilities.h"
#include "numeric.h"

class CSISData;
class CSISDataUnit;

#define SYSTEMDRIVE		'$'

typedef struct {
	wchar_t*		iName;
} SKeyword1;

class CSISFileDescription : public CStructure <CSISFieldRoot::ESISFileDescription>
	{
public:
	/**
	 * Enumeration to specify various file operation.
	 */
	typedef enum 
		{
		EOpNone = 0,
		EOpInstall = 1,
		EOpRun = 2,
		EOpText = 4,
		EOpNull = 8,
		EOpIllegal = 16
		} TSISFileOperation;

	/**
	 * Enumeration to specify various installation options
	 */ 
	typedef enum 
		{
		EInstFileNone =						0,

		//EOpRun Options
		EInstFileRunOptionInstall =			1 << 1,		// Run at installation
		EInstFileRunOptionUninstall =		1 << 2,		// Run at uninstallation
		EInstFileRunOptionByMimeType =		1 << 3,		// Run using MIME type
		EInstFileRunOptionWaitEnd =			1 << 4,		// Wait for end before continuing
		EInstFileRunOptionSendEnd =			1 << 5,		// Terminate after (un)install ends
		EInstFileRunOptionBeforeShutdown=	1 << 6,		// Run at uninstallation , before any running applications are shut down.
		EInstFileRunOptionAfterInstall=		1 << 7,		// Run after install	

		//EOpText Options
		EInstFileTextOptionContinue	= 		1 << 9,		// Continue button
		EInstFileTextOptionSkipIfNo	=		1 << 10,	// Yes/No - skip next file if user selects no	
		EInstFileTextOptionAbortIfNo =		1 << 11,	// Yes/No - abort       install if user selects no
		EInstFileTextOptionExitIfNo	=		1 << 12,	// Yes/No - uninstall if user selects no
		EInstFileTextOptionForceAbort =		1 << 13,	// Continue button .Installation aborts. 

		EInstVerifyOnRestore =				1 << 15		// Verify on Restore

		} TSISInstOption;

public:
	/** 
	 * Default constructor
	 */
	CSISFileDescription ();
	/**
	 * Copy constructor.
	 */
	CSISFileDescription (const CSISFileDescription& aInitialiser);
	
public:
	/**
	 * Fix minor issues with the structure.
	 */
	virtual void MakeNeat ();
	/**
	 * This function verifies the structure
	 * @param aLanguage - language
	 */ 
	virtual void Verify (const TUint32 aLanguages) const;
	/**
	 * @return name of the class.
	 */
	virtual std::string Name () const;
#ifdef GENERATE_ERRORS
	virtual void CreateDefects ();
#endif // GENERATE_ERRORS

	/**
	 * Set MIME type of the last file of the base controller's install block.
	 * @param aMimeType MIME type
	 */
	void SetMimeType (const std::wstring& aMimeType);
	/**
	 * Set the target for this file
	 * @param aTarget new taret location for this file.
	 */
	void SetTarget (const std::wstring& aTarget);
	/**
	 * Sets the new file operation.
	 * @param new file operation.
	 */
	void SetOperation (const TUint32 aOperation);
	/**
	 * Sets the new file options.
	 * @param new file options.
	 */
	void SetOptions (const TUint32 aOperationOptions);
	/**
	 * Sets the file index of this file. Its an index into the data unit.
	 * @param aFileIndex new file index to be set.
	 */
	void SetFileIndex (const TUint32 aFileIndex);
	/**
	 * Set compressed and uncompressed lengths of this file.
	 * @param aUncompressed uncompressed size
	 * @param aCompressed compressed size
	 */
	void SetLengths (const TUint64 aUncompressed, const TUint64 aCompressed);
	/**
	 * Checks if the file has a MIME type or not.
	 * @return true for success else false
	 */
	bool HasMimeType () const;
	/**
	 * Sets the hash of the file.
	 * @param aHash new hash value
	 */
	void SetHash (const CSISHash& aHash);

	/**
	 * @return File index of the file.
	 */
	TUint32 FileIndex() const;
	/**
	 * @return file operation (TSISFileOperation).
	 */
	CSISFileDescription::TSISFileOperation Operation() const;
	/**
	 * @return file options (TSISInstOption).
	 */
	CSISFileDescription::TSISInstOption OperationOptions() const;
	/**
	 * @return file length
	 */
	CSISUInt64&	Length() const;
	/**
	 * @return uncompressed file length
	 */
	CSISUInt64&	UncompressedLength() const;
	/**
	 * @return Hash of the file.
	 */
	CSISHash& Hash() const;
	/**
	 * @return Target location of the file.
	 */
	inline const CSISString& Target() const;
	/**
	 * @return File MIME type.
	 */
	inline const CSISString& MimeType() const;
	/**
	 * Converts the string into installation option (only if its valid) and sets the
	 * option to this file.
	 * @param aOption installation option string.
	 * @return returns the installation option
	 */
	TSISInstOption InterpretOption (const std::wstring& aOption);
	/**
	 * Sets the hash of the file.
	 * @param aHash hash value
	 * @param aHashSize size of hash
	 */
	void SetHash(const TUint8* aHash, TUint32 aHashSize);
	/**
	 * Extracts capabilities of this file.
	 * @param file name whose capabilities need to be retieved.
	 */
	void ExtractCapabilities(const std::wstring& aFileName);
	/**
	 * Retrieves the file name
	 */
	const wchar_t* GetFileName() const;
	/**
	 * Adds package entry related to the file description.
	 * @param aStream stream into which the package details need to be written.
	 * @param aVerbose If this option is set then detail description of pkg
	 * 			will be written into the stream.
	 */
	void AddPackageEntry(std::wostream& aStream, bool aVerbose) const;
	/**
	 * Retrieves the capability list associated with this sis file.
	 * @param aCapList list to be filled by this function.
	 */
	inline void GetCapabilityList(std::vector<std::wstring>& aCapList) const;
	/**
	 * Retrieves the capability list associated with this sis file.
	 * @return capability list in number format.
	 */
	inline TUint32 Capabilities() const;
	
private:
	void InsertMembers ();

private:
	CSISString			iTarget;
	CSISString			iMimeType;
	CSISCapabilities	iCapabilities;
	CSISHash			iHash;
	CSISUInt32			iOperation;
	CSISUInt32			iOperationOptions;
	CSISUInt64			iLength;
	CSISUInt64			iUncompressedLength;
	CSISUInt32			iFileIndex;
	};


inline CSISFileDescription::TSISInstOption operator | (const CSISFileDescription::TSISInstOption aLHS, const CSISFileDescription::TSISInstOption aRHS)
	{
		return static_cast <CSISFileDescription::TSISInstOption> (static_cast <TUint32> (aLHS) | static_cast <TUint32> (aRHS));
	}


inline CSISFileDescription::CSISFileDescription ()
	{ 
	InsertMembers (); 
	}


inline void CSISFileDescription::SetMimeType (const std::wstring& aMimeType)
	{ 
	iMimeType = aMimeType; 
	}


inline void CSISFileDescription::SetTarget (const std::wstring& aTarget)
	{ 
	iTarget = aTarget; 
	}


inline void CSISFileDescription::SetOperation (const TUint32 aOperation)
	{ 
	iOperation |= aOperation; 
	}


inline void CSISFileDescription::SetOptions (const TUint32 aOperationOptions)
	{ 
	iOperationOptions |= aOperationOptions; 
	}


inline void CSISFileDescription::SetFileIndex (const TUint32 aFileIndex)
	{ 
	iFileIndex = aFileIndex; 
	}


inline void CSISFileDescription::SetLengths (const TUint64 aUncompressed, const TUint64 aCompressed)
	{
	iLength = aCompressed;
	iUncompressedLength = aUncompressed;
	}

inline bool CSISFileDescription::HasMimeType () const
	{
	return ! iMimeType.empty ();
	}

inline TUint32 CSISFileDescription::FileIndex() const
	{
	return iFileIndex.Value();
	}

inline CSISFileDescription::TSISFileOperation CSISFileDescription::Operation() const
	{
	return (CSISFileDescription::TSISFileOperation)(int)iOperation;
	}

inline CSISFileDescription::TSISInstOption CSISFileDescription::OperationOptions() const
	{
	return (CSISFileDescription::TSISInstOption)(int)iOperationOptions;
	}

inline CSISUInt64& CSISFileDescription::Length() const
	{
	return (CSISUInt64&)iLength;
	}

inline CSISUInt64& CSISFileDescription::UncompressedLength() const
	{
	return (CSISUInt64&)iUncompressedLength;
	}

inline CSISHash& CSISFileDescription::Hash() const
	{
	return (CSISHash&)iHash;
	}

inline const CSISString& CSISFileDescription::Target() const
	{
	return iTarget;
	}

inline const CSISString& CSISFileDescription::MimeType() const
	{
	return iMimeType;
	}

inline void CSISFileDescription::GetCapabilityList(std::vector<std::wstring>& aCapList) const
	{
	iCapabilities.GetCapabilityList(aCapList);
	}

inline TUint32 CSISFileDescription::Capabilities() const
	{
	return iCapabilities.Capabilities();
	}

#endif // __SISFILEDESCRIPTION_H__

