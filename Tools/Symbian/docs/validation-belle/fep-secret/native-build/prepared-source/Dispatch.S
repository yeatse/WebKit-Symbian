.include "/workspace/symbian-fep-secret-native-build-20261101/work/source/sv.s"

.global EHUIOpenGlobalTextView
.global EHUIOpenGlobalTextViewWithSecret
.global EHUIGetStoredText
.global EHUICancelGlobalTextView
.global EHUIIsKeypadBased
.global EHUIIsManualInput
.global EHUISetInputAvailable

.type EHUIOpenGlobalTextView, %function
EHUIOpenGlobalTextView:
    CallHleDispatch 0xA0
.size EHUIOpenGlobalTextView, .-EHUIOpenGlobalTextView

.type EHUIGetStoredText, %function
EHUIGetStoredText:
    CallHleDispatch 0xA1
.size EHUIGetStoredText, .-EHUIGetStoredText

.type EHUICancelGlobalTextView, %function
EHUICancelGlobalTextView:
    CallHleDispatch 0xA2
.size EHUICancelGlobalTextView, .-EHUICancelGlobalTextView

.type EHUIIsKeypadBased, %function
EHUIIsKeypadBased:
    CallHleDispatch 0xA3
.size EHUIIsKeypadBased, .-EHUIIsKeypadBased

.type EHUIIsManualInput, %function
EHUIIsManualInput:
    CallHleDispatch 0xA4
.size EHUIIsManualInput, .-EHUIIsManualInput

.type EHUISetInputAvailable, %function
EHUISetInputAvailable:
    CallHleDispatch 0xA5
.size EHUISetInputAvailable, .-EHUISetInputAvailable

// Explicit secret-text request; A0 remains the original four-argument ABI.
.type EHUIOpenGlobalTextViewWithSecret, %function
EHUIOpenGlobalTextViewWithSecret:
    CallHleDispatch 0xA6
.size EHUIOpenGlobalTextViewWithSecret, .-EHUIOpenGlobalTextViewWithSecret
