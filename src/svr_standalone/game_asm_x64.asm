; Assembly functions because inline assembly is not allowed in 64-bit MSVC.
; This file is linked together with the svr_standalone.dll and svr_standalone64.dll projects.

; game_load_xmm1, game_store_xmm1
; Used to load and store the dt variable in the engine filter time function.

_text segment
_value$ = -16
game_load_xmm1 proc
sub rsp, 16
movss dword ptr _value$[rsp + 16], xmm1
fld dword ptr _value$[rsp + 16]
add rsp, 16
ret 0
game_load_xmm1 endp
_text ends

_text segment
_value$ = 8
game_store_xmm1 proc
movss xmm1, dword ptr _value$[rsp - 4]
ret 0
game_store_xmm1 endp
_text ends

end
