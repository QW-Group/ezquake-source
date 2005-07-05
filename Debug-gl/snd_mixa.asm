 .386P
 .model FLAT
 externdef _vright:dword
 externdef _vup:dword
 externdef _vpn:dword
_TEXT SEGMENT
 externdef _snd_scaletable:dword
 externdef _paintbuffer:dword
 externdef _snd_linear_count:dword
 externdef _snd_p:dword
 externdef _snd_vol:dword
 externdef _snd_out:dword
 public _SND_PaintChannelFrom8
_SND_PaintChannelFrom8:
 push esi
 push edi
 push ebx
 push ebp
 mov ebx,ds:dword ptr[4+16+esp]
 mov esi,ds:dword ptr[8+16+esp]
 mov eax,ds:dword ptr[4+ebx]
 mov edx,ds:dword ptr[8+ebx]
 cmp eax,255
 jna LLeftSet
 mov eax,255
LLeftSet:
 cmp edx,255
 jna LRightSet
 mov edx,255
LRightSet:
 and eax,0F8h
 add esi,20
 and edx,0F8h
 mov edi,ds:dword ptr[16+ebx]
 mov ecx,ds:dword ptr[12+16+esp]
 add esi,edi
 shl eax,7
 add edi,ecx
 shl edx,7
 mov ds:dword ptr[16+ebx],edi
 add eax,offset _snd_scaletable
 add edx,offset _snd_scaletable
 sub ebx,ebx
 mov bl,ds:byte ptr[-1+esi+ecx*1]
 test ecx,1
 jz LMix8Loop
 mov edi,ds:dword ptr[eax+ebx*4]
 mov ebp,ds:dword ptr[edx+ebx*4]
 add edi,ds:dword ptr[_paintbuffer+0-8+ecx*8]
 add ebp,ds:dword ptr[_paintbuffer+4-8+ecx*8]
 mov ds:dword ptr[_paintbuffer+0-8+ecx*8],edi
 mov ds:dword ptr[_paintbuffer+4-8+ecx*8],ebp
 mov bl,ds:byte ptr[-2+esi+ecx*1]
 dec ecx
 jz LDone
LMix8Loop:
 mov edi,ds:dword ptr[eax+ebx*4]
 mov ebp,ds:dword ptr[edx+ebx*4]
 add edi,ds:dword ptr[_paintbuffer+0-8+ecx*8]
 add ebp,ds:dword ptr[_paintbuffer+4-8+ecx*8]
 mov bl,ds:byte ptr[-2+esi+ecx*1]
 mov ds:dword ptr[_paintbuffer+0-8+ecx*8],edi
 mov ds:dword ptr[_paintbuffer+4-8+ecx*8],ebp
 mov edi,ds:dword ptr[eax+ebx*4]
 mov ebp,ds:dword ptr[edx+ebx*4]
 mov bl,ds:byte ptr[-3+esi+ecx*1]
 add edi,ds:dword ptr[_paintbuffer+0-8*2+ecx*8]
 add ebp,ds:dword ptr[_paintbuffer+4-8*2+ecx*8]
 mov ds:dword ptr[_paintbuffer+0-8*2+ecx*8],edi
 mov ds:dword ptr[_paintbuffer+4-8*2+ecx*8],ebp
 sub ecx,2
 jnz LMix8Loop
LDone:
 pop ebp
 pop ebx
 pop edi
 pop esi
 ret
 public _Snd_WriteLinearBlastStereo16
_Snd_WriteLinearBlastStereo16:
 push esi
 push edi
 push ebx
 mov ecx,ds:dword ptr[_snd_linear_count]
 mov ebx,ds:dword ptr[_snd_p]
 mov esi,ds:dword ptr[_snd_vol]
 mov edi,ds:dword ptr[_snd_out]
LWLBLoopTop:
 mov eax,ds:dword ptr[-8+ebx+ecx*4]
 imul eax,esi
 sar eax,8
 cmp eax,07FFFh
 jg LClampHigh
 cmp eax,0FFFF8000h
 jnl LClampDone
 mov eax,0FFFF8000h
 jmp LClampDone
LClampHigh:
 mov eax,07FFFh
LClampDone:
 mov edx,ds:dword ptr[-4+ebx+ecx*4]
 imul edx,esi
 sar edx,8
 cmp edx,07FFFh
 jg LClampHigh2
 cmp edx,0FFFF8000h
 jnl LClampDone2
 mov edx,0FFFF8000h
 jmp LClampDone2
LClampHigh2:
 mov edx,07FFFh
LClampDone2:
 shl edx,16
 and eax,0FFFFh
 or edx,eax
 mov ds:dword ptr[-4+edi+ecx*2],edx
 sub ecx,2
 jnz LWLBLoopTop
 pop ebx
 pop edi
 pop esi
 ret
 public _Snd_WriteLinearBlastStereo16_SwapStereo
_Snd_WriteLinearBlastStereo16_SwapStereo:
 push esi
 push edi
 push ebx
 mov ecx,ds:dword ptr[_snd_linear_count]
 mov ebx,ds:dword ptr[_snd_p]
 mov esi,ds:dword ptr[_snd_vol]
 mov edi,ds:dword ptr[_snd_out]
SwapLWLBLoopTop:
 mov eax,ds:dword ptr[-4+ebx+ecx*4]
 imul eax,esi
 sar eax,8
 cmp eax,07FFFh
 jg SwapLClampHigh
 cmp eax,0FFFF8000h
 jnl SwapLClampDone
 mov eax,0FFFF8000h
 jmp SwapLClampDone
SwapLClampHigh:
 mov eax,07FFFh
SwapLClampDone:
 mov edx,ds:dword ptr[-8+ebx+ecx*4]
 imul edx,esi
 sar edx,8
 cmp edx,07FFFh
 jg SwapLClampHigh2
 cmp edx,0FFFF8000h
 jnl SwapLClampDone2
 mov edx,0FFFF8000h
 jmp SwapLClampDone2
SwapLClampHigh2:
 mov edx,07FFFh
SwapLClampDone2:
 shl edx,16
 and eax,0FFFFh
 or edx,eax
 mov ds:dword ptr[-4+edi+ecx*2],edx
 sub ecx,2
 jnz SwapLWLBLoopTop
 pop ebx
 pop edi
 pop esi
 ret
_TEXT ENDS
 END
