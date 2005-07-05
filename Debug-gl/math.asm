 .386P
 .model FLAT
 externdef _vright:dword
 externdef _vup:dword
 externdef _vpn:dword
_DATA SEGMENT
 align 4
Ljmptab dd Lcase0, Lcase1, Lcase2, Lcase3
 dd Lcase4, Lcase5, Lcase6, Lcase7
_DATA ENDS
_TEXT SEGMENT
 externdef _BOPS_Error:dword
 align 2
 public _BoxOnPlaneSide
_BoxOnPlaneSide:
 push ebx
 mov edx,ds:dword ptr[4+12+esp]
 mov ecx,ds:dword ptr[4+4+esp]
 xor eax,eax
 mov ebx,ds:dword ptr[4+8+esp]
 mov al,ds:byte ptr[17+edx]
 cmp al,8
 jge Lerror
 fld ds:dword ptr[0+edx]
 fld st(0)
 jmp dword ptr[Ljmptab+eax*4]
Lcase0:
 fmul ds:dword ptr[ebx]
 fld ds:dword ptr[0+4+edx]
 fxch st(2)
 fmul ds:dword ptr[ecx]
 fxch st(2)
 fld st(0)
 fmul ds:dword ptr[4+ebx]
 fld ds:dword ptr[0+8+edx]
 fxch st(2)
 fmul ds:dword ptr[4+ecx]
 fxch st(2)
 fld st(0)
 fmul ds:dword ptr[8+ebx]
 fxch st(5)
 faddp st(3),st(0)
 fmul ds:dword ptr[8+ecx]
 fxch st(1)
 faddp st(3),st(0)
 fxch st(3)
 faddp st(2),st(0)
 jmp LSetSides
Lcase1:
 fmul ds:dword ptr[ecx]
 fld ds:dword ptr[0+4+edx]
 fxch st(2)
 fmul ds:dword ptr[ebx]
 fxch st(2)
 fld st(0)
 fmul ds:dword ptr[4+ebx]
 fld ds:dword ptr[0+8+edx]
 fxch st(2)
 fmul ds:dword ptr[4+ecx]
 fxch st(2)
 fld st(0)
 fmul ds:dword ptr[8+ebx]
 fxch st(5)
 faddp st(3),st(0)
 fmul ds:dword ptr[8+ecx]
 fxch st(1)
 faddp st(3),st(0)
 fxch st(3)
 faddp st(2),st(0)
 jmp LSetSides
Lcase2:
 fmul ds:dword ptr[ebx]
 fld ds:dword ptr[0+4+edx]
 fxch st(2)
 fmul ds:dword ptr[ecx]
 fxch st(2)
 fld st(0)
 fmul ds:dword ptr[4+ecx]
 fld ds:dword ptr[0+8+edx]
 fxch st(2)
 fmul ds:dword ptr[4+ebx]
 fxch st(2)
 fld st(0)
 fmul ds:dword ptr[8+ebx]
 fxch st(5)
 faddp st(3),st(0)
 fmul ds:dword ptr[8+ecx]
 fxch st(1)
 faddp st(3),st(0)
 fxch st(3)
 faddp st(2),st(0)
 jmp LSetSides
Lcase3:
 fmul ds:dword ptr[ecx]
 fld ds:dword ptr[0+4+edx]
 fxch st(2)
 fmul ds:dword ptr[ebx]
 fxch st(2)
 fld st(0)
 fmul ds:dword ptr[4+ecx]
 fld ds:dword ptr[0+8+edx]
 fxch st(2)
 fmul ds:dword ptr[4+ebx]
 fxch st(2)
 fld st(0)
 fmul ds:dword ptr[8+ebx]
 fxch st(5)
 faddp st(3),st(0)
 fmul ds:dword ptr[8+ecx]
 fxch st(1)
 faddp st(3),st(0)
 fxch st(3)
 faddp st(2),st(0)
 jmp LSetSides
Lcase4:
 fmul ds:dword ptr[ebx]
 fld ds:dword ptr[0+4+edx]
 fxch st(2)
 fmul ds:dword ptr[ecx]
 fxch st(2)
 fld st(0)
 fmul ds:dword ptr[4+ebx]
 fld ds:dword ptr[0+8+edx]
 fxch st(2)
 fmul ds:dword ptr[4+ecx]
 fxch st(2)
 fld st(0)
 fmul ds:dword ptr[8+ecx]
 fxch st(5)
 faddp st(3),st(0)
 fmul ds:dword ptr[8+ebx]
 fxch st(1)
 faddp st(3),st(0)
 fxch st(3)
 faddp st(2),st(0)
 jmp LSetSides
Lcase5:
 fmul ds:dword ptr[ecx]
 fld ds:dword ptr[0+4+edx]
 fxch st(2)
 fmul ds:dword ptr[ebx]
 fxch st(2)
 fld st(0)
 fmul ds:dword ptr[4+ebx]
 fld ds:dword ptr[0+8+edx]
 fxch st(2)
 fmul ds:dword ptr[4+ecx]
 fxch st(2)
 fld st(0)
 fmul ds:dword ptr[8+ecx]
 fxch st(5)
 faddp st(3),st(0)
 fmul ds:dword ptr[8+ebx]
 fxch st(1)
 faddp st(3),st(0)
 fxch st(3)
 faddp st(2),st(0)
 jmp LSetSides
Lcase6:
 fmul ds:dword ptr[ebx]
 fld ds:dword ptr[0+4+edx]
 fxch st(2)
 fmul ds:dword ptr[ecx]
 fxch st(2)
 fld st(0)
 fmul ds:dword ptr[4+ecx]
 fld ds:dword ptr[0+8+edx]
 fxch st(2)
 fmul ds:dword ptr[4+ebx]
 fxch st(2)
 fld st(0)
 fmul ds:dword ptr[8+ecx]
 fxch st(5)
 faddp st(3),st(0)
 fmul ds:dword ptr[8+ebx]
 fxch st(1)
 faddp st(3),st(0)
 fxch st(3)
 faddp st(2),st(0)
 jmp LSetSides
Lcase7:
 fmul ds:dword ptr[ecx]
 fld ds:dword ptr[0+4+edx]
 fxch st(2)
 fmul ds:dword ptr[ebx]
 fxch st(2)
 fld st(0)
 fmul ds:dword ptr[4+ecx]
 fld ds:dword ptr[0+8+edx]
 fxch st(2)
 fmul ds:dword ptr[4+ebx]
 fxch st(2)
 fld st(0)
 fmul ds:dword ptr[8+ecx]
 fxch st(5)
 faddp st(3),st(0)
 fmul ds:dword ptr[8+ebx]
 fxch st(1)
 faddp st(3),st(0)
 fxch st(3)
 faddp st(2),st(0)
LSetSides:
 faddp st(2),st(0)
 fcomp ds:dword ptr[12+edx]
 xor ecx,ecx
 fnstsw ax
 fcomp ds:dword ptr[12+edx]
 and ah,1
 xor ah,1
 add cl,ah
 fnstsw ax
 and ah,1
 add ah,ah
 add cl,ah
 pop ebx
 mov eax,ecx
 ret
Lerror:
 call near ptr _BOPS_Error
_TEXT ENDS
 END
