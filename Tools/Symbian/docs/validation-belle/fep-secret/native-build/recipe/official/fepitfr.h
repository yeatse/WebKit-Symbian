/*
* Copyright (c) 1997-2009 Nokia Corporation and/or its subsidiary(-ies).
* All rights reserved.
* This component and the accompanying materials are made available
* under the terms of "Eclipse Public License v1.0"
* which accompanies this distribution, and is available
* at the URL "http://www.eclipse.org/legal/epl-v10.html".
*
* Initial Contributors:
* Nokia Corporation - initial contribution.
*
* Contributors:
*
* Description: 
* the class for inline-text format retrieval - in its own header file to prevent low-level 
* components dependent on this class from gaining unnecessary (and undesirable) dependencies 
* on high-level components
*
*/


#ifndef __FEPITFR_H__
#define __FEPITFR_H__

#include <e32std.h>

class TCharFormat;


/** Specifies the mixin protocol for finding out the formatting to apply to inline 
text. 

This class should be overridden by front end processors which support inline editing.

An instance of a class which implements this protocol should be passed to 
MCoeFepAwareTextEditor::StartFepInlineEditL(). 

@publishedAll 
@released */
class MFepInlineTextFormatRetriever // to be overridden by inline-editing front-end processors
	{
public:
	/** Gets the character formatting to apply to the inline text. 
	
	This function may be used to differentiate the formatting of the inline text 
	from the surrounding text. The first parameter should be set to contain the 
	character formatting values to use.
	
	MCoeFepAwareTextEditor provides a member function for finding out the character 
	formatting of the surrounding text - GetFormatForFep(). This can be called 
	inside the implementation of this function to make any necessary adjustments 
	to the format of the inline text to ensure that it is different from the surrounding 
	text.
	
	The second and third parameters enable different parts of the inline text 
	to have different formatting. 
	
	All implementations of this function need to set the aNumberOfCharactersWithSameFormat 
	parameter. If all of the inline text uses the same formatting then aNumberOfCharactersWithSameFormat 
	should be set to the length of the inline text and the aPositionOfCharacter 
	parameter can be ignored.
	
	When the inline text does not all use the same formatting, the second parameter 
	should be set to the number of characters starting at aPositionOfCharacter 
	which have the same formatting: see the following example.
	
	Example
	
	If the inline text is 7 characters long, the first 4 characters are in format 
	A, the next 2 characters are in format B, and the remaining character is in 
	format C.
	
	When the FEP framework (which knows that the inline text is 7 characters long) 
	needs to find out the format of the inline text, it calls GetFormatOfFepInlineText() 
	passing zero into aPositionOfCharacter (to signify the start of the inline 
	text). 
	
	As the first 4 characters are all in format A, the function should set aFormat 
	to A and it should set aNumberOfCharactersWithSameFormat to 4. As the FEP 
	framework has only been told the format of the first 4 characters, it calls 
	GetFormatOfFepInlineText() again passing 4 into aPositionOfCharacter. As the 
	2 characters starting at position 4 are all in format B, the function should 
	set aFormat to B and it should set aNumberOfCharactersWithSameFormat to 2. 
	The FEP framework still does not know the format of the 7th character so it 
	calls GetFormatOfFepInlineText() a third time passing 6 into aPositionOfCharacter. 
	As the character at position 6 is in format C, the function should set aFormat 
	to C and it should set aNumberOfCharactersWithSameFormat to 1. The FEP framework 
	now knows the format of all of the inline text.
	
	@param aFormat On return, contains the formatting to apply to the inline text 
	(or to a portion of it).
	@param aNumberOfCharactersWithSameFormat On return, contains the number of 
	characters in the inline text (starting at aPositionOfCharacter) which have 
	the same formatting.
	@param aPositionOfCharacter Start position within the inline text. */
	virtual void GetFormatOfFepInlineText(TCharFormat& aFormat, TInt& aNumberOfCharactersWithSameFormat, TInt aPositionOfCharacter) const=0;
private: // reserved. do not override!
	IMPORT_C virtual void MFepInlineTextFormatRetriever_Reserved_1();
	IMPORT_C virtual void MFepInlineTextFormatRetriever_Reserved_2();
	};

#endif	// __FEPITFR_H__

