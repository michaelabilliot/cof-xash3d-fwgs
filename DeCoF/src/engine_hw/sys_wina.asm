; sys_wina.s converted to MASM by gas2masm (Quake WinQuake sys_wina.s)
 .386P
 .model FLAT
_DATA SEGMENT
 align 4
fpenv:
 dd 0, 0, 0, 0, 0, 0, 0, 0
_DATA ENDS
_TEXT SEGMENT
 public _MaskExceptions
_MaskExceptions:
 fnstenv ds:dword ptr[fpenv]
 or ds:dword ptr[fpenv],03Fh
 fldenv ds:dword ptr[fpenv]
 ret
_TEXT ENDS
_DATA SEGMENT
 align 4
 public ceil_cw, single_cw, full_cw, high_cw_guess, cw, pushed_cw, spare_cw_guess
ceil_cw dd 0
single_cw dd 0
full_cw dd 0
; GoldSrc: Sys_HighFPPrecision loads its own copy of the full precision word
high_cw_guess dd 0
cw dd 0
pushed_cw dd 0
; GoldSrc: never referenced
spare_cw_guess dd 0
_DATA ENDS
_TEXT SEGMENT
 public _Sys_LowFPPrecision
_Sys_LowFPPrecision:
 fldcw ds:word ptr[single_cw]
 ret
 public _Sys_HighFPPrecision
_Sys_HighFPPrecision:
 fldcw ds:word ptr[high_cw_guess]
 ret
 public _Sys_PushFPCW_SetHigh
_Sys_PushFPCW_SetHigh:
 fnstcw ds:word ptr[pushed_cw]
 fldcw ds:word ptr[full_cw]
 ret
 public _Sys_PopFPCW
_Sys_PopFPCW:
 fldcw ds:word ptr[pushed_cw]
 ret
 public _Sys_SetFPCW
_Sys_SetFPCW:
 fnstcw ds:word ptr[cw]
 mov eax,ds:dword ptr[cw]
 and ah,0F0h
 or ah,003h
 mov ds:dword ptr[full_cw],eax
 mov ds:dword ptr[high_cw_guess],eax
 and ah,0F0h
 or ah,00Ch
 mov ds:dword ptr[single_cw],eax
 and ah,0F0h
 or ah,008h
 mov ds:dword ptr[ceil_cw],eax
 ret
; GoldSrc: empty, called by Sys_Init after Sys_CheckOSVersion
 public _Sys_InitFPU_guess
_Sys_InitFPU_guess:
 ret
_TEXT ENDS
 END
