.include "../../../priv/inc/sv.S"

.global EHUIOpenGlobalTextView
.global EHUIGetStoredText
.global EHUICancelGlobalTextView

EHUIOpenGlobalTextView:
    CallHleDispatch 0xA0

EHUIGetStoredText:
    CallHleDispatch 0xA1

EHUICancelGlobalTextView:
    CallHleDispatch 0xA2
