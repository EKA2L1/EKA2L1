.include "../../../priv/inc/sv.S"
.global ETlsCreate
.global ETlsDestroy
.global ETlsCommand
ETlsCreate:
    CallHleDispatch 0xC0
ETlsDestroy:
    CallHleDispatch 0xC1
ETlsCommand:
    CallHleDispatch 0xC2
