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
* Description:  Implementation of AVKON query dialogs.
*
*/


#include <barsread.h>
#include <eikbtgpc.h>
#include <eikcapc.h>
#include <eiktxlbm.h>

#include "avkon.hrh"
#include <avkon.rsg>
#include <avkon.mbg>

#include "AknQueryDialog.h" 
#include "aknQueryControl.h"

#include "aknmultilinequerycontrol.h"
#include "aknPopupHeadingPane.h"
#include "AknPanic.h"
#include "aknborders.h"
#include "aknpopuplayout.h"
#include "aknenv.h"
#include "aknconsts.h"
#include "aknsoundsystem.h"

#include <AknsDrawUtils.h>
#include <AknsFrameBackgroundControlContext.h>
#include "AknDebug.h"

#include <layoutmetadata.cdl.h>
#include <aknlayoutscalable_avkon.cdl.h>
#include <akntouchpane.h>

#ifdef RD_UI_TRANSITION_EFFECTS_POPUPS
#include <gfxtranseffect/gfxtranseffect.h>
#include <akntransitionutils.h>
#include <akntranseffect.h>
#endif

#include <AknTasHook.h> // for testability hooks
/**
 *  CAknQueryDialogExtension  
 */
NONSHARABLE_CLASS(CAknQueryDialogExtension) : public CBase, public MCoeCaptionRetrieverForFep
{
public:
    /**
     * Destructor.
     */
    ~CAknQueryDialogExtension();
    
    /**
     * Constructor.
     */
    CAknQueryDialogExtension(CAknQueryDialog* aQueryDialog);
    
private:
    
    /**
     * from MCoeCaptionRetrieverForFep
     * fill aCaption with the target control's caption 
     */
  	IMPORT_C virtual void GetCaptionForFep(TDes& aCaption) const;
  	
public:
    CAknQueryDialog* iParent;
};


// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------
//
CAknQueryDialogExtension::~CAknQueryDialogExtension()
    {
    }


// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
//
CAknQueryDialogExtension::CAknQueryDialogExtension
    (CAknQueryDialog* aQueryDialog) : iParent(aQueryDialog)
    {
    }
    
// ---------------------------------------------------------------------------
// GetCaptionForFep
// Overrides MCoeCaptionRetrieverForFep::GetCaptionForFep
// Responsible for retrieving the caption from query dialog to FEP
// ---------------------------------------------------------------------------
//
EXPORT_C void CAknQueryDialogExtension::GetCaptionForFep(TDes& aCaption) const
    {
    aCaption = KNullDesC;
    
    CAknQueryControl* control = iParent->QueryControl();
    if ( control )
        {
        // Returns prompt of query control if it's available
        control->GetCaption(aCaption);
        }
    else
        {
        // Returns heading text if heading pane is available
        CAknPopupHeadingPane* controlHeading = iParent->QueryHeading();
        if ( controlHeading )
            {
            TPtr headTitle= controlHeading->PromptText();
            if ( headTitle != KNullDesC )
                {
                const TInt maximumLength = aCaption.MaxLength();
                if ( headTitle.Length() > maximumLength )
                    {
                    aCaption=headTitle.Left(maximumLength);
                    }
                else
                    {
                    aCaption= headTitle;
                    }
                }
            }
        }
    }


/**
 *  CAknMultilineQueryDialogExtension  
 */
NONSHARABLE_CLASS(CAknMultilineQueryDialogExtension) : public CAknQueryDialogExtension
{
public:
    enum TQueryType
        {
        EMultDataQuery,
        EMultIPQuery
        };
    /**
     * Destructor.
     */
    ~CAknMultilineQueryDialogExtension();
    
    /**
     * Constructor.
     */
    CAknMultilineQueryDialogExtension(CAknQueryDialog* aQueryDialog, TInt aQueryType);
    
private:
    
    /**
     * from MCoeCaptionRetrieverForFep
     * fill aCaption with the target control's caption 
     */
  	IMPORT_C void GetCaptionForFep(TDes& aCaption) const;

public:
    TInt iQueryType;
};


// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------
//
CAknMultilineQueryDialogExtension::~CAknMultilineQueryDialogExtension()
    {
    }

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
//
CAknMultilineQueryDialogExtension::CAknMultilineQueryDialogExtension
    (CAknQueryDialog* aQueryDialog, TInt aQueryType):CAknQueryDialogExtension(aQueryDialog),iQueryType(aQueryType)
    {
    }

// ---------------------------------------------------------------------------
// GetCaptionForFep
// Overrides MCoeCaptionRetrieverForFep::GetCaptionForFep
// Responsible for retrieving the caption from query dialog to FEP
// ---------------------------------------------------------------------------
//
EXPORT_C void CAknMultilineQueryDialogExtension::GetCaptionForFep(TDes& aCaption) const
    {
    switch(iQueryType)
        {
        case EMultDataQuery:
            {
            CAknMultiLineDataQueryDialog* dialog = static_cast<CAknMultiLineDataQueryDialog*>(iParent);
            if( dialog )
                {
                CAknMultilineQueryControl* control = NULL;
                if( dialog->CurrentLine() == 0 )
                    {
                    control = dialog->FirstControl();
                    }
                else if ( dialog->CurrentLine() == 1 )
                    {
                    control = dialog->SecondControl();
                    }
                
                if(control)
                    {
                    // Returns prompt of current focused control
                    control->GetCaption(aCaption);
                    }
                }
            }
            break;
        case EMultIPQuery:
            {
            CAknMultiLineIpQueryDialog* dialog = static_cast<CAknMultiLineIpQueryDialog*>(iParent);
            if( dialog )
                {
                CAknExtMultilineQueryControl* control = NULL;
                if( dialog->CurrentLine() == 0 )
                    {
                    control = dialog->FirstControl();
                    }
                else if( dialog->CurrentLine() == 1 )
                    {
                    control = dialog->SecondControl();
                    }
                
                if( control )
                    {
                    // returns prompt of current focused control
                    control->GetCaption(aCaption);
                    }
                }
            }
            break;
        default:
            break;
            
        }
    
    // if there's no prompt, then return heading text in heading pane
    if ( aCaption == KNullDesC )
        {
        // returns heading text if heading pane is available
        CAknPopupHeadingPane* controlHeading = iParent->QueryHeading();
        if ( controlHeading )
            {
            TPtr headTitle= controlHeading->PromptText();
            if ( headTitle != KNullDesC )
                {
                const TInt maximumLength = aCaption.MaxLength();
                if ( headTitle.Length() > maximumLength )
                    {
                    aCaption=headTitle.Left(maximumLength);
                    }
                else
                    {
                    aCaption= headTitle;
                    }
                }
            }
        }
    
    }


/**************************
 * CAknQueryDialog
 **************************/

/**
 * Second phase construction required to align with multiline queries API
 * In future might use MAknQueryData mixin in the base class, hence requiring
 * second phase construction.
 */
EXPORT_C CAknQueryDialog* CAknQueryDialog::NewL(const TTone& aTone)
    {
    CAknQueryDialog* self = new (ELeave) CAknQueryDialog(aTone);
    AKNTASHOOK_ADDL( self, "CAknQueryDialog" );
    return self;
    }

/**
 * Return pointer to CAknTextQueryDialog
 */
EXPORT_C CAknQueryDialog* CAknQueryDialog::NewL(TDes& aText, const TTone& aTone)
    {
    return CAknTextQueryDialog::NewL(aText,aTone);
    }

/**
 * Return pointer to CAknNumberQueryDialog
 */
EXPORT_C CAknQueryDialog* CAknQueryDialog::NewL(TInt& aNumber, const TTone& aTone)
    {
    return CAknNumberQueryDialog::NewL(aNumber,aTone);
    }

/**
 * Return pointer to CAknTimeQueryDialog
 */
EXPORT_C CAknQueryDialog* CAknQueryDialog::NewL(TTime& aTime, const TTone& aTone)
    {
    return CAknTimeQueryDialog::NewL(aTime,aTone);
    }

/**
 * Return pointer to CAknDurationQueryDialog
 */
EXPORT_C CAknQueryDialog* CAknQueryDialog::NewL(TTimeIntervalSeconds& aDur, const TTone& aTone)
    {
    return CAknDurationQueryDialog::NewL(aDur,aTone);
    }

/**
 * Return pointer to CAknFloatQueryDialog
 */
EXPORT_C CAknQueryDialog* CAknQueryDialog::NewL(TReal& aNumber, const TTone& aTone)
    {
    return CAknFloatingPointQueryDialog::NewL(aNumber,aTone);
    }

/**
 * Return pointer to CAknIpQueryDialog
 */
EXPORT_C CAknQueryDialog* CAknQueryDialog::NewL(TInetAddr& aInetAddr, const TTone& aTone)
    {
    return CAknIpAddressQueryDialog::NewL(aInetAddr,aTone);
    }

EXPORT_C CAknQueryDialog* CAknQueryDialog::NewL(TPosition &/*aPosition*/, const TTone& /*aTone*/)
	{
	//return CAknLocationQueryDialog::NewL(aPosition, aTone);
	return NULL;
	}

EXPORT_C CAknQueryDialog::CAknQueryDialog(const TTone& aTone)
    {
#ifdef RD_UI_TRANSITION_EFFECTS_POPUPS
    GfxTransEffect::Register( this, KGfxQueryControlUid );
#endif
    iTone = aTone;
    if (iEikonEnv->AppUi())
        {
         iSoundSystem = iAvkonAppUiBase->KeySounds();          
        }
    } 

/**
 * deprecated
 */
EXPORT_C CAknQueryDialog::CAknQueryDialog()
    {
    iTone = ENoTone;
    if (iEikonEnv->AppUi())
        {
         iSoundSystem = iAvkonAppUiBase->KeySounds();          
        }
    } 

/**
 * deprecated
 */
EXPORT_C CAknQueryDialog::CAknQueryDialog(TDesC& aPrompt,const TTone& aTone)
    {
    iTone = aTone;
    if (iEikonEnv->AppUi())
        {
        iSoundSystem = iAvkonAppUiBase->KeySounds();          
        }
    
    TRAP_IGNORE(SetPromptL(aPrompt));
    }

EXPORT_C CAknQueryDialog::~CAknQueryDialog()
    {
    AKNTASHOOK_REMOVE();
    delete iPrompt;
    delete iExtension;
    
#ifdef RD_UI_TRANSITION_EFFECTS_POPUPS
    CAknTransitionUtils::RemoveData( ( TInt )this );
#endif
    }


EXPORT_C TInt CAknQueryDialog::RunLD()
    {
    CAknDialog::ConstructL( R_AVKON_MENUPANE_EMPTY) ;
    ReportUserActivity();
    PlayTone();
    CAknKeySoundSystem* soundSystem = NULL;
    if (iSoundSystem)
        {
        iSoundSystem->PushContextL(R_AVKON_DEFAULT_SKEY_LIST);
        // Need to store local copy of iSoundSystem, since it will have been
        // destroyed when RunLD returns
        soundSystem = iSoundSystem;
        }

    CAknsFrameBackgroundControlContext* cc = (CAknsFrameBackgroundControlContext*)AknsDrawUtils::ControlContext( this );
    cc->SetCenter(KAknsIIDQsnFrPopupCenterQuery);

    SetGloballyCapturing(ETrue); 
    TInt ret = CAknDialog::RunLD();
    if (soundSystem)
        {
        soundSystem->PopContext();
        }
    SetGloballyCapturing(EFalse); 
    return ret;
    }

/**
 * deprecated
 */
EXPORT_C TInt CAknQueryDialog::RunDlgLD(TInt aResourceId)
    {
    return ExecuteLD(aResourceId);
    }


/**
 * Call PrepareLC and display dialog.
 *
 * Identical to version in CEikDialog but need to override 
 * this because of version with prompt,
 * compiler will complain if this is not implemented in
 * derived class as well
 */
EXPORT_C TInt CAknQueryDialog::ExecuteLD(TInt aResourceId)
    {
    PrepareLC(aResourceId);
    return(RunLD());
    }

/**
 * Call PrepareLC, set the prompt and display dialog.
 */
EXPORT_C TInt CAknQueryDialog::ExecuteLD(TInt aResourceId,const TDesC& aPrompt)
    {
    PrepareLC(aResourceId);
    SetPromptL(aPrompt);
    return(RunLD());
    }
/**
 * Get layout information from the control and use this layout
 * information to do layout. If there is a heading, add the 
 * heading height to the height returned by the control layout
 * as long as it still fits on the screen.
 *
 * Assume there is always a control panel present. If we used
 * iAvkonAppUi->ClientRect() this would return the correct size
 * available depending on whether the control pane is there or not.
 * However then global queries (drawn by eiksrv which has no control
 * pane) get confused because they appear on top of a focused apps
 * which does have a control pane.
 *
 * Therefore use iAvkonAppUi->ApplicationRect() minus the height 
 * of the control pane (KAknSoftkeyPaneHeight) instead of ClientRect().
 */
EXPORT_C void CAknQueryDialog::SetSizeAndPosition( const TSize& /*aSize*/ )
    {
    CAknQueryControl* control = QueryControl();
    CAknPopupHeadingPane* controlHeading = QueryHeading();
    
    if(control)
        {
        //this is because prompt must be relayouted before setting dialog size
        //in case of a layout switch - adding HandleResourceChange to
        //dialog is not possible anymore
        control->HandleResourceChange(KEikDynamicLayoutVariantSwitch);
        
#ifdef RD_SCALABLE_UI_V2            
        if ( AknLayoutUtils::PenEnabled() )
            {
            CEikDialog* dialog = NULL;
    		control->MopGetObject( dialog );
       		if ( dialog && ( dialog->DialogFlags() & EEikDialogFlagVirtualInput ) )
       			{
       			TAknWindowLineLayout lay = AknLayoutScalable_Avkon::application_window(0);
       			TRect re(0, 0, lay.iW, lay.iH);
           		SetRect(re);
           		return;
       			}
            }
#endif
        TAknWindowLineLayout windowLineLayoutScreen = AknLayoutScalable_Avkon::Screen().LayoutLine();
        TRect rectZero = TRect(0,0,0,0);
        TAknLayoutRect layoutRect;
        layoutRect.LayoutRect( rectZero, windowLineLayoutScreen );
        TRect rectScreen( layoutRect.Rect() );
    	TSize maxSize = rectScreen.Size();
    	
        if ( AknLayoutUtils::PenEnabled() )
            {
            CAknDialog::SetSizeAndPosition( PreferredSize( maxSize ) );
            }
        else
            {
            TAknWindowLineLayout layout;
            control->WindowLayout( layout );
           
            TRect mainPane;
            AknLayoutUtils::LayoutMetricsRect( AknLayoutUtils::EPopupParent,
                    mainPane );
              
        
            TAknLayoutRect layoutWindow;
            layoutWindow.LayoutRect( mainPane, layout); 
            TRect winRect(layoutWindow.Rect());
                    
            if (controlHeading)
            	{
                TAknLayoutRect headingLayoutRect;
                headingLayoutRect.LayoutRect(mainPane,
                        AknLayoutScalable_Avkon::heading_pane(0)); 
                winRect.iTl.iY -= headingLayoutRect.Rect().Height();
                }
            SetRect(winRect);
            }            
        }
    }

/**
 *
 */
EXPORT_C void CAknQueryDialog::PreLayoutDynInitL()
    {
    SetBorder( TGulBorder::ENone );
    SetEditableL( ETrue ) ;
    DoSetPromptL();

    CAknQueryControl* control = QueryControl();
    if(control)
        {
        control->SetQueryControlObserver(this);
        control->SetAndUseFlagsL( iFlags );
        }

    CAknPopupHeadingPane* controlHeading = QueryHeading();
    if (controlHeading)
        { 
        SetLineNonFocusing(EAknMessageQueryHeaderId); 
        
        if (control)
            control->SetNumberOfEditorLines(1);
        }
    
    // Create CaptionRetriever for FEP
    delete iExtension;
    iExtension = NULL;
    iExtension = new(ELeave) CAknQueryDialogExtension(this);
    }

/**
 *
 */
EXPORT_C void CAknQueryDialog::PostLayoutDynInitL()
    {
    CAknQueryControl* control = QueryControl();
    if (control)
        control->StartAnimationL();

    }

/**
 *
 */
EXPORT_C TKeyResponse CAknQueryDialog::OfferKeyEventL(const TKeyEvent& aKeyEvent, 
                                                            TEventCode aType)
    {
    if( NeedToDismissQueryL(aKeyEvent) )
            return EKeyWasConsumed;
 	
	if (aType == EEventKey && aKeyEvent.iCode == EKeyOK) 	
		{
		CAknQueryControl* control = QueryControl();
		if (control)
			{
			if (IsLeftSoftkeyVisible())
				{
				TryExitL(EEikBidOk);
				return EKeyWasConsumed;
				}
            }
		}
	else if ( aType == EEventKey && aKeyEvent.iCode == EKeyEnter )	
        {
		CAknQueryControl* control = QueryControl();
		
		if (control)
			{
			if (IsLeftSoftkeyVisible())
				{
				TryExitL(EEikBidOk);
				return EKeyWasConsumed;
				}
            }
		}
	

    return CAknDialog::OfferKeyEventL(aKeyEvent,aType);
    }

/**
 * Allows dismissing of queries. Only mandatory requirement is that PIN
 * queries are dismissed by the # key. Clients can
 * override this and implement something different. 
 *
 */
EXPORT_C TBool CAknQueryDialog::NeedToDismissQueryL(const TKeyEvent& aKeyEvent)
    {
    if (aKeyEvent.iScanCode == EStdKeyHash)
        {
        CAknQueryControl* control = QueryControl();
        if (control && control->QueryType() == EPinLayout)
            {
            DismissQueryL();
            return ETrue;
            }
        }
    return EFalse;
    }

/**
 * Dismiss query. Query is accepted if the left softkey is displayed 
 * (left softkey is displayed only if there is valid data in the query).
 * Query is discarded if the left softkey is not displayed.
 * Clients can override this and implement something different.
 */
EXPORT_C void CAknQueryDialog::DismissQueryL()
    {
    if (IsLeftSoftkeyVisible())
        {
        TryExitL(EEikBidOk);
        }
    else
        {
        TryExitL(EEikBidCancel);
        }
    }

/**
 *
 */
EXPORT_C TBool CAknQueryDialog::OkToExitL(TInt aButtonId)
    {
    if ( aButtonId == EAknSoftkeyEmergencyCall )
        {
        // ECS number entered and "call" softkey pressed,
        // send a key event to the query control to
        // initiate the call.
        CAknQueryControl* control = QueryControl();
        if ( control )
            {
            control->AttemptEmergencyCallL();
            }
        
        return ETrue;
        }
    else if((IsLeftSoftkeyVisible() && 
        (aButtonId == GetLeftCBAShortKeyPress() || aButtonId == EEikBidOk))
        || aButtonId == GetRightCBAShortKeyPress() )
        {   
        return ETrue;
        }
    return EFalse;
    }

/**
 * Return pointer to query heading or NULL (use QueryHeading())
 *
 * @deprecated - will be removed (use appropriate SetHeading methods instead)
 */
EXPORT_C CAknPopupHeadingPane* CAknQueryDialog::Heading() const
    {
    return QueryHeading();
    }

/**
 * Set prompt text for query. 
 */
EXPORT_C void CAknQueryDialog::SetPromptL(const TDesC& aPrompt)
    {
    delete iPrompt;
    iPrompt = NULL;
    iPrompt = aPrompt.AllocL();
    DoSetPromptL();
    }

EXPORT_C void CAknQueryDialog::DoSetPromptL()
    {
    CAknQueryControl* control = QueryControl();
    if( control && iPrompt && iPrompt->Length() )
        {
        control->SetPromptL( *iPrompt );
        }
    LayoutAndDraw();
    }

/**
CCoeControl* CAknQueryDialog::FindControlOnAnyPageWithControlType(TInt aControlType, TInt* aLineIndex, TInt* aPageIndex) const

  @ aControlType - Enum used to identify the control type on that CEikCaptionedControl Line
  @ aLineIndex - Optional argument, if used contains reference to an int which holds the initial
                line to start searching on, and on finding a control of that type, is 
                assigned the next position to start searching.
  @ aPageIndex - Optional argument, if used contains reference to an int which holds the initial
                dialog page to start searching on, and on finding a control of that type, is 
                assigned the page that the control was found on.

To be used to go through every control in the dialog, on every page, 
in Dialog-internal storage order, until a control with that type is found.
If such a control is not found, a null pointer will be returned.
*/

CCoeControl* CAknQueryDialog::FindControlOnAnyPageWithControlType(TInt aControlType, TInt* aLineIndex, TInt* aPageIndex) const
    {
    CEikCaptionedControl* controlPtr=NULL;
    const TInt KMaxNumPages= GetNumberOfPages();
    for (TInt pageCount= (!aPageIndex) ? 0 : (*aPageIndex);pageCount<KMaxNumPages ;pageCount++)
        {
        const TInt KMaxNumLinesOnThisPage= GetNumberOfLinesOnPage(pageCount);
        for (TInt lineCount=(!aLineIndex) ? 0 : (*aLineIndex);lineCount< KMaxNumLinesOnThisPage;lineCount++)
            {
            controlPtr = GetLineByLineAndPageIndex(lineCount, pageCount);
            if (controlPtr && controlPtr->iControlType == aControlType) 
                {
/*
update the values passed
if aLineIndex, update it with the next value, so it doesn't look in the same place twice.
pass aPageIndex as per usual.
*/
                if (aLineIndex) 
                    *aLineIndex=(lineCount+1); // set the restart value to the next line.
                if (aPageIndex) // start at the same page.
                    *aPageIndex=(pageCount);
                return controlPtr->iControl;
                }
            }
        }
    return 0;
    }

EXPORT_C CAknQueryControl* CAknQueryDialog::QueryControl() const
    {
    CCoeControl* controlPtr = FindControlOnAnyPageWithControlType(EAknCtQuery);
    if (controlPtr)
        return STATIC_CAST(CAknQueryControl*,controlPtr);
/* 
Must have reached limit of pages and lines without finding desired object
This should not happen.
*/
    return 0;
    }

EXPORT_C CAknPopupHeadingPane* CAknQueryDialog::QueryHeading() const
    {
    CCoeControl* controlPtr = FindControlOnAnyPageWithControlType(EAknCtPopupHeadingPane);
    if (controlPtr)
        return STATIC_CAST(CAknPopupHeadingPane*,controlPtr);
    return 0;
    }

EXPORT_C void CAknQueryDialog::MakeLeftSoftkeyVisible(TBool aVisible)
    {  
    CEikButtonGroupContainer& btnGroupContainer = ButtonGroupContainer();

    // left softkey
    TBool isVisible = btnGroupContainer.IsCommandVisibleByPosition(CEikButtonGroupContainer::ELeftSoftkeyPosition);
    if ( isVisible != aVisible )
        {
        btnGroupContainer.MakeCommandVisibleByPosition(CEikButtonGroupContainer::ELeftSoftkeyPosition,aVisible);
        }

    // middle softkey
    if ( btnGroupContainer.ButtonGroup()->CommandId(0) == btnGroupContainer.ButtonGroup()->CommandId(3) )
        {
        isVisible = btnGroupContainer.IsCommandVisibleByPosition(CEikButtonGroupContainer::EMiddleSoftkeyPosition);
        if ( isVisible != aVisible )
            {
            btnGroupContainer.MakeCommandVisibleByPosition(CEikButtonGroupContainer::EMiddleSoftkeyPosition,aVisible);
            }
        }
    }



void CAknQueryDialog::PlayTone()
    {
    if (iTone != 0 && iSoundSystem)
        {
        iSoundSystem->PlaySound(iTone);
        }
    }

/** 
 * Reset timers monitoring user inactivity. Will make applications
 * using these timers react, e.g. if the screen saver is running,
 * this will disactivate it (as from screen saver specs)
 */ 
void CAknQueryDialog::ReportUserActivity() const 
    {
#ifdef AVKON_RDEBUG_INFO
    RDebug::Print(_L("Reset user inactivity"));
#endif
    User::ResetInactivityTime();
    }

TInt CAknQueryDialog::GetLeftCBAShortKeyPress()
    {
    return TInt16(0xffff & TInt16(ButtonGroupContainer().ButtonGroup()->CommandId(0)));
    }
  
TInt CAknQueryDialog::GetRightCBAShortKeyPress()
    {
    return TInt16(0xffff & TInt16(ButtonGroupContainer().ButtonGroup()->CommandId(2)));
    }

TBool CAknQueryDialog::IsLeftSoftkeyVisible()
    {
    return ButtonGroupContainer().ButtonGroup()->IsCommandVisible(
                      ButtonGroupContainer().ButtonGroup()->CommandId(0));
    }

/**
 * Sets emergency call support enabled. Must be called prior to PreLayoutDynamicInitL
 */
EXPORT_C void CAknQueryDialog::SetEmergencyCallSupport( TBool aOnOff )
    {
    iFlags.Assign(CAknQueryControl::EEmergencyCallsEnabledByAPI, aOnOff );
    }

EXPORT_C void CAknQueryDialog::SetPredictiveTextInputPermitted( TBool aPermitted )
    {
    iFlags.Assign( CAknQueryControl::EPredictiveTextEntryPermitted, aPermitted );
    }

EXPORT_C void CAknQueryDialog::RemoveEditorIndicator()
    {
    iFlags.Assign(CAknQueryControl::EEditorIndicatorOff,ETrue);
    }

EXPORT_C void CAknQueryDialog::SetHeaderTextL(const TDesC& aHeader)
    {
    CAknPopupHeadingPane* controlHeading = QueryHeading();
    if (controlHeading)
        controlHeading->SetTextL(aHeader);
        
    LayoutAndDraw();
    }

/** 
 * Show left CBA only if editor text is valid
 */ 
EXPORT_C TBool CAknQueryDialog::HandleQueryEditorStateEventL
    (
    CAknQueryControl*       /*aQueryControl*/, 
    TQueryControlEvent      aEventType, 
    TQueryValidationStatus  /*aStatus*/
    )
    {
    if (aEventType == EEmergencyCallAttempted)
        {
        TryExitL(EEikBidCancel);
        }
    else
        {
        UpdateLeftSoftKeyL();
        }
    return EFalse;
    }


EXPORT_C void CAknQueryDialog::SetHeaderImageL(CEikImage* aImage)
    {
    CAknPopupHeadingPane* controlHeading = Heading(); 
    if ( controlHeading )
        {
        controlHeading->SetHeaderImageL(aImage);
        }
    LayoutAndDraw();
    delete aImage;
    aImage = NULL;
    }


/** 
 * If control text is ok then display left soft key
 * otherwise don't
 */ 
EXPORT_C void CAknQueryDialog::UpdateLeftSoftKeyL()
    {
    CAknQueryControl* control = QueryControl();
    if ( control )
        {
        MakeLeftSoftkeyVisible( control->EditorContentIsValidL() );
        }
    }

/** 
 * Handle editors size changes. This is called from CAknQueryControl 
 * when editor size changes.
 */ 
EXPORT_C TBool CAknQueryDialog::HandleQueryEditorSizeEventL
    (
    CAknQueryControl*   /*aQueryControl*/, 
    TQueryControlEvent  /*aEventType*/
    )
    {
    return EFalse;
    }

/**
 * Used to encapsulate the rules of how long the string is 
 * NB for a stringlength to be valid it must be <= Maxlength of descriptor.
 */
EXPORT_C TInt CAknQueryDialog::MaxTextLength(const CAknQueryControl* aControl, const TDes& aDataText,TInt aApiValue) 
    {
    return GetMaxTextLength(aControl,aDataText,aApiValue);
    }

EXPORT_C void CAknQueryDialog::HandlePointerEventL(const TPointerEvent& aPointerEvent) 
    { 
    if ( AknLayoutUtils::PenEnabled() )
        {

        CAknTouchPane* touchPane = iAvkonAppUi->TouchPane();
        
        if ( !Rect().Contains( aPointerEvent.iPosition ) && touchPane
            && touchPane->IsVisible() )
            {
            // touchpane is a window-owning control -> Rect() cannot be used
            TRect touchPaneRect( touchPane->Position(), touchPane->Size() );
                
            if ( touchPaneRect.Contains( aPointerEvent.iParentPosition ) )
                {
                TPointerEvent pointerEvent( aPointerEvent );
                   
                // make event's coordinates touch pane relative
                pointerEvent.iPosition = aPointerEvent.iParentPosition - 
                    touchPaneRect.iTl;
                    
                static_cast<CCoeControl*>( touchPane )->HandlePointerEventL( 
                    pointerEvent );
                }
            }
        else
            {
            // Forward also those pointerevents that the dialog rect does not contain
            CAknDialog::HandlePointerEventL( aPointerEvent );
            }
        }
    }

EXPORT_C void* CAknQueryDialog::ExtensionInterface( TUid /*aInterface*/ ) 
    { 
    return NULL;
    }

/**
 * deprecated
 */
EXPORT_C TInt CAknQueryDialog::MaxTextLength(const CAknQueryControl* aControl, const TDes* aDataText,TInt aApiValue) 
    {
    __ASSERT_DEBUG(aDataText,Panic(EAknPanicInvalidValue));     
    return MaxTextLength(aControl, *aDataText,aApiValue);
    }


/**
 * If control is activiated re-do layout and draw
 * Typically called by methods that might cause a 
 * resize such as SetPrompL()
 */
void CAknQueryDialog::LayoutAndDraw()
    {
    if (IsActivated())
        {
        MakeVisible(EFalse);
        TAknWindowLineLayout lay = AKN_LAYOUT_WINDOW_screen;
        TAknLayoutRect layoutR;
        layoutR.LayoutRect(Rect(), lay);
		SetSizeAndPosition(PreferredSize(layoutR.Rect().Size()));
        MakeVisible(ETrue);
        }
    }

EXPORT_C TPtrC CAknQueryDialog::Prompt() const
    {
    if (iPrompt)
        return *iPrompt;
    return TPtrC(0,0);
    }

EXPORT_C void CAknQueryDialog::CEikDialog_Reserved_1()
    {
    }

EXPORT_C void CAknQueryDialog::CEikDialog_Reserved_2()
    {
    }

EXPORT_C void CAknQueryDialog::CAknDialog_Reserved()
    {
    }

EXPORT_C void CAknQueryDialog::CAknQueryDialog_Reserved()
    {
    }

// ---------------------------------------------------------------------------
// Get InputCapabilities.  Overrides CoeControl::InputCapabilities
// ---------------------------------------------------------------------------
// 
EXPORT_C TCoeInputCapabilities CAknQueryDialog::InputCapabilities() const
	{
	return TCoeInputCapabilities(TCoeInputCapabilities::EAllText, NULL, iExtension);
	}


// ---------------------------------------------------------------------------
// Sets the CBA label changing support for ECS.
// If enabled, the LSK label is changed to "call" whenever an emergency
// number is entered to the query.
// ---------------------------------------------------------------------------
//
EXPORT_C void CAknQueryDialog::SetEmergencyCallSupportForCBA( TBool aOnOff )
    {
    if ( aOnOff )
        {
        // Ensure also that the EEmergencyCallsEnabledByAPI is set.
        iFlags.Assign( CAknQueryControl::EEmergencyCallsEnabledByAPI, aOnOff );
        }
    iFlags.Assign( CAknQueryControl::EEmergencyCallsCBASupport, aOnOff );
    }



/************************
 * CAknTextQueryDialog
 ************************/

/**
 * Second phase construction required to align API with multiline queries 
 * and because in future MAknQueryData will be used.
 */
EXPORT_C CAknTextQueryDialog* CAknTextQueryDialog::NewL(TDes& aDataText, const TTone& aTone)
    {
    CAknTextQueryDialog* self = new (ELeave) CAknTextQueryDialog(aDataText, aTone);
    AKNTASHOOK_ADDL( self, "CAknTextQueryDialog" );
    return self;
    }


EXPORT_C CAknTextQueryDialog::CAknTextQueryDialog
    (TDes& aDataText,const TTone& aTone) : CAknQueryDialog(aTone), iDataText(aDataText)
    {
    }

/**
 * deprecated
 */
EXPORT_C CAknTextQueryDialog::CAknTextQueryDialog(TDes& aDataText,TDesC& aPrompt,const TTone& aTone)
:CAknQueryDialog(aPrompt,aTone), iDataText(aDataText)
    {
    }

EXPORT_C CAknTextQueryDialog::~CAknTextQueryDialog()
    {
    AKNTASHOOK_REMOVE();
    }

EXPORT_C TBool CAknTextQueryDialog::CheckIfEntryTextOk() const
    {
    CAknQueryControl* control = QueryControl();
    if ( control )
        {
        TBool ret = EFalse;
        TRAPD(err, ret = control->EditorContentIsValidL());
        if (err == KErrNone)
            return ret;
        }
    return EFalse;
    }

EXPORT_C void CAknTextQueryDialog::SetDefaultInputMode(TInt aInputMode)
    {
    CAknQueryControl* control = QueryControl();
    if ( control )
        {
        CEikSecretEditor* secretEditor = STATIC_CAST(CEikSecretEditor*, control->ControlByLayoutOrNull( ECodeLayout ));
        if (secretEditor)
            {
            secretEditor->SetDefaultInputMode(aInputMode);
            }
        }
    }

EXPORT_C void CAknTextQueryDialog::PreLayoutDynInitL()
    {
    CAknQueryDialog::PreLayoutDynInitL();

    SetControlTextL();
    UpdateLeftSoftKeyL();
    }


EXPORT_C void CAknTextQueryDialog::SetMaxLength(TInt aLength)
    {
    iTextMaxLength = aLength;
    }
    
/**
 * Handle editor size change. 
 *
 * This is called from CAknQueryControl when the editor size changes.
 */
EXPORT_C TBool CAknTextQueryDialog::HandleQueryEditorSizeEventL(
                                        CAknQueryControl* /*aQueryControl*/, 
                                        TQueryControlEvent /*aEventType*/)
    { 
    Layout();
    DrawNow();
    return EFalse;
    }

/**
 * Accept or reject query.
 *
 * The query is approved if the pressed button is the left softkey or
 * the selection key. The query is discarded if pressed button is
 * the right softkey.
 *
 * In addition, for the query to be approved the left softkey must be
 * visible. Left softkey being visible is equivalent to editor text
 * being valid.
 *
 * @see IsLeftSoftkeyVisible
 */
EXPORT_C TBool CAknTextQueryDialog::OkToExitL( TInt aButtonId )
    {
    if ( aButtonId == EAknSoftkeyEmergencyCall )
        {
        // ECS number entered and "call" softkey pressed,
        // send a key event to the query control to
        // initiate the call.
        CAknQueryControl* control = QueryControl();
        if ( control )
            {
            control->AttemptEmergencyCallL();
            }
        
        return ETrue;
        }
    else if ( ( IsLeftSoftkeyVisible() && 
              ( aButtonId == GetLeftCBAShortKeyPress() || aButtonId == EEikBidOk ) ) )
        {
        CAknQueryControl* control = QueryControl();
        if ( control )
            {
            control->GetText( iDataText );
            }
            
        return ETrue;
        }
    else if( aButtonId == GetRightCBAShortKeyPress() )
        {
        return ETrue;
        }
 
    return EFalse;
    }

void CAknTextQueryDialog::SetControlTextL()
    {
    CAknQueryControl* control = QueryControl();
    if (control)
        {
        control->SetTextEntryLength( MaxTextLength(control, iDataText,iTextMaxLength));
        control->SetTextL(iDataText);
        }
    }

EXPORT_C void CAknTextQueryDialog::HandlePointerEventL(const TPointerEvent& aPointerEvent) 
    { 
    CAknQueryDialog::HandlePointerEventL(aPointerEvent); 
    }

EXPORT_C void* CAknTextQueryDialog::ExtensionInterface( TUid /*aInterface*/ ) 
    { 
    return NULL;
    }

EXPORT_C void CAknTextQueryDialog::CEikDialog_Reserved_1()
    {
    }

EXPORT_C void CAknTextQueryDialog::CEikDialog_Reserved_2()
    {
    }

EXPORT_C void CAknTextQueryDialog::CAknDialog_Reserved()
    {
    }

EXPORT_C void CAknTextQueryDialog::CAknQueryDialog_Reserved()
    {
    }

/************************
 * CAknNumberQueryDialog
 ************************/

/**
 * Second phase construction required to align API with multiline queries 
 * and because in future MAknQueryData will be used.
 */
EXPORT_C CAknNumberQueryDialog* CAknNumberQueryDialog::NewL(TInt& aNumber, const TTone& aTone)
    {
    CAknNumberQueryDialog* self = new (ELeave) CAknNumberQueryDialog(aNumber, aTone);
    AKNTASHOOK_ADDL( self, "CAknNumberQueryDialog" );
    return self;
    }


EXPORT_C CAknNumberQueryDialog::CAknNumberQueryDialog
    (TInt& aNumber,const TTone& aTone) : CAknQueryDialog(aTone), iNumber(aNumber)
    {
    }

EXPORT_C CAknNumberQueryDialog::~CAknNumberQueryDialog()
    {
    AKNTASHOOK_REMOVE();
    }

EXPORT_C void CAknNumberQueryDialog::PreLayoutDynInitL()
    {
    CAknQueryDialog::PreLayoutDynInitL();

    CAknQueryControl* control = QueryControl();
    if (control)
        control->SetNumberL(iNumber);
    }

EXPORT_C void CAknNumberQueryDialog::SetMinimumAndMaximum( TInt aMinimumValue,
                                                           TInt aMaximumValue)
{
    CAknQueryControl* control = QueryControl();
    if ( control )
        {
        control->SetMinimumAndMaximum(aMinimumValue,aMaximumValue);
        }
}

EXPORT_C TBool CAknNumberQueryDialog::OkToExitL( TInt aButtonId )
    {
    if ( aButtonId == EAknSoftkeyEmergencyCall )
        {
        // ECS number entered and "call" softkey pressed,
        // send a key event to the query control to
        // initiate the call.
        CAknQueryControl* control = QueryControl();
        if ( control )
            {
            control->AttemptEmergencyCallL();
            }
        
        return ETrue;
        }
    else if ( aButtonId == GetRightCBAShortKeyPress() )
        {
        return ETrue;
        }
    else if ( ( IsLeftSoftkeyVisible() && 
              ( aButtonId == GetLeftCBAShortKeyPress() || aButtonId == EEikBidOk ) ) )
        {
        TInt e = KErrNone;
        CAknQueryControl* control = QueryControl();
        if ( control && control->CheckNumber() )
            {
            TRAP( e, control->PrepareForFocusLossL() );
            if ( e != KErrNone )
                {
                return EFalse;
                }
            iNumber = control->GetNumber();
            return ETrue;
            }
       }

    return EFalse;
    }

EXPORT_C void CAknNumberQueryDialog::HandlePointerEventL(const TPointerEvent& aPointerEvent) 
    { 
    CAknQueryDialog::HandlePointerEventL(aPointerEvent); 
    }

EXPORT_C void* CAknNumberQueryDialog::ExtensionInterface( TUid /*aInterface*/ ) 
    { 
    return NULL;
    }

EXPORT_C void CAknNumberQueryDialog::CEikDialog_Reserved_1()
    {
    }

EXPORT_C void CAknNumberQueryDialog::CEikDialog_Reserved_2()
    {
    }

EXPORT_C void CAknNumberQueryDialog::CAknDialog_Reserved()
    {
    }

EXPORT_C void CAknNumberQueryDialog::CAknQueryDialog_Reserved()
    {
    }


/************************
 * CAknTimeQueryDialog
 ************************/

/**
 * Second phase construction required to align API with multiline queries 
 * and because in future MAknQueryData will be used.
 */
EXPORT_C CAknTimeQueryDialog* CAknTimeQueryDialog::NewL(TTime& aTime, const TTone& aTone)
    {
    CAknTimeQueryDialog* self = new (ELeave) CAknTimeQueryDialog(aTime, aTone);
    AKNTASHOOK_ADDL( self, "CAknTimeQueryDialog" );
    return self;
    }

EXPORT_C CAknTimeQueryDialog::CAknTimeQueryDialog
    (TTime& aTime, const TTone& aTone) : CAknQueryDialog(aTone), iTime(aTime)
    {
    }

/**
 * deprecated
 */
EXPORT_C CAknTimeQueryDialog::CAknTimeQueryDialog(TTime& aTime,TDesC& aPrompt,const TTone& aTone)
    : CAknQueryDialog(aPrompt,aTone), iTime(aTime)
    {
    }

EXPORT_C CAknTimeQueryDialog::~CAknTimeQueryDialog()
    {
    AKNTASHOOK_REMOVE();
    }

EXPORT_C void CAknTimeQueryDialog::PreLayoutDynInitL()
    {
    CAknQueryDialog::PreLayoutDynInitL();
    CAknQueryControl* control = QueryControl();
    if (control)
        control->SetTime(iTime);
    }

EXPORT_C void CAknTimeQueryDialog::SetMinimumAndMaximum( const TTime& aMinimum, 
                                                         const TTime& aMaximum)
{
    CAknQueryControl* control = QueryControl();
    if ( control )
        {
        control->SetMinimumAndMaximum(aMinimum,aMaximum);
        }
}

EXPORT_C TBool CAknTimeQueryDialog::OkToExitL(TInt aButtonId)
    {
    if((IsLeftSoftkeyVisible() && 
        (aButtonId == GetLeftCBAShortKeyPress() || aButtonId == EEikBidOk)))
        {   
        CAknQueryControl* control = QueryControl();
        if ( control )
            {
            iTime = control->GetTime();
            }
        return ETrue;
        }
    else if(aButtonId == GetRightCBAShortKeyPress())
        return ETrue;

    return EFalse;
    }

EXPORT_C void CAknTimeQueryDialog::HandlePointerEventL(const TPointerEvent& aPointerEvent) 
    { 
    CAknQueryDialog::HandlePointerEventL(aPointerEvent); 
    }

EXPORT_C void* CAknTimeQueryDialog::ExtensionInterface( TUid /*aInterface*/ ) 
    { 
    return NULL;
    }

EXPORT_C void CAknTimeQueryDialog::CEikDialog_Reserved_1()
    {
    }

EXPORT_C void CAknTimeQueryDialog::CEikDialog_Reserved_2()
    {
    }

EXPORT_C void CAknTimeQueryDialog::CAknDialog_Reserved()
    {
    }

EXPORT_C void CAknTimeQueryDialog::CAknQueryDialog_Reserved()
    {
    }

/************************
 * CAknDurationQueryDialog
 ************************/

/**
 * Second phase construction required to align API with multiline queries 
 * and because in future MAknQueryData will be used.
 */
EXPORT_C CAknDurationQueryDialog* CAknDurationQueryDialog::NewL(TTimeIntervalSeconds& aDuration, const TTone& aTone)
    {
    CAknDurationQueryDialog* self = new (ELeave) CAknDurationQueryDialog(aDuration, aTone);
    AKNTASHOOK_ADDL( self, "CAknDurationQueryDialog" );
    return self;
    }

EXPORT_C CAknDurationQueryDialog::CAknDurationQueryDialog
    (TTimeIntervalSeconds& aDuration, const TTone& aTone) : CAknQueryDialog(aTone), iDuration(aDuration)
    {
    }

EXPORT_C CAknDurationQueryDialog::~CAknDurationQueryDialog()
    {
    AKNTASHOOK_REMOVE();
    }

EXPORT_C void CAknDurationQueryDialog::PreLayoutDynInitL()
    {
    CAknQueryDialog::PreLayoutDynInitL();
    CAknQueryControl* control = QueryControl();
    
    if (control)
        control->SetDuration(iDuration);
    }

EXPORT_C void CAknDurationQueryDialog::SetMinimumAndMaximum( const TTimeIntervalSeconds& aMinimumDuration, 
                                                             const TTimeIntervalSeconds& aMaximumDuration)
{
    CAknQueryControl* control = QueryControl();
    if ( control )
        {
        control->SetMinimumAndMaximum(aMinimumDuration, aMaximumDuration);
        }
}

EXPORT_C TBool CAknDurationQueryDialog::OkToExitL(TInt aButtonId)
    {
    if((IsLeftSoftkeyVisible() && 
        (aButtonId == GetLeftCBAShortKeyPress() || aButtonId == EEikBidOk)))
        {   
        CAknQueryControl* control = QueryControl();
        if ( control )
            {
            iDuration = control->GetDuration();
            }
        return ETrue;
        }
    else if(aButtonId == GetRightCBAShortKeyPress())
        {
        return ETrue;
        }
    return EFalse;
    }
    
EXPORT_C void CAknDurationQueryDialog::HandlePointerEventL(const TPointerEvent& aPointerEvent) 
    { 
    CAknQueryDialog::HandlePointerEventL(aPointerEvent); 
    }

EXPORT_C void* CAknDurationQueryDialog::ExtensionInterface( TUid /*aInterface*/ ) 
    { 
    return NULL;
    }

EXPORT_C void CAknDurationQueryDialog::CEikDialog_Reserved_1()
    {
    }

EXPORT_C void CAknDurationQueryDialog::CEikDialog_Reserved_2()
    {
    }

EXPORT_C void CAknDurationQueryDialog::CAknDialog_Reserved()
    {
    }

EXPORT_C void CAknDurationQueryDialog::CAknQueryDialog_Reserved()
    {
    }

/**************************************
 * CAknFloatingPointQueryDialog
 **************************************/

/**
 * Second phase construction required to align API with multiline queries 
 * and because in future MAknQueryData will be used.
 */
EXPORT_C CAknFloatingPointQueryDialog* CAknFloatingPointQueryDialog::NewL(TReal& aNumber, const TTone& aTone)
    {
    CAknFloatingPointQueryDialog* self = new (ELeave) CAknFloatingPointQueryDialog(aNumber, aTone);
    AKNTASHOOK_ADDL( self, "CAknFloatingPointQueryDialog" );
    return self;
    }

EXPORT_C CAknFloatingPointQueryDialog::CAknFloatingPointQueryDialog
    (TReal& aNumber, const TTone& aTone) : CAknQueryDialog(aTone), iNumber(aNumber)
    {
    }

EXPORT_C CAknFloatingPointQueryDialog::~CAknFloatingPointQueryDialog()
    {
    AKNTASHOOK_REMOVE();
    }

EXPORT_C void CAknFloatingPointQueryDialog::PreLayoutDynInitL()
    {
    CAknQueryDialog::PreLayoutDynInitL();
    CAknQueryControl* control = QueryControl();
  
    if ( control )
        {
        control->SetQueryControlObserver(this);
        control->SetFloatingPointNumberL(&iNumber);
        DoSetPromptL();
        }
    }

EXPORT_C void CAknFloatingPointQueryDialog::SetMinimumAndMaximum( const TReal& aMinimumNumber, 
                                                             const TReal& aMaximumNumber)
{
    CAknQueryControl* control = QueryControl();
    if ( control )
        {
        control->SetMinimumAndMaximum(aMinimumNumber, aMaximumNumber);
        }
}

EXPORT_C TBool CAknFloatingPointQueryDialog::OkToExitL(TInt aButtonId)
    {
    if((IsLeftSoftkeyVisible() && 
        (aButtonId == GetLeftCBAShortKeyPress() || aButtonId == EEikBidOk)))
        {   
        CAknQueryControl* control = QueryControl();
        if ( control )
            {
            iNumber = control->GetFloatingPointNumberL();
            }
        return ETrue;
        }
    else if(aButtonId == GetRightCBAShortKeyPress())
        {
        return ETrue;
        }
    return EFalse;
    }
    
EXPORT_C void CAknFloatingPointQueryDialog::HandlePointerEventL(const TPointerEvent& aPointerEvent) 
    { 
    CAknQueryDialog::HandlePointerEventL(aPointerEvent); 
    }    

EXPORT_C void* CAknFloatingPointQueryDialog::ExtensionInterface( TUid /*aInterface*/ ) 
    { 
    return NULL;
    }

EXPORT_C void CAknFloatingPointQueryDialog::CEikDialog_Reserved_1()
    {
    }

EXPORT_C void CAknFloatingPointQueryDialog::CEikDialog_Reserved_2()
    {
    }

EXPORT_C void CAknFloatingPointQueryDialog::CAknDialog_Reserved()
    {
    }

EXPORT_C void CAknFloatingPointQueryDialog::CAknQueryDialog_Reserved()
    {
    }


/**************************************
 * CAknMultiLineDataQueryDialog
 **************************************/

EXPORT_C CAknMultiLineDataQueryDialog* CAknMultiLineDataQueryDialog::NewL(TTime& aTime1, TTime& aTime2, TTone aTone)
    {return DoNewL(aTime1,aTime2,aTone);}

EXPORT_C CAknMultiLineDataQueryDialog* CAknMultiLineDataQueryDialog::NewL(TDes&  aText1, TDes&  aText2, TTone aTone)
    {return DoNewL(aText1,aText2,aTone);}

EXPORT_C CAknMultiLineDataQueryDialog* CAknMultiLineDataQueryDialog::NewL(TDes&  aText1, TTime& aTime2, TTone aTone)
    {return DoNewL(aText1,aTime2,aTone);}

EXPORT_C CAknMultiLineDataQueryDialog* CAknMultiLineDataQueryDialog::NewL(TDes&  aText1, TInt&  aNum2,  TTone aTone)
    {return DoNewL(aText1,aNum2,aTone);}

EXPORT_C CAknMultiLineDataQueryDialog* CAknMultiLineDataQueryDialog::NewL(TDes& aText1,  TTimeIntervalSeconds& aDur2, TTone aTone)
    {return DoNewL(aText1,aDur2,aTone);}

EXPORT_C CAknMultiLineDataQueryDialog* CAknMultiLineDataQueryDialog::NewL(TTime& aTime1, TTimeIntervalSeconds& aDur2, TTone aTone)
    {return DoNewL(aTime1,aDur2,aTone);}

EXPORT_C CAknMultiLineDataQueryDialog* CAknMultiLineDataQueryDialog::NewL(TInt&  aNum1,  TInt&  aNum2, TTone aTone)
    {return DoNewL(aNum1,aNum2,aTone);}

EXPORT_C CAknMultiLineDataQueryDialog* CAknMultiLineDataQueryDialog::NewL(TPosition &aPos, TTone aTone)
	{
	return DoNewL(aPos, aPos, aTone);
	}

EXPORT_C CAknMultiLineDataQueryDialog::CAknMultiLineDataQueryDialog(const TTone& aTone) 
    : CAknQueryDialog(aTone)
    {
    AKNTASHOOK_ADD( this, "CAknMultiLineDataQueryDialog" );
    }

EXPORT_C CAknMultiLineDataQueryDialog::~CAknMultiLineDataQueryDialog()
    {
    AKNTASHOOK_REMOVE();
    delete iFirstData;
    delete iSecondData;

    delete iSecondPrompt;
    }

EXPORT_C CAknMultilineQueryControl* CAknMultiLineDataQueryDialog::FirstControl() const
    {
/* It's very important that lineIndexCount and pageIndexCount are zero initialised 
as they are used as a way of remembering how far the search has traversed.
*/
    TInt lineIndexCount=0;
    TInt pageIndexCount=0;

    FOREVER
        {
        CCoeControl* controlPtr = FindControlOnAnyPageWithControlType(EAknCtMultilineQuery,&lineIndexCount,&pageIndexCount);
        if (controlPtr)
            {
            CAknMultilineQueryControl* multiLineQueryPtr = STATIC_CAST(CAknMultilineQueryControl*,controlPtr);
            if (multiLineQueryPtr->IsFirst())
                return multiLineQueryPtr;
/*
if not the first line, continue searching, with new values of lineIndexCount and pageIndexCount 
updated by FindControlOnAnyPageWithControlType
*/
            }
        else 
/* 
Must have reached limit of pages and lines without finding desired object
This should not happen.
*/
            return 0;
        }
    }

EXPORT_C CAknMultilineQueryControl* CAknMultiLineDataQueryDialog::SecondControl() const
    {
/* It's very important that lineIndexCount and pageIndexCount are zero initialised 
as they are used as a way of remembering how far the search has traversed.
*/
    TInt lineIndexCount=0;
    TInt pageIndexCount=0;

    FOREVER
        {
        CCoeControl* controlPtr = FindControlOnAnyPageWithControlType(EAknCtMultilineQuery,&lineIndexCount,&pageIndexCount);
        if (controlPtr)
            {
            CAknMultilineQueryControl* multiLineQueryPtr = STATIC_CAST(CAknMultilineQueryControl*,controlPtr);
            if (multiLineQueryPtr->IsSecond())
                return multiLineQueryPtr;
/*
if not the second line, continue searching, with new values of lineIndexCount and pageIndexCount 
updated by FindControlOnAnyPageWithControlType
*/
            }
        else 
/* 
Must have reached limit of pages and lines without finding desired object
This should not happen.
*/
            return 0;
        }
    }
    
EXPORT_C CAknQueryControl* CAknMultiLineDataQueryDialog::QueryControl() const
    {
	if (!FirstLineEnabled() && SecondLineEnabled()) return SecondControl();
    return FirstControl();
    }

/**
 * According to the Eur LAF specs, v. 3.3, combined data-code queries cannot
 * have a heading. Returning NULL here guarantees that the heading is not
 * layed out even if there is a heading in the resource file.
 */
EXPORT_C CAknPopupHeadingPane* CAknMultiLineDataQueryDialog::QueryHeading() const
    {
    return NULL;
    }


EXPORT_C void CAknMultiLineDataQueryDialog::PreLayoutDynInitL()
    {
    if (Layout_Meta_Data::IsLandscapeOrientation())
        {
        SetBorder( TGulBorder::ENone );
        }
	SetMultilineQuery(ETrue);
    SetEditableL( ETrue );
    DoSetPromptL();
    
    CAknMultilineQueryControl* firstControl = FirstControl();
    CAknMultilineQueryControl* secondControl = SecondControl();

    firstControl->SetNbrOfPromptLines( firstControl->NbrOfPromptLines(), 
                                        secondControl->NbrOfPromptLines());
    secondControl->SetNbrOfPromptLines( firstControl->NbrOfPromptLines(), 
                                        secondControl->NbrOfPromptLines());
    
    firstControl->SetQueryControlObserver(this);
    secondControl->SetQueryControlObserver(this);
    
    // Set and then deploy the flags
    firstControl->SetAndUseFlagsL( iFlags );
    secondControl->SetAndUseFlagsL( iFlags );

    if (iFirstData) 
        iFirstData->SetL(firstControl,iFirstEditorMaxLength);
    if (iSecondData) 
        iSecondData->SetL(secondControl,iSecondEditorMaxLength);
    
	SetInitialCurrentLine();
	UpdateLeftSoftKeyL();
    HandleOrientationSwitch();
    
    // Create CaptionRetriever for FEP
    delete iExtension;
    iExtension = NULL;
    iExtension = new(ELeave) CAknMultilineQueryDialogExtension(this, CAknMultilineQueryDialogExtension::EMultDataQuery);
    }

EXPORT_C void CAknMultiLineDataQueryDialog::SetPromptL(const TDesC& aFP, const TDesC& aSP)
//
//Store new prompt values
//
    {
    delete iSecondPrompt;
    iSecondPrompt = NULL;
    iSecondPrompt = aSP.AllocL();

    CAknQueryDialog::SetPromptL(aFP);
    }

EXPORT_C void CAknMultiLineDataQueryDialog::DoSetPromptL()
    {
//
//Set prompt in query controls, called during layout
//
    CAknMultilineQueryControl* secondControl = SecondControl();
    if(secondControl && iSecondPrompt && iSecondPrompt->Length())
        secondControl->SetPromptL(*iSecondPrompt);
    
    CAknQueryDialog::DoSetPromptL();        
    }

/**
* Allows dismissing of queries. Same as base class implementation only take into
* consideration both controls
*/
EXPORT_C TBool CAknMultiLineDataQueryDialog::NeedToDismissQueryL(const TKeyEvent& aKeyEvent)
    {
    if (aKeyEvent.iScanCode == EStdKeyHash)
        {
        CAknMultilineQueryControl* firstControl = FirstControl();
        CAknMultilineQueryControl* secondControl = SecondControl();
        
        //second component control is the editor
        if ( (firstControl && firstControl->ComponentControl(1)->IsFocused() 
              &&(firstControl->QueryType() == EMultiDataFirstPinEd) ) ||
             (secondControl && secondControl->ComponentControl(1)->IsFocused() 
              && (secondControl->QueryType() == EMultiDataSecondPinEd) ) )
            {
            DismissQueryL();
            return ETrue;
            }
        }

	if (aKeyEvent.iCode == EKeyTab && (!(FirstLineEnabled()&&SecondLineEnabled())) )
		{

		int line = 0;
	    CAknMultilineQueryControl* firstControl = FirstControl();
    	CAknMultilineQueryControl* secondControl = SecondControl();	
		TInt line1 = FindLineIndex(*firstControl);
		TInt line2 = FindLineIndex(*secondControl);

		if (CurrentLine()==0) line = line2;
		if (CurrentLine()==1) line = line1;

        if ( CAknEnv::Static()->TransparencyEnabled() )
            {
            // Another case of missed RD_NO_DIALOG_BORDERS?  	    
            SetBorder( TGulBorder::ENone );
            }
        else
            {
        	if (line == line1)
		      	{
        	    SetBorder( TGulBorder::ENone );
			    }
		    else
			    {
        	    SetBorder(AknBorderId::EAknBorderNotePopup);
			    }
            }

		CEikCaptionedControl *ctrl = GetLineByLineAndPageIndex(line, 0);
		TryChangeFocusToL(ctrl->iId);
        HandleOrientationSwitch();

        UpdateLeftSoftKeyL();
		return ETrue;
		}


    return EFalse;
    }


TBool CAknMultiLineDataQueryDialog::FirstLineEnabled() const
{
	return (CurrentLine()==0 && Layout_Meta_Data::IsLandscapeOrientation())
		|| !Layout_Meta_Data::IsLandscapeOrientation();
}

TBool CAknMultiLineDataQueryDialog::SecondLineEnabled() const
{
	return (CurrentLine()==1 && Layout_Meta_Data::IsLandscapeOrientation())
		|| !Layout_Meta_Data::IsLandscapeOrientation();
}

TInt CAknMultiLineDataQueryDialog::CurrentLine() const
{
   CAknMultilineQueryControl* secondControl = SecondControl();
        CAknMultilineQueryControl* firstControl = FirstControl();  
   if (firstControl && firstControl->IsFocused()) return 0;
   if (secondControl && secondControl->IsFocused()) return 1;
   return -1;
}

void CAknMultiLineDataQueryDialog::HandleOrientationSwitch()
{
	TBool firstLineEnabled = FirstLineEnabled();
	TBool secondLineEnabled = SecondLineEnabled();

    if (!firstLineEnabled && !secondLineEnabled)
    {
	    TInt controlID = IdOfFocusControl();
	    if (controlID  )
	    {
	        Line(controlID)->SetFocus(ETrue, EDrawNow);
	    	firstLineEnabled = FirstLineEnabled();
	        secondLineEnabled = SecondLineEnabled();
	    }	
	    else
	        firstLineEnabled = ETrue;      
    }

    CAknMultilineQueryControl* firstControl = FirstControl();
    CAknMultilineQueryControl* secondControl = SecondControl();	
	TInt line1 = FindLineIndex(*firstControl);
	TInt line2 = FindLineIndex(*secondControl);
	CEikCaptionedControl *ctrl1 = GetLineByLineAndPageIndex(line1, 0);
	CEikCaptionedControl *ctrl2 = GetLineByLineAndPageIndex(line2, 0);
	ctrl1->SetLatent(!firstLineEnabled);
	ctrl2->SetLatent(!secondLineEnabled);
	ctrl1->SetLatentGroupLineFollows(!firstLineEnabled);
	ctrl2->SetLatentGroupLineFollows(!secondLineEnabled);
	ctrl1->SetFocusing(firstLineEnabled);
	ctrl2->SetFocusing(secondLineEnabled);
	Layout();
}

EXPORT_C void CAknMultiLineDataQueryDialog::HandleResourceChange(TInt aType)
{
	CAknQueryDialog::HandleResourceChange(aType);
	if (aType == KEikDynamicLayoutVariantSwitch)
		{
        if (Layout_Meta_Data::IsLandscapeOrientation())
            {
            SetBorder( TGulBorder::ENone );
            }        	
        else
            {
            SetBorder(AknBorderId::EAknBorderNotePopup);
            }
		HandleOrientationSwitch();
		TRAP_IGNORE( UpdateLeftSoftKeyL() );
		}
}

EXPORT_C TBool CAknMultiLineDataQueryDialog::OkToExitL(TInt aButtonId)
    {
    if (Layout_Meta_Data::IsLandscapeOrientation())
    	{
    if((IsLeftSoftkeyVisible() && 
        (aButtonId == GetLeftCBAShortKeyPress() || aButtonId == EEikBidOk)))
        {
        CAknMultilineQueryControl* firstControl = FirstControl();
        CAknMultilineQueryControl* secondControl = SecondControl();
        	if ( FirstControl()->EditorContentIsValidL() && FirstControl()->IsFocused() )
        		{
                if ( !CAknEnv::Static()->TransparencyEnabled() )
                    {
                    // It may be that this should be under the RD_NO_DIALOG_BORDERS?
                    // Anyway, multiline query's second query in landscape bugged
                    // if the EAknBorderNotePopup was set.. Maybe the alternate
                    // code path should set TGulBorder::ENone?
                    SetBorder(AknBorderId::EAknBorderNotePopup);
                    }
				TInt line1 = FindLineIndex(*firstControl);
				TInt line2 = FindLineIndex(*secondControl);
				CEikCaptionedControl *ctrl1 = GetLineByLineAndPageIndex(line1, 0);
				CEikCaptionedControl *ctrl2 = GetLineByLineAndPageIndex(line2, 0);
				TryChangeFocusToL(ctrl2->iId);
                HandleOrientationSwitch();

                UpdateLeftSoftKeyL();

        		return EFalse;      		        		        		
        		}
        	}
    	}
    if((IsLeftSoftkeyVisible() && 
        (aButtonId == GetLeftCBAShortKeyPress() || aButtonId == EEikBidOk)))
        {
        CAknMultilineQueryControl* firstControl = FirstControl();
        CAknMultilineQueryControl* secondControl = SecondControl();
        
        if (iFirstData)
            iFirstData->Get(firstControl);
        
        if(iSecondData)
            iSecondData->Get(secondControl);

        if(iText)
            {
            TInt len = firstControl->GetTextLength();
            if ( len != -1 )
                {
                HBufC* text = HBufC::NewLC( len );
                TPtr ptext = text->Des();
                firstControl->GetText( ptext );
                iText->Copy( *text );                        
                CleanupStack::PopAndDestroy( text );   	
                }
            }
        if(iSecondText)
            {
            TInt len = secondControl->GetTextLength();
            if ( len != -1 )
                {
                HBufC* text = HBufC::NewLC( len );
                TPtr ptext = text->Des();
                secondControl->GetText( ptext );
                iSecondText->Copy( *text );                        
                CleanupStack::PopAndDestroy( text );   	
                }
            }

        return ETrue;
        }
    else if(aButtonId == GetRightCBAShortKeyPress())
        {
        return ETrue;
        }
    
    return EFalse;
    }

EXPORT_C void CAknMultiLineDataQueryDialog::UpdateLeftSoftKeyL()
    {
    
    if (Layout_Meta_Data::IsLandscapeOrientation())
    	{
	    if ( (FirstControl()->EditorContentIsValidL() && FirstControl()->IsFocused()) || (SecondControl()->EditorContentIsValidL() && FirstControl()->EditorContentIsValidL()) )
	        {
	        MakeLeftSoftkeyVisible(ETrue);
	        }
	    else
	        {
	        MakeLeftSoftkeyVisible(EFalse);
	        }	        
	    return;
    	}
    	
    if ( FirstControl()->EditorContentIsValidL() &&
         SecondControl()->EditorContentIsValidL())
        {
        MakeLeftSoftkeyVisible(ETrue);
        }
    else
        {
        MakeLeftSoftkeyVisible(EFalse);
        }
    }

EXPORT_C void CAknMultiLineDataQueryDialog::SetMaxLengthOfFirstEditor(TInt aFirstTextEditorMaxLength)
    {
    iFirstEditorMaxLength = aFirstTextEditorMaxLength;
    };

EXPORT_C void CAknMultiLineDataQueryDialog::SetMaxLengthOfSecondEditor(TInt aSecondTextEditorMaxLength)
    {
    iSecondEditorMaxLength=aSecondTextEditorMaxLength;
    };

EXPORT_C TPtrC CAknMultiLineDataQueryDialog::SecondPrompt() const
    { if (iSecondPrompt)
        return *iSecondPrompt; 
      return TPtrC(0,0);    
    }

/**
 * Deprecated constructors
 */

inline const TDesC& DesOrNull(const TDesC* aDes) 
    {return aDes ? *aDes : KNullDesC();}


EXPORT_C CAknMultiLineDataQueryDialog::CAknMultiLineDataQueryDialog( TTime* aTime, 
                                                                    TTime* aTime2,
                                                                    TDesC* aPrompt,
                                                                    TDesC* aPrompt2,
                                                                    const TTone& aTone)
                                                                    : CAknQueryDialog(aTone)
    {
    TRAPD(ignore, SetPromptL(DesOrNull(aPrompt), DesOrNull(aPrompt2)));
    TRAP(ignore,SetDataL(*aTime,*aTime2));
    AKNTASHOOK_ADD( this, "CAknMultiLineDataQueryDialog" );
    }

EXPORT_C CAknMultiLineDataQueryDialog::CAknMultiLineDataQueryDialog( TDes* aDataText,
                                                                    TDes* aDataText2,
                                                                    TDesC* aPrompt,
                                                                    TDesC* aPrompt2,
                                                                    const TTone& aTone)
                                                                    : CAknQueryDialog(aTone)
    {
    TRAPD(ignore, SetPromptL(DesOrNull(aPrompt), DesOrNull(aPrompt2)));
    TRAP(ignore,SetDataL(*aDataText,*aDataText2));

    iText = aDataText;
    iSecondText = aDataText2;
    AKNTASHOOK_ADD( this, "CAknMultiLineDataQueryDialog" );
    }

EXPORT_C CAknMultiLineDataQueryDialog::CAknMultiLineDataQueryDialog( TDes* aDataText, 
                                                                    TTime* aTime,
                                                                    TDesC* aPrompt,
                                                                    TDesC* aPrompt2,
                                                                    const TTone& aTone)
                                                                    : CAknQueryDialog(aTone)
    {
    TRAPD(ignore, SetPromptL(DesOrNull(aPrompt), DesOrNull(aPrompt2)));
    TRAP(ignore,SetDataL(*aDataText,*aTime));

    iText = aDataText;
    AKNTASHOOK_ADD( this, "CAknMultiLineDataQueryDialog" );
    }

EXPORT_C CAknMultiLineDataQueryDialog::CAknMultiLineDataQueryDialog( TDes* aDataText, 
                                                                    TInt* aNumber,
                                                                    TDesC* aPrompt,
                                                                    TDesC* aPrompt2,
                                                                    const TTone& aTone)
                                                                    : CAknQueryDialog(aTone)
    {
    TRAPD(ignore, SetPromptL(DesOrNull(aPrompt), DesOrNull(aPrompt2)));
    TRAP(ignore,SetDataL(*aDataText,*aNumber));

    iText = aDataText;
    AKNTASHOOK_ADD( this, "CAknMultiLineDataQueryDialog" );
    }

EXPORT_C CAknMultiLineDataQueryDialog::CAknMultiLineDataQueryDialog( TInt* aNumber, 
                                                                    TInt* aNumber2,
                                                                    TDesC* aPrompt,
                                                                    TDesC* aPrompt2,
                                                                    const TTone& aTone)
                                                                    : CAknQueryDialog(aTone)
    {
    TRAPD(ignore, SetPromptL(DesOrNull(aPrompt), DesOrNull(aPrompt2)));
    TRAP(ignore,SetDataL(*aNumber,*aNumber2));
    AKNTASHOOK_ADD( this, "CAknMultiLineDataQueryDialog" );
    }

EXPORT_C CAknMultiLineDataQueryDialog::CAknMultiLineDataQueryDialog( TDes* aDataText, 
                                                                    TTimeIntervalSeconds* aDuration,
                                                                    TDesC* aPrompt,
                                                                    TDesC* aPrompt2,
                                                                    const TTone& aTone)
                                                                    : CAknQueryDialog(aTone)
    {
    TRAPD(ignore, SetPromptL(DesOrNull(aPrompt), DesOrNull(aPrompt2)));
    TRAP(ignore,SetDataL(*aDataText,*aDuration));
    
    iText = aDataText;
    AKNTASHOOK_ADD( this, "CAknMultiLineDataQueryDialog" );
    }

EXPORT_C CAknMultiLineDataQueryDialog::CAknMultiLineDataQueryDialog( TTime* aTime, 
                                                                    TTimeIntervalSeconds* aDuration,
                                                                    TDesC* aPrompt,
                                                                    TDesC* aPrompt2,
                                                                    const TTone& aTone)
                                                                    : CAknQueryDialog(aTone)
    {
    TRAPD(ignore, SetPromptL(DesOrNull(aPrompt), DesOrNull(aPrompt2)));
    TRAP(ignore,SetDataL(*aTime,*aDuration));
    AKNTASHOOK_ADD( this, "CAknMultiLineDataQueryDialog" );
    }
    
EXPORT_C void CAknMultiLineDataQueryDialog::HandlePointerEventL(const TPointerEvent& aPointerEvent) 
    { 
    CAknQueryDialog::HandlePointerEventL(aPointerEvent); 
    }

EXPORT_C void* CAknMultiLineDataQueryDialog::ExtensionInterface( TUid /*aInterface*/ ) 
    { 
    return NULL;
    }

EXPORT_C void CAknMultiLineDataQueryDialog::CEikDialog_Reserved_1()
    {
    }

EXPORT_C void CAknMultiLineDataQueryDialog::CEikDialog_Reserved_2()
    {
    }

EXPORT_C void CAknMultiLineDataQueryDialog::CAknDialog_Reserved()
    {
    }

EXPORT_C void CAknMultiLineDataQueryDialog::CAknQueryDialog_Reserved()
    {
    }


/**************************************
 * CAknIpAddressQueryDialog
 **************************************/


/**
 * Second phase construction required to align API with multiline queries 
 * and because in future MAknQueryData will be used.
 */
EXPORT_C CAknIpAddressQueryDialog* CAknIpAddressQueryDialog::NewL(TInetAddr& aInetAddr, const TTone& aTone)
    {
    CAknIpAddressQueryDialog* self = new (ELeave) CAknIpAddressQueryDialog(aInetAddr, aTone);
    AKNTASHOOK_ADDL( self, "CAknIpAddressQueryDialog" );
    return self;
    }

CAknIpAddressQueryDialog::CAknIpAddressQueryDialog
    (TInetAddr& aInetAddr, const TTone& aTone) : CAknQueryDialog(aTone), iInetAddr(aInetAddr)
    {
    }

EXPORT_C CAknIpAddressQueryDialog::~CAknIpAddressQueryDialog()
    {
    AKNTASHOOK_REMOVE();
    }
EXPORT_C void CAknIpAddressQueryDialog::SetSizeAndPosition(
        const TSize& aSize )
    {
    CAknExtQueryControl* control = STATIC_CAST(CAknExtQueryControl*,QueryControl());    
    if(control)
        {
        if ( AknLayoutUtils::PenEnabled() )
            {
            CAknQueryDialog::SetSizeAndPosition( aSize );
            }
        else
            {
            TRect mainPane;
            AknLayoutUtils::LayoutMetricsRect( AknLayoutUtils::EPopupParent,
                    mainPane );

            TAknWindowLineLayout layout;
            control->WindowLayout( layout );

            CAknPopupHeadingPane* controlHeading = QueryHeading();

            TAknLayoutRect layoutWindow;
            layoutWindow.LayoutRect( mainPane, layout); 
            TRect winRect(layoutWindow.Rect());
                    
            if (controlHeading)
                {
                TAknWindowLineLayout headingL =
                AKN_LAYOUT_WINDOW_popup_list_heading_pane( control->Rect() );

                TAknLayoutRect layoutHeading;
                layoutHeading.LayoutRect(mainPane, headingL); 
                TRect headRect(layoutHeading.Rect());

                winRect.iTl.iY -= headRect.Height();
                }
            
            SetRect(winRect);
            }
        }
    }

EXPORT_C CAknQueryControl* CAknIpAddressQueryDialog::QueryControl() const
    {
    CCoeControl* controlPtr = FindControlOnAnyPageWithControlType(EAknExtCtQuery);
    if (controlPtr)
        return STATIC_CAST(CAknQueryControl*,controlPtr);
/* 
Must have reached limit of pages and lines without finding desired object
This should not happen.
*/
    return 0;
    }

EXPORT_C void CAknIpAddressQueryDialog::PreLayoutDynInitL()
    {
#ifndef RD_NO_DIALOG_BORDERS
    SetBorder(AknBorderId::EAknBorderNotePopup);
#else
    SetBorder(TGulBorder::ENone);
#endif
    SetEditableL( ETrue ) ;
    DoSetPromptL();

    CAknExtQueryControl* control = STATIC_CAST(CAknExtQueryControl*,QueryControl());
    if(control)
        {
        control->SetQueryControlObserver(this);
        control->SetAndUseFlagsL( iFlags );
        control->SetInetAddress(iInetAddr);
        }

    CAknPopupHeadingPane* controlHeading = QueryHeading();
    if (controlHeading)
        { 
        SetLineNonFocusing(EAknMessageQueryHeaderId); 
        
        if (control)
            control->SetNumberOfEditorLines(1);
        }
    // Create CaptionRetriever for FEP
    delete iExtension;
    iExtension = NULL;
    iExtension = new(ELeave) CAknQueryDialogExtension(this);
    }

EXPORT_C void CAknIpAddressQueryDialog::PostLayoutDynInitL()
    {
    CAknExtQueryControl* control = STATIC_CAST(CAknExtQueryControl*,QueryControl());
    if (control)
        control->StartAnimationL();
    }

EXPORT_C TBool CAknIpAddressQueryDialog::NeedToDismissQueryL(const TKeyEvent& /*aKeyEvent*/)
    {   
    return EFalse;
    }

EXPORT_C void CAknIpAddressQueryDialog::SetMinimumAndMaximum( const TInetAddr& aMinimumAddress,                                                              const TInetAddr& aMaximumAddress)
{
    CAknExtQueryControl* control = STATIC_CAST(CAknExtQueryControl*,QueryControl());
    if ( control )
        {
        control->SetMinimumAndMaximum(aMinimumAddress, aMaximumAddress);
        }
}

EXPORT_C TBool CAknIpAddressQueryDialog::OkToExitL(TInt aButtonId)
    {
    if((IsLeftSoftkeyVisible() && 
        (aButtonId == GetLeftCBAShortKeyPress() || aButtonId == EEikBidOk)))
        {   
        CAknExtQueryControl* control = STATIC_CAST(CAknExtQueryControl*,QueryControl());
        if ( control )
            {
            iInetAddr = control->GetInetAddress();
            }
        return ETrue;
        }
    else if(aButtonId == GetRightCBAShortKeyPress())
        {
        return ETrue;
        }
    return EFalse;
    }


EXPORT_C void CAknIpAddressQueryDialog::UpdateLeftSoftKeyL()
    {
    CAknExtQueryControl* control = STATIC_CAST(CAknExtQueryControl*,QueryControl());
    if ( control )
        {
        MakeLeftSoftkeyVisible( control->EditorContentIsValidL() );
        }
    }

EXPORT_C void CAknIpAddressQueryDialog::DoSetPromptL()
    {
    CAknExtQueryControl* control = STATIC_CAST(CAknExtQueryControl*,QueryControl());
    if( control && iPrompt && iPrompt->Length() )
        {
        control->SetPromptL( *iPrompt );
        }
    LayoutAndDraw();
    }


EXPORT_C void CAknIpAddressQueryDialog::CEikDialog_Reserved_1()
    {
    }

EXPORT_C void CAknIpAddressQueryDialog::CEikDialog_Reserved_2()
    {
    }

EXPORT_C void CAknIpAddressQueryDialog::CAknDialog_Reserved()
    {
    }

EXPORT_C void CAknIpAddressQueryDialog::CAknQueryDialog_Reserved()
    {
    }



/**************************************
 * CAknFixedPointQueryDialog
 **************************************/


/**
 * Second phase construction required to align API with multiline queries 
 * and because in future MAknQueryData will be used.
 */
EXPORT_C CAknFixedPointQueryDialog* CAknFixedPointQueryDialog::NewL(TInt& aNumber, const TTone& aTone)
    {
    CAknFixedPointQueryDialog* self = new (ELeave) CAknFixedPointQueryDialog(aNumber, aTone);
    AKNTASHOOK_ADDL( self, "CAknFixedPointQueryDialog" );
    return self;
    }

CAknFixedPointQueryDialog::CAknFixedPointQueryDialog
    (TInt& aNumber, const TTone& aTone) : CAknQueryDialog(aTone), iNumber(aNumber)
    {
    }

EXPORT_C CAknFixedPointQueryDialog::~CAknFixedPointQueryDialog()
    {
    AKNTASHOOK_REMOVE();
    }
EXPORT_C void CAknFixedPointQueryDialog::SetSizeAndPosition(
        const TSize& aSize )
    {
    CAknExtQueryControl* control = STATIC_CAST(CAknExtQueryControl*,QueryControl());    
    if(control)
        {
        if ( AknLayoutUtils::PenEnabled() )
            {
            CAknQueryDialog::SetSizeAndPosition( aSize );
            }
        else
            {
            TRect mainPane;
            AknLayoutUtils::LayoutMetricsRect( AknLayoutUtils::EPopupParent,
                    mainPane );
            
            TAknWindowLineLayout layout;
            control->WindowLayout( layout );

            CAknPopupHeadingPane* controlHeading = QueryHeading();

            TAknLayoutRect layoutWindow;
            layoutWindow.LayoutRect( mainPane, layout); 
            TRect winRect(layoutWindow.Rect());
                    
            if (controlHeading)
                {
                TAknWindowLineLayout headingL =
                    AKN_LAYOUT_WINDOW_popup_list_heading_pane( control->Rect() );

                TAknLayoutRect layoutHeading;
                layoutHeading.LayoutRect(mainPane, headingL); 
                TRect headRect(layoutHeading.Rect());

                winRect.iTl.iY -= headRect.Height();
                }
            
            SetRect(winRect);
            }
        }
    }

EXPORT_C CAknQueryControl* CAknFixedPointQueryDialog::QueryControl() const
    {
    CCoeControl* controlPtr = FindControlOnAnyPageWithControlType(EAknExtCtQuery);
    if (controlPtr)
        return STATIC_CAST(CAknQueryControl*,controlPtr);
/* 
Must have reached limit of pages and lines without finding desired object
This should not happen.
*/
    return 0;
    }

EXPORT_C void CAknFixedPointQueryDialog::PreLayoutDynInitL()
    {
#ifndef RD_NO_DIALOG_BORDERS
    SetBorder(AknBorderId::EAknBorderNotePopup);
#else
    SetBorder(TGulBorder::ENone);
#endif
    SetEditableL( ETrue ) ;
    DoSetPromptL();

    CAknExtQueryControl* control = STATIC_CAST(CAknExtQueryControl*,QueryControl());
    if(control)
        {
        control->SetQueryControlObserver(this);
        control->SetAndUseFlagsL( iFlags );
        control->SetFixedPointNumberL(&iNumber);
        }

    CAknPopupHeadingPane* controlHeading = QueryHeading();
    if (controlHeading)
        { 
        SetLineNonFocusing(EAknMessageQueryHeaderId); 
        
        if (control)
            control->SetNumberOfEditorLines(1);
        }
    // Create CaptionRetriever for FEP
    delete iExtension;
    iExtension = NULL;
    iExtension = new(ELeave) CAknQueryDialogExtension(this);
    }

EXPORT_C void CAknFixedPointQueryDialog::PostLayoutDynInitL()
    {
    CAknExtQueryControl* control = STATIC_CAST(CAknExtQueryControl*,QueryControl());
    if (control)
        control->StartAnimationL();
    }

EXPORT_C TBool CAknFixedPointQueryDialog::NeedToDismissQueryL(const TKeyEvent& /*aKeyEvent*/)
    {   
    return EFalse;
    }

EXPORT_C void CAknFixedPointQueryDialog::SetMinimumAndMaximum( TInt aMinimumValue, TInt aMaximumValue)                                                             
{
    CAknExtQueryControl* control = STATIC_CAST(CAknExtQueryControl*,QueryControl());
    if ( control )
        {
        control->SetMinimumAndMaximum(aMinimumValue, aMaximumValue);
        }
}

EXPORT_C TBool CAknFixedPointQueryDialog::OkToExitL(TInt aButtonId)
    {
    if((IsLeftSoftkeyVisible() && 
        (aButtonId == GetLeftCBAShortKeyPress() || aButtonId == EEikBidOk)))
        {   
        CAknExtQueryControl* control = STATIC_CAST(CAknExtQueryControl*,QueryControl());
        if ( control )
            {
            iNumber = control->GetFixedPointNumber();
            }
        return ETrue;
        }
    else if(aButtonId == GetRightCBAShortKeyPress())
        {
        return ETrue;
        }
    return EFalse;
    }


EXPORT_C void CAknFixedPointQueryDialog::UpdateLeftSoftKeyL()
    {
    CAknExtQueryControl* control = STATIC_CAST(CAknExtQueryControl*,QueryControl());
    if ( control )
        {
        MakeLeftSoftkeyVisible( control->EditorContentIsValidL() );
        }
    }

EXPORT_C void CAknFixedPointQueryDialog::DoSetPromptL()
    {
    CAknExtQueryControl* control = STATIC_CAST(CAknExtQueryControl*,QueryControl());
    if( control && iPrompt && iPrompt->Length() )
        {
        control->SetPromptL( *iPrompt );
        }
    LayoutAndDraw();
    }


EXPORT_C void CAknFixedPointQueryDialog::CEikDialog_Reserved_1()
    {
    }

EXPORT_C void CAknFixedPointQueryDialog::CEikDialog_Reserved_2()
    {
    }

EXPORT_C void CAknFixedPointQueryDialog::CAknDialog_Reserved()
    {
    }

EXPORT_C void CAknFixedPointQueryDialog::CAknQueryDialog_Reserved()
    {
    }



/**************************************
 * CAknMultiLineIpQueryDialog
 **************************************/

EXPORT_C CAknMultiLineIpQueryDialog* CAknMultiLineIpQueryDialog::NewL(TInetAddr&  aAddress1,  TInetAddr&  aAddress2, TTone aTone)
    {return DoNewL(aAddress1,aAddress2,aTone);}



CAknMultiLineIpQueryDialog::CAknMultiLineIpQueryDialog(const TTone& aTone) 
    : CAknMultiLineDataQueryDialog(aTone)
    {
    AKNTASHOOK_ADD( this, "CAknMultiLineIpQueryDialog" );
    }

EXPORT_C CAknMultiLineIpQueryDialog::~CAknMultiLineIpQueryDialog()
    {
    AKNTASHOOK_REMOVE();
    delete iFirstData;
    delete iSecondData;

    delete iSecondPrompt;
    }

EXPORT_C CAknExtMultilineQueryControl* CAknMultiLineIpQueryDialog::FirstControl() const
    {
/* It's very important that lineIndexCount and pageIndexCount are zero initialised 
as they are used as a way of remembering how far the search has traversed.
*/
    TInt lineIndexCount=0;
    TInt pageIndexCount=0;

    FOREVER
        {
        CCoeControl* controlPtr = FindControlOnAnyPageWithControlType(EAknExtCtMultilineQuery,&lineIndexCount,&pageIndexCount);
        if (controlPtr)
            {
            CAknExtMultilineQueryControl* multiLineQueryPtr = STATIC_CAST(CAknExtMultilineQueryControl*,controlPtr);
            if (multiLineQueryPtr->IsFirst())
                return multiLineQueryPtr;
/*
if not the first line, continue searching, with new values of lineIndexCount and pageIndexCount 
updated by FindControlOnAnyPageWithControlType
*/
            }
        else 
/* 
Must have reached limit of pages and lines without finding desired object
This should not happen.
*/
            return 0;
        }
    }

EXPORT_C CAknExtMultilineQueryControl* CAknMultiLineIpQueryDialog::SecondControl() const
    {
/* It's very important that lineIndexCount and pageIndexCount are zero initialised 
as they are used as a way of remembering how far the search has traversed.
*/
    TInt lineIndexCount=0;
    TInt pageIndexCount=0;

    FOREVER
        {
        CCoeControl* controlPtr = FindControlOnAnyPageWithControlType(EAknExtCtMultilineQuery,&lineIndexCount,&pageIndexCount);
        if (controlPtr)
            {
            CAknExtMultilineQueryControl* multiLineQueryPtr = STATIC_CAST(CAknExtMultilineQueryControl*,controlPtr);
            if (multiLineQueryPtr->IsSecond())
                return multiLineQueryPtr;
/*
if not the second line, continue searching, with new values of lineIndexCount and pageIndexCount 
updated by FindControlOnAnyPageWithControlType
*/
            }
        else 
/* 
Must have reached limit of pages and lines without finding desired object
This should not happen.
*/
            return 0;
        }
    }
    
EXPORT_C CAknQueryControl* CAknMultiLineIpQueryDialog::QueryControl() const
    {
    if (!FirstLineEnabled() && SecondLineEnabled()) return SecondControl();
    return FirstControl();  
    }


EXPORT_C void CAknMultiLineIpQueryDialog::PreLayoutDynInitL()
    {
    if (Layout_Meta_Data::IsLandscapeOrientation())
        {
        SetBorder( TGulBorder::ENone );
        }
    SetEditableL( ETrue );
    DoSetPromptL();
    
    CAknExtMultilineQueryControl* firstControl = FirstControl();
    CAknExtMultilineQueryControl* secondControl = SecondControl();

    firstControl->SetNbrOfPromptLines( firstControl->NbrOfPromptLines(), 
                                        secondControl->NbrOfPromptLines());
    secondControl->SetNbrOfPromptLines( firstControl->NbrOfPromptLines(), 
                                        secondControl->NbrOfPromptLines());
    
    firstControl->SetQueryControlObserver(this);
    secondControl->SetQueryControlObserver(this);
    
    // Set and then deploy the flags
    firstControl->SetAndUseFlagsL( iFlags );
    secondControl->SetAndUseFlagsL( iFlags );

    if (iFirstData) 
        iFirstData->SetL(firstControl,iFirstEditorMaxLength);
    if (iSecondData) 
        iSecondData->SetL(secondControl,iSecondEditorMaxLength);

    UpdateLeftSoftKeyL();
    HandleOrientationSwitch();
    
    // Create CaptionRetriever for FEP
    delete iExtension;
    iExtension = NULL;
    iExtension = new(ELeave) CAknMultilineQueryDialogExtension(this, CAknMultilineQueryDialogExtension::EMultIPQuery);
    }

EXPORT_C void CAknMultiLineIpQueryDialog::SetPromptL(const TDesC& aFP, const TDesC& aSP)
//
//Store new prompt values
//
    {
    delete iSecondPrompt;
    iSecondPrompt = NULL;
    iSecondPrompt = aSP.AllocL();

    CAknQueryDialog::SetPromptL(aFP);
    }

EXPORT_C void CAknMultiLineIpQueryDialog::DoSetPromptL()
    {
//
//Set prompt in query controls, called during layout
//
    CAknExtMultilineQueryControl* secondControl = SecondControl();
    if(secondControl && iSecondPrompt && iSecondPrompt->Length())
        secondControl->SetPromptL(*iSecondPrompt);
    
    CAknQueryDialog::DoSetPromptL();        
    }

/**
* Allows dismissing of queries. Same as base class implementation only take into
* consideration both controls
*/
EXPORT_C TBool CAknMultiLineIpQueryDialog::NeedToDismissQueryL(const TKeyEvent& /*aKeyEvent*/)
    {
    return EFalse;
    }

TBool CAknMultiLineIpQueryDialog::FirstLineEnabled() const
    {
    return (CurrentLine()==0 && Layout_Meta_Data::IsLandscapeOrientation())
        || !Layout_Meta_Data::IsLandscapeOrientation();
    }

TBool CAknMultiLineIpQueryDialog::SecondLineEnabled() const
    {
    return (CurrentLine()==1 && Layout_Meta_Data::IsLandscapeOrientation())
        || !Layout_Meta_Data::IsLandscapeOrientation();
    }

TInt CAknMultiLineIpQueryDialog::CurrentLine() const
    {
    CAknExtMultilineQueryControl* secondControl = SecondControl();
    CAknExtMultilineQueryControl* firstControl = FirstControl();  
   
    if (firstControl && firstControl->IsFocused()) return 0;
    if (secondControl && secondControl->IsFocused()) return 1;
    return -1;
    }

void CAknMultiLineIpQueryDialog::HandleOrientationSwitch()
    {
    TBool firstLineEnabled = FirstLineEnabled();
    TBool secondLineEnabled = SecondLineEnabled();

    if (!firstLineEnabled && !secondLineEnabled)
    {
	    TInt controlID = IdOfFocusControl();
	    if (controlID  )
	    {
	        Line(controlID)->SetFocus(ETrue, EDrawNow);
	    	firstLineEnabled = FirstLineEnabled();
	        secondLineEnabled = SecondLineEnabled();
	    }	
	    else
	        firstLineEnabled = ETrue;      
    }

    CAknExtMultilineQueryControl* firstControl = FirstControl();
    CAknExtMultilineQueryControl* secondControl = SecondControl();	
    TInt line1 = FindLineIndex(*firstControl);
    TInt line2 = FindLineIndex(*secondControl);
    CEikCaptionedControl *ctrl1 = GetLineByLineAndPageIndex(line1, 0);
    CEikCaptionedControl *ctrl2 = GetLineByLineAndPageIndex(line2, 0);
    ctrl1->SetLatent(!firstLineEnabled);
    ctrl2->SetLatent(!secondLineEnabled);
    ctrl1->SetLatentGroupLineFollows(!firstLineEnabled);
    ctrl2->SetLatentGroupLineFollows(!secondLineEnabled);
    ctrl1->SetFocusing(firstLineEnabled);
    ctrl2->SetFocusing(secondLineEnabled);
    Layout();
    }


EXPORT_C void CAknMultiLineIpQueryDialog::HandleResourceChange(TInt aType)
    {
    CAknQueryDialog::HandleResourceChange(aType);
    if (aType == KEikDynamicLayoutVariantSwitch)
        {
        if (Layout_Meta_Data::IsLandscapeOrientation())
            {
            SetBorder( TGulBorder::ENone );
            }        	
        else
            {
            SetBorder(AknBorderId::EAknBorderNotePopup);
            }
        HandleOrientationSwitch();
        TRAP_IGNORE( UpdateLeftSoftKeyL() );
        }
    }
    
EXPORT_C TBool CAknMultiLineIpQueryDialog::OkToExitL(TInt aButtonId)
    {    
    if (Layout_Meta_Data::IsLandscapeOrientation())
        {
        if((IsLeftSoftkeyVisible() && (aButtonId == GetLeftCBAShortKeyPress() || aButtonId == EEikBidOk)))
            {
            CAknExtMultilineQueryControl* firstControl = FirstControl();
            CAknExtMultilineQueryControl* secondControl = SecondControl();
            if ( FirstControl()->EditorContentIsValidL() && FirstControl()->IsFocused() )
        	    {
                if ( !CAknEnv::Static()->TransparencyEnabled() )
                    {
                    // Another case of missed RD_NO_DIALOG_BORDERS?  	    
        	        SetBorder(AknBorderId::EAknBorderNotePopup);
                    }
                TInt line1 = FindLineIndex(*firstControl);
                TInt line2 = FindLineIndex(*secondControl);
                CEikCaptionedControl *ctrl1 = GetLineByLineAndPageIndex(line1, 0);
                CEikCaptionedControl *ctrl2 = GetLineByLineAndPageIndex(line2, 0);
                ctrl1->SetFocusing(ETrue);
                ctrl2->SetFocusing(ETrue);
                TryChangeFocusToL(ctrl2->iId);
                HandleOrientationSwitch();
				
				UpdateLeftSoftKeyL();
       		    return EFalse;      		        		        		
        		}
        	}
    	}
    	
    if((IsLeftSoftkeyVisible() && 
        (aButtonId == GetLeftCBAShortKeyPress() || aButtonId == EEikBidOk)))
        {
        CAknExtMultilineQueryControl* firstControl = FirstControl();
        CAknExtMultilineQueryControl* secondControl = SecondControl();
        
        if (iFirstData)
            iFirstData->Get(firstControl);
        
        if(iSecondData)
            iSecondData->Get(secondControl);

        if(iText)
            {
            TInt len = firstControl->GetTextLength();
            if ( len != -1 )
                {
                HBufC* text = HBufC::NewLC( len );
                TPtr ptext = text->Des();
                firstControl->GetText( ptext );
                iText->Copy( *text );                        
                CleanupStack::PopAndDestroy( text );   	
                }
            }
        if(iSecondText)
            {
            TInt len = secondControl->GetTextLength();
            if ( len != -1 )
                {
                HBufC* text = HBufC::NewLC( len );
                TPtr ptext = text->Des();
                secondControl->GetText( ptext );
                iSecondText->Copy( *text );                        
                CleanupStack::PopAndDestroy( text );   	
                }
            }

        return ETrue;
        }
    else if(aButtonId == GetRightCBAShortKeyPress())
        {
        return ETrue;
        }
    
    return EFalse;
    }

EXPORT_C void CAknMultiLineIpQueryDialog::UpdateLeftSoftKeyL()
    {
    if (Layout_Meta_Data::IsLandscapeOrientation())
    	{
	    if ( (FirstControl()->EditorContentIsValidL() && FirstControl()->IsFocused()) || (SecondControl()->EditorContentIsValidL() && FirstControl()->EditorContentIsValidL()) )
	        {
	        MakeLeftSoftkeyVisible(ETrue);
	        }
	    else
	        {
	        MakeLeftSoftkeyVisible(EFalse);
	        }	        
	    return;
    	}
    	
    if ( FirstControl()->EditorContentIsValidL() &&
         SecondControl()->EditorContentIsValidL())
        {
        MakeLeftSoftkeyVisible(ETrue);
        }
    else
        {
        MakeLeftSoftkeyVisible(EFalse);
        }
    }

EXPORT_C void CAknMultiLineIpQueryDialog::SetMaxLengthOfFirstEditor(TInt aFirstTextEditorMaxLength)
    {
    iFirstEditorMaxLength = aFirstTextEditorMaxLength;
    };

EXPORT_C void CAknMultiLineIpQueryDialog::SetMaxLengthOfSecondEditor(TInt aSecondTextEditorMaxLength)
    {
    iSecondEditorMaxLength=aSecondTextEditorMaxLength;
    };

EXPORT_C TPtrC CAknMultiLineIpQueryDialog::SecondPrompt() const
    { if (iSecondPrompt)
        return *iSecondPrompt; 
      return TPtrC(0,0);    
    }

// End of File
