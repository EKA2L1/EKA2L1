.include "../../../priv/inc/sv.S"

.global EHUIOpenGlobalTextView
.global EHUIGetStoredText
.global EHUICancelGlobalTextView
.global EHUIIsKeypadBased
.global EHUIIsManualInput
.global EHUISetInputAvailable

EHUIOpenGlobalTextView:
    CallHleDispatch 0xA0

EHUIGetStoredText:
    CallHleDispatch 0xA1

EHUICancelGlobalTextView:
    CallHleDispatch 0xA2

EHUIIsKeypadBased:
    CallHleDispatch 0xA3

EHUIIsManualInput:
    CallHleDispatch 0xA4

EHUISetInputAvailable:
    CallHleDispatch 0xA5
