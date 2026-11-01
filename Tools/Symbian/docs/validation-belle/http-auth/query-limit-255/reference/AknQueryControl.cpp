/*
* Copyright (c) 2002-2008 Nokia Corporation and/or its subsidiary(-ies).
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
* Description:  Implementation of AVKON query control.
*
*/


// INCLUDE FILES
#ifdef SYMBIAN_ENABLE_SPLIT_HEADERS
#include <uikon/eikdefmacros.h>
#endif
#include "aknQueryControl.h"    //This class
#include <barsread.h>           //For TResourceReader

#include <aknenv.h>
#include <AknBitmapAnimation.h>
#include <aknconsts.h>
#include <avkon.mbg>
#include <avkon.rsg>
#include <aknborders.h>
#include <AknEcs.h>
#include <aknedsts.h>
#include <AknLayoutDef.h>
#include <AknLayout.lag>

#include "akntextcontrol.h"
#include "aknqueryeditorindicator.h"
#include "aknEditStateIndicator.h"
#include "AknUtils.h"
#include "aknnoteattributes.h"

#include <AknsDrawUtils.h>
#include <AknsConstants.h>
#include <AknsFrameBackgroundControlContext.h>
#include <AknsPointerStore.h>

#include <in_sock.h>

#include <AknPictographInterface.h>
#include <AknPictographDrawerInterface.h>

#include "AknQueryExtension.h"
#include <skinlayout.cdl.h>

#include <AknIconUtils.h>
#include "aknappui.h"
#include <layoutmetadata.cdl.h>
#include <aknlayoutscalable_avkon.cdl.h>
#include <aknlayoutscalable_apps.cdl.h>
#include "aknkeypad.h"


#if defined( RD_SCALABLE_UI_V2)
//class CAknButton is available from week 20 version 5.0 onwards
#include "aknbutton.h"
// For MAknMfneCommandObserver
#include <aknmfnecommandobserver.h>
#endif //if defined( RD_SCALABLE_UI_V2)
#include <aknlocationed.h>

#include <eikdialg.h>
#include <eikdialogext.h>
#include <gulicon.h>
#include <StringLoader.h>

#include <AknTasHook.h> // for testability hooks
#include <tacticon.h>

const TInt KPinCodeTacticonInterval = 30000000; // 30s
const TInt KPinCodeMaxTacticons = 10;
/*******
 * CAknQueryControlExtension
 */
 
NONSHARABLE_CLASS(CAknQueryControlExtension) : public CBase
{
public:
    ~CAknQueryControlExtension();
    
	CAknLocationEditor *iLocationEd;
	
	// Saved animation ID for later updates due to skin change.
	// The value is saved in CAknQueryControl::ConstructQueryL().
	TInt iAnimationId;
	 
	HBufC* iOriginalPrompt;
	CAknButton* iBackspaceButton;
	CAknKeypad* iKeypad;
	
	// Flag indicating whether or not the query CBA has been
	// changed to ECS softkeys (Call and Cancel).
	TBool iEcsCbaShown;

    // Timer for pincode query tacticon playback
    CPeriodic* iPinTacticonTimer;
    // Counter for tacticons played in a pincode query
    TInt iPinTacticonsPlayed;
};

CAknQueryControlExtension::~CAknQueryControlExtension()
    {
    delete iOriginalPrompt;
    delete iBackspaceButton;
    delete iKeypad;

    delete iPinTacticonTimer;
    iPinTacticonTimer = NULL;
    }
    
CAknLocationEditor *CAknQueryControl::LocationEd() const
{
   if (iExtension)
	return iExtension->iLocationEd;
   else
	return NULL;
}
CAknLocationEditor *&CAknQueryControl::LocationEd()
{
	return iExtension->iLocationEd;
}


/*****************************
 * CAknQueryExtension
 *****************************/

void CAknQueryExtension::CreateEditorContextL()
    {
    delete iEditorContext;
    iEditorContext = NULL;
    iEditorContext = CAknsFrameBackgroundControlContext::NewL(
        KAknsIIDQsnFrInput, TRect(0,0,1,1), TRect(0,0,1,1), EFalse );
    }

CAknsFrameBackgroundControlContext* CAknQueryExtension::EditorContext()
    {
    return iEditorContext;
    }

CAknQueryExtension* CAknQueryExtension::Instance(
    const CAknQueryControl* aQueryControl )
    {
    return static_cast<CAknQueryExtension*>(
        AknsPointerStore::RetrievePointer( 
        static_cast<const CBase*>(aQueryControl) ) );
    }

void CAknQueryExtension::CreateExtensionL(
    const CAknQueryControl* aQueryControl )
    {
    CAknQueryExtension* extension = CAknQueryExtension::NewL();
    CleanupStack::PushL( extension );
    AknsPointerStore::StorePointerL( 
        static_cast<const CBase*>(aQueryControl), extension );
    CleanupStack::Pop(); // extension
    }

void CAknQueryExtension::DestroyExtension(
    const CAknQueryControl* aQueryControl )
    {
    delete Instance( aQueryControl );
    AknsPointerStore::RemovePointer( 
        static_cast<const CBase*>(aQueryControl) );
    }

CAknQueryExtension* CAknQueryExtension::NewL()
    {
    return new (ELeave) CAknQueryExtension();
    }

CAknQueryExtension::~CAknQueryExtension()
    {
    delete iEditorContext;
    }

/*****************************
 * TAknQueryEcsObserver
 *****************************/

/**
*
* This class is local to this cpp file.  It is used to ensure that all reference to aknecs.h is 
* done within the CAknQueryControl class implementation and not in its header; that would cause
* the client of CAknQueryControl (probably via CAknQueryDialog) to have to have Shared data and 
* phone client .h files in the INCLUDE path 
*
*/
NONSHARABLE_CLASS(TAknQueryEcsObserver): public MAknEcsObserver
    {
    public:
        TAknQueryEcsObserver( CAknQueryControl* aParent );
        // May be called from a sub-class of CAknEcsDetector in another DLL
        IMPORT_C virtual void HandleEcsEvent(CAknEcsDetector* /*aDetector*/, CAknEcsDetector::TState aUpdatedState);
    private:
        CAknQueryControl* iParent; // Not owned; set by constructor initializaton
    };

TAknQueryEcsObserver::TAknQueryEcsObserver( CAknQueryControl* aParent ): iParent( aParent )
    {
    }


EXPORT_C void TAknQueryEcsObserver::HandleEcsEvent( CAknEcsDetector* /*aDetector*/,
                                                    CAknEcsDetector::TState aUpdatedState )
    {
    TRAP_IGNORE( iParent->SetEcsCbaVisibleL( aUpdatedState == CAknEcsDetector::ECompleteMatch ) );
    
    // Hide the emergency call number again.
    // Number should be visible only in Partial or Complete match state
    if ( aUpdatedState != CAknEcsDetector::EPartialMatch &&
         aUpdatedState != CAknEcsDetector::ECompleteMatch  )
        {
        if ( iParent->QueryType() == EPinLayout )
            {
            static_cast<CAknNumericSecretEditor*>
                ( iParent->ControlByLayoutOrNull( EPinLayout ) )->RevealSecretText( EFalse );
            }
        }
    
    // This check must be last action in the routine.  
    // If the query is cancelled, the iParent is deleted!
    if ( aUpdatedState == CAknEcsDetector::ECallAttempted )
        {
        TRAP_IGNORE( iParent->CancelQueryL() );
        }
    }


const TInt KMaxLinesInQuery=5;

/*****************************
 * CAknQueryControl
 *****************************/

/************************************************
 * KQueryTypeToLayout
 * 
 * The index into this table is the query type, 
 * according to the query constants enum in 
 * avkon.hrh. The values
 * are the layout methods according to the enum
 * TLayoutMethod. So this table maps a query type
 * as read from resource file to a lyout method
 ************************************************/

const CAknQueryControl::TLayoutMethod KQueryTypeToLayout[]= {
    CAknQueryControl::EDataQueryLayoutM,   // NOT USED  
    CAknQueryControl::EDataQueryLayoutM,   // EDataLayout=1,
    CAknQueryControl::EConfQueryLayoutM,   // EConfirmationQueryLayout and EConfirmationLayout
    CAknQueryControl::ECodeQueryLayoutM,   // ECodeLayout,
    CAknQueryControl::ETimeQueryLayoutM,   // ETimeLayout,
    CAknQueryControl::EDateQueryLayoutM,   // EDateLayout,
    CAknQueryControl::ECodeQueryLayoutM,   // EListLayout,
    CAknQueryControl::ECodeQueryLayoutM,   // EMultiListLayout,
    CAknQueryControl::ECodeQueryLayoutM,   // EMultiLineDataLayout,
    CAknQueryControl::ETimeQueryLayoutM,   // EDurationLayout,
    CAknQueryControl::ECodeQueryLayoutM,   // ETextSoftNotification, - THIS IS USED FOR NOTES
    CAknQueryControl::EDataQueryLayoutM,   // ENumberLayout,
    CAknQueryControl::ECodeQueryLayoutM,   // EPinLayout,
    CAknQueryControl::EDataQueryLayoutM,   // EPhoneLayout,
    CAknQueryControl::EDataQueryLayoutM,   // EMessageQueryLayout,
    CAknQueryControl::EDataQueryLayoutM,   // EStaticSizeDataLayout
    CAknQueryControl::EDataQueryLayoutM,   // EFloatingPointLayout
    CAknQueryControl::ETimeQueryLayoutM,   // EIpLayout
    CAknQueryControl::EDataQueryLayoutM,    // EFixedPointLayout
    CAknQueryControl::EDataQueryLayoutM    // EDataLayoutAllowEmptyInput - USED FOR EMPTY INPUT
    };

///
/// Construction and Destruction methods
///

/**
 *
 */
EXPORT_C CAknQueryControl::CAknQueryControl() :/* iQueryType(EConfirmationLayout),*/ iNumberOfEditorLines(-1), iHasEditor(EFalse)
    {
    AKNTASHOOK_ADD( this, "CAknQueryControl" );
    }

/**
 *
 */
EXPORT_C CAknQueryControl::~CAknQueryControl()
    {
    AKNTASHOOK_REMOVE();
	if (iEdwin)
        AknsUtils::DeregisterControlPosition(iEdwin);
    else if (iNumberEdwin)
        AknsUtils::DeregisterControlPosition(iNumberEdwin);
    else if (iFloatingPointEditor)
        AknsUtils::DeregisterControlPosition(iFloatingPointEditor);
    else if (iDateEdwin)
        AknsUtils::DeregisterControlPosition(iDateEdwin);
    else if (iTimeEdwin)
        AknsUtils::DeregisterControlPosition(iTimeEdwin);
    else if (iDurationEdwin)
        AknsUtils::DeregisterControlPosition(iDurationEdwin);
    else if (iSecretEd)
        AknsUtils::DeregisterControlPosition(iSecretEd);
    else if (iPinEdwin)
        AknsUtils::DeregisterControlPosition(iPinEdwin);
    else if (iExtension && LocationEd())
    	AknsUtils::DeregisterControlPosition(LocationEd());
    

    delete iLineWidths;
    delete iEcsDetector;
    delete iEcsObserver;
    delete iPrompt;
    delete iEditIndicator;
    if ( iImage && iImage->IsPictureOwnedExternally() )
        {
        delete ((CApaMaskedBitmap*) iImage->Bitmap());
        }
    delete iImage;
    delete iEdwin;
    delete iDateEdwin;
    delete iTimeEdwin;
    delete iDurationEdwin;
    delete iSecretEd;
    delete iNumberEdwin;
    delete iPinEdwin;
    delete iFloatingPointEditor;
    if (iExtension)
    delete LocationEd();
    delete iAnimation;
    if ( iQueryType ) // As we must maintain BC, we cannot add private members or change protected members. 
        {         // We have to use iQueryType to distinguish if the context was added to stack or not
              // This won't affect to normal usage of iQueryType any way, only a way to indicate if 
                          // construction was succesful until context was set. 
        CCoeBrushAndPenContext* brushAndPenContext = (CCoeBrushAndPenContext*)ControlContext();
        delete brushAndPenContext;
        }
    CAknQueryExtension::DestroyExtension( this );
	delete iExtension;
        
#if defined( RD_SCALABLE_UI_V2)
    delete iIncreaseValueButton;
    delete iDecreaseValueButton;
#endif //defined( RD_SCALABLE_UI_V2)
    }

/**
 * Construct from resource
 */
EXPORT_C void CAknQueryControl::ConstructFromResourceL(TResourceReader &aRes)
    {
    // Construct extension
    CAknQueryExtension::CreateExtensionL( this );
	iExtension = new (ELeave)CAknQueryControlExtension;
    
    iEcsObserver   = new (ELeave) TAknQueryEcsObserver(this);
    iEditIndicator = CAknQueryEditIndicator::NewL(this);
    iEditIndicator->SetObserver(this);
    
    iLineWidths = new(ELeave)CArrayFixFlat<TInt>(5);

    CCoeBrushAndPenContext* brushAndPenContext=CCoeBrushAndPenContext::NewL();
    brushAndPenContext->SetBrushStyle(CGraphicsContext::ENullBrush);
    SetControlContext(brushAndPenContext);
    // This must be straight after setting Graphics Context, it is the only way to indicate
    // brushAndPenContext is added to stack or not.
    iQueryType = aRes.ReadUint16();
    __ASSERT_DEBUG(iQueryType, Panic(EAknPanicLayoutResourceProblem));
    
    iEditIndicator->SetQueryType(iQueryType);
    
    SetLineWidthsL();
    ReadPromptL(aRes);
    ConstructQueryL(aRes);

    CEikDialog* dlg;
    MopGetObject( dlg );
    if ( dlg )
        {
        if ( CAknEnv::Static()->TransparencyEnabled() )
            {
            dlg->Extension()->iPublicFlags.Set( CEikDialogExtension::EClipChildControlRect );
            }

        // set dialog's window as button group's container window in case a fullscreen
        // virtual input dialog is requested
        if ( EmbeddedVirtualInput() )
            {   
            CEikCba* cba = static_cast<CEikCba*>( dlg->ButtonGroupContainer().ButtonGroup() );
            cba->SetContainerWindowL( *this );
            dlg->ButtonGroupContainer().SetContainerWindowL( *cba );
            dlg->ButtonGroupContainer().SetParent( NULL );
            cba->SetPosition( TPoint( 0, 0 ) );
            
            iExtension->iKeypad = CAknKeypad::NewL( *this, iQueryType );
            }
        }
    }

void CAknQueryControl::ConstructQueryL(TResourceReader& aRes)
    {
    switch(iQueryType)
        {
        case EConfirmationQueryLayout:
            {
            TPtrC bmpFile=aRes.ReadTPtrC();
            TInt bmpId=aRes.ReadInt16();
            TInt bmpMask=aRes.ReadInt16();
            TInt animationId = aRes.ReadUint32();

            // Animation overrides bitmap if it is set.
            if ( (animationId == 0) && (bmpFile.Length() != 0))
                {   
                iImage = new(ELeave) CEikImage;
                iImage->SetContainerWindowL(*this);
                
                TBool skinnedFound( EFalse );
                if ( bmpFile.CompareF(KAvkonBitmapFile) == 0 ) // Skinning is supported only avkon bitmaps
                    {
                    TAknsItemID id;
                    id.Set(0,0);
                    id = CAknNoteAttributes::GetSkinnedBitmapID(bmpId);
                    if ( id.iMajor && id.iMinor ) // skin id found, try loading
                        {
                        CApaMaskedBitmap* bitmap = NULL;
                        MAknsSkinInstance* skin = AknsUtils::SkinInstance();

                        TRAPD( err, bitmap = AknsUtils::CreateMaskedBitmapL(skin,id));
                        if ( !err ) 
                            {
                            CFbsBitmap* mask = bitmap->Mask();
                            iImage->SetPicture(bitmap,mask);
                            iImage->SetPictureOwnedExternally(ETrue);
                            skinnedFound = ETrue;
                            }
                        }
                    }
                if ( !skinnedFound) 
                    {
                    iImage->CreatePictureFromFileL(bmpFile,bmpId,bmpMask);
                    }
                }
            else
                {
                if (animationId == 0) 
                    {
                    // Default animation if neither bitmap nor animation is set.
                    animationId = R_QGN_NOTE_QUERY_ANIM;
                    }
                }
            
            if (animationId != 0)
                {
                if (!iAnimation)
                    {// no animation previously set
                    // Save animation ID for possible skin changes
                    
                    //iExtension->iAnimationId = animationId; 

                    TRAPD( err, SetAnimationL( animationId ) );
                    if ( err != KErrNone )
                        {
                        iAnimation = NULL;
                        }
                    }
                }
            break;
            }
        case EDataLayoutAllowEmptyInput:
            {
            CAknQueryExtension* extension = QueryExtension();
            if( extension )
                {
                extension->iAllowEmptyInput = ETrue;
                }
            iQueryType = EDataLayout;    
            // fall through
            }
        case EDataLayout:
        case EStaticSizeDataLayout:
            {
            iEdwin = new(ELeave)CEikEdwin;
            iEdwin->SetContainerWindowL(*this);
            iEdwin->SetEdwinObserver(this);
            iEdwin->ConstructFromResourceL(aRes);
            iEdwin->SetObserver(this);
            iEdwin->SetEdwinSizeObserver(this);
            
            
            
#if defined( RD_SCALABLE_UI_V2)

            if ( AknLayoutUtils::PenEnabled() )
                {
                if ( EmbeddedVirtualInput() )
        			{
      				CreateBackspaceButtonL();
      				//HACK
      				iEdwin->AddFlagToUserFlags( CEikEdwin::EAvkonDisableVKB );
      				
      				iEdwin->SetMaximumHeightInLines( 1 );
      				
    
        			}
                }
            
#endif //if defined( RD_SCALABLE_UI_V2)
            

//Set maximum height in lines to KMaxLinesInQuery if it is unset, or an
//invalid value has been used.
            if (!iEdwin->MaximumHeightInLines()||iEdwin->MaximumHeightInLines()>KMaxLinesInQuery)
                iEdwin->SetMaximumHeightInLines(KMaxLinesInQuery);

//If the maximum number of lines is 1, 
//  ensure flags are consistent with a single-line editor scrolling mechanism
//else
//  ensure flags are consistent with a multi-line editor scrolling mechanism

            if (1==iEdwin->MaximumHeightInLines())
                iEdwin->RemoveFlagFromUserFlags(EEikEdwinNoHorizScrolling | EEikEdwinResizable);
            else
                iEdwin->AddFlagToUserFlags(EEikEdwinNoHorizScrolling | EEikEdwinResizable);
            if (EDataLayout==iQueryType && iEdwin->InputCapabilities().IsNone())
                {
                iEdwin->SetAknEditorAllowedInputModes(EAknEditorTextInputMode | EAknEditorNumericInputMode );              
                }
            break;
            }
        case ECodeLayout:
            {
            iSecretEd = new(ELeave)CEikSecretEditor;
            iSecretEd->SetContainerWindowL(*this);
            iSecretEd->ConstructFromResourceL(aRes);
            iSecretEd->SetObserver(this);
            iSecretEd->SetSkinTextColorL(EAknsCIQsnTextColorsCG27);
            
            
#if defined( RD_SCALABLE_UI_V2)

            if ( AknLayoutUtils::PenEnabled() )
                {
                if ( EmbeddedVirtualInput() )
        			{
      				CreateBackspaceButtonL();
      				iSecretEd->SetFeature(CEikSecretEditor::EDisablePenInput, 1);
        			}
                }
            
#endif //if defined( RD_SCALABLE_UI_V2)
            
            break;
            }
        case EDateLayout:
            {
            iDateEdwin = new(ELeave)CEikDateEditor;
            iDateEdwin->SetContainerWindowL(*this);
            iDateEdwin->ConstructFromResourceL(aRes);
            iDateEdwin->SetObserver(this);
            iDateEdwin->SetSkinTextColorL(EAknsCIQsnTextColorsCG27);
            

#if defined( RD_SCALABLE_UI_V2)

            if ( AknLayoutUtils::PenEnabled() )
                {
       			if ( EmbeddedVirtualInput() )
        			{
      				CreateBackspaceButtonL();
      				iDateEdwin->SetFeature(CEikMfne::EDisablePenInput, 1);
        			}
        		else
        			{
        			iDateEdwin->SetFeature( CEikMfne::EFingerSupport, CEikMfne::EnableWithAllHighlight );
        			}
                }
            
#endif //if defined( RD_SCALABLE_UI_V2)
            
            break;
            }
        case ETimeLayout:
            {
            iTimeEdwin = new(ELeave)CEikTimeEditor;
            iTimeEdwin->SetContainerWindowL(*this);
            iTimeEdwin->ConstructFromResourceL(aRes);
            iTimeEdwin->SetObserver(this);
            iTimeEdwin->SetSkinTextColorL(EAknsCIQsnTextColorsCG27);
            
            
#if defined( RD_SCALABLE_UI_V2)

            if ( AknLayoutUtils::PenEnabled() )
                {
                if ( EmbeddedVirtualInput() )
        			{
      				CreateBackspaceButtonL();
      				iTimeEdwin->SetFeature(CEikMfne::EDisablePenInput, 1);
        			}
        		else
        			{
        			iTimeEdwin->SetFeature( CEikMfne::EFingerSupport, CEikMfne::EnableWithAllHighlight );
        			}
                }
                
#endif //if defined( RD_SCALABLE_UI_V2)
            
            break;
            }
        case EDurationLayout:
            {
            iDurationEdwin = new(ELeave)CEikDurationEditor;
            iDurationEdwin->SetContainerWindowL(*this);
            iDurationEdwin->ConstructFromResourceL(aRes);
            iDurationEdwin->SetObserver(this);
            iDurationEdwin->SetSkinTextColorL(EAknsCIQsnTextColorsCG27);
#if defined( RD_SCALABLE_UI_V2)

            if ( AknLayoutUtils::PenEnabled() )
                {
                iDurationEdwin->SetFeature( CEikMfne::EFingerSupport, CEikMfne::EnableWithAllHighlight );
                }
            
#endif //if defined( RD_SCALABLE_UI_V2)            
            break;
            }
        case ENumberLayout:
            {
            iNumberEdwin = new(ELeave)CAknIntegerEdwin;
            iNumberEdwin->SetContainerWindowL(*this);
            iNumberEdwin->ConstructFromResourceL(aRes);
            iNumberEdwin->SetObserver(this);
            break;
            }
        case EPinLayout:
            {
            iPinEdwin = new(ELeave)CAknNumericSecretEditor;
            iPinEdwin->SetContainerWindowL(*this);
            iPinEdwin->ConstructFromResourceL(aRes);
            iPinEdwin->SetObserver(this);
            iPinEdwin->SetSkinTextColorL(EAknsCIQsnTextColorsCG27);
            
            // reset tacticon playback counter and play tacticon, which
            // also resets the tacticon timer
            if ( iExtension )
                {
                iExtension->iPinTacticonsPlayed = 0;
                PlayPinCodeTacticon();
                }
            
#if defined( RD_SCALABLE_UI_V2)

            if ( AknLayoutUtils::PenEnabled() )
                {
                if ( EmbeddedVirtualInput() )
        			{
      				CreateBackspaceButtonL();
      				iPinEdwin->SetFeature(CEikSecretEditor::EDisablePenInput, 1);
        			}
                }
            
#endif //if defined( RD_SCALABLE_UI_V2)
                   
            break;
            }
        case EPhoneLayout:
            {
            iEdwin = new(ELeave)CEikEdwin;
            iEdwin->AddFlagToUserFlags(EEikEdwinAlternativeWrapping);
            iEdwin->SetContainerWindowL(*this);
            iEdwin->SetEdwinObserver(this);
            iEdwin->ConstructFromResourceL(aRes);
            iEdwin->SetEdwinSizeObserver(this);

//Set maximum height in lines to KMaxLinesInQuery if it is unset, or an
//invalid value has been used.
            if (!iEdwin->MaximumHeightInLines()||iEdwin->MaximumHeightInLines()>KMaxLinesInQuery)
                iEdwin->SetMaximumHeightInLines(KMaxLinesInQuery);

//If the maximum number of lines is 1, 
//  ensure flags are consistent with a single-line editor scrolling mechanism
//else
//  ensure flags are consistent with a multi-line editor scrolling mechanism

            if (1==iEdwin->MaximumHeightInLines())
                iEdwin->RemoveFlagFromUserFlags(EEikEdwinNoHorizScrolling | EEikEdwinResizable);
            else
                iEdwin->AddFlagToUserFlags(EEikEdwinNoHorizScrolling | EEikEdwinResizable);
            iEdwin->SetAknEditorInputMode(EAknEditorNumericInputMode);
            iEdwin->SetAknEditorAllowedInputModes(EAknEditorNumericInputMode);
            iEdwin->SetObserver(this);
            break;
            }
        case EFloatingPointLayout:
            {
            iFloatingPointEditor = new ( ELeave ) CEikFloatingPointEditor;
            iFloatingPointEditor->SetContainerWindowL( *this );
            iFloatingPointEditor->SetEdwinObserver( this );
            iFloatingPointEditor->ConstructFromResourceL( aRes );
            iFloatingPointEditor->SetAknEditorAllowedInputModes( EAknEditorNumericInputMode );
            iFloatingPointEditor->SetObserver(this);
            break;
            }
        default:
              __ASSERT_DEBUG(0, Panic(EAknPanicLayoutControlResourceProblem));
            break;
    }

    // Construct editor context
    CAknQueryExtension* extension = QueryExtension();
    if( extension )
        {
        extension->CreateEditorContextL();
        }
    }

TInt CAknQueryControl::PinCodeTimeoutCallback( TAny* aThis )
    {
    // play tacticon
    static_cast<CAknQueryControl*>( aThis )->PlayPinCodeTacticon();
    
    return KErrNone;
    }

void CAknQueryControl::PlayPinCodeTacticon()
    {
    // play tacticon
    RTacticon client;
    if ( KErrNone == client.Connect() )
    	{
    	client.PlayTacticon( ENeutralTacticon );
    	client.Close();
    	}
    
    if ( iExtension )
        {
        iExtension->iPinTacticonsPlayed++;
        
        // if the maximum amount of pincode query tacticons has not been
        // reached, reset timer
        if ( iExtension->iPinTacticonsPlayed < KPinCodeMaxTacticons )
            {
            ResetPinCodeTacticonTimer();
            }
        else
            {
            // max amount reached, stop timer for good
            StopPinCodeTacticonTimer();
            }
        }
    }

void CAknQueryControl::ResetPinCodeTacticonTimer()
    {
    // stop old timer
    StopPinCodeTacticonTimer();

    if ( iExtension )
        {
        // create new one
        iExtension->iPinTacticonTimer = 
            CPeriodic::New( CActive::EPriorityStandard );
        
        // start timer, if creation succeeded
        if ( iExtension->iPinTacticonTimer )
            {
            iExtension->iPinTacticonTimer->Start( 
                        KPinCodeTacticonInterval, KPinCodeTacticonInterval,
                        TCallBack( PinCodeTimeoutCallback, this ) );
            }
        }
    }

void CAknQueryControl::StopPinCodeTacticonTimer()
    {
    if ( iExtension )
        {
        // delete timer
        delete iExtension->iPinTacticonTimer;
        
        // set the timer null, so that it will not be used again
        iExtension->iPinTacticonTimer = NULL;
        }
    }

///
/// CCoeControl methods
///

/**
 *Returns the control owned by this control by the index
 */
CCoeControl* CAknQueryControl::ComponentControl(TInt anIndex) const
    {
    if ( AknLayoutUtils::PenEnabled() )
        {
        CCoeControl* cba = NULL;
        CEikDialog* dialog;
        
        const_cast<CAknQueryControl*>( this )->MopGetObject( dialog );
        
        if ( dialog && EmbeddedVirtualInput() )
            {
            cba = static_cast<CEikCba*>( dialog->ButtonGroupContainer().ButtonGroup() );
            }

        CCoeControl* controls[] = { iPrompt, 
                                    iEditIndicator, 
                                iEdwin, iDateEdwin, LocationEd(), 
                                    iTimeEdwin, iSecretEd, iDurationEdwin,
                                    iNumberEdwin, iPinEdwin, iFloatingPointEditor,
                                    iImage, iAnimation
#if defined( RD_SCALABLE_UI_V2)                              
                                    ,iIncreaseValueButton, iDecreaseValueButton
                                    ,( iExtension && iExtension->iKeypad) ? cba : NULL
                                    ,iExtension ? iExtension->iKeypad : NULL
                                    ,iExtension ? iExtension->iBackspaceButton : NULL
#endif //if defined( RD_SCALABLE_UI_V2)                         
                                    };
        
        for (TUint ii=0;ii<sizeof(controls)/sizeof(CCoeControl*);ii++)
            {
            //subtle: index is decremented only if control is not NULL!
            if (controls[ii] && anIndex-- == 0) 
                {
                return controls[ii];
                }
            }
        }
    else
        {
        CCoeControl* controls[] = { iPrompt, 
                                    iEditIndicator, 
                                	iEdwin, iDateEdwin, LocationEd(), 
                                    iTimeEdwin, iSecretEd, iDurationEdwin,
                                    iNumberEdwin, iPinEdwin, iFloatingPointEditor,
                                    iImage, iAnimation
                                    };
        //TODO add buttons to array

        
        for (TUint ii=0;ii<sizeof(controls)/sizeof(CCoeControl*);ii++)
            {
            //subtle: index is decremented only if control is not NULL!
            if (controls[ii] && anIndex-- == 0) 
                {
                return controls[ii];
                }
            }
        }
    return NULL;
    }     

/**
 * Returns the number of controls inside the compound control.
 */
TInt CAknQueryControl::CountComponentControls() const
    {
    TInt count = 0;
    if ( AknLayoutUtils::PenEnabled() )
        {
        CCoeControl* cba = NULL;
        CEikDialog* dialog;
        
        const_cast<CAknQueryControl*>( this )->MopGetObject( dialog );
        
        if ( dialog && EmbeddedVirtualInput() )
            {
            cba = static_cast<CEikCba*>( dialog->ButtonGroupContainer().ButtonGroup() );
            }

        CCoeControl* controls[] = { iPrompt,
                                    iEditIndicator,
                                    iEdwin, iDateEdwin, LocationEd(),
                                    iTimeEdwin, iSecretEd, iDurationEdwin,
                                    iNumberEdwin, iPinEdwin, iFloatingPointEditor,
                                    iImage, iAnimation
#if defined( RD_SCALABLE_UI_V2)                              
                                    ,iIncreaseValueButton, iDecreaseValueButton,
                                    ( iExtension && iExtension->iKeypad) ? cba : NULL,
                                    iExtension ? iExtension->iKeypad : NULL,
                                    iExtension ? iExtension->iBackspaceButton : NULL
#endif //if defined( RD_SCALABLE_UI_V2)                                   
                                };
                                
        for (TUint ii=0;ii<sizeof(controls)/sizeof(CCoeControl*);ii++)
            {
            if (controls[ii])
                {
                count++;
                }
            }
        }
    else
        {
        CCoeControl* controls[] = { iPrompt,
                                    iEditIndicator,
                                	iEdwin, iDateEdwin, LocationEd(),
                                    iTimeEdwin, iSecretEd, iDurationEdwin,
                                    iNumberEdwin, iPinEdwin, iFloatingPointEditor,
                                    iImage, iAnimation
                                };
                                
        for (TUint ii=0;ii<sizeof(controls)/sizeof(CCoeControl*);ii++)
            {
            if (controls[ii])
                {
                count++;
                }
            }
        }
    return count;
    }

/**
 * Forward key event to the editor
 */
EXPORT_C TKeyResponse CAknQueryControl::OfferKeyEventL( const TKeyEvent& aKeyEvent, TEventCode aType) 
    {
    TKeyResponse response( EKeyWasNotConsumed );

    // Send key handling for emergency call support
    if ( aKeyEvent.iCode == EKeyYes )
        {
        if ( iEcsDetector )
            {
            AttemptEmergencyCallL();
            }
        // Always eat the key, however, as in former handling of this key
        response = EKeyWasConsumed;
        }

    CCoeControl* ctrl = ControlByLayoutOrNull( iQueryType );
    if ( ctrl && ctrl->IsFocused() )
        {
        response = ctrl->OfferKeyEventL( aKeyEvent, aType );
        
        // pin query received key, reset timer if it exists
        if ( iExtension && iExtension->iPinTacticonTimer && 
             iQueryType == EPinLayout && response == EKeyWasConsumed )
            {
            ResetPinCodeTacticonTimer();
            }
        }
    return response;
    }

/**
 * Handle a focus changed
 */
void CAknQueryControl::FocusChanged(TDrawNow aDrawNow)
    {
    CCoeControl* ctrl = ControlByLayoutOrNull( iQueryType );
    if (ctrl)
        {
        ctrl->DrawDeferred();
        ctrl->SetFocus( IsFocused(), aDrawNow );
        Layout();
        }
    if ( iAnimation )
        {
        iAnimation->SetFocus( IsFocused(), aDrawNow );
        }
    }

void CAknQueryControl::PrepareForFocusLossL()
    {
    CCoeControl* ctrl = ControlByLayoutOrNull( iQueryType );
    if (ctrl)
        {
        ctrl->PrepareForFocusLossL(); 
        }
    }

///
/// Observer related methods (Handle* methods)
///

//
//Set observer for component
//
EXPORT_C void CAknQueryControl::SetQueryControlObserver(MAknQueryControlObserver* aQueryControlObserver)
    {
    iQueryControlObserver = aQueryControlObserver;
    }

//
//Handle edwin events
//
EXPORT_C void CAknQueryControl::HandleEdwinEventL(CEikEdwin* /*aEdwin*/,TEdwinEvent /*aEventType*/)
    {

    }

//
//Call observer when edwin size changed
//
EXPORT_C TBool CAknQueryControl::HandleEdwinSizeEventL(CEikEdwin* /*aEdwin*/, TEdwinSizeEvent /*aEventType*/, TSize /*aDesirableEdwinSize*/)
    {
    TInt editorNbr = NbrOfEditorLines(); 
    TInt maxLines = KMaxLinesInQuery; 
    if (iEdwin)
        {
        const TInt mHIL = iEdwin->MaximumHeightInLines();
        if (mHIL)
            {
            maxLines = mHIL;
            }
        }

    if(iQueryControlObserver  && editorNbr <= maxLines)
        {
        iQueryControlObserver->HandleQueryEditorSizeEventL(this,MAknQueryControlObserver::EQueryControltSizeChanging);
        }
    return EFalse;
    }


EXPORT_C void CAknQueryControl::HandleControlEventL(CCoeControl* aControl,TCoeEvent aEventType)
    {
    if (iEditIndicator && aControl == iEditIndicator)
    	{
    	if ( iExtension && iExtension->iKeypad )
        	{
        	iExtension->iKeypad->SetState(iEditIndicator->State());
        	}
        return;	
    	}
    
    if(iQueryControlObserver && aEventType == EEventStateChanged)
        {
        if ( iNumberEdwin || iFloatingPointEditor)
            {
            TInt intValue;
            TReal realValue;
            CAknNumericEdwin::TValidationStatus status = iNumberEdwin ?
                iNumberEdwin->GetTextAsInteger(intValue) : 
                    iFloatingPointEditor->GetValueAsReal(realValue);
            switch( status )
                {
                case CAknNumericEdwin::EEmpty:
                    {
                    iQueryControlObserver->HandleQueryEditorStateEventL(this,MAknQueryControlObserver::EQueryControlEditorStateChanging, MAknQueryControlObserver::EEditorEmpty);   
                    break;
                    }
                case CAknNumericEdwin::EValueTooLarge:
                    {
                    iQueryControlObserver->HandleQueryEditorStateEventL(this,MAknQueryControlObserver::EQueryControlEditorStateChanging, MAknQueryControlObserver::EEditorValueTooLarge);   
                    break;
                    }
                case CAknNumericEdwin::EValueTooSmall:
                    {
                    iQueryControlObserver->HandleQueryEditorStateEventL(this,MAknQueryControlObserver::EQueryControlEditorStateChanging, MAknQueryControlObserver::EEditorValueTooSmall);   
                    break;
                    }
                case CAknNumericEdwin::EValueNotParsed:
                    {
                    iQueryControlObserver->HandleQueryEditorStateEventL(this,MAknQueryControlObserver::EQueryControlEditorStateChanging, MAknQueryControlObserver::EEditorValueNotParsed);  
                    break;
                    }
                case CAknNumericEdwin::EValueValid:
                    {
                    iQueryControlObserver->HandleQueryEditorStateEventL(this,MAknQueryControlObserver::EQueryControlEditorStateChanging, MAknQueryControlObserver::EEditorValueValid);  
                    break;
                    }
                default:
                    break;
                }
            }
        else
            {
            if(GetTextLength())
                {
                iQueryControlObserver->HandleQueryEditorStateEventL(this,MAknQueryControlObserver::EQueryControlEditorStateChanging, MAknQueryControlObserver::EEditorValueValid);
                }
            else
                {
                iQueryControlObserver->HandleQueryEditorStateEventL(this,MAknQueryControlObserver::EQueryControlEditorStateChanging, MAknQueryControlObserver::EEditorEmpty);
                }
            }

        // Emergency Call support. Set the buffer in the Ecs copied from the editor
        if ( iEcsDetector )
            {
            if ( iNumberEdwin || iPinEdwin )
                {
                // In either case, we need to copy the first KAknEcsMaxMatchingLength
                // characters to the emergency call buffer
                if ( iNumberEdwin )
                    {
                    TBuf<KAknEcsMaxMatchingLength> buf;
                    iNumberEdwin->Text()->Extract(buf, 0, KAknEcsMaxMatchingLength );
                    iEcsDetector->SetBuffer( buf );
                    }   
                else if ( iPinEdwin )
                    {
                    TPtrC secretBuffer = iPinEdwin->Buffer().Left(KAknEcsMaxMatchingLength);
                    iEcsDetector->SetBuffer( secretBuffer );
                    // Manage appearance of the PIN query
                    // If match is complete, or complete + Send key, show the number
                    TBool reveal = EFalse;
                    TInt ecsState( iEcsDetector->State() );
                    if ( ( ecsState == CAknEcsDetector::ECompleteMatch ) ||
                         ( ecsState == CAknEcsDetector::ECompleteMatchThenSendKey ) )
                        {
                        // Further check to ensure that the matched number is the entire buffer
                        // Get the matched text and see if is the same length as the current query length
                        if ( GetTextLength() == iEcsDetector->CurrentMatch().Length() ) 
                            {
                            reveal = ETrue;
                            
                            if ( ecsState == CAknEcsDetector::ECompleteMatch )
                                {
                                SetEcsCbaVisibleL( ETrue );                                            
                                }
                            }
                        }
                    iPinEdwin->RevealSecretText( reveal );
                    }
                }
            }
            
        if (aControl)
            {
            
#if defined( RD_SCALABLE_UI_V2)
            if ( AknLayoutUtils::PenEnabled() )
                {
                if(aControl == iIncreaseValueButton)
                    {
                    if(iTimeEdwin)
                        {
                        iTimeEdwin->HandleMfneCommandL(MAknMfneCommandObserver::EMfneIncrementCurrentFieldValue);
                        }
                    if(iDateEdwin)
                        {
                        iDateEdwin->HandleMfneCommandL(MAknMfneCommandObserver::EMfneIncrementCurrentFieldValue);
                        }
                    if(iDurationEdwin)
                        {
                        iDurationEdwin->HandleMfneCommandL(MAknMfneCommandObserver::EMfneIncrementCurrentFieldValue);
                        }                            
                    if(iExtension && LocationEd())
                        {
                        LocationEd()->HandleMfneCommandL(MAknMfneCommandObserver::EMfneIncrementCurrentFieldValue);
                        }
                         
                    }
                else if(aControl == iDecreaseValueButton)
                    {
                    if(iTimeEdwin)
                        {
                        iTimeEdwin->HandleMfneCommandL(MAknMfneCommandObserver::EMfneDecrementCurrentFieldValue);
                        }
                    if(iDateEdwin)
                        {
                        iDateEdwin->HandleMfneCommandL(MAknMfneCommandObserver::EMfneDecrementCurrentFieldValue);
                        }
                    if(iDurationEdwin)
                        {
                        iDurationEdwin->HandleMfneCommandL(MAknMfneCommandObserver::EMfneDecrementCurrentFieldValue);
                        }                        
                    if(iExtension && LocationEd())
                        {
                        LocationEd()->HandleMfneCommandL(MAknMfneCommandObserver::EMfneIncrementCurrentFieldValue);
                        }
                    }
                else if( iExtension && aControl == iExtension->iBackspaceButton )
                    {
                    TInt scanCode = EStdKeyBackspace;
            		TRawEvent event;
            		event.Set( TRawEvent::EKeyDown, scanCode );
            		event.SetTip( ETrue );
            		
					if (iEikonEnv->AppUi())
						{
						static_cast<CAknAppUi*>(iEikonEnv->EikAppUi())->DisableNextKeySound( scanCode );
						}	
            		
            		CCoeEnv::Static()->WsSession().SimulateRawEvent( event );
            		event.Set( TRawEvent::EKeyUp, scanCode );
            		CCoeEnv::Static()->WsSession().SimulateRawEvent( event );
                    }
                }
#endif //if defined( RD_SCALABLE_UI_V2)
            }
        }
    }

///
/// DRAWING METHODS
///

/**
 * Draw the following:-
 *  - Text editor frame if there is a text editor, which is true for all
 *    queries except confirmation queries  
 */
void CAknQueryControl::Draw(const TRect& /*aRect*/) const
    {
    CWindowGc& gc=SystemGc(); 
    TRect rect(Rect());
    
    TRect dialogRect = DialogRect();
    rect.iBr.iY += dialogRect.iBr.iY; 

    MAknsSkinInstance* skin = AknsUtils::SkinInstance();

    TRgb color;
    TInt error = AknsUtils::GetCachedColor( skin, color, KAknsIIDQsnTextColors, EAknsCIQsnTextColorsCG19 );
    if ( !error )
        {        
        TRAP_IGNORE(AknLayoutUtils::OverrideControlColorL( *(CAknQueryControl*)this, EColorLabelText, color ) );
        }

    MAknsControlContext* cc = AknsDrawUtils::ControlContext( this );

    if ( CAknEnv::Static()->TransparencyEnabled() )
        {
        RRegion clipReg;
        clipReg.AddRect( rect );
    
        if ( iHasEditor )
            {
            clipReg.SubRect( iEditorFrame.Rect() );
            }
    
        if ( iEditIndicator )
            {
            clipReg.SubRect( 
                TRect( iEditIndicator->Position(),
                iEditIndicator->Size() ) );
            }

        gc.SetClippingRegion( clipReg );
        clipReg.Close();
        
        AknsDrawUtils::Background( skin, cc, this, gc, rect, KAknsDrawParamNoClearUnderImage );
        
        clipReg.AddRect( rect );

        CCoeControl* ctrl = const_cast<CAknQueryControl*>(this)->ControlByLayoutOrNull( iQueryType );
        if ( ctrl )
            {
            clipReg.SubRect( ctrl->Rect() );
            }

        gc.CancelClippingRegion();
        gc.SetClippingRegion( clipReg );
        clipReg.Close();
        }
    else
        {
            AknsDrawUtils::Background( skin, cc, this, gc, rect );
        }

    DrawEditorFrame(gc,rect);
 
    if ( CAknEnv::Static()->TransparencyEnabled() )
        {
        gc.CancelClippingRegion();
        }
    }

/**
 * Draw text editor frame and shadow 
 */
void CAknQueryControl::DrawEditorFrame(CWindowGc& aGc,TRect& /*aRect*/) const
    {
    if (iHasEditor)
        {
        TBool skinnedDraw( EFalse );

        CAknQueryExtension* extension = QueryExtension();
        if( extension && extension->EditorContext() )
            {
            // If first-level image exists, frame has already been drawn
            // as part of the context.

            if ( CAknEnv::Static()->TransparencyEnabled() )
                {
                const CCoeControl* ctrl = const_cast<CAknQueryControl*>(this)->ControlByLayoutOrNull( iQueryType );
                if ( ctrl )
                    {
                    skinnedDraw = AknsDrawUtils::Background( 
                        AknsUtils::SkinInstance(), 
                        extension->EditorContext(), 
                        ctrl,
                        aGc,
                        iEditorFrame.Rect(),
                        KAknsDrawParamNoClearUnderImage ); 
                    }
                }
            else
                {
                skinnedDraw = AknsDrawUtils::Background( 
                    AknsUtils::SkinInstance(), 
                    extension->EditorContext(), 
                    aGc, 
                    iEditorFrame.Rect() );                 
                }
            }

        // ... otherwise, draw the non-skinned frame
        if( !skinnedDraw )
            {
            iEditorFrame.DrawOutLineRect(aGc);
            iEditorVerShadow.DrawRect(aGc); 
            iEditorHorShadow.DrawRect(aGc);
            }
        }
    }


///
/// LAYOUT METHODS
///

/**
 * Depending on query type and on the number of prompt or editor lines 
 * return the window layout.
 *
 * This is used by a dialog to layout the control correctly.
 *
 */
void CAknQueryControl::WindowLayout( TAknWindowLineLayout& aLayout ) const
    {
    if ( EmbeddedVirtualInput() )
        {
        aLayout = AknLayoutScalable_Avkon::application_window( 0 );
        return;
        }

    TIndex LAFIndex(NbrOfPromptLines());

    switch (KQueryTypeToLayout[iQueryType])
        {
        case EConfQueryLayoutM:
            aLayout = AknLayoutScalable_Avkon::popup_note_window(LAFIndex.PNWindow());
            break;
               
        case EDataQueryLayoutM: 
            aLayout = AknLayoutScalable_Avkon::popup_query_data_window(LAFIndex.PQDWindow(NbrOfEditorLines()));
            break;
   
        case ECodeQueryLayoutM:
            aLayout = AknLayoutScalable_Avkon::popup_query_code_window(LAFIndex.PQCWindow());
            break;
        
        case ETimeQueryLayoutM:
            aLayout = AknLayoutScalable_Avkon::popup_query_code_window(LAFIndex.PQCWindow());
            break;

        case EDateQueryLayoutM:
            aLayout = AknLayoutScalable_Avkon::popup_query_code_window(LAFIndex.PQCWindow());;
            break;

        default:
            __ASSERT_DEBUG(0,Panic(EAknPanicNotSupported));
            aLayout = AknLayoutScalable_Avkon::popup_note_window(LAFIndex.PNWindow());            
            break;
        }
    }

/**
 * Layout information in LAF is relative to the size of the dialog and not to the
 * size of the internal area (dialog size - margins from UIKLAF). Therefore when
 * doing layout do not use Rect(), but LayoutRect() - which returns the size of the
 * dialog.
 *
 * Position is always (0,0) - top 
 *
 */
TRect CAknQueryControl::LayoutRect() 
    {
#ifndef RD_NO_DIALOG_BORDERS    
	TPoint topLeft = Position();
    TRect parent   = DialogRect();
	TAknLayoutRect layout;

	if (KQueryTypeToLayout[iQueryType] == EDataQueryLayoutM)
		layout.LayoutRect(parent, AKN_LAYOUT_WINDOW_Data_query_pop_up_window_graphics_Line_5(parent));
    else
		layout.LayoutRect(parent, AKN_LAYOUT_WINDOW_Note_pop_up_window_graphics_Line_5(parent));

	TRect rect(layout.Rect());
	topLeft.iX -= rect.iTl.iX;
    topLeft.iY -= rect.iTl.iY;

    TPoint bottomRight (topLeft);
    TSize  size (DialogSize());
    bottomRight.iY += size.iHeight; 
    bottomRight.iX += size.iWidth;

    return TRect(topLeft,bottomRight);
#else
    return Rect();
#endif        		
    }

/**
 * Do layout (call SizeChanged) and if the control is
 * activated do a redraw
 */
void CAknQueryControl::Layout()
    {
    SizeChanged();
    }

/**
 * Perform layout depending on query type.
 */
void CAknQueryControl::SizeChanged()
    {  
    DialogSizeChanged();
    iPrompt->SetRect(LayoutRect());
    LayoutPrompt(KQueryTypeToLayout[iQueryType]);

    LayoutImageOrAnim(KQueryTypeToLayout[iQueryType]);
    LayoutEditor(KQueryTypeToLayout[iQueryType]);
    
    
	// Layout Keypad
    
    if ( iExtension && iExtension->iKeypad )
    	{
        TBool landscape = Layout_Meta_Data::IsLandscapeOrientation();
     
        TAknLayoutRect layoutRect;
        layoutRect.LayoutRect( LayoutRect(), TAknWindowComponentLayout::Compose(
            AknLayoutScalable_Apps::popup_vitu2_window( landscape ? 1 : 0 ), 
            AknLayoutScalable_Apps::grid_vitu2_itu_pane( landscape ? 5 : 0 ) ).LayoutLine() );
                 
        iExtension->iKeypad->SetRect( layoutRect.Rect() ); 
        }

        
    // Layout Backspace Button
     
     if ( iExtension && iExtension->iBackspaceButton )
        {
        TBool landscape = Layout_Meta_Data::IsLandscapeOrientation();
     
        TAknLayoutRect layoutRect;
 		layoutRect.LayoutRect( LayoutRect(), TAknWindowComponentLayout::Compose(
 			AknLayoutScalable_Apps::popup_vitu2_window( landscape ? 1 : 0 ), 
    		AknLayoutScalable_Apps::area_vitu2_query_pane( landscape ? 1 : 0 ) ).LayoutLine() );
    
        TAknLayoutRect layout;
 		layout.LayoutRect(layoutRect.Rect(), AknLayoutScalable_Apps::bg_button_pane_cp022( 0 ));
 
		iExtension->iBackspaceButton->SetRect(layout.Rect());
        }
        
    // Layout CBA
    
    CEikDialog* dlg;
    MopGetObject( dlg );
    if ( dlg )
        {
        // Note: iExtension->iKeypad is used to ensure that SetRect is not called too early
	    if ( iExtension && iExtension->iKeypad && EmbeddedVirtualInput() )
            {   
            CEikCba* cba = static_cast<CEikCba*>( dlg->ButtonGroupContainer().ButtonGroup() );   
            
            TBool landscape = Layout_Meta_Data::IsLandscapeOrientation();
            
        	TAknLayoutRect layoutRect;
 			layoutRect.LayoutRect( LayoutRect(), TAknWindowComponentLayout::Compose(
 				AknLayoutScalable_Apps::popup_vitu2_window( landscape ? 1 : 0 ), 
    		AknLayoutScalable_Apps::area_vitu2_query_pane( landscape ? 1 : 0 ) ).LayoutLine() );
        
			TAknLayoutRect leftLayout;
			leftLayout.LayoutRect(layoutRect.Rect(), AknLayoutScalable_Apps::bg_button_pane_cp018( 0 ));

			TAknLayoutRect rightLayout;
			rightLayout.LayoutRect(layoutRect.Rect(), AknLayoutScalable_Apps::bg_button_pane_cp021( 0 ));
       		
       		TRect rect;
       		if ( AknLayoutUtils::LayoutMirrored() )
       		    {
       		    rect.iTl = rightLayout.Rect().iTl;
       		    rect.iBr = leftLayout.Rect().iBr;
       		    }
       		else
       		    {
       		    rect.iTl = leftLayout.Rect().iTl;
       		    rect.iBr = rightLayout.Rect().iBr;
       		    }
       		    
       		cba->SetRect( rect );
            }
        }
    
    CAknQueryExtension* extension = QueryExtension();
    if( iHasEditor && extension && extension->EditorContext() )
        {
        // Layout editor context
        TAknLayoutRect topLeft;
        TAknLayoutRect bottomRight;
        
        //How to handle lanscape layout in full screen queries? 
        
        topLeft.LayoutRect(iEditorFrame.Rect(), SkinLayout::Input_field_skin_placing__general__Line_2());                
        bottomRight.LayoutRect(TRect(iEditorFrame.Rect().iBr, iEditorFrame.Rect().iBr), SkinLayout::Input_field_skin_placing__general__Line_5());

        TRect outerRect = TRect(topLeft.Rect().iTl, bottomRight.Rect().iBr);
        TRect innerRect = TRect(topLeft.Rect().iBr, bottomRight.Rect().iTl);
        

        extension->EditorContext()->SetFrameRects( outerRect, innerRect );
        // Chain with the background (since the frame doesn't occupy the entire
        // layout and it may even be transparent)
        extension->EditorContext()->SetParentContext( AknsDrawUtils::ControlContextOfParent( this ) );
        

#if defined( RD_SCALABLE_UI_V2)

		// Create backspace button if not created yet
		if ( AknLayoutUtils::PenEnabled() && EmbeddedVirtualInput() )
			{
			TRAP_IGNORE( CreateBackspaceButtonL() );
			}

#endif //if defined( RD_SCALABLE_UI_V2)
        }

    if (iImage)
        {
        TAknLayoutRect rect;
        rect.LayoutRect( Rect(), AKN_LAYOUT_WINDOW_Note_pop_up_window_elements_Line_1 );
        AknIconUtils::SetSize( CONST_CAST( CFbsBitmap*, iImage->Bitmap() ), rect.Rect().Size() );
        }            
    }

/**
 * Return the minimum size for the control. 
 *
 * This depends on the type of query and the number of prompt and/or
 * editor lines.
 *
 * Changed this to use only data-query layout
 */
EXPORT_C TSize CAknQueryControl::MinimumSize()
    {	
    DialogSizeChanged();
#ifndef RD_NO_DIALOG_BORDERS
	TRect parent = DialogRect();
	TAknLayoutRect layout;
	layout.LayoutRect(parent, AKN_LAYOUT_WINDOW_Data_query_pop_up_window_graphics_Line_5(parent));
	TRect rect(layout.Rect());
	return (TSize(rect.Size()));
#else
    return DialogRect().Size();
#endif
    }

/**
 * Return rect that is used by dialog to lay itself out
 * This is determined by LAF values only.
 */
TRect CAknQueryControl::DialogRect() const
    {
    return TRect(TPoint(0,0),DialogSize());
    }
    
/**
 * Return the size of the dialog 
 */
TSize CAknQueryControl::DialogSize() const
    {
    return iDialogSize;
    }
    
/**
 * Calculates the size of the dialog. This is determined by
 * LAF values only.
 */
void CAknQueryControl::DialogSizeChanged()
    {
    TAknWindowLineLayout lay;
    WindowLayout( lay );
	TAknLayoutRect layout;
	layout.LayoutRect(Rect(), lay);
	TRect rect(layout.Rect());	
	iDialogSize = TSize(rect.Size());
    }
    
/**
* Layout the query prompt. 
*
* There are two possible ways of laying
* out the text, either according to the general note LAF specs
* or according to the data query LAF specs
*
* Data queries layout the prompt according to data query LAF specs.
* Other queries layout the text according to the general note spces
*
* This method is called from SizeChanged, which determines the layout method
* to use (general note or data query) depending on the query type.
*/
void CAknQueryControl::LayoutPrompt(const TLayoutMethod& aLayoutM)
    {
    __ASSERT_DEBUG(iPrompt,Panic(EAknPanicNullPointer));
    
    if ( AknLayoutUtils::PenEnabled() && EmbeddedVirtualInput() )
    	{
    	LayoutPromptForFullScreen();
        return;     		
   		}
       
    TRect rect = LayoutRect();
    for (TInt i = 0; i < NbrOfPromptLines(); i++)
        {
        if (aLayoutM == EConfQueryLayoutM)
            {
            // AknLayoutScalable_Avkon::popup_note_window_t1 ? 
            AknLayoutUtils::LayoutLabel(iPrompt->Line(i), rect, AKN_LAYOUT_TEXT_Note_pop_up_window_texts_Line_1(i) );
            }
        else if (aLayoutM == ECodeQueryLayoutM || aLayoutM == ETimeQueryLayoutM || aLayoutM == EDateQueryLayoutM )
            {
            // AknLayoutScalable_Avkon::popup_query_code_window_t1 ? 
            AknLayoutUtils::LayoutLabel(iPrompt->Line(i), rect, AKN_LAYOUT_TEXT_Code_query_pop_up_window_texts_Line_1(i) );
            }
        else if (aLayoutM == EDataQueryLayoutM)
            {
            // AknLayoutScalable_Avkon::popup_query_code_window_t1 ?             
            AknLayoutUtils::LayoutLabel(iPrompt->Line(i), rect, AKN_LAYOUT_TEXT_Data_query_pop_up_window_texts_Line_1(i));
            }        
        else
            {
            __ASSERT_DEBUG(0,Panic(EAknPanicNotSupported));
            }
        }
    }

/**
 * Layout the image or the animation.
 *
 * Confirmation queries can have either an image or an animation and the
 * layout is similar to general notes.
 *
 * This method is called from SizeChanged.
 */
void CAknQueryControl::LayoutImageOrAnim(const TLayoutMethod& aLayoutM)
    {
    if (aLayoutM == EConfQueryLayoutM)
        {
        // AknLayoutScalable_Avkon::popup_note_window_g1() ? 
        if (iImage)
            AknLayoutUtils::LayoutImage(iImage, LayoutRect(), AKN_LAYOUT_WINDOW_Note_pop_up_window_elements_Line_1);
        
        if (iAnimation)
            AknLayoutUtils::LayoutControl(iAnimation, LayoutRect(), AKN_LAYOUT_WINDOW_Note_pop_up_window_elements_Line_1);
        }
    }

/**
 * Layout the editor and its frame.
 *
 * Queries other than confirmation queries have an editor that the user needs
 * to input the query data. This method does the layout of the editor. 
 *
 * Data queries have a resizable editor and its layout is specified in the LAF.
 * 
 * Other queries have a fixed size editor with layout specified.
 *
 *
 * This method is called from SizeChanged. Depending on the query type, the
 * layout method is chosen accordingly, e.g. data queries use the data query
 * method, other queires use the code query method.
 *
 * The macros at the top are just for avoiding code repetition.
 */
void CAknQueryControl::LayoutEditor(const TLayoutMethod& aLayoutM)
    {
    if ( AknLayoutUtils::PenEnabled() && EmbeddedVirtualInput() )
    	{
    	LayoutEditorForFullScreen( aLayoutM );
    	return;
    	}
    
    TIndex LAFIndex(NbrOfPromptLines());
    
    if (aLayoutM == EDataQueryLayoutM)
        {
        iHasEditor = ETrue;
        
        LayoutEditorFrame(aLayoutM);
        
        LayoutEditorIndicator(aLayoutM);

        TInt promptLines = NbrOfPromptLines();
        TInt numberTextLines = NbrOfEditorLines();

        TInt isNumeric = 0;
        CEikEdwin* editor = iEdwin;
        if (iNumberEdwin)
            {
            editor = iNumberEdwin;
            }
        else if (iFloatingPointEditor)
            {
            editor = iFloatingPointEditor;
            }
        
        if ( editor )
            {
            TRect parentRect = LayoutRect();
            if (promptLines > 1)
                {
                // There isn't way to give number of prompt lines to layout adaptation layer 
                // through Data_query_pop_up_window_texts_Line_2 layout API. Adaptation layer
                // does'n know which layout should be used. For that reason 2nd and 3rd prompt
                // line area is reduced from the parent rect and adaptation layer expects
                // that only one prompt line exists. 
                TAknLayoutText prompt1;
                prompt1.LayoutText(parentRect, AKN_LAYOUT_TEXT_Data_query_pop_up_window_texts_Line_1(0));
                
                TAknLayoutText prompt2;
                prompt2.LayoutText(parentRect, AKN_LAYOUT_TEXT_Data_query_pop_up_window_texts_Line_1(1));

                TInt parentOffset = prompt2.TextRect().iBr.iY - prompt1.TextRect().iBr.iY;
                parentRect.iTl.iY += parentOffset * (promptLines - 1);
                }
            AknLayoutUtils::LayoutEdwin( editor, parentRect,
                AKN_LAYOUT_MULTILINE_TEXT_Data_query_pop_up_window_texts_Line_2( isNumeric, numberTextLines ), EAknsCIQsnTextColorsCG27);
            
            //layout data not correct? if editor height is bigger than frame - reduce editor height by one step
            if (numberTextLines >= KMaxLinesInQuery && editor->Rect().iBr.iY > iEditorFrame.Rect().iBr.iY) 
                {
            	AknLayoutUtils::LayoutEdwin( editor, parentRect,
                    AKN_LAYOUT_MULTILINE_TEXT_Data_query_pop_up_window_texts_Line_2( isNumeric, numberTextLines-1 ), EAknsCIQsnTextColorsCG27);            	
                    
                //last line possibly w/ cursor might disappear - reveal it
                TRAP_IGNORE(editor->MoveDisplayL(TCursorPosition::EFLineDown));
                }

            editor->SetBorder(TGulBorder::ENone); 
            if (IsFocused() && !(editor->IsFocused()) ) 
                editor->SetFocus(ETrue);
            }
		
        AknsUtils::RegisterControlPosition(editor);
        }
    else if (aLayoutM == ECodeQueryLayoutM || aLayoutM == EDateQueryLayoutM || aLayoutM == ETimeQueryLayoutM)
        {
        iHasEditor = ETrue;

        LayoutEditorFrame(aLayoutM);
        
        LayoutEditorIndicator(aLayoutM);

        if (iSecretEd)
            {
            AknLayoutUtils::LayoutSecretEditor(iSecretEd, LayoutRect(), 
            AKN_LAYOUT_TEXT_Code_query_pop_up_window_texts_Line_2(LAFIndex.PromptLine()));
            iSecretEd->SetBorder(TGulBorder::ENone);
            if (IsFocused() && !iSecretEd->IsFocused())
                iSecretEd->SetFocus(ETrue);
            }    
        if (iPinEdwin)
            {
            AknLayoutUtils::LayoutSecretEditor(iPinEdwin, LayoutRect(), 
            AKN_LAYOUT_TEXT_Code_query_pop_up_window_texts_Line_2(LAFIndex.PromptLine()));
            iPinEdwin->SetBorder(TGulBorder::ENone);
            if (IsFocused() && !iPinEdwin->IsFocused())
                iPinEdwin->SetFocus(ETrue);
            }

//LAYOUT for TIME, DATE and DURATION EDITORS 
        CEikMfne* edwin = NULL;
        
        if ( iTimeEdwin )
            {
            edwin = iTimeEdwin;
            }        
        else if ( iDateEdwin )
            {
            edwin = iDateEdwin;
            }
        else if ( iDurationEdwin )
            {
            edwin = iDurationEdwin;
            }
        else if ( LocationEd() )
            {
            edwin = LocationEd();
            }
            
        if ( edwin )
            {
            TInt variety( LAFIndex.PromptLine() );
        
            AknLayoutUtils::LayoutMfne( edwin, LayoutRect(), 
                TAknWindowComponentLayout::ComposeText( 
                    AknLayoutScalable_Avkon::query_popup_pane( variety ),
                    AknLayoutScalable_Avkon::query_popup_pane_t1() ) );

            edwin->SetBorder( TGulBorder::ENone );
            if ( IsFocused() && !edwin->IsFocused() )
                {
                edwin->SetFocus( ETrue );
                }
            edwin->SetUpAndDownKeysConsumed( ETrue );
            }     
        }
    }

/**
 * Layout the editor frame.
 *
 * For queries that do have an editor, this method will position its frame.
 *
 * This method is called by LayoutEditor only if the layout
 * method implies the presence of an editor, hence the panic if
 * the layout is not recognised.
 *
 */
void CAknQueryControl::LayoutEditorFrame(const TLayoutMethod& aLayoutM)
    {
    TIndex LAFIndex(NbrOfPromptLines());

    TRect rect   = LayoutRect();
    TRect parent = DialogRect();
     
    if (aLayoutM == EDataQueryLayoutM)
        {
        iEditorVerShadow.LayoutRect( rect, AKN_LAYOUT_WINDOW_Data_query_pop_up_window_elements_Line_1(LAFIndex.PromptLine()));
        iEditorHorShadow.LayoutRect( rect, AKN_LAYOUT_WINDOW_Data_query_pop_up_window_elements_Line_2(parent,LAFIndex.PromptLine()));
        iEditorFrame.LayoutRect    ( rect, AKN_LAYOUT_WINDOW_Data_query_pop_up_window_elements_Line_3(parent,LAFIndex.PromptLine()));
        }
    else if (aLayoutM == ECodeQueryLayoutM || aLayoutM == EDateQueryLayoutM || aLayoutM == ETimeQueryLayoutM)
        {
        TInt variety( LAFIndex.PromptLine() );
        iEditorVerShadow.LayoutRect( rect, AKN_LAYOUT_WINDOW_Code_query_pop_up_window_elements_Line_1( variety ) );
        iEditorHorShadow.LayoutRect( rect, AKN_LAYOUT_WINDOW_Code_query_pop_up_window_elements_Line_2( variety ) );
               
        iEditorFrame.LayoutRect( rect, AknLayoutScalable_Avkon::query_popup_pane( variety ) );
        }
    else 
        __ASSERT_DEBUG(0,Panic(EAknPanicNotSupported));
    }

/**
 * Layout editor indicator.
 *
 * For queries that do have an editor, this method does
 * layout for its editing state icons.
 *
 * If these icons are not to be displayed then this method
 * returns without doing anything. 
 *
 * This method is called by LayoutEditor only if the layout
 * method implies the presence of an editor, hence the panic if
 * the layout is not recognised.
 */
void CAknQueryControl::LayoutEditorIndicator(const TLayoutMethod& aLayoutM)
    {
    TIndex LAFIndex(NbrOfPromptLines());

    if (!iEditIndicator)
        return;

    if (aLayoutM == EDataQueryLayoutM)
        {
        AknLayoutUtils::LayoutControl(iEditIndicator, LayoutRect(), 
            AKN_LAYOUT_WINDOW_Data_query_pop_up_window_elements_Line_4(LAFIndex.PromptLine()));
        }
    else if (aLayoutM == ECodeQueryLayoutM || aLayoutM == EDateQueryLayoutM || aLayoutM == ETimeQueryLayoutM)
        {
        TInt variety( LAFIndex.PromptLine() );
        
        AknLayoutUtils::LayoutControl(iEditIndicator, LayoutRect(), 
            AknLayoutScalable_Avkon::indicator_popup_pane( variety ) );
        }
    else
        __ASSERT_DEBUG(0,Panic(EAknPanicNotSupported));
    }

//
// Extension
//

CAknQueryExtension* CAknQueryControl::QueryExtension() const
    {
    return CAknQueryExtension::Instance( this );
    }

//
// Prompt related methods
//

/**
 * Read prompt text from resouce
 */
EXPORT_C void CAknQueryControl::ReadPromptL(TResourceReader &aRes)
    {
    delete iPrompt;
    iPrompt = NULL;

    iPrompt = CAknTextControl::NewL(CAknText::EFormatted);
    iPrompt->ConstructFromResourceL(aRes);
    iPrompt->SetContainerWindowL(*this);

    TCallBack cb( StaticPictographCallBack, this );
    iPrompt->SetPictographCallBackL( cb );

    DoSetPromptL();
    }

/**
 * Set new text into prompt and parse the text
 */
EXPORT_C void CAknQueryControl::SetPromptL(const TDesC& aDesC)
    {
    __ASSERT_DEBUG(iPrompt,Panic(EAknPanicNullPointer));
    iPrompt->SetTextL(aDesC);
    DoSetPromptL();
    }

/**
 * Parse the text in the prompt
 *
 * Line widths of prompt depend on layout information and on 
 * query type
 */
void CAknQueryControl::DoSetPromptL()
    {
     __ASSERT_DEBUG(iPrompt,Panic(EAknPanicNullPointer));     

    TAknLayoutText textRect;

    TAknWindowLineLayout dialogLay;
    WindowLayout( dialogLay );

    TAknLayoutRect dialogLayRect;
    dialogLayRect.LayoutRect(iAvkonAppUi->ApplicationRect(), dialogLay);
    TSize dialogSize = dialogLayRect.Rect().Size(); 
    TRect parent(TPoint(0,0),dialogSize);
    
    if (iQueryType == EConfirmationQueryLayout)
        {
        textRect.LayoutText(parent, AKN_LAYOUT_TEXT_Note_pop_up_window_texts_Line_1(1));
        }
    else        
        {
        textRect.LayoutText(parent, AKN_LAYOUT_TEXT_Data_query_pop_up_window_texts_Line_1(1));
        }
           
    SetLineWidthsL();            
    const CFont *font = textRect.Font();
    iPrompt->ParseTextL(font,iLineWidths);
    
    // Next code block inserts line feeds into the given prompt text so that 
    // it always consumes the same amount of vertical space. Prompt should
    // always be placed just above the editor i.e. empty lines are added before
    // the prompt if needed.
    //
    
    if ( EmbeddedVirtualInput() )
        {
        // Set line widths for prompt text using Query Input layout
        
  		TBool landscape = Layout_Meta_Data::IsLandscapeOrientation();
    
    	TAknLayoutRect rect;
 		rect.LayoutRect( LayoutRect(), TAknWindowComponentLayout::Compose(
 			AknLayoutScalable_Apps::popup_vitu2_window( landscape ? 1 : 0 ), 
    		AknLayoutScalable_Apps::area_vitu2_query_pane( landscape ? 1 : 0 ) ).LayoutLine() );
    				
    	TAknLayoutText layoutText;
    	layoutText.LayoutText( rect.Rect(),
    		AknLayoutScalable_Apps::area_vitu2_query_pane_t4( 1 ) );
                
        font = layoutText.Font();
    	
    	iLineWidths->Reset();
        for (TInt i = 0; i < 3; i++)
        	{
        	iLineWidths->AppendL(layoutText.TextRect().Width());
        	}
        	
        // text parsing is needed to get correct return value
        // from function NbrOfPromptLines 	
        iPrompt->ParseTextL( font, iLineWidths );
        	
        
        // calculate needed amount of empty lines
        TInt lineFeeds = 3 - NbrOfPromptLines();
        
        // add empty lines if needed
        if ( lineFeeds > 0)
        	{
        	HBufC* oldText = iPrompt->Text().AllocLC();
        	HBufC* newText = oldText->ReAllocL( oldText->Length() + lineFeeds );
        
        	CleanupStack::Pop(); // oldText
        	CleanupStack::PushL( newText );
        
        	// add empty lines
        	for ( TInt i = 0; i < lineFeeds; ++i )
            	{
            	newText->Des().Insert( 0, _L( "\n" ) );
            	}
        	iPrompt->SetTextL( *newText );
        
        	// text parsing is needed because empty lines are added
        	iPrompt->ParseTextL( font, iLineWidths );

        	CleanupStack::PopAndDestroy(); // newText
        	}
        }
    
    Layout();
    }

void CAknQueryControl::SetLineWidthsL() 
    {
    if (!iLineWidths)
        return;
    
    iLineWidths->Reset();

    switch(KQueryTypeToLayout[iQueryType])
        {
        case EConfQueryLayoutM: 
            {
            TAknLayoutRect parentLayoutRect;
            parentLayoutRect.LayoutRect(iAvkonAppUi->ClientRect(), AknLayoutScalable_Avkon::popup_note_window(0));
            TAknLayoutText textRect;
            for (TInt i = 0; i < 5; i++)
                {
                textRect.LayoutText(parentLayoutRect.Rect(), AKN_LAYOUT_TEXT_Note_pop_up_window_texts_Line_1(i));
                iLineWidths->AppendL(textRect.TextRect().Width());
                }
            break;
            }
        case EDataQueryLayoutM:
            {
            TAknLayoutRect parentLayoutRect;
            parentLayoutRect.LayoutRect(iAvkonAppUi->ClientRect(), AknLayoutScalable_Avkon::popup_query_data_window(0));
            TAknLayoutText textRect;
            for (TInt i = 0; i < 3; i++)
                {
                textRect.LayoutText(parentLayoutRect.Rect(), AKN_LAYOUT_TEXT_Data_query_pop_up_window_texts_Line_1(i));
                iLineWidths->AppendL(textRect.TextRect().Width());
                }
            break;
            }
        case ECodeQueryLayoutM:
        case ETimeQueryLayoutM:
        case EDateQueryLayoutM:
            {
            TAknLayoutRect parentLayoutRect;
            parentLayoutRect.LayoutRect(iAvkonAppUi->ClientRect(), AknLayoutScalable_Avkon::popup_query_code_window(0));
            TAknLayoutText textRect;
            for (TInt i = 0; i < 3; i++)
                {                
                textRect.LayoutText(parentLayoutRect.Rect(), AKN_LAYOUT_TEXT_Code_query_pop_up_window_texts_Line_1(i));
                iLineWidths->AppendL(textRect.TextRect().Width());
                }
            break;
            }
        default:
            __ASSERT_DEBUG(0,Panic(EAknPanicNotSupported));
        }
    }


///
/// Editor text related methos
///

/**
 *Get text from editors
 */
EXPORT_C void CAknQueryControl::GetText(TDes &aDes) const 
    {
    if(iEdwin)
        {
        iEdwin->GetText(aDes);
        }
    else if(iSecretEd)
        {
        iSecretEd->GetText(aDes);
        }
    else if(iPinEdwin)
        {
        iPinEdwin->GetText(aDes);
        }
    else if(iNumberEdwin)
        {
        iNumberEdwin->GetText(aDes);
        }
    else if (iFloatingPointEditor)
        {
        iFloatingPointEditor->GetText(aDes);
        }
    }

/**
 *Set default text in editors
 */
EXPORT_C void CAknQueryControl::SetTextL(const TDesC &aDesC)
    {
    if( iEdwin )
        {
        __ASSERT_DEBUG
            (
            iEdwin->MaxLength()>=aDesC.Length(), 
            Panic(EAknPanicOverflow)
            );
        iEdwin->SetTextL(&aDesC);
        }
    else if( iSecretEd )
        {
        __ASSERT_DEBUG
            (
            iSecretEd->MaxLength()>=aDesC.Length(), 
            Panic(EAknPanicOverflow)
            );
        iSecretEd->SetText(aDesC);
        }
    else if( iPinEdwin )
        {
        __ASSERT_DEBUG
            ( // secret editor derived
            iPinEdwin->MaxLength()>=aDesC.Length(), 
            Panic(EAknPanicOverflow)
            );
        iPinEdwin->SetText(aDesC);
        }
    else if( iNumberEdwin )
        {
        __ASSERT_DEBUG
            (
            iNumberEdwin->MaxLength()>=aDesC.Length(), 
            Panic(EAknPanicOverflow)
            );

        iNumberEdwin->SetTextL(&aDesC);
        }
    else if (iFloatingPointEditor)
        {
        __ASSERT_DEBUG
            (
            iFloatingPointEditor->MaxLength()>=aDesC.Length(), 
            Panic(EAknPanicOverflow)
            );

        iFloatingPointEditor->SetTextL(&aDesC);
        }
    }

/**
 *Set max. length in text based editors.
 */
EXPORT_C void CAknQueryControl::SetTextEntryLength(TInt aLength)
    {
    if( iEdwin )
        {
        iEdwin->SetMaxLength(aLength);
        }
    else if( iSecretEd )
        {
        iSecretEd->SetMaxLength(aLength);
        }
    else if( iPinEdwin )
        {
        iPinEdwin->SetMaxLength(aLength);
        }
    else if( iNumberEdwin )
        {
        iNumberEdwin->SetMaxLength(aLength);
        }
    else if ( iFloatingPointEditor )
        {
        iFloatingPointEditor->SetMaxLength(aLength);
        }
    }

/**
 * Get maximum text length in text based editors
 */
EXPORT_C TInt CAknQueryControl::GetTextEntryLength() const
    {
    if( iEdwin )
        {
        return iEdwin->MaxLength();
        }
    if(iSecretEd)
        {
        return iSecretEd->MaxLength();
        }
    if(iPinEdwin)
        {
        return iPinEdwin->MaxLength();
        }
    if(iNumberEdwin)
        {
        return iNumberEdwin->MaxLength();
        }
    if (iFloatingPointEditor)
        {
        return iFloatingPointEditor->MaxLength();
        }
    return -1;
    }

///
/// API Set methods
///

EXPORT_C void CAknQueryControl::SetMinimumAndMaximum(const TTime& aMinimum, const TTime& aMaximum)
    {
    if( iDateEdwin )
        {
        iDateEdwin->SetMinimumAndMaximum(aMinimum, aMaximum);
        }
    else if( iTimeEdwin )
        {
        iTimeEdwin->SetMinimumAndMaximum(aMinimum, aMaximum);
        }
    }

EXPORT_C void CAknQueryControl::SetMinimumAndMaximum(const TTimeIntervalSeconds& aMinimumDuration, const TTimeIntervalSeconds& aMaximumDuration)
    {
    if( iDurationEdwin )
        {
        iDurationEdwin->SetMinimumAndMaximum(aMinimumDuration, aMaximumDuration);
        }
    }

EXPORT_C void CAknQueryControl::SetMinimumAndMaximum(TInt aMinimumValue, TInt aMaximumValue)    
    {
    if( iNumberEdwin )
        {
        iNumberEdwin->SetMinimumIntegerValue(aMinimumValue);
        iNumberEdwin->SetMaximumIntegerValue(aMaximumValue);
        }
    }

EXPORT_C void CAknQueryControl::SetMinimumAndMaximum(const TReal& aMinimumValue, const TReal& aMaximumValue)    
    {
    if( iFloatingPointEditor )
        {
        iFloatingPointEditor->SetMinMax(aMinimumValue, aMaximumValue);
        }
    }



EXPORT_C TBool CAknQueryControl::CheckNumber()    
    {    
    return iNumberEdwin->CheckNumber();
    }

EXPORT_C void CAknQueryControl::SetDuration(TTimeIntervalSeconds& aDuration)
//
//Set duration to editor
//
{
    if( iDurationEdwin )
        {
        iDurationEdwin->SetDuration(aDuration);
        }
}

EXPORT_C void CAknQueryControl::SetNumberL(TInt aNumber)
//
//Set number from editor
//
{
    if( iNumberEdwin )
        {
        iNumberEdwin->SetValueL(aNumber);
        }
}

EXPORT_C void CAknQueryControl::SetTime(TTime& aTime)
//
//Set time or date to editor
//
{
    if( iTimeEdwin )
        {
        iTimeEdwin->SetTime(aTime);
        }
    else if( iDateEdwin )
        {
        iDateEdwin->SetDate(aTime);
        }
}

EXPORT_C void CAknQueryControl::SetFloatingPointNumberL(const TReal* aNumber)
//
//Set number from editor
//
{
    if( iFloatingPointEditor )
        {
        iFloatingPointEditor->SetValueL( aNumber );
        }
}

EXPORT_C void CAknQueryControl::SetLocation(const TPosition &aLocation)
{
	if (iExtension && LocationEd())
		{
		LocationEd()->Set( aLocation );
		if ( AknLayoutUtils::PenEnabled() )
            {
            LocationEd()->SetFeature( CEikMfne::EFingerSupport, CEikMfne::EnableWithAllHighlight );
	        }
		}
}

EXPORT_C void CAknQueryControl::SetNumberOfEditorLines(TInt aNum)
    {
    iNumberOfEditorLines = aNum;
    }

///
/// API Get methods
///

EXPORT_C TInt CAknQueryControl::GetTextLength() const
//
//Get text length
//
{
    if ( iSecretEd )
        {
        return iSecretEd->Buffer().Length();
        }
    if ( iPinEdwin )
        {
        return iPinEdwin->Buffer().Length();
        }
    if( iEdwin )
        {
        return iEdwin->TextLength();
        }
    if( iNumberEdwin )
        {
        return iNumberEdwin->TextLength();
        }
    if (iFloatingPointEditor)
        {
        return iFloatingPointEditor->TextLength();
        }
      
    return -1;
}

EXPORT_C TTime CAknQueryControl::GetTime() const
//
//Get time or date from editor
//
{
    TTime time(0);

    if( iTimeEdwin )
        {
        return iTimeEdwin->Time();
        }

    if( iDateEdwin )
        {
        return iDateEdwin->Date();
        }

    return time;
}

EXPORT_C TReal CAknQueryControl::GetFloatingPointNumberL() const
//
//Set number from editor
//
{
    TReal ret = 0;
    if( iFloatingPointEditor )
        {
        iFloatingPointEditor->GetValueAsReal( ret );
        }
    return ret;
}

EXPORT_C void CAknQueryControl::GetLocation(TPosition &aLocation) const
{
    if (LocationEd())
    	{
    	LocationEd()->Get(aLocation);
    	}
}


EXPORT_C TInt CAknQueryControl::GetNumber() const
{
    TInt value = 0;
    if(iNumberEdwin)
        {
        iNumberEdwin->GetTextAsInteger( value );
        }
    else
        {
        ASSERT(EFalse);
        }
    return value;
}

EXPORT_C TTimeIntervalSeconds CAknQueryControl::GetDuration() const
//
//Get duration from editor
//
{
    if( iDurationEdwin )
        {
        return iDurationEdwin->Duration();
        }
    return NULL;
}

/**
 * Return number of lines in the editor
 * 
 * For data queries this can be 1 to 5 lines except if there are 3 lines
 * of prompt text in which case only 1 line is permitted.
 *
 * For all other queries there is only 1 line of text.
 */
EXPORT_C TInt CAknQueryControl::NbrOfEditorLines() const
    {
    TInt nbrOfLines = 0;
    if(iEdwin &&  KQueryTypeToLayout[iQueryType] == EDataQueryLayoutM)
        {
        if (iNumberOfEditorLines == -1)
            {
            TInt nL = Min(iEdwin->TextLayout()->NumFormattedLines(),KMaxLinesInQuery);
            const TInt mHIL= Min(iEdwin->MaximumHeightInLines(),KMaxLinesInQuery);
            
            if ( mHIL )
                {
                nbrOfLines = Min(nL,mHIL);
                }
            else
                {
                nbrOfLines = nL;
                }
            }
    
        else if (iNumberOfEditorLines >= 0 && iNumberOfEditorLines <= KMaxLinesInQuery)
            {
            nbrOfLines = iNumberOfEditorLines;
            }
        }

    return Max(1,nbrOfLines);
    }

/**
 * Return number of prompt lines
 *
 * Only confirmation queries can have more than 2 lines
 */
EXPORT_C TInt CAknQueryControl::NbrOfPromptLines() const
    {
    if (iPrompt)
        return iPrompt->NumberOfLines();
    return 1;
    }


/**
 * Return text editor depening on query type
 */
EXPORT_C CCoeControl* CAknQueryControl::ControlByLayoutOrNull(TInt aLayout)
    {
    CCoeControl* ctrl=NULL;
    switch ( aLayout )
        {
        case EStaticSizeDataLayout:
        case EDataLayout:
        case EPhoneLayout:
            ctrl=iEdwin;
            break;
        case ECodeLayout:
            ctrl=iSecretEd;
            break;
        case EDateLayout:
            ctrl=iDateEdwin;
            break;
        case ETimeLayout:
            ctrl=iTimeEdwin;
            break;
        case EDurationLayout:
            ctrl=iDurationEdwin;
            break;
        case ENumberLayout:
            ctrl=iNumberEdwin;
            break;
        case EPinLayout:
            ctrl=iPinEdwin;
            break;
        case EFloatingPointLayout:
            ctrl=iFloatingPointEditor;
			break;
		case ELocationLayout:
			ctrl=LocationEd();
			break;
        default:
            break;
        }
    return ctrl;
    }

/**
*
* Sends and ESC key to the app.  This cannot be called from within OfferKeyEventL.  It must be called
* from within some other non-key event that subsequently does not require references to the object
*
*/
void CAknQueryControl::CancelQueryL()
    {
    if (iQueryControlObserver)
        {
        iQueryControlObserver->HandleQueryEditorStateEventL(this, MAknQueryControlObserver::EEmergencyCallAttempted, MAknQueryControlObserver::EEditorValueValid);
        }
    else 
        {
        TKeyEvent key;
        key.iCode=EKeyEscape;
        key.iModifiers=0;
        key.iScanCode = EStdKeyNull;
        iEikonEnv->SimulateKeyEventL(key, EEventKey);
        }
    }

/**
* This method looks at the flags that have been set in the query control and 
* acts upon them if possible
* 
*/
void CAknQueryControl::DeployFlagsL()
    {
    // Handle the Emergency call support flags
    if (!iEcsDetector )
        {
        if ( 
            (iNumberEdwin && iFlags.IsSet( EEmergencyCallsEnabledByAPI) )
            ||
            ( iPinEdwin && iFlags.IsSet( EEmergencyCallsEnabledByAPI) )
            )
            {
            iEcsDetector = CAknEcsDetector::NewL();
            // Set the control to be the CAknEcsDetector observer, if the object exists
            iEcsDetector->SetObserver( iEcsObserver );
            }

        }

    // Handle the T9 suppression in edwins. If the flag has not been set, then
    // suppress the T9
    if ( iFlags.IsClear ( EPredictiveTextEntryPermitted ) )
        {
        if ( iEdwin )
            {
            // set the noT9 flag
            iEdwin->SetAknEditorFlags( EAknEditorFlagNoT9 | iEdwin->AknEdwinFlags() );
            }
        }       

    }

/**
* Method to first set the flags and then to use them on the contained editors
*
*/

void CAknQueryControl::SetAndUseFlagsL(TBitFlags16 aFlags)
    {
    SetFlags( aFlags );
    DeployFlagsL();
    }

void CAknQueryControl::SetFlags(TBitFlags16 aFlags)
    {
    iFlags = aFlags;
    iEditIndicator->SetFlags( aFlags );
    }

EXPORT_C void CAknQueryControl::SetImageL(CEikImage* aImage)
    { // Gets ownership of the aImage
    
	if ( iImage && iImage->IsPictureOwnedExternally() )
        {
        delete ((CApaMaskedBitmap*) iImage->Bitmap());
        }
    
	delete iImage;
    iImage = aImage;

    if ( iImage )
        {
        iImage->SetContainerWindowL(*this);
        }

    delete iAnimation;
    iAnimation = NULL;

    Layout();
    }

EXPORT_C void CAknQueryControl::SetImageL(const TDesC& aImageFile,
                                          TInt aBmpId, TInt aBmpMaskId)
    {
    CEikImage* image = new(ELeave) CEikImage;
    CleanupStack::PushL( image );
               
    TBool skinnedFound( EFalse );
    if ( aImageFile.CompareF(KAvkonBitmapFile) == 0 ) // Skinning is supported only avkon bitmaps
	    {
        TAknsItemID id;
        id.Set(0,0);
        id = CAknNoteAttributes::GetSkinnedBitmapID(aBmpId);
        if ( id.iMajor && id.iMinor ) // skin id found, try loading
	        {
            CApaMaskedBitmap* bitmap = NULL;
            MAknsSkinInstance* skin = AknsUtils::SkinInstance();
            TRAPD( err, bitmap = AknsUtils::CreateMaskedBitmapL(skin,id));
            if ( !err ) 
	           {
               CFbsBitmap* mask = bitmap->Mask();
               iImage->SetPicture(bitmap,mask);
               iImage->SetPictureOwnedExternally(ETrue);
               skinnedFound = ETrue;
               }
			}
        }
    if ( !skinnedFound) 
		{
        image->CreatePictureFromFileL(aImageFile,aBmpId,aBmpMaskId);
        }

    CleanupStack::Pop(); //image
    SetImageL(image); // gets ownership
    }

EXPORT_C void CAknQueryControl::SetAnimationL(TInt aResource)
    {
    delete iAnimation;
    iAnimation = NULL;
    iAnimation = CAknBitmapAnimation::NewL();
    iAnimation->SetContainerWindowL(*this);

    TAknsItemID iid;
    iid.Set( EAknsMajorAvkon, aResource );
    if( !iAnimation->ConstructFromSkinL( iid ) )
        {
        TResourceReader readerForAnimation;
        iCoeEnv->CreateResourceReaderLC(readerForAnimation,aResource);
        iAnimation->ConstructFromResourceL(readerForAnimation);
        CleanupStack::PopAndDestroy(); // readerForAnimation
        }

    // Save the animation resource id, because this function will be re-called 
    // when handling resource change. 
    iExtension->iAnimationId = aResource;
    
    Layout();
    }

EXPORT_C void CAknQueryControl::StartAnimationL()
    {
    if (iAnimation)
        {
        CBitmapAnimClientData *animClientData = iAnimation->BitmapAnimData();
        if (animClientData != NULL && animClientData->FrameArray().Count() <= 1)
            {
            // Not set background frame, if there is only one frame in animation.
            }
        else
            {
            const TDisplayMode displayMode( 
            CCoeEnv::Static()->ScreenDevice()->DisplayMode() );
            
            CFbsBitmap* bitmap = new(ELeave) CFbsBitmap;
            CleanupStack::PushL(bitmap);
            
            User::LeaveIfError( bitmap->Create( iAnimation->Rect().Size(), displayMode ) );
            
            CFbsBitmapDevice* doubleBufferDev = CFbsBitmapDevice::NewL( bitmap );
            CleanupStack::PushL(doubleBufferDev);    
            
            CFbsBitGc* doubleBufferGc = 0;
            User::LeaveIfError( doubleBufferDev->CreateContext( doubleBufferGc ) );    
            CleanupStack::PushL(doubleBufferGc);    
            
            MAknsSkinInstance* skin = AknsUtils::SkinInstance();
            MAknsControlContext* cc = AknsDrawUtils::ControlContext( this );
            
            AknsDrawUtils::DrawBackground( skin, cc, this, *doubleBufferGc,
                TPoint(0,0), iAnimation->Rect(), KAknsDrawParamDefault );
            
            CleanupStack::PopAndDestroy(2); // doubleBufferGc, doubleBufferDev
            
            CBitmapFrameData* data = CBitmapFrameData::NewL();
            data->SetBitmapsOwnedExternally( EFalse );
            data->SetBitmap( bitmap );
            
            // Set frame to animation
            iAnimation->BitmapAnimData()->SetBackgroundFrame( data ); // gets ownership
            
            CleanupStack::Pop(bitmap); 
            }

        iAnimation->StartAnimationL();
        }
    }

EXPORT_C TInt CAknQueryControl::CancelAnimation()
    {
    if ( iAnimation )
        {
        return iAnimation->CancelAnimation();
        }
    return KErrGeneral;
    }



/** 
 * Return true if editor content is valid for query, false 
 * otherwise. Some editors always have a valid content. Others
 * (those that have a text associated with them) may or may
 * not have a valid content.
 */ 
TBool CAknQueryControl::EditorContentIsValidL() const
    {
    TInt   ret  = EFalse;
    if (iDateEdwin || iTimeEdwin || iDurationEdwin ||LocationEd()) 
        {
        CEikMfne* editor = iDateEdwin;
        if (!editor && iTimeEdwin)
            editor = iTimeEdwin;
        if (!editor && iDurationEdwin)
            editor = iDurationEdwin;
        if (!editor && LocationEd())
        	editor = static_cast<CEikMfne*>(LocationEd());
        ret = editor->Field(editor->CurrentField())->IsValid();
        }
    else if (iNumberEdwin || iFloatingPointEditor)
        {
        TInt intValue;
        TReal realValue;

        CAknNumericEdwin::TValidationStatus status = iNumberEdwin ?
            iNumberEdwin->GetTextAsInteger(intValue) : iFloatingPointEditor->GetValueAsReal(realValue);

        // At this point too large or small values don't affect the valicy of content,
        // range is checked when OkToExitL-method is called when user tries to confirm the query 
        if (iNumberEdwin && (status == CAknNumericEdwin::EValueTooSmall || 
            status == CAknNumericEdwin::EValueTooLarge ) ) 
            {
            status = CAknNumericEdwin::EValueValid;
	        }
        ret = status == CAknNumericEdwin::EValueValid;
        }
    else
        {//Remaining editors have a non-empty text with at least one char
        // which is not a space and is not a control char
        CAknQueryExtension* extension = QueryExtension();
        if( extension )
            {
            if ( extension->iAllowEmptyInput )
                {
                return ETrue;
                }
            }
        
        HBufC* buf = HBufC::NewLC(GetTextEntryLength()); //max length
        TPtr ptr = buf->Des();
        if (GetTextEntryLength()) //check if zero is set as maxlength
            {
                GetText(ptr);
            }
        ret = AknTextUtils::IsEmptyText( ptr );
        
        if ( !ret && ptr.Length() > 0 && iSecretEd )            
            {
            TBool isSpace = ETrue;
            for ( TInt i = 0; i < ptr.Length(); i++ )            
                {
                TChar c( ptr[i] );
                if ( !c.IsSpace() )
                    {
                    isSpace = EFalse;
                    break;
                    }
                }
            if ( isSpace )
                {
                ret = ETrue;                
                }
            }        

		// Check if pictographs in buffer	
		if (!ret && iEdwin && iPrompt && ptr.Length())
			{
			CAknPictographInterface* picto = iPrompt->PictographInterface();

			if ( picto && picto->Interface() && picto->Interface()->ContainsPictographs( ptr ) )
				{
				ret = ETrue;
				}
			}		
		CleanupStack::PopAndDestroy(); // buf    
        }
    return ret; 
    }

/*****************************************************************
 * CAknQueryControl::TIndex
 *
 * Manage indexes into LAF tables
 *
 * PN stands for "Popup Note"
 *
 * PQD stands for "Popup Query Data'
 *
 * PQC stands for "Popup Query Code'
 *
 * 'DPQ' stands for "Data Query Popup"
 ******************************************************************/

  
CAknQueryControl::TIndex::TIndex(TInt aNumberOfPromptLines)
    : iNumberOfPromptLines(aNumberOfPromptLines)
    {
    }
        
void CAknQueryControl::TIndex::SelfTest() const
    {
    __ASSERT_DEBUG(iNumberOfPromptLines >= 0  && iNumberOfPromptLines <= 5,
        Panic(EAknPanicInvalidValue));
    }

/**
 * Return index into LAF tables that depend directly on number of prompt lines,
 * index is equal to number of prompt lines minus one unless there are no prompt
 * lines in which case the index is zero
 */
TInt CAknQueryControl::TIndex::PromptLine() const
    {
    SelfTest();
    return iNumberOfPromptLines > 0 ? iNumberOfPromptLines - 1 : 0;
    }

/**
 * Return index into Main Pane PQDC Window, which
 * depends on total number of prompt lines minus 2, e.g.
 * index 0 for 2 prompt lines total.
 *
 */
TInt CAknQueryControl::TIndex::DQPWindowTextsLine2(TInt aLineNum) const
    {
    SelfTest();
    __ASSERT_DEBUG(aLineNum > 0 && aLineNum <= 5,Panic(EAknPanicInvalidValue));

    static const TInt  KDataQueryPopupWindowTextsLine2Index[3][5] = 
    {   {0,  1, 2, 3, 4},    //1 PROMPT LINE
        {5,  6, 7, 8, 9},    //2 PROMPT LINES
        {10, 10, 10, 10, 10}  }; //3 PROMPT LINES
    
    return KDataQueryPopupWindowTextsLine2Index[PromptLine()][aLineNum-1];
    }

TInt CAknQueryControl::TIndex::PNWindow() const
    {
    SelfTest();
 
    static const TInt KPopupNoteWindowIndex[5] = { 0,0,0,1,2 };
    
    AknLayoutUtils::TAknCbaLocation cbaLocation = AknLayoutUtils::CbaLocation();
    if (cbaLocation == AknLayoutUtils::EAknCbaLocationRight)
        {
        // variety numbers for right CBA are 3,4 and 5 
        return (KPopupNoteWindowIndex[PromptLine()] + 3);  
        }
    else if (cbaLocation == AknLayoutUtils::EAknCbaLocationLeft)
        {
        /// variety numbers for left CBA are 6,7 and 8 
        return (KPopupNoteWindowIndex[PromptLine()] + 6);
        }
    else // bottom
        {
        return KPopupNoteWindowIndex[PromptLine()];
        }
    }

TInt CAknQueryControl::TIndex::PQDWindow(TInt aLineNum) const
    {
    SelfTest();
    __ASSERT_DEBUG(aLineNum > 0 && aLineNum <= 5,Panic(EAknPanicInvalidValue));

    AknLayoutUtils::TAknCbaLocation cbaLocation = AknLayoutUtils::CbaLocation();
    
    if (cbaLocation == AknLayoutUtils::EAknCbaLocationRight)
        {
        static const TInt KPopupQueryDataWindowIndex[7] = 
            {13, 12, 11, 10, 9, 8, 7}; // variety numbers for right CBA are 7-13 
        return KPopupQueryDataWindowIndex[PromptLine()+aLineNum-1];
        }
    else if (cbaLocation == AknLayoutUtils::EAknCbaLocationLeft)
        {
        static const TInt KPopupQueryDataWindowIndex[7] = 
            {20, 19, 18, 17, 16, 15, 14}; // variety numbers for left CBA are 14-20 
        return KPopupQueryDataWindowIndex[PromptLine()+aLineNum-1];
        }
    else // bottom
        {
        static const TInt KPopupQueryDataWindowIndex[7] = 
            {0, 1, 2, 3, 4, 5, 6}; // variety numbers for bottom CBA are 0-6
        return KPopupQueryDataWindowIndex[PromptLine()+aLineNum-1];
        }
    }
    
TInt CAknQueryControl::TIndex::PQCWindow() const
    {
    SelfTest();
    
    AknLayoutUtils::TAknCbaLocation cbaLocation = AknLayoutUtils::CbaLocation();
    
    if (cbaLocation == AknLayoutUtils::EAknCbaLocationRight)
        {
        // variety numbers for right CBA are 3,4 and 5 
        return iNumberOfPromptLines > 0 ? (iNumberOfPromptLines - 1 + 3) : 3;
        }
    else if (cbaLocation == AknLayoutUtils::EAknCbaLocationLeft)
        {
        // variety numbers for left CBA are 6,7 and 8 
        return iNumberOfPromptLines > 0 ? (iNumberOfPromptLines - 1 + 6) : 6;
        }
    else // bottom
        {
        // variety numbers for bottom CBA are 0,1 and 2 
        return iNumberOfPromptLines > 0 ? (iNumberOfPromptLines - 1) : 0;
        }
    }    

TTypeUid::Ptr CAknQueryControl::MopSupplyObject( TTypeUid aId )
    {
    CAknQueryExtension* extension = QueryExtension();
    if( (aId.iUid == MAknsControlContext::ETypeId) && 
        extension && extension->EditorContext() && iHasEditor )
        {
        // Return specific context iff editor exists and the context
        // has been constructed.
        return MAknsControlContext::SupplyMopObject( 
            aId, extension->EditorContext() );
        }
        
    if ( aId.iUid == CAknQueryControl::ETypeId )
    	{
    	return aId.MakePtr( this );
    	}
        
    return SupplyMopObject( aId, iEditIndicator );
    }

// Callback for doing a redraw when animating pictographs
TInt CAknQueryControl::StaticPictographCallBack( TAny* aPtr )
    {
    CAknQueryControl* me = static_cast<CAknQueryControl*>( aPtr );
    me->PictographCallBack();
    return KErrNone;
    }

void CAknQueryControl::PictographCallBack()
    {
    MAknsSkinInstance* skin = AknsUtils::SkinInstance();
    MAknsControlContext* cc = AknsDrawUtils::ControlContext( this );

    for ( TInt i = 0 ; i < iPrompt->NumberOfLines() ; i++ )
        {
        CAknPictographInterface* picto = iPrompt->PictographInterface();

        // Only update the note line if it contains pictographs

        if ( picto->Interface()->ContainsPictographs( 
            *( iPrompt->Line( i )->Text() ) ) )
            {
            TRect lineRect = iPrompt->Line(i)->Rect();
            Window().Invalidate( lineRect );

            ActivateGc();
            Window().BeginRedraw();

            AknsDrawUtils::Background(
                skin,
                cc,
                this,
#ifdef RD_UI_TRANSITION_EFFECTS_POPUPS
				SystemGc(),
#else				
                iCoeEnv->SystemGc(),
#endif                
                lineRect );

            Window().EndRedraw();
            DeactivateGc();

            // Draw the label after the background.
            iPrompt->Line( i )->ActivateL(); // Never leaves
            iPrompt->Line( i )->DrawNow();
            iPrompt->SetLineModified( i, EFalse );
            }
        }
    }

TBool CAknQueryControl::EmbeddedVirtualInput() const
    {
    if ( !QueryExtension()->iVirtualInputStatusFlags.IsSet( CAknQueryExtension::EVirtualInputCheckedAlready ) )
        {
        QueryExtension()->iVirtualInputStatusFlags.Set( CAknQueryExtension::EVirtualInputCheckedAlready );
        
        CEikDialog* dialog = NULL;

        // Remove constness. This is unfortunate but this method needs to be
        // called from several const methods so better to do it in one place.
        const_cast<CAknQueryControl*>( this )->MopGetObject( dialog );

        if ( dialog )
            {
            if ( dialog->DialogFlags() & EEikDialogFlagVirtualInput )
                {
                QueryExtension()->iVirtualInputStatusFlags.Set( CAknQueryExtension::EVirtualInputEnabled );
                }
            }
        }
        
#ifdef RD_SCALABLE_UI_V2            
    if ( AknLayoutUtils::PenEnabled() )
    	{
    	return QueryExtension()->iVirtualInputStatusFlags.IsSet( CAknQueryExtension::EVirtualInputEnabled );
    	}
#endif
	return false;    	
    }
    
    
CEikDialog* CAknQueryControl::Dialog() const
    {
    CAknQueryControl* control = const_cast<CAknQueryControl*>( this );
    
    CEikDialog* dialog = NULL;
    control->MopGetObject( dialog );
    return dialog;
    }
   
    
void CAknQueryControl::CreateIncAndDecButtonsL()
	{
	if (!iIncreaseValueButton)
		{
		iIncreaseValueButton = 
			CAknButton::NewL(KAvkonBitmapFile,
			EMbmAvkonQgn_indi_button_increase,
			EMbmAvkonQgn_indi_button_increase_mask,
            -1,
            -1,
            EMbmAvkonQgn_indi_button_increase_pressed,
            EMbmAvkonQgn_indi_button_increase_pressed_mask,
            -1,
            -1,
            KNullDesC,
            KNullDesC,
            KAknButtonNoFrame | KAknButtonKeyRepeat,
            0,
            KAknsIIDQgnIndiButtonIncrease,
            KAknsIIDNone,
            KAknsIIDQgnIndiButtonIncreasePressed,
            KAknsIIDNone );
            
        iIncreaseValueButton->SetContainerWindowL(*this);
        iIncreaseValueButton->SetParent(this);
        iIncreaseValueButton->SetObserver(this);
        iIncreaseValueButton->MakeVisible(ETrue);
        iIncreaseValueButton->SetFocusing(EFalse);
		}
		  
	if (!iDecreaseValueButton)
		{
		iDecreaseValueButton = 
        	CAknButton::NewL(KAvkonBitmapFile,
            EMbmAvkonQgn_indi_button_decrease,
            EMbmAvkonQgn_indi_button_decrease_mask,
            -1,
            -1,
            EMbmAvkonQgn_indi_button_decrease_pressed,
			EMbmAvkonQgn_indi_button_decrease_pressed_mask,
			-1,
			-1,
			KNullDesC,
			KNullDesC,
			KAknButtonNoFrame | KAknButtonKeyRepeat,
			0,
			KAknsIIDQgnIndiButtonDecrease,
			KAknsIIDNone,
			KAknsIIDQgnIndiButtonDecreasePressed,
			KAknsIIDNone );
			
		iDecreaseValueButton->SetContainerWindowL(*this);
		iDecreaseValueButton->SetParent(this);
		iDecreaseValueButton->SetObserver(this);
		iDecreaseValueButton->MakeVisible(ETrue);
		iDecreaseValueButton->SetFocusing(EFalse);
		}
	}
    
void CAknQueryControl::CreateBackspaceButtonL()
	{
	if (iExtension && !iExtension->iBackspaceButton)
		{
		iExtension->iBackspaceButton = 
			CAknButton::NewL(KAvkonBitmapFile,
			EMbmAvkonQgn_indi_navi_arrow_left,			
			EMbmAvkonQgn_indi_navi_arrow_left_mask,
			-1,
			-1,
			-1,
			-1,
			-1,
			-1,
			KNullDesC,
			KNullDesC,
			KAknButtonKeyRepeat | KAknButtonReportOnKeyDown,
			0,
			KAknsIIDQgnIndiItutBackspace, 
			KAknsIIDNone,
			KAknsIIDNone,
			KAknsIIDNone );
		iExtension->iBackspaceButton->SetContainerWindowL(*this);
		iExtension->iBackspaceButton->SetParent(this);
		iExtension->iBackspaceButton->SetObserver(this);
		iExtension->iBackspaceButton->MakeVisible(ETrue);
		iExtension->iBackspaceButton->SetFocusing(EFalse);	
		
		
        iExtension->iBackspaceButton->SetBackgroundIds( KAknsIIDQsnFrFunctionButtonNormal,
        	KAknsIIDQsnFrFunctionButtonPressed, KAknsIIDQsnFrFunctionButtonInactive,
        	KAknsIIDQsnFrFunctionButtonPressed, KAknsIIDQsnFrFunctionButtonInactive );
     
		
    	CFbsBitmap *icon = NULL;
    	CFbsBitmap *mask = NULL;    
    
    	MAknsSkinInstance* skin = AknsUtils::SkinInstance();
    	AknsUtils::CreateColorIconL( skin,
    		KAknsIIDQgnIndiItutBackspace,
    		KAknsIIDQsnTextColors,
    		EAknsCIQsnIconColorsCG30,
    		icon, mask,
    		KAvkonBitmapFile,
    		EMbmAvkonQgn_indi_navi_arrow_left,
    		EMbmAvkonQgn_indi_navi_arrow_left_mask,
    		KRgbBlack );    
    
    	CGulIcon* gulIcon = CGulIcon::NewL( icon, mask );
    	iExtension->iBackspaceButton->State()->SetIcon( gulIcon );
		}
	}
 
	
void CAknQueryControl::LayoutEditorForFullScreen( const TLayoutMethod& /*aLayoutM*/ )
	{
	TBool landscape = Layout_Meta_Data::IsLandscapeOrientation();
	
	TIndex LAFIndex(NbrOfPromptLines());
	iHasEditor = ETrue;
    
    TInt variety( LAFIndex.PromptLine() );
    iEditorVerShadow.LayoutRect( LayoutRect(), AKN_LAYOUT_WINDOW_Code_query_pop_up_window_elements_Line_1( variety ) );
    iEditorHorShadow.LayoutRect( LayoutRect(), AKN_LAYOUT_WINDOW_Code_query_pop_up_window_elements_Line_2( variety ) );
    
    TAknLayoutRect layoutRect;
 	layoutRect.LayoutRect( LayoutRect(), TAknWindowComponentLayout::Compose(
 		AknLayoutScalable_Apps::popup_vitu2_window( landscape ? 1 : 0 ), 
    	AknLayoutScalable_Apps::area_vitu2_query_pane( landscape ? 1 : 0 ) ).LayoutLine() );
    
    
    // Layout editor frame
 		
 	iEditorFrame.LayoutRect( layoutRect.Rect(), AknLayoutScalable_Apps::input_focus_pane_cp09( 0 ) );
       
   	// Layout editor indicator
        
	TAknWindowLineLayout lineLayout = AknLayoutScalable_Apps::area_vitu2_query_pane_g1( 0 );
	AknLayoutUtils::LayoutControl(iEditIndicator, layoutRect.Rect(), lineLayout );
        
	// Layout secret editor
	
	 if( iSecretEd )
        {
        TAknLayoutRect layoutRect;
 		layoutRect.LayoutRect( LayoutRect(), AknLayoutScalable_Apps::popup_vitu2_window( landscape ? 1 : 0 ) ); 
     
        AknLayoutUtils::LayoutSecretEditor( iSecretEd, layoutRect.Rect(),
       		TAknWindowComponentLayout::ComposeText( 
            AknLayoutScalable_Apps::area_vitu2_query_pane( landscape ? 1 : 0 ),
            AknLayoutScalable_Apps::area_vitu2_query_pane_t3( 1 ) ) );
     	
       	iSecretEd->SetBorder( TGulBorder::ENone );
       	if ( IsFocused() && !iSecretEd->IsFocused() )
       		{
       		iSecretEd->SetFocus( ETrue );
       		}
       	}
        
	// Layout pin editor
	
	if( iPinEdwin )
        {
        TAknLayoutRect layoutRect;
 		layoutRect.LayoutRect( LayoutRect(), AknLayoutScalable_Apps::popup_vitu2_window( landscape ? 1 : 0 ) ); 
        
        AknLayoutUtils::LayoutSecretEditor( iPinEdwin, layoutRect.Rect(),
       		TAknWindowComponentLayout::ComposeText( 
            AknLayoutScalable_Apps::area_vitu2_query_pane( landscape ? 1 : 0 ),
            AknLayoutScalable_Apps::area_vitu2_query_pane_t3( 1 ) ) );
        		
       	iPinEdwin->SetBorder( TGulBorder::ENone );
       	if ( IsFocused() && !iPinEdwin->IsFocused() )
       		{
       		iPinEdwin->SetFocus( ETrue );
       		}
       	}

	// Layout MFNE editor
	
	CEikMfne* edwin = NULL;
    if ( iTimeEdwin )
    	{
        edwin = iTimeEdwin;
        }        
    else if ( iDateEdwin )
        {
        edwin = iDateEdwin;
        }
    if ( edwin )
    	{

		TAknLayoutRect layoutRect;
 		layoutRect.LayoutRect( LayoutRect(), AknLayoutScalable_Apps::popup_vitu2_window( landscape ? 1 : 0 ) ); 
                                               
        AknLayoutUtils::LayoutMfne( edwin, layoutRect.Rect(), 
        	TAknWindowComponentLayout::ComposeText( 
            AknLayoutScalable_Apps::area_vitu2_query_pane( landscape ? 1 : 0 ),
            AknLayoutScalable_Apps::area_vitu2_query_pane_t3( 1 ) ) );
   
        edwin->SetBorder( TGulBorder::ENone );
        if ( IsFocused() && !edwin->IsFocused() )
        	{
            edwin->SetFocus( ETrue );
            }
        edwin->SetUpAndDownKeysConsumed( ETrue );
        }   
        
    // Layout Edwin editor    
        
    if ( iEdwin )
        {      
        TAknLayoutRect layoutRect;
 		layoutRect.LayoutRect( LayoutRect(), TAknWindowComponentLayout::Compose(
 			AknLayoutScalable_Apps::popup_vitu2_window( landscape ? 1 : 0 ), 
    		AknLayoutScalable_Apps::area_vitu2_query_pane( landscape ? 1 : 0 ) ).LayoutLine() );
      
        TAknTextComponentLayout textLayout =
            AknLayoutScalable_Apps::area_vitu2_query_pane_t3(1);
         
        TRect rect( layoutRect.Rect() );
        rect.iBr.iX -= 10;        
        AknLayoutUtils::LayoutEdwin( iEdwin,
            rect,
            textLayout.LayoutLine(),
            EAknsCIQsnTextColorsCG27,
            0,
            ETrue );
            
        iEdwin->SetBorder( TGulBorder::ENone ); 
        if ( IsFocused() && !( iEdwin->IsFocused() ) )
            {
            iEdwin->SetFocus(ETrue);
            }
        }
    }
	    
void CAknQueryControl::LayoutPromptForFullScreen()
	{
    if ( NbrOfPromptLines() != 3 )
    	{
    	return;	
    	}
    
    TBool landscape = Layout_Meta_Data::IsLandscapeOrientation();
    
    TAknLayoutRect rect;
 	rect.LayoutRect( LayoutRect(), TAknWindowComponentLayout::Compose(
 		AknLayoutScalable_Apps::popup_vitu2_window( landscape ? 1 : 0 ), 
    	AknLayoutScalable_Apps::area_vitu2_query_pane( landscape ? 1 : 0 ) ).LayoutLine() );
    	
    TAknTextLineLayout lineLayout = AknLayoutScalable_Apps::area_vitu2_query_pane_t4( 1 ).LayoutLine();
    AknLayoutUtils::LayoutLabel(iPrompt->Line(0), rect.Rect(), lineLayout );
          
    lineLayout = AknLayoutScalable_Apps::area_vitu2_query_pane_t5( 1 ).LayoutLine();
    AknLayoutUtils::LayoutLabel(iPrompt->Line(1), rect.Rect(), lineLayout );
  
    lineLayout = AknLayoutScalable_Apps::area_vitu2_query_pane_t6( 1 ).LayoutLine();
    AknLayoutUtils::LayoutLabel(iPrompt->Line(2), rect.Rect(), lineLayout );   
	}

EXPORT_C void CAknQueryControl::HandlePointerEventL(const TPointerEvent& aPointerEvent) 
    { 
    CEikMfne* edwin = NULL;
    if ( iTimeEdwin )
        {
        edwin = iTimeEdwin;
        }        
    else if ( iDateEdwin )
        {
        edwin = iDateEdwin;
        }
    else if ( LocationEd() )
        {
        edwin = LocationEd();
        }
    else if ( iDurationEdwin )
        {
        edwin = iDurationEdwin;
        }
        
    if ( edwin && iEditorFrame.Rect().Contains(aPointerEvent.iPosition) )
        {
        edwin->HandlePointerEventL(aPointerEvent);
        }
    else
        {
        CAknControl::HandlePointerEventL(aPointerEvent); 
        }
    }

EXPORT_C void* CAknQueryControl::ExtensionInterface( TUid /*aInterface*/ ) 
    { 
    return NULL;
    }
    
EXPORT_C void CAknQueryControl::HandleResourceChange(TInt aType)
    {
    CCoeControl::HandleResourceChange(aType);
    
    if( aType == KAknsMessageSkinChange && iExtension->iAnimationId )
        {
        TRAP_IGNORE(SetAnimationL( iExtension->iAnimationId ));
        TRAP_IGNORE(StartAnimationL());
        }    
    else if( aType == KEikDynamicLayoutVariantSwitch )
        {
        SizeChanged();
        TRAP_IGNORE(DoSetPromptL());
        }
    }    

// ---------------------------------------------------------------------------
// Return prompt text needed by FEP to query dialog
// ---------------------------------------------------------------------------
//
void CAknQueryControl::GetCaption(TDes& aCaption) const
    {
    aCaption = KNullDesC;
    
    TPtr prompt= iPrompt->Text();
    if ( prompt != KNullDesC )
       {
       const TInt maximumLength=aCaption.MaxLength();
       
       if ( prompt.Length() > maximumLength )
          {
          aCaption=prompt.Left(maximumLength);
          }
       else
          {
          aCaption= prompt;
          }
       }
    }


// ---------------------------------------------------------------------------
// Sets the state of ECS number visibility in the query.
// ---------------------------------------------------------------------------
//
void CAknQueryControl::SetEcsCbaVisibleL( TBool aVisible )
    {
    if ( iExtension &&
         !COMPARE_BOOLS( aVisible, iExtension->iEcsCbaShown ) &&
         iFlags.IsSet( EEmergencyCallsCBASupport ) )
        {
        CEikDialog* dlg;
        MopGetObject( dlg );
        if ( dlg )
            {
            CEikButtonGroupContainer& cba = dlg->ButtonGroupContainer();
            
            if ( aVisible 
                && (GetTextLength() == iEcsDetector->CurrentMatch().Length()) )
                // Further check to ensure that the matched number is the
                // entire buffer. Get the matched text and see if is the same
                // length as the current query length.
                {
                HBufC* cbaLabel =
                    StringLoader::LoadLC( R_AVKON_SOFTKEY_CALL_TEXT );
                cba.AddCommandToStackL( 0,
                                        EAknSoftkeyEmergencyCall,
                                        *cbaLabel );
                CleanupStack::PopAndDestroy( cbaLabel );
                iExtension->iEcsCbaShown = ETrue;
                }
            else
                {
                if ( iExtension->iEcsCbaShown )
                    {
                    cba.RemoveCommandFromStack( 0, EAknSoftkeyEmergencyCall );
                    iExtension->iEcsCbaShown = EFalse;
                    }
                }
            }
        }
    }


// ---------------------------------------------------------------------------
// Attempts an emergency call.
// ---------------------------------------------------------------------------
//
void CAknQueryControl::AttemptEmergencyCallL()
    {
    if ( iEcsDetector )
        {
        if ( iEcsDetector->State() == CAknEcsDetector::ECompleteMatch )
            {
            // Further check to ensure that the matched number is the
            // entire buffer. Get the matched text and see if is the same
            // length as the current query length.
            if ( GetTextLength() == iEcsDetector->CurrentMatch().Length() )
                {
                iEcsDetector->AddChar( (TText)EKeyYes );
                SetEcsCbaVisibleL( EFalse );
                
                // stop playing tacticons for pincode query
                StopPinCodeTacticonTimer();
                }
            }
        }
    }


/*****************************
 * CAknExtQueryControl
 *****************************/


///
/// Construction and Destruction methods
///

/**
 *
 */
EXPORT_C CAknExtQueryControl::CAknExtQueryControl()  : CAknQueryControl()
    {
    AKNTASHOOK_ADD( this, "CAknExtQueryControl" );
    }

/**
 *
 */
EXPORT_C CAknExtQueryControl::~CAknExtQueryControl()
    {
    AKNTASHOOK_REMOVE();
	if (iFixedPointEditor)
        AknsUtils::DeregisterControlPosition(iFixedPointEditor);
	if (iIpEditor)
        AknsUtils::DeregisterControlPosition(iIpEditor);
    delete iIpEditor;
    delete iFixedPointEditor;
    }


void CAknExtQueryControl::ConstructQueryL(TResourceReader& aRes)
    {   
    if ( iQueryType == EIpLayout)   
        {
        iIpEditor = new(ELeave)CAknIpFieldEditor;
        iIpEditor->SetContainerWindowL(*this);
        iIpEditor->ConstructFromResourceL(aRes);
        iIpEditor->SetObserver(this);
        iIpEditor->SetSkinTextColorL(EAknsCIQsnTextColorsCG27);
        if ( AknLayoutUtils::PenEnabled() )
            {
            iIpEditor->SetFeature( CEikMfne::EFingerSupport, CEikMfne::EnableWithAllHighlight );
	        }
        }
    else if ( iQueryType == EFixedPointLayout)  
        {
        iFixedPointEditor = new(ELeave)CEikFixedPointEditor;
        iFixedPointEditor->SetContainerWindowL(*this);
        iFixedPointEditor->ConstructFromResourceL(aRes);
        iFixedPointEditor->SetAknEditorAllowedInputModes( EAknEditorNumericInputMode );
        iFixedPointEditor->SetObserver(this);
        }
    else
         __ASSERT_DEBUG(0, Panic(EAknPanicLayoutControlResourceProblem));   

    // Construct editor context
    CAknQueryExtension* extension = QueryExtension();
    if( extension )
        {
        extension->CreateEditorContextL();
        }
    }


///
/// CCoeControl methods
///

/**
 *Returns the control owned by this control by the index
 */
CCoeControl* CAknExtQueryControl::ComponentControl(TInt anIndex) const
    {
    CCoeControl* controls[] = { iPrompt, 
                                iEditIndicator, 
                                iIpEditor,
                                iFixedPointEditor,
                                iImage, iAnimation };
    
    for (TUint ii=0;ii<sizeof(controls)/sizeof(CCoeControl*);ii++)
        {
        //subtle: index is decremented only if control is not NULL!
        if (controls[ii] && anIndex-- == 0) 
            {
            return controls[ii];
            }
        }
    return NULL;
    }     

/**
 * Returns the number of controls inside the compound control.
 */
TInt CAknExtQueryControl::CountComponentControls() const
    {
    TInt count = 0;
    CCoeControl* controls[] = { iPrompt,
                                iEditIndicator,
                                iIpEditor,
                                iFixedPointEditor,
                                iImage, iAnimation };
                                
    for (TUint ii=0;ii<sizeof(controls)/sizeof(CCoeControl*);ii++)
        {
        if (controls[ii])
            {
            count++;
            }
        }
    return count;
    }


/**
 * Handle a focus changed
 */
void CAknExtQueryControl::FocusChanged(TDrawNow aDrawNow)
    {
    CCoeControl* ctrl = ControlByLayoutOrNull( iQueryType );
    if (ctrl)
        {
        ctrl->SetFocus( IsFocused(), aDrawNow );
        Layout();
        }
    }

void CAknExtQueryControl::PrepareForFocusLossL()
    {
    CCoeControl* ctrl = ControlByLayoutOrNull( iQueryType );
    if (ctrl)
        {
        ctrl->PrepareForFocusLossL(); 
        }
    }

EXPORT_C void CAknExtQueryControl::HandleControlEventL(CCoeControl* /*aControl*/,TCoeEvent aEventType)
    {
    //CAknQueryControl::HandleControlEventL(NULL, aEventType);
    
    if(iQueryControlObserver && aEventType == EEventStateChanged)
        {                
        if(GetTextLength())
            {
            iQueryControlObserver->HandleQueryEditorStateEventL(this,MAknQueryControlObserver::EQueryControlEditorStateChanging, MAknQueryControlObserver::EEditorValueValid);
            }
        else
            {
            iQueryControlObserver->HandleQueryEditorStateEventL(this,MAknQueryControlObserver::EQueryControlEditorStateChanging, MAknQueryControlObserver::EEditorEmpty);
            }
        }
    }


///
/// DRAWING METHODS
///

/**
 * Layout the ip editor and its frame.
 * 
 * 
 * Queries other than confirmation queries have an editor that the user needs
 * to input the query data. This method does the layout of the editor. 
 *
 * Data queries have a resizable editor and its layout is specified in the LAF.
 * 
 * Other queries have a fixed size editor with layout specified.
 *
 *
 * This method is called from SizeChanged. Depending on the query type, the
 * layout method is chosen accordingly, e.g. data queries use the data query
 * method, other queires use the code query method.
 *
 * The macros at the top are just for avoiding code repetition.
 */
void CAknExtQueryControl::LayoutEditor(const TLayoutMethod& aLayoutM)
    {   
    TIndex LAFIndex(NbrOfPromptLines());
    
    if (aLayoutM == EDataQueryLayoutM)
        {
        iHasEditor = ETrue;
        
        LayoutEditorFrame(aLayoutM);        
        LayoutEditorIndicator(aLayoutM);

        TInt promptLines = NbrOfPromptLines();
        TInt numberTextLines = NbrOfEditorLines();
        TInt isNumeric = 0;

        if ( iFixedPointEditor )
            {
            if ( promptLines >= 3 )
                {
                AknLayoutUtils::LayoutEdwin( iFixedPointEditor,
                                             LayoutRect(),
                                             AKN_LAYOUT_TEXT_Data_query_pop_up_window_texts_Line_2( LAFIndex.DQPWindowTextsLine2(numberTextLines), isNumeric ),
                                             EAknsCIQsnTextColorsCG27 ) ;
                }
            else
                {
                if ( promptLines == 2 )
                    {
                    AknLayoutUtils::LayoutEdwin( iFixedPointEditor,
                                                 LayoutRect(),
                                                 AKN_LAYOUT_TEXT_Data_query_pop_up_window_texts_Line_2( LAFIndex.DQPWindowTextsLine2(1), isNumeric  ),
                                                 EAknsCIQsnTextColorsCG27 );
                    }
                else
                    {
                    AknLayoutUtils::LayoutEdwin( iFixedPointEditor,
                                                 LayoutRect(),
                                                 AKN_LAYOUT_MULTILINE_TEXT_Data_query_pop_up_window_texts_Line_2( isNumeric, numberTextLines ),
                                                 EAknsCIQsnTextColorsCG27 );
                    }
                }
            iFixedPointEditor->SetBorder(TGulBorder::ENone); 
            if (IsFocused() && !(iFixedPointEditor->IsFocused()) ) 
                iFixedPointEditor->SetFocus(ETrue);

            AknsUtils::RegisterControlPosition( iFixedPointEditor );
            }
        }    
    else if (aLayoutM == ETimeQueryLayoutM)
        {
        iHasEditor = ETrue;

        LayoutEditorFrame(aLayoutM);
        
        LayoutEditorIndicator(aLayoutM);

//LAYOUT for IP EDITOR
        if(iIpEditor)
            {
            AknLayoutUtils::LayoutMfne(iIpEditor,LayoutRect(),
              AKN_LAYOUT_TEXT_Code_query_pop_up_window_texts_Line_2(LAFIndex.PromptLine()));

            iIpEditor->SetBorder( TGulBorder::ENone ); 
            if ( IsFocused() && !iIpEditor->IsFocused() ) 
                iIpEditor->SetFocus(ETrue);
            iIpEditor->SetUpAndDownKeysConsumed( ETrue );
            }
        }
    }

EXPORT_C void CAknExtQueryControl::HandlePointerEventL(const TPointerEvent& aPointerEvent) 
    { 
    if ( iIpEditor && iEditorFrame.Rect().Contains(aPointerEvent.iPosition) )
        {
        iIpEditor->HandlePointerEventL(aPointerEvent); 
        }
    else
        {
        CAknQueryControl::HandlePointerEventL(aPointerEvent);
        }
    }

EXPORT_C void* CAknExtQueryControl::ExtensionInterface( TUid /*aInterface*/ ) 
    { 
    return NULL;
    }

EXPORT_C TInetAddr CAknExtQueryControl::GetInetAddress() const
    {   
    TInetAddr ret = TInetAddr();
    if( iIpEditor )
        {
        return iIpEditor->Address();
        }
    return ret; 
    }


EXPORT_C void CAknExtQueryControl::SetInetAddress(TInetAddr& aInetAddress)
    {
    if( iIpEditor )
        {
        iIpEditor->SetAddress(aInetAddress);
        }
    }

EXPORT_C TInt CAknExtQueryControl::GetFixedPointNumber() const
    {   
    TInt ret = 0;
    if( iFixedPointEditor )
        {
        return iFixedPointEditor->Value();
        }
    return ret; 
    }


EXPORT_C void CAknExtQueryControl::SetFixedPointNumberL(const TInt* aNumber)
    {
    if( iFixedPointEditor )
        {
        iFixedPointEditor->SetValueL(aNumber);
        }
    }


EXPORT_C void CAknExtQueryControl::SetMinimumAndMaximum(const TInetAddr& aMinimumAddress, const TInetAddr& aMaximumAddress)
    {
    if( iIpEditor )
        {
        iIpEditor->SetMinimumAndMaximum(aMinimumAddress, aMaximumAddress);
        } 
    }

EXPORT_C void CAknExtQueryControl::SetMinimumAndMaximum(TInt aMinimumValue, TInt aMaximumValue)    
    {
    if( iFixedPointEditor )
        {
        iFixedPointEditor->SetMinMax(aMinimumValue, aMaximumValue);
        }
    }


/**
 * Return text editor depening on query type
 */
EXPORT_C CCoeControl* CAknExtQueryControl::ControlByLayoutOrNull(TInt aLayout)
    {
    CCoeControl* ctrl=NULL;
    if ( aLayout == EIpLayout )
        {
            ctrl = iIpEditor;
        }
    else if ( aLayout == EFixedPointLayout )
        {
            ctrl = iFixedPointEditor;
        }
    return ctrl;
    }


/**
* This method looks at the flags that have been set in the query control and 
* acts upon them if possible
* 
*/
void CAknExtQueryControl::DeployFlagsL()
    {       
    }

/**
* Method to first set the flags and then to use them on the contained editors
*
*/

void CAknExtQueryControl::SetAndUseFlagsL(TBitFlags16 /*aFlags*/)
    {
    }

void CAknExtQueryControl::SetFlags(TBitFlags16 /*aFlags*/)
    {
    }


/** 
 * Return true if editor content is valid for query, false 
 * otherwise. Ip editor always have a valid content.
 * 
 * 
 */ 
TBool CAknExtQueryControl::EditorContentIsValidL() const
    {
    TInt ret = EFalse;
    if ( iIpEditor ) 
        {
        CEikMfne* editor = iIpEditor;
        ret = editor->Field(editor->CurrentField())->IsValid();
        }
    else if ( iFixedPointEditor ) 
        {
        TInt intValue;
        CAknNumericEdwin::TValidationStatus status = iFixedPointEditor->GetValueAsInteger(intValue);
        ret = status == CAknNumericEdwin::EValueValid;
        }
    return ret; 
    }


/*****************************************************************
 * CAknExtQueryControl::TIndex
 *
 * Manage indexes into LAF tables
 *
 * PN stands for "Popup Note"
 *
 * PQD stands for "Popup Query Data'
 *
 * 'DPQ' stands for "Data Query Popup"
 ******************************************************************/

  
CAknExtQueryControl::TIndex::TIndex(TInt aNumberOfPromptLines)
    : iNumberOfPromptLines(aNumberOfPromptLines)
    {
    }
        
void CAknExtQueryControl::TIndex::SelfTest() const
    {
    __ASSERT_DEBUG(iNumberOfPromptLines >= 0  && iNumberOfPromptLines <= 5,
        Panic(EAknPanicInvalidValue));
    }

/**
 * Return index into LAF tables that depend directly on number of prompt lines,
 * index is equal to number of prompt lines minus one unless there are no prompt
 * lines in which case the index is zero
 */
TInt CAknExtQueryControl::TIndex::PromptLine() const
    {
    SelfTest();
    return iNumberOfPromptLines > 0 ? iNumberOfPromptLines - 1 : 0;
    }

/**
 * Return index into Main Pane PQDC Window, which
 * depends on total number of prompt lines minus 2, e.g.
 * index 0 for 2 prompt lines total.
 *
 */
TInt CAknExtQueryControl::TIndex::DQPWindowTextsLine2(TInt aLineNum) const
    {
    SelfTest();
    __ASSERT_DEBUG(aLineNum > 0 && aLineNum <= 5,Panic(EAknPanicInvalidValue));

    static const TInt  KDataQueryPopupWindowTextsLine2Index[3][5] = 
    {   {0,  1, 2, 3, 4},    //1 PROMPT LINE
        {5,  6, 7, 8, 9},    //2 PROMPT LINES
        {10, 10, 10, 10, 10}  }; //3 PROMPT LINES
    
    return KDataQueryPopupWindowTextsLine2Index[PromptLine()][aLineNum-1];
    }

TInt CAknExtQueryControl::TIndex::PNWindow() const
    {
    __ASSERT_DEBUG(0,Panic(EAknPanicNotSupported)); // We should never be here
    return 0;
    /*
    SelfTest();
    static const TInt  KPopupNoteWindowIndex[5] = { 0,0,0,1,2 };
    
    return KPopupNoteWindowIndex[PromptLine()];
    */
    }

TInt CAknExtQueryControl::TIndex::PQDWindow(TInt aLineNum) const
    {
    (void)aLineNum; // to remove warning
    __ASSERT_DEBUG(0,Panic(EAknPanicNotSupported)); // We should never be here
    return 0;
    /*
    SelfTest();
    __ASSERT_DEBUG(aLineNum > 0 && aLineNum <= 5,Panic(EAknPanicInvalidValue));

    static const TInt  KPopupQueryDataWindowIndex[3][5] = 
    {   {0, 1, 3, 6, 8},   //1 PROMPT LINE
        {2, 4, 7, 9, 10},  //2 PROMPT LINES
        {5, 6, 8, 10, 11} }; //3 PROMPT LINES
    
    return KPopupQueryDataWindowIndex[PromptLine()][aLineNum-1];
    */
    }


// Callback for doing a redraw when animating pictographs
TInt CAknExtQueryControl::StaticPictographCallBack( TAny* aPtr )
    {
    CAknExtQueryControl* me = static_cast<CAknExtQueryControl*>( aPtr );
    me->PictographCallBack();
    return KErrNone;
    }

void CAknExtQueryControl::PictographCallBack()
    {
    MAknsSkinInstance* skin = AknsUtils::SkinInstance();
    MAknsControlContext* cc = AknsDrawUtils::ControlContext( this );

    for ( TInt i = 0 ; i < iPrompt->NumberOfLines() ; i++ )
        {
        CAknPictographInterface* picto = iPrompt->PictographInterface();

        // Only update the note line if it contains pictographs

        if ( picto->Interface()->ContainsPictographs( 
            *( iPrompt->Line( i )->Text() ) ) )
            {
            TRect lineRect = iPrompt->Line(i)->Rect();
            Window().Invalidate( lineRect );

            ActivateGc();
            Window().BeginRedraw();

            AknsDrawUtils::Background(
                skin,
                cc,
                this,
#ifdef RD_UI_TRANSITION_EFFECTS_POPUPS
				SystemGc(),
#else				
                iCoeEnv->SystemGc(),
#endif                
                lineRect );

            Window().EndRedraw();
            DeactivateGc();

            // Draw the label after the background.
            iPrompt->Line( i )->ActivateL(); // Never leaves
            iPrompt->Line( i )->DrawNow();
            iPrompt->SetLineModified( i, EFalse );
            }
        }
    }

// End of File
