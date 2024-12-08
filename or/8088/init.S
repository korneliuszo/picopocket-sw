.8086

_TEXT  segment word public 'CODE'
assume  cs:_TEXT
dw 0AA55h
db 4
public entry
entry:
	jmp short entry0
org 0x10
call_from_utilities:
	jmp short callback
org 0x14
public port_no
port_no:
	dw 0xF8
org 0x16
	dw ptr irqentry
org 0x20
public entry0
entry0: ; boot
	push  ax
	push  ax
	pushf
	push ax
	mov ax, 0
	jmp short callback

entry1: ; irq
	push  ax
	pushf
	push ax
	mov ax, 1
	jmp short callback

callback proc near
assume  cs:_TEXT
extrn _start: near
	push cx
	push dx
	push bx
	push bp
	push si
	push di
	push ds
	push es

	//catch stack frame
	mov bp,sp

	push ax

   	mov ax,DGROUP
	mov bx, cs
	add ax, bx
    assume  ds:DGROUP
	mov	ds, ax

	cld
	pop bx
	push ss
	mov ax, bp
	push ax
	push bx

	call _start

	add sp, 6

	test ax,ax
	js fcall_handled
	jz irq_handled
; chain
	pop es
	pop ds
	pop di
	pop si
	pop bp
	pop bx
	pop dx
	pop cx
	pop ax
	popf
	retf
irq_handled label near
	pop es
	pop ds
	pop di
	pop si
	pop bp
	pop bx
	pop dx
	pop cx
	pop ax
	popf
	add sp, 4
	iret
fcall_handled label near
	pop es
	pop ds
	pop di
	pop si
	pop bp
	pop bx
	pop dx
	pop cx
	pop ax
	popf
	add sp, 4
	retf

callback	endp

stackcode_ proc near
public stackcode_:
;-2
	push bp ;-4
	push ds ;-6
	mov bp, sp
	add bp, 6

	push bp ;-8

	xchg bx,bx;brek

	//load ds from argument
	mov ds, cx
	//save to stack
	push bx ; -10
	push cx ; -12
	pushf ; -14

	;push routine address
	sub sp, 4 ;-18
	mov ss:[bp-16], cx
	mov ss:[bp-18], ax


	mov es, ds:[bx+0]
	mov di, ds:[bx+4]
	mov si, ds:[bx+6]
	mov dx, ds:[bx+10]
	mov cx, ds:[bx+12]

	mov ax, ds:[bx+2] ;ds
	push ax ;-20
	mov ax, ds:[bx+8] ;bx
	push ax ;-22
	mov ax, ds:[bx+16] ;f
	push ax ;-24
	mov ax, ds:[bx+14]

	popf     ;-22
	pop bx   ;-20
	pop ds   ;-18
	call far PTR ss:[bp-18]
	pushf    ;-20
	push bx  ;-22
	push ds  ;-24
	add sp,10;-14
	popf     ;-12
	pop bx   ;-10
	mov ds,bx
	pop bx   ;-8
	pop bp   ;-6

	mov ds:[bx+14], ax
	mov ax, ds:[bx-20] ; f
	mov ds:[bx+16], ax
	mov ds:[bx+12], cx
	mov ds:[bx+10], dx
	mov ax, ds:[bx-22] ; bx
	mov ds:[bx+8], ax
	mov ds:[bx+6], si
	mov ds:[bx+4], di
	mov ax, ds:[bx-24] ; ds
	mov ds:[bx+2], ax
	mov ds:[bx+0], es

	pop ds   ;-4
	pop bp   ;-2
	ret      ;0
stackcode_	endp

public irqentry
irqentry:
FORC _X, "12345678"
FORC _Y, "12345678"
FORC _Z, "1234"
	call near PTR entry1
ENDM
ENDM
ENDM

_TEXT ends

	end


