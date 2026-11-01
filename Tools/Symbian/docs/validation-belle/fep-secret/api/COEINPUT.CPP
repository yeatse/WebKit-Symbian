// Copyright (c) 1997-2009 Nokia Corporation and/or its subsidiary(-ies).
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

#include <coeinput.h>


//
// TCoeInputCapabilities
//

EXPORT_C TCoeInputCapabilities::TCoeInputCapabilities()
	{
	}

/** Constructor which initialises the capabilities. 

The new object has NULL pointers to the FEP aware text editor and the FEP 
caption retriever objects.

@param aCapabilities A bitmask of the text input capability flags giving the 
supported capabilities. The possible flags are given in the anonymous 
enumeration in this class, beginning with ENone. */
EXPORT_C TCoeInputCapabilities::TCoeInputCapabilities(TUint aCapabilities)
	:iCapabilities(aCapabilities),
	 iFepAwareTextEditor(NULL),
	 iCaptionRetrieverForFep(NULL),
	 iFepUid(0),
	 iFepSpecificExtensions(NULL),
     iObjectProvider(NULL)
	{
	}

/** Constructor which sets the capabilities, a FEP aware text editor and a 
FEP caption retriever object. 

@param aCapabilities A bitmask of the text input capability flags giving the 
supported capabilities. The possible flags are given in the anonymous 
enumeration in this class, beginning with ENone.
@param aFepAwareTextEditor A FEP aware text editor. This enables FEPs to 
do things like inline editing, retrieving portions of text, enquiring 
the position of the insertion-point (cursor), etc.
@param aCaptionRetrieverForFep A caption retriever for a FEP. */
EXPORT_C TCoeInputCapabilities::TCoeInputCapabilities(TUint aCapabilities, MCoeFepAwareTextEditor* aFepAwareTextEditor, MCoeCaptionRetrieverForFep* aCaptionRetrieverForFep)
	:iCapabilities(aCapabilities),
	 iFepAwareTextEditor(aFepAwareTextEditor),
	 iCaptionRetrieverForFep(aCaptionRetrieverForFep),
	 iFepUid(0),
	 iFepSpecificExtensions(NULL),
     iObjectProvider(NULL)
	{
	}

/** Constructor which sets the capabilities, a FEP aware text editor, a 
FEP caption retriever object, a FEP UID and a FEP specific extensions object.

This overload is provided to allow an application to report its specialised 
input capabilities, if any, to a FEP. The application should override 
CCoeControl::InputCapabilities() and/or CCoeAppUi::InputCapabilities(), 
to return a TCoeInputCapabilities object created using this overload, passing 
the FEP's UID (as published in its header file) and the address of a 
MCoeFepSpecificExtensions object.

@param aCapabilities A bitmask of the text input capability flags giving the 
supported capabilities. The possible flags are given in the anonymous 
enumeration in this class, beginning with ENone.
@param aFepAwareTextEditor A FEP aware text editor. This enables FEPs to do 
things like inline editing, retrieving portions of text, enquiring 
the position of the insertion-point (cursor), etc.
@param aCaptionRetrieverForFep A caption retriever for a FEP.
@param aFepUid The UID of the FEP for which the extended capabilities aFepSpecificExtensions 
apply.
@param aFepSpecificExtensions FEP specific extensions supported by the control 
or app UI returning this object.
@see FepSpecificExtensions() */
EXPORT_C TCoeInputCapabilities::TCoeInputCapabilities(TUint aCapabilities, MCoeFepAwareTextEditor* aFepAwareTextEditor, MCoeCaptionRetrieverForFep* aCaptionRetrieverForFep, TUid aFepUid, MCoeFepSpecificExtensions* aFepSpecificExtensions)
	:iCapabilities(aCapabilities),
	 iFepAwareTextEditor(aFepAwareTextEditor),
	 iCaptionRetrieverForFep(aCaptionRetrieverForFep),
	 iFepUid(aFepUid.iUid),
	 iFepSpecificExtensions((aFepUid.iUid==0)? NULL: aFepSpecificExtensions),
     iObjectProvider(NULL)
	{
	}

/** Copy constructor. Constructs this object using the capabilities of another 
instance.

@param aAnother The input capabilities object to be copied. */
EXPORT_C TCoeInputCapabilities::TCoeInputCapabilities(const TCoeInputCapabilities& aAnother)
	:iCapabilities(aAnother.iCapabilities),
	 iFepAwareTextEditor(aAnother.iFepAwareTextEditor),
	 iCaptionRetrieverForFep(aAnother.iCaptionRetrieverForFep),
	 iFepUid(aAnother.iFepUid),
	 iFepSpecificExtensions(aAnother.iFepSpecificExtensions),
     iObjectProvider(aAnother.iObjectProvider)
	{
	}

/** Assignment operator. 

This copies the capabilities of aAnother into this object.

@param aAnother The object to be copied
@return This object. */
EXPORT_C TCoeInputCapabilities& TCoeInputCapabilities::operator=(const TCoeInputCapabilities& aAnother)
	{
	iCapabilities=aAnother.iCapabilities;
	iFepAwareTextEditor=aAnother.iFepAwareTextEditor;
	iCaptionRetrieverForFep=aAnother.iCaptionRetrieverForFep;
	iFepUid=aAnother.iFepUid;
	iFepSpecificExtensions=aAnother.iFepSpecificExtensions;
    iObjectProvider=aAnother.iObjectProvider;
	return *this;
	}

/** Equality operator. 

Compares this and aAnother for equality.

@param aAnother The object to be compared to this object.
@return ETrue if the objects are equal, otherwise EFalse. */
EXPORT_C TBool TCoeInputCapabilities::operator==(const TCoeInputCapabilities& aAnother) const
	{
	if ((iCapabilities==aAnother.iCapabilities) &&
		(iFepAwareTextEditor==aAnother.iFepAwareTextEditor) &&
		(iCaptionRetrieverForFep==aAnother.iCaptionRetrieverForFep) &&
		(iFepUid==aAnother.iFepUid) &&
		(iFepSpecificExtensions==aAnother.iFepSpecificExtensions) &&
        (iObjectProvider==aAnother.iObjectProvider))
		{
		return ETrue;
		}
	return EFalse;
	}

/** Inequality operator. 

Compares this and aAnother for inequality.

@param aAnother The object to be compared to this object.
@return ETrue if aAnother is not equal to this object, otherwise EFalse. */
EXPORT_C TBool TCoeInputCapabilities::operator!=(const TCoeInputCapabilities& aAnother) const
	{
	if ((iCapabilities!=aAnother.iCapabilities) ||
		(iFepAwareTextEditor!=aAnother.iFepAwareTextEditor) ||
		(iCaptionRetrieverForFep!=aAnother.iCaptionRetrieverForFep) ||
		(iFepUid!=aAnother.iFepUid) ||
		(iFepSpecificExtensions!=aAnother.iFepSpecificExtensions) ||
        (iObjectProvider!=aAnother.iObjectProvider))
		{
		return ETrue;
		}
	return EFalse;
	}

/** Merges the capabilities of a specified TCoeInputCapabilities with this object. 

The capabilities are merged using a logical OR. The pointers to the FEP aware 
text editor, caption retriever and object provider are merged only if this 
object has NULL pointers.

@param aAnother The capabilities to be merged. */
EXPORT_C void TCoeInputCapabilities::MergeWith(const TCoeInputCapabilities& aAnother)
	{
	iCapabilities|=aAnother.iCapabilities;
	if (iFepAwareTextEditor==NULL)
		{
		iFepAwareTextEditor=aAnother.iFepAwareTextEditor;
		}
	if (iCaptionRetrieverForFep==NULL)
		{
		iCaptionRetrieverForFep=aAnother.iCaptionRetrieverForFep;
		}
	if ((iFepUid==0) && (iFepSpecificExtensions==NULL))
		{
		iFepUid=aAnother.iFepUid;
		iFepSpecificExtensions=aAnother.iFepSpecificExtensions;
		}
    if (!iObjectProvider)
        {
        iObjectProvider=aAnother.iObjectProvider;
        }
	}

/** Sets the input capability flags of this object.

@param aCapabilities The input capabilities. This is a bitwise-"or" of one 
or more values from the anonymous enum in this class.
@see Capabilities() */
EXPORT_C void TCoeInputCapabilities::SetCapabilities(TUint aCapabilities)
	{
	iCapabilities=aCapabilities;
	}

/** Gets the input capability flags of this object.

@return The input capabilities. This is a bitwise OR of one or more values 
from the anonymous enum in this class.
@see SetCapabilities() */
EXPORT_C TUint TCoeInputCapabilities::Capabilities() const
	{
	return iCapabilities;
	}

/** Tests whether the control supports any type of text input.

@return ETrue if no text input is supported, otherwise EFalse. */
EXPORT_C TBool TCoeInputCapabilities::IsNone() const
	{
	return iCapabilities==ENone;
	}

/** Tests whether the control supports entry of positive integers.

@return ETrue if positive integers are supported, otherwise EFalse. */
EXPORT_C TBool TCoeInputCapabilities::SupportsWesternNumericIntegerPositive() const
	{
	return iCapabilities&EWesternNumericIntegerPositive;
	}

/** Tests whether the control supports entry of negative integers.

@return ETrue if negative integers are supported, otherwise EFalse. */
EXPORT_C TBool TCoeInputCapabilities::SupportsWesternNumericIntegerNegative() const
	{
	return iCapabilities&EWesternNumericIntegerNegative;
	}

/** Tests whether the control supports entry of real numbers.

@return ETrue if real numbers are supported, otherwise EFalse. */
EXPORT_C TBool TCoeInputCapabilities::SupportsWesternNumericReal() const
	{
	return iCapabilities&EWesternNumericReal;
	}

/** Tests whether the control supports entry of text in the western alphabets.

@return ETrue if the western alphabet is supported, otherwise EFalse. */
EXPORT_C TBool TCoeInputCapabilities::SupportsWesternAlphabetic() const
	{
	return iCapabilities&EWesternAlphabetic;
	}

/** Tests whether the control supports text input in Japanese Hiragana.

@return ETrue if Japanese Hiragana is supported, otherwise EFalse. */
EXPORT_C TBool TCoeInputCapabilities::SupportsJapaneseHiragana() const
	{
	return iCapabilities&EJapaneseHiragana;
	}

/** Tests whether the control supports text input in half width Japanese Katakana.

@return ETrue if half width Japanese Katakana is supported, otherwise EFalse. */
EXPORT_C TBool TCoeInputCapabilities::SupportsJapaneseKatakanaHalfWidth() const
	{
	return iCapabilities&EJapaneseKatakanaHalfWidth;
	}

/** Tests whether the control supports text input in full width Japanese Katakana.

@return ETrue if full width Japanese Katakana is supported, otherwise EFalse. */
EXPORT_C TBool TCoeInputCapabilities::SupportsJapaneseKatakanaFullWidth() const
	{
	return iCapabilities&EJapaneseKatakanaFullWidth;
	}

/** Tests whether the control supports dialable characters as text input.

@return ETrue if dialable characters are supported, otherwise EFalse. */
EXPORT_C TBool TCoeInputCapabilities::SupportsDialableCharacters() const
	{
	return iCapabilities&EDialableCharacters;
	}

/** Tests whether the control supports secret text.

@return ETrue if secret text is supported, otherwise EFalse. */
EXPORT_C TBool TCoeInputCapabilities::SupportsSecretText() const
	{
	return iCapabilities&ESecretText;
	}

/** Tests whether the control supports auto sentence case.

@return ETrue if auto sentence case is supported, otherwise EFalse. */
EXPORT_C TBool TCoeInputCapabilities::SupportsAutoSentenceCase() const
	{
	return iCapabilities&EAutoSentenceCase;
	}

/** Tests whether the control supports non-predictive input.

@return ETrue if non-predictive input is supported, otherwise EFalse. */
EXPORT_C TBool TCoeInputCapabilities::SupportsNonPredictive() const
	{
	return iCapabilities&ENonPredictive;
	}

/** Tests whether the control supports all types of text input.

@return ETrue if all text input is supported, otherwise EFalse. */
EXPORT_C TBool TCoeInputCapabilities::SupportsAllText() const
	{
	return iCapabilities&EAllText;
	}

/** Tests whether the control supports navigation keys.

@return ETrue if navigation keys are supported, otherwise EFalse. */
EXPORT_C TBool TCoeInputCapabilities::SupportsNavigation() const
	{
	return iCapabilities&ENavigation;
	}

/** Gets the FEP aware text editor object pointed to by this object.

@return The FEP aware text editor object pointed to by this object */
EXPORT_C MCoeFepAwareTextEditor* TCoeInputCapabilities::FepAwareTextEditor() const
	{
	return iFepAwareTextEditor;
	}

/** Gets the caption retriever pointed to by this object.

@return The caption retriever pointed to by this object. */
EXPORT_C MCoeCaptionRetrieverForFep* TCoeInputCapabilities::CaptionRetrieverForFep() const
	{
	return iCaptionRetrieverForFep;
	}

/** Gets the specialised input capabilities of the application.

This function is called by the FEP and returns the specialised capabilities 
supported by the application. If the application has no specialised input 
capabilities, or if the application does not know about this FEP, the 
function returns NULL.

Notes:

The application only knows about one FEP. This is the one that was written to implement 
its specialised capabilities. If aFepUid does not match the UID of this known 
FEP then NULL is returned.

If an MCoeFepSpecificExtensions is returned, the FEP then calls the supported 
extension functions in the application via its virtual interface. The 
functions return some value, which, depending on the current context, may 
instruct the FEP to perform some actions.

Background information:

The specialised capabilities are defined as pure virtual functions in the 
concrete FEP's MCoeFepSpecificExtensions interface, and are implemented 
by the app UI. 

When the focus in the app UI changes, the FEP queries the application's 
capabilities using CCoeControl::InputCapabilities() and/or CCoeAppUi::InputCapabilities(). 
It then gets the extensions using this function.

The FEP then calls any member functions of MCoeFepSpecificExtensions that 
it needs to (to inquire about the extended capabilities). The functions return 
a value to the FEP instructing it to perform an action, if that extended functionality 
is appropriate to the current state of the application. 

@param aFepUid The UID of the current FEP.
@return NULL if there are no FEP specific input capabilities or if the FEP 
is not "known" about by the application. */
EXPORT_C TCoeInputCapabilities::MCoeFepSpecificExtensions* TCoeInputCapabilities::FepSpecificExtensions(TUid aFepUid) const
	{
	if (aFepUid.iUid==(TInt)iFepUid)
		{
		return iFepSpecificExtensions;
		}
	return NULL;
	}

/** Gets the object provider of the control which supplied this TCoeInputCapabilities 
object.

This function can be called by a FEP to gain access to the object provider 
tree. For instance, the FEP might need to update an input mode indicator that 
is located in the same object provider tree as the editor with focus.

@return The object provider. This allows the FEP to access objects in the 
context of the control which supplied this TCoeInputCapabilities object. */
EXPORT_C MObjectProvider* TCoeInputCapabilities::ObjectProvider() const
    {
    return iObjectProvider;
    }

/** Sets the object provider of the control which supplied this TCoeInputCapabilities 
object.

This allows the control to give the FEP access to its object provider tree.

@param aObjectProvider The object provider. */
EXPORT_C void TCoeInputCapabilities::SetObjectProvider(MObjectProvider* aObjectProvider)
    {
    iObjectProvider=aObjectProvider;
    }
