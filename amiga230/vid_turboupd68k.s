**
** vid_turboupd68k.s
**
** turbo video buffer update for Quake-Amiga68k (68040/060 only!)
**
** Written by Frank Wille <frank@phoenix.owl.de>
**

	code

	xdef	_TurboUpdate68k

	cnop	0,4
_TurboUpdate68k:
; a0 = vid_buffer
; a1 = gfxaddr
; d4 = bytesperrow
; d0 = x
; d1 = y
; d2 = width
; d3 = height

	mulu	d4,d1
	add.l	d0,a1
	sub.l	d2,d4
	add.l	d1,a1
	lsr.l	#6,d2
1$:	move.l	d2,d0
2$:	move16	(a0)+,(a1)+
	move16	(a0)+,(a1)+
	move16	(a0)+,(a1)+
	move16	(a0)+,(a1)+
	subq.l	#1,d0
	bne.b	2$
	add.l	d4,a1
	subq.l	#1,d3
	bne.b	1$
	rts
