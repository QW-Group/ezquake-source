 .386P
 .model FLAT
 externdef _vright:dword
 externdef _vup:dword
 externdef _vpn:dword
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
 public ceil_cw, single_cw, full_cw, cw, pushed_cw
ceil_cw dd 0
single_cw dd 0
full_cw dd 0
cw dd 0
pushed_cw dd 0
_DATA ENDS
_TEXT SEGMENT
 public _Sys_LowFPPrecision
_Sys_LowFPPrecision:
 fldcw ds:word ptr[single_cw]
 ret
 public _Sys_HighFPPrecision
_Sys_HighFPPrecision:
 fldcw ds:word ptr[full_cw]
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
 and ah,0F0h
 or ah,00Ch
 mov ds:dword ptr[single_cw],eax
 and ah,0F0h
 or ah,008h
 mov ds:dword ptr[ceil_cw],eax
 ret
_TEXT ENDS
 END
