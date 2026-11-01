/*
* Copyright (c) 1997-1999 Nokia Corporation and/or its subsidiary(-ies).
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
*
*/


#if !defined(__EIKSECED_H__)
#define __EIKSECED_H__

#if !defined(__EIKDEF_H__)
#include <eikdef.h>
#endif

#if !defined(__EIKBCTRL_H__)
#include <eikbctrl.h>
#endif

#include <fepbase.h>

// FORWARD DECLARATIONS
class TResourceReader;
class CEikSecretEditorExtension;
class MAknsControlContext;

/**
 * A single line text editor which displays an asterisk for every
 * character entered. 
 * 
 * The editor is suitable for editing confidential information such as
 * a password. You can paste to the editor but no other clipboard
 * functions are available.
 * 
 * The editor has an associated resource struct SECRETED and control
 * factory identifier EEikCtSecretEd.
 */
class CEikSecretEditor : public CEikBorderedControl, public MCoeFepAwareTextEditor, public MCoeFepAwareTextEditor_Extension1
	{
public:
    enum { 
        /**
         * The maximum number of characters in the displayed
         * text buffer.
         */
        EMaxSecEdBufLength = 32,
        /**
         * Indicates the maximum number of characters which may be entered into
         * the editor. 
         * 
         * Editors additionally have a maximum length, see SetMaxLength(),
         * which must not exceed this value.
         */
        EMaxSecEdSecArrayLength = 255
        };
            
    /**
     * Settable features for Secret Editor. See SetFeature().
     *
     * @since S60 5.0
     */
    enum TFeatureId
        {
        /** Disables VKB. Non-zero (or ETrue) parameter disables VKB, 
            0 (or EFalse) enables VKB. When disabled, 
            editor doesn't request PenInputServer to start VKB */ 
        EDisablePenInput,
        
        ELaunchPenInputAutomatic,
        
        EPartialScreenInput
        };    
        
        
public:
    /**
     * Default constructor.
     * 
     * This constructs a editor with a border type of
     * TGulBorder::ESingleGray.
     * 
     * This function should be used as the first stage in two stage
     * construction, followed by a call to either ConstructFromResourceL() or
     * SetMaxLength().
     */
    IMPORT_C CEikSecretEditor();

    /**
     * Destructor.
     */
    IMPORT_C ~CEikSecretEditor();
    
public:    // from CCoeControl
    /**
     * Second-phase construction from a resource file. 
     * 
     * This function reads the number of characters which may be entered by
     * the user into the editor from a SECRETED resource. You should call
     * either this function or SetMaxLength() before drawing the editor and
     * offering key events, otherwise the editor will not accept text input.
     * 
     * @param aReader A resource file reader.
     * @panic EIKCTL 13  If the value read from resource is greater than
     * EMaxSecEdLength.
     */
    IMPORT_C virtual void ConstructFromResourceL(TResourceReader& aReader);
    
    /**
     * Gets the editor's minimum size in pixels.
     * 
     * Overrides CCoeControl::MinimumSize().
     * 
     * @return The editor's minimum size in pixels.
     */
    IMPORT_C virtual TSize MinimumSize();
    
    /**
     * Responds to key presses.
     * 
     * Overrides CCoeControl::OfferKeyEventL().
     * 
     * @param aKeyEvent The key event.
     * @param aType Not used.
     * @return Indicates whether or not the key event was consumed.
     */
    IMPORT_C virtual TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent,TEventCode aType);
    
    /**
     * Gets the list of logical colours used to draw the editor.
     * 
     * Overrides CCoeControl::GetColorUseListL().
     * 
     * @param aColorUseList On return, the colour list.
     */
    IMPORT_C virtual void GetColorUseListL(CArrayFix<TCoeColorUse>& aColorUseList) const; // not available before Release 005u
    
    /**
     * Handles a change to the editor's resources. 
     * 
     * The types of resources handled are those which are shared across the
     * environment, e.g. colours or fonts.
     * 
     * Calls CEikBorderedControl::HandleResourceChange().
     * 
     * @param aType A message UID value.
     */
    IMPORT_C virtual void HandleResourceChange(TInt aType);    // not available before Release 005u

    /**
     * Gets the editor's input capabilities.
     * 
     * Overrides CCoeControl::InputCapabilities().
     * 
     * If this function is overrided in a subclass, the subclass should
     * obtain this class' InputCapabilities' object provider through
     * TCoeInputCapabilities::ObjectProvider() and set that as a part of
     * the subclass' InputCapabilities object provider chain to ensure
     * maximum functionality.
     * 
     * @return The editor's input capabilities. The ESecretText and
     * ENavigation flags are set.
     */
    IMPORT_C virtual TCoeInputCapabilities InputCapabilities() const;

    /**
     * From @c CCoeControl.
     * 
     * Handles pointer events.
     *
     * @param aPointerEvent The pointer event.
     */
    IMPORT_C void HandlePointerEventL(const TPointerEvent& aPointerEvent);
    
    IMPORT_C void FocusChanged( TDrawNow aDrawNow );
    
public:    // specific
    /**
     * Gets the editor's text string.
     * 
     * This may be called from the container class when the model data
     * should be updated according to the text contained in the editor.
     * 
     * @param aText On return, the editor's text. This is what is stored in
     * memory, not what is displayed. This descriptor must be capable of
     * holding the maxlength of the internal buffer.
     * 
     */
    IMPORT_C void GetText(TDes& aText) const;

    /**
     * Set the contents of the character buffer.
     *
     * @param aText The text to be set as the contents of the character buffer.
     */
    IMPORT_C virtual void SetText(const TDesC& aText);
    
    /**
     * Set the initial display of the secret editor.
     * Calling this routine doesn't affect the actual character buffer.
     * 
     * @param aNumberOfCharacters Must be >=0 and <= EMaxSecEdBufLength.
     */
    IMPORT_C virtual void InitializeDisplay(TInt aNumberOfChars);
    
    /**
     * Clears the editor's text string removing any trace of it from memory.
     */
    IMPORT_C void Reset();

    /**
     * Set the maximum length of the editor.
     * 
     * This is the maximum number of characters which may be entered by the
     * user into the editor.
     * 
     * @param aMaxLength The maximum number of characters which may be
     * entered.
     * @panic EIKCTL 13 If aMaxLength is greater than EMaxSecEdLength.
     * 
     */
    IMPORT_C void SetMaxLength(TInt aMaxLength);

    /**
     * Sets the font that is used to draw the text in this editor.
     * 
     * @param aFont A pointer to a CFont object that is used to draw the
     * text in this editor.
     */
    IMPORT_C void AknSetFont(const CFont &aFont);

    /**
     * Sets the alignment of the editor.
     *
     * @param aAlign The editor's alignment.
     */
    IMPORT_C void AknSetAlignment(const CGraphicsContext::TTextAlign &aAlign);

    /**
     * Changes default and current input mode of a secret editor. By default
     * input mode in the secret editor is alpha input mode 
     * (EAknEditorSecretAlphaInputMode). By using this method the default input
     * mode can be changed to numeric input mode (EAknEditorNumericInputMode).
     *
     * @since 2.1
     * @param aInputMode Default input mode in the secret editor.  
     *   Only EAknEditorNumericInputMode and EAknEditorSecretAlphaInputMode
     *   are supported.
     */
    IMPORT_C void SetDefaultInputMode(TInt aInputMode);

    /**
     * Returns the current maximum length of the internal buffer.
     *
     * @return the current maximum length of the internal buffer.
     */
    IMPORT_C TInt MaxLength() const;

    /**
     * Access method for getting at the buffer without copying out from it.
     *
     * @return Non-modifiable descriptor reference to the contained buffer.
     */
    IMPORT_C const TDesC& Buffer() const;

    /**
     * Method to reveal (or hide) the currently input editor contents.
     *
     * @param aReveal - if ETrue, reveals the contents; if EFalse hides them.
     */
    IMPORT_C void RevealSecretText( TBool aReveal );

    /**
     * Method for enabling/disabling special character table from 
     * the secret editor. By default the SCT is enabled in the secret
     * editors.
     * ConstructFromResourceL need to be called before this method.
     * 
     * @since 3.2
     *
     * @param aEnable Boolean value for enabling or disabling the special
     *   character table.
     */
    IMPORT_C void EnableSCT( TBool aEnable = ETrue );

    /**
     * Set the skin control context to be used with this secret editor. If this
     * method is not called, the control context available through MOP (if any)
     * is used. If @c NULL control context is specified, no control context is
     * used at all.
     *
     * @since 2.0
     *
     * @param aContext The background control context used to draw the
     *   background of this editor or @c NULL. Ownership is not transferred.
     */
    IMPORT_C void SetSkinBackgroundControlContextL( MAknsControlContext* aContext );
    
    // for cursor support
    TPoint CursorPos();
    IMPORT_C void EnableCursor( TBool aEnable );    

protected:    // inherited
    // methods to enable CAknNumericSecretEditor to inherit from CEikSecretEditor
    /**
     * Appends a character to the editor.
     *
     * @param aKeyCode The character to append.
     */
    IMPORT_C virtual void AppendCharacterL( TInt aKeyCode );
    
    /**
     * Updates and redraws the editor.
     */
    IMPORT_C virtual void Update();
    
    void InsertSecretChar();
    
    /**
     * Responds to changes to the size and position of
     * the contents of this control.
     * 
     * Overrides CCoeControl::SizeChanged().
     */
    IMPORT_C void SizeChanged();
    
private: // virtual
    IMPORT_C virtual void Draw(const TRect& aRect) const;

private: // from MCoeFepAwareTextEditor
    IMPORT_C void StartFepInlineEditL(const TDesC& aInitialInlineText, TInt aPositionOfInsertionPointInInlineText, TBool aCursorVisibility, const MFormCustomDraw* aCustomDraw, MFepInlineTextFormatRetriever& aInlineTextFormatRetriever, MFepPointerEventHandlerDuringInlineEdit& aPointerEventHandlerDuringInlineEdit);
    IMPORT_C void UpdateFepInlineTextL(const TDesC& aNewInlineText, TInt aPositionOfInsertionPointInInlineText);
    IMPORT_C void SetInlineEditingCursorVisibilityL(TBool aCursorVisibility);
    IMPORT_C void CancelFepInlineEdit();
    IMPORT_C TInt DocumentLengthForFep() const;
    IMPORT_C TInt DocumentMaximumLengthForFep() const;
    IMPORT_C void SetCursorSelectionForFepL(const TCursorSelection& aCursorSelection);
    IMPORT_C void GetCursorSelectionForFep(TCursorSelection& aCursorSelection) const;
    IMPORT_C void GetEditorContentForFep(TDes& aEditorContent, TInt aDocumentPosition, TInt aLengthToRetrieve) const;
    IMPORT_C void GetFormatForFep(TCharFormat& aFormat, TInt aDocumentPosition) const;
    IMPORT_C void GetScreenCoordinatesForFepL(TPoint& aLeftSideOfBaseLine, TInt& aHeight, TInt& aAscent, TInt aDocumentPosition) const;
    IMPORT_C void DoCommitFepInlineEditL();
    IMPORT_C MCoeFepAwareTextEditor_Extension1* Extension1(TBool& aSetToTrue);
    IMPORT_C void SetStateTransferingOwnershipL(CState* aState, TUid aTypeSafetyUid);
    IMPORT_C CState* State(TUid aTypeSafetyUid); // this function does *not* transfer ownership

private:
    void OverflowAlert();

private: // from CCoeControl
    IMPORT_C void Reserved_1();
    IMPORT_C void Reserved_2();
private:
    /**
    * From CAknControl
    */
    IMPORT_C void* ExtensionInterface( TUid aInterface );
private: // from MCoeFepAwareTextEditor
    IMPORT_C void MCoeFepAwareTextEditor_Reserved_2();
private:  // from MCoeFepAwareTextEditor_Extension1
    IMPORT_C virtual void MCoeFepAwareTextEditor_Extension1_Reserved_2();
    IMPORT_C virtual void MCoeFepAwareTextEditor_Extension1_Reserved_3();
    IMPORT_C virtual void MCoeFepAwareTextEditor_Extension1_Reserved_4();

private: // specific
    void InsertChar();
    TInt CharsFitOnEditor() const;
    IMPORT_C virtual MCoeFepAwareTextEditor_Extension1::CState* CreateFepStateL();
    void CalculateAscent();
    void ReportUpdate();
    
    // for cursor support    
    void SetCursorFormat();    
    void UpdateCursor();
    void InitCRedirectionListenerL();
    
private:
    typedef TBuf<EMaxSecEdBufLength> TSecEdBuf;
protected:
    HBufC* iSecCharArr;
    TInt iSecPos;
    TSecEdBuf iBuf;
    TInt iMaxLen;
    TInt iCharWidth;
    TInt iAscent;
    TBool iBufferFull;

public: // timer methods
    void StartTimer();
private:
    static TInt TimerCallback(TAny* aThis);
private:
    CPeriodic* iTimer;
    const CFont *iFont;
    CGraphicsContext::TTextAlign iAlign;
    TBool iRevealSecretText; // holds the state of whether text is shown or not
    CEikSecretEditorExtension* iExtension;
    CState* iFepState;
    HBufC* iInlineEditText;

public:
    /**
     * Sets the colors for text and background as skin IDs.
     * 
     * @since 3.0
     * @param aAknSkinIDForTextColor ID for text color.
     * @param aAknSkinIDForBgColor ID for background color.
     */
    IMPORT_C void SetSkinTextColorL(TInt aAknSkinIDForTextColor, TInt aAknSkinIdForBgColor=KErrNotFound);
    
    /**
     * Used for setting various flag-like features to the editor.
     *
     * @param aFeatureId The feature id, see TFeatureId
     * @param aFeatureParam The feature parameter. This is usually
     *                      enabled or disabled. For more info, see
     *                      the feature documentation in TFeatureId.
     * @return KErrNone if the feature modification succeeded
     * @since S60 5.0
     **/
    IMPORT_C TInt SetFeature( TInt aFeatureId, TInt aFeatureParam );
     
    /**
     * Used to getting feature statuses.
     *
     * @param aFeatureId The feature id, see TFeatureId
     * @param aFeatureParam On return, the parameter for the feature
     *                      (usually non-zero for an enabled feature
     *                      and zero for disabled)
     * @return KErrNone if the feature is supported and fetching its value
     *                  succeeded
     * @since S60 5.0
     */
    IMPORT_C TInt GetFeature( TInt aFeatureId, TInt& aFeatureParam ) const;

    /**
     * Used for checking if the editor supports a feature.
     * For features, see TFeatureId.
     *
     * @param aFeatureId
     * @return ETrue if the feature is supported
     * @since S60 5.0
     */
    IMPORT_C TBool SupportsFeature( TInt aFeatureId ) const;    
    };

#endif
