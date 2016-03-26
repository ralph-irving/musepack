;
%include "tools.inc"
;
%define reg0    eax
%define reg1    eax + esi
%define reg2    eax + edx
%define reg3    eax + edi

%define off1    esi
%define off2    edx
%define off3    edi
;
                segment_data
;
                align   32
negativ         dd       0x80000000, 0
;
%macro          turn 2                          ; dst, tmp
                punpckldq       %2, %1          ; tmp = src.l | tmp.l
                punpckhdq       %1, %2          ; src = src.l | src.h
%endmacro

;
; cftmdl ( const int n, const int l, float* a, float* w );
;
                segment_code

                align 32
proc            cftmdl_3DNow_1
                pushd   ebx, esi, edi, ebp
$n1             arg     4
$l1             arg     4
$a1             arg     4
$w1             arg     4

;    for ( j = 0; j < l; j += 2 ) {             a, a+l a+2*l a+3*l
;        j1        = j  + l;
;        j2        = j1 + l;
;        j3        = j2 + l;
;        x0r       = a[j]      + a[j1];
;        x0i       = a[j + 1]  + a[j1 + 1];
;        x1r       = a[j]      - a[j1];
;        x1i       = a[j + 1]  - a[j1 + 1];
;        x2r       = a[j2]     + a[j3];
;        x2i       = a[j2 + 1] + a[j3 + 1];
;        x3r       = a[j2]     - a[j3];
;        x3i       = a[j2 + 1] - a[j3 + 1];
;        a[j]      = x0r + x2r;
;        a[j + 1]  = x0i + x2i;
;        a[j2]     = x0r - x2r;
;        a[j2 + 1] = x0i - x2i;
;        a[j1]     = x1r - x3i; -x3i
;        a[j1 + 1] = x1i + x3r; +x3r
;        a[j3]     = x1r + x3i;
;        a[j3 + 1] = x1i - x3r;
;    }

                pmov    mm7, qword [negativ]    ; + | -
                mov     eax, [sp($a1)]          ; eax = a
                xor     ebx, ebx                ; ebx = j
                mov     ecx, [sp($l1)]          ; ecx = l

                lea     off1, [4*ecx]
                lea     off2, [8*ecx]
                lea     off3, [off1 + 8*ecx]
                shr     ecx, 1
lbl1:
                pmov    mm0, [reg0]
                pfadd   mm0, [reg1]             ; x0r, x0i
                pmov    mm1, [reg0]
                pfsub   mm1, [reg1]             ; x1r, x1i
                pmov    mm2, [reg2]
                pfadd   mm2, [reg3]             ; x2r, x2i
                pmov    mm3, [reg2]
                pfsub   mm3, [reg3]             ; x3r, x3i
                pmov    mm4, mm0
                pfadd   mm4, mm2
                pmov    [reg0], mm4
                pfsub   mm0, mm2
                pmov    [reg2], mm0
                turn    mm3, mm4
                pxor    mm3, mm7
                pmov    mm4, mm1
                pfadd   mm4, mm3
                pmov    [reg1], mm4
                pfsub   mm1, mm3
                pmov    [reg3], mm1
                add     eax, byte 8
                dec     ecx
                jnz     lbl1

;    m    = l << 2;                             ; ebp = m
;    wk1r = w[2];
;    for ( j = m; j < l + m; j += 2 ) {
;        j1        = j  + l;
;        j2        = j1 + l;
;        j3        = j2 + l;
;        x0r       = a[j]      + a[j1];
;        x0i       = a[j + 1]  + a[j1 + 1];
;        x1r       = a[j]      - a[j1];
;        x1i       = a[j + 1]  - a[j1 + 1];
;        x2r       = a[j2]     + a[j3];
;        x2i       = a[j2 + 1] + a[j3 + 1];
;        x3r       = a[j2]     - a[j3];
;        x3i       = a[j2 + 1] - a[j3 + 1];
;        a[j]      = x0r + x2r;
;        a[j + 1]  = x0i + x2i;
;        a[j2]    =-(x0i - x2i);
;        a[j2 + 1] = x0r - x2r;
;        x0r       = x1r - x3i;                 ; x1r -x3i
;        x0i       = x1i + x3r;                 ; x1i  x3r
;        a[j1]     = wk1r * (x0r - x0i);
;        a[j1 + 1] = wk1r * (x0r + x0i);
;        x0r       = x3i + x1r;                 ; x1r -x3i      1 - 3   x1r + x3i
;        x0i       = x3r - x1i;                 ; x1i  x3r              x1i - x3r
;        a[j3]     = wk1r * (x0i - x0r);
;        a[j3 + 1] = wk1r * (x0i + x0r);
;    }

                mov     eax, [sp($a1)]          ; eax = a
                mov     ecx, [sp($l1)]          ; ecx = l
                mov     ebp, [sp($w1)]          ; ebp = w
                pmov    mm6, [ebp + 8]          ; mm6 =  ?   | w[2]
                punpckldq mm6, mm6              ; mm6 = w[2] | w[2]

                lea     eax, [eax + 8*ecx]
                lea     eax, [eax + 8*ecx]

                lea     off1, [4*ecx]
                lea     off2, [8*ecx]
                lea     off3, [off1 + 8*ecx]
                shr     ecx, 1
lbl2:
                pmov    mm0, [reg0]
                pfadd   mm0, [reg1]             ; x0r, x0i
                pmov    mm1, [reg0]
                pfsub   mm1, [reg1]             ; x1r, x1i
                pmov    mm2, [reg2]
                pfadd   mm2, [reg3]             ; x2r, x2i
                pmov    mm3, [reg2]
                pfsub   mm3, [reg3]             ; x3r, x3i
                pmov    mm4, mm0
                pfadd   mm4, mm2
                pmov    [reg0], mm4
                pfsub   mm0, mm2
                turn    mm0, mm4
                pxor    mm0, mm7
                pmov    [reg2], mm0
                turn    mm3, mm4
                pxor    mm3, mm7
                pmov    mm4, mm1
                pfadd   mm4, mm3
                pmov    mm5, mm4
                punpckldq mm4, mm4
                punpckhdq mm5, mm5              ;  r r
                pxor    mm5, mm7                ; -i i
                pfadd   mm4, mm5
                pfmul   mm4, mm6
                pmov    [reg1], mm4

                pfsub   mm1, mm3
                pmov    mm4, mm1
                punpckldq mm1, mm1
                pxor    mm1, mm7                ; -r r
                punpckhdq mm4, mm4              ;  i i
                pfsubr  mm4, mm1
                pfmul   mm4, mm6
                pmov    [reg3], mm4

                add     eax, byte 8
                dec     ecx
                jnz     near lbl2

                femms
                popd    ebx, esi, edi, ebp
                ret


                align 32
proc            cftmdl_3DNow_2
                pushd   ebx, esi, edi, ebp
$n2             arg     4
$l2             arg     4
$a2             arg     4
$w2             arg     4

                mov     eax, [sp($a2)]          ; eax = a
                mov     ecx, [sp($l2)]          ; ecx = l
                mov     ebp, [sp($w2)]          ; ebp = w

                push    ebp                     ; w + 2*k1      = (esp+20)
                push    ebp                     ; w + k1        = (esp+16)

                lea     ebx, [4*ecx]
                push    dword 0                 ; k1 = 0        = (esp+12)
                push    ebx                     ; m  = 4*l      = (esp+ 8)
                add     ebx, ebx
                push    ebx                     ; k  = 2*m      = (esp+ 4)
                push    dword 0                 ; k1 = 0        = (esp+ 0)

;    for ( k = 2*m; k < n; k += 2*m ) {
;        k1  += 2;
;        wk2r = w[k1];
;        wk2i = w[k1 + 1];
;        wk1r = w[2*k1];
;        wk1i = w[2*k1 + 1];
;        wk3r = wk1r - 2 * wk2i * wk1i;
;        wk3i = wk1i - 2 * wk2i * wk1r;


lbl3:
                add     dword [esp+16], byte  8
                add     dword [esp+20], byte 16
                mov     ebx, [esp+16]
                pmov    mm6, [ebx]              ; mm6 = wk2
                mov     ebx, [esp+20]
                pmov    mm5, [ebx]              ; mm5 = mk1
                pmov    mm4, mm6                ; mk1
                punpckhdq mm4, mm4              ; mk1i mk1i
                pfadd   mm4, mm4                ; 2*mk1i 2*mk1i
                pmov    mm3, mm5
                turn    mm3, mm2
                pfmul   mm4, mm3
                pfsubr  mm4, mm5                ; mm4 = mk3

;        j    = k;

lbl4:
;        do {
;            j1        = j  + l;
;            j2        = j1 + l;
;            j3        = j2 + l;
;            x0r       = a[j]      + a[j1];
;            x0i       = a[j + 1]  + a[j1 + 1];
;            x1r       = a[j]      - a[j1];
;            x1i       = a[j + 1]  - a[j1 + 1];
;            x2r       = a[j2]     + a[j3];
;            x2i       = a[j2 + 1] + a[j3 + 1];
;            x3r       = a[j2]     - a[j3];
;            x3i       = a[j2 + 1] - a[j3 + 1];
;
                pmov    mm0, [reg0]
                pfadd   mm0, [reg1]
                pmov    mm1, [reg0]
                pfsub   mm1, [reg1]
                pmov    mm2, [reg2]
                pfadd   mm2, [reg3]
                pmov    mm3, [reg2]
                pfsub   mm3, [reg3]

;            a[j]      = x0r + x2r;
;            a[j + 1]  = x0i + x2i;

                pmov    mm7, mm0
                pfadd   mm7, mm2
                pmov    [reg0], mm7

;            x0r      -= x2r;
;            x0i      -= x2i;

                pfsub   mm0, mm2

;            a[j2]     = wk2r * x0r - wk2i * x0i;
;            a[j2 + 1] = wk2r * x0i + wk2i * x0r;       // frei sind (mm0), mm2, mm7

                pmov    mm2, mm0
                turn    mm2, mm7                ; x0i  x0r
                pmov    mm7, mm6
                punpckhdq mm7, mm7              ; wk2i wk2i
                pxor    mm2, [negativ]          ;-x0i  x0r
                pfmul   mm2, mm7
                pmov    mm7, mm6
                punpckldq mm7, mm7              ; wk2r wk2r
                pfmul   mm7, mm0
                pfadd   mm2, mm7
                pmov    [reg2], mm2

;            x0r       = x1r - x3i;
;            x0i       = x1i + x3r;

                turn    mm3, mm2
                pxor    mm3, [negativ]
                pmov    mm0, mm1
                pfadd   mm0, mm3

;            a[j1]     = wk1r * x0r - wk1i * x0i;
;            a[j1 + 1] = wk1r * x0i + wk1i * x0r;

;            x1r      += x3i;
;            x1i      -= x3r;

                pfsub   mm1, mm3

;            a[j3]     = wk3r * x1r + wk3i * x1i;
;            a[j3 + 1] = wk3r * x1i - wk3i * x1r;

;        } while ( j += 2, j < l + k );
;
                dec     ecx
                jnz     near lbl4


;        wk1r = w[2*k1 + 2];
;        wk1i = w[2*k1 + 3];
;        wk3r = wk1r - 2 * wk2r * wk1i;
;        wk3i = wk1i - 2 * wk2r * wk1r;

                mov     ebx, [esp+20]
                pmov    mm5, [ebx+ 8]           ; mm5 = mk1
                pmov    mm4, mm6                ; mk1
                punpckldq mm4, mm4              ; mk1r mk1r
                pfadd   mm4, mm4                ; 2*mk1i 2*mk1i
                pmov    mm3, mm5
                turn    mm3, mm2
                pfmul   mm4, mm3
                pfsubr  mm4, mm5                ; mm4 = mk3




;        j    = k + m;

lbl5:

;        do {
;            j1        = j  + l;
;            j2        = j1 + l;
;            j3        = j2 + l;
;            x0r       = a[j]      + a[j1];
;            x0i       = a[j + 1]  + a[j1 + 1];
;            x1r       = a[j]      - a[j1];
;            x1i       = a[j + 1]  - a[j1 + 1];
;            x2r       = a[j2]     + a[j3];
;            x2i       = a[j2 + 1] + a[j3 + 1];
;            x3r       = a[j2]     - a[j3];
;            x3i       = a[j2 + 1] - a[j3 + 1];

                pmov    mm0, [reg0]
                pfadd   mm0, [reg1]
                pmov    mm1, [reg0]
                pfsub   mm1, [reg1]
                pmov    mm2, [reg2]
                pfadd   mm2, [reg3]
                pmov    mm3, [reg2]
                pfsub   mm3, [reg3]

;            a[j]      = x0r + x2r;
;            a[j + 1]  = x0i + x2i;

                pmov    mm7, mm0
                pfadd   mm7, mm2
                pmov    [reg0], mm7

;            x0r      -= x2r;
;            x0i      -= x2i;

                pfsub   mm0, mm2

;            a[j2]     = -wk2i * x0r - wk2r * x0i;
;            a[j2 + 1] = -wk2i * x0i + wk2r * x0r;

                pmov    mm2, mm0
                turn    mm2, mm7                ; x0i  x0r
                pmov    mm7, mm6
                punpckldq mm7, mm7              ; wk2i wk2i
                pxor    mm2, [negativ]          ;-x0i  x0r
                pfmul   mm2, mm7
                pmov    mm7, mm6
                punpckhdq mm7, mm7              ; wk2r wk2r
                pfmul   mm7, mm0
                pfsubr  mm2, mm7                ; ?
                pmov    [reg2], mm2

;            x0r       = x1r - x3i;
;            x0i       = x1i + x3r;

                turn    mm3, mm2
                pxor    mm3, [negativ]
                pmov    mm0, mm1
                pfadd   mm0, mm3

;            a[j1]     = wk1r * x0r - wk1i * x0i;
;            a[j1 + 1] = wk1r * x0i + wk1i * x0r;

;            x1r      -= x3i;
;            x1i      -= x3r;

                pfsub   mm1, mm3

;            a[j3]     = wk3r * x1r + wk3i * x1i;
;            a[j3 + 1] = wk3r * x1i - wk3i * x1r;

;        } while ( j += 2, j < l+k+m );

                dec     ecx
                jnz     near lbl5
;    }

                jc      near lbl3

                femms
                add     esp, 16
                popd    ebx, esi, edi, ebp
                ret


;##################################################################
