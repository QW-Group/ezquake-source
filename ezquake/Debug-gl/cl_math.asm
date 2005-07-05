 .386P
 .model FLAT
 externdef _vright:dword
 externdef _vup:dword
 externdef _vpn:dword
_DATA SEGMENT
_DATA ENDS
_TEXT SEGMENT
 public _Invert24To16
_Invert24To16:
 mov ecx,ds:dword ptr[4+esp]
 mov edx,0100h
 cmp ecx,edx
 jle LOutOfRange
 sub eax,eax
 div ecx
 ret
LOutOfRange:
 mov eax,0FFFFFFFFh
 ret
 align 2
 public _TransformVector
_TransformVector:
 mov eax,ds:dword ptr[4+esp]
 mov edx,ds:dword ptr[8+esp]
 fld ds:dword ptr[eax]
 fmul ds:dword ptr[_vright]
 fld ds:dword ptr[eax]
 fmul ds:dword ptr[_vup]
 fld ds:dword ptr[eax]
 fmul ds:dword ptr[_vpn]
 fld ds:dword ptr[4+eax]
 fmul ds:dword ptr[_vright+4]
 fld ds:dword ptr[4+eax]
 fmul ds:dword ptr[_vup+4]
 fld ds:dword ptr[4+eax]
 fmul ds:dword ptr[_vpn+4]
 fxch st(2)
 faddp st(5),st(0)
 faddp st(3),st(0)
 faddp st(1),st(0)
 fld ds:dword ptr[8+eax]
 fmul ds:dword ptr[_vright+8]
 fld ds:dword ptr[8+eax]
 fmul ds:dword ptr[_vup+8]
 fld ds:dword ptr[8+eax]
 fmul ds:dword ptr[_vpn+8]
 fxch st(2)
 faddp st(5),st(0)
 faddp st(3),st(0)
 faddp st(1),st(0)
 fstp ds:dword ptr[8+edx]
 fstp ds:dword ptr[4+edx]
 fstp ds:dword ptr[edx]
 ret
_TEXT ENDS
 END
