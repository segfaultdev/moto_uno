F_GETBYTE .equ 0x2800
F_RDVRRNG .equ 0x2803
F_ERARNGE .equ 0x2806
F_PRGRNGE .equ 0x2809
F_DELNUS  .equ 0x280C

M_PAGE_COUNT .equ 0x84

M_CTRLBYT .equ 0x88
M_CPUSPD  .equ 0x89
M_LADDR   .equ 0x8A
M_DATA    .equ 0x8C

PORTA .equ 0x00
PORTB .equ 0x01
DDRA  .equ 0x04
DDRB  .equ 0x05

strap:
	mov #0x04, M_CPUSPD
	mov #0x40, M_CTRLBYT
	ldhx #0xF800
	jsr F_ERARNGE
	bset #0, DDRB
	mov #0x32, M_PAGE_COUNT
	ldhx #0xF81F
	sthx M_LADDR
strap_page:
	bset #0, PORTB
	ldhx #M_DATA
strap_byte:
	jsr F_GETBYTE
	sta , x
	incx
	cpx #(M_DATA + 0x20)
	blo strap_byte
	mov #0x04, M_CPUSPD
	ldhx M_LADDR
	lda M_PAGE_COUNT
	cmp #0x02
	bne strap_skip
	ldhx #0xFFDF
	sthx M_LADDR
strap_skip:
	aix #-0x1F ; 0xE1
	bclr #0, PORTB
	pshx
	pshh
	jsr F_PRGRNGE
	pulh
	pulx
	aix #0x3F
	sthx M_LADDR
	dbnz M_PAGE_COUNT, strap_page
	; dec M_PAGE_COUNT
	; tst M_PAGE_COUNT
	; bne strap_page
strap_halt:
	; bra strap_halt
	stop
