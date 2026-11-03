##
## AmigaOS4 Input interrupt routine for mouse control
## Based on in_amigamouseMOS.s
##

# struct InputEvent from devices/inputevent.h
.set	ie_NextEvent,0
.set	ie_Class,4
.set	ie_Code,6
.set	ie_x,10
.set	ie_y,12

# defines from devices/inputevent.h
.set	IECLASS_RAWMOUSE,2
.set	IECODE_UP_PREFIX,0x80
.set	IECODE_LBUTTON,0x68
.set	IECODE_RBUTTON,0x69
.set	IECODE_MBUTTON,0x6a

# struct InputIntDat
.set	iid_LeftButtonDown,0
.set	iid_MidButtonDown,4
.set	iid_RightButtonDown,8
.set	iid_LeftButtonUp,12
.set	iid_MidButtonUp,16
.set	iid_RightButtonUp,20
.set	iid_MouseX,24
.set	iid_MouseY,28
.set	iid_MouseSpeed,32


	.text

	.align	3
	.globl	InputIntCode
InputIntCode:
# r3 = *InputEvent
# r4 = *InputIntDat
	mr	r5,r4			# r5 InputIntDat
	mr	r4,r3			# r4 InputEvent
	lwz	r6,iid_MouseSpeed(r5)
	li	r10,-1
	li	r0,0
.loop:
	lbz	r11,ie_Class(r4)
	cmpwi	r11,IECLASS_RAWMOUSE
	bne	.next
	lhz	r12,ie_Code(r4)
	cmpwi	r12,IECODE_LBUTTON
	beq	.lmb_down
	cmpwi	r12,IECODE_MBUTTON
	beq	.mmb_down
	cmpwi	r12,IECODE_RBUTTON
	beq	.rmb_down
	cmpwi	r12,IECODE_UP_PREFIX|IECODE_LBUTTON
	beq	.lmb_up
	cmpwi	r12,IECODE_UP_PREFIX|IECODE_MBUTTON
	beq	.mmb_up
	cmpwi	r12,IECODE_UP_PREFIX|IECODE_RBUTTON
	beq	.rmb_up
	b	.move
.lmb_down:
	stw	r10,iid_LeftButtonDown(r5)
	stw	r0,iid_LeftButtonUp(r5)
	b	.move
.mmb_down:
	stw	r10,iid_MidButtonDown(r5)
	stw	r0,iid_MidButtonUp(r5)
	b	.move
.rmb_down:
	stw	r10,iid_RightButtonDown(r5)
	stw	r0,iid_RightButtonUp(r5)
	b	.move
.lmb_up:
	stw	r0,iid_LeftButtonDown(r5)
	stw	r10,iid_LeftButtonUp(r5)
	b	.move
.mmb_up:
	stw	r0,iid_MidButtonDown(r5)
	stw	r10,iid_MidButtonUp(r5)
	b	.move
.rmb_up:
	stw	r0,iid_RightButtonDown(r5)
	stw	r10,iid_RightButtonUp(r5)
.move:
	lha	r11,ie_x(r4)
	mullw	r11,r11,r6
	lha	r12,ie_y(r4)
	stw	r11,iid_MouseX(r5)
	mullw	r12,r12,r6
	stw	r12,iid_MouseY(r5)
.next:
	lwz	r4,ie_NextEvent(r4)
	cmpwi	r4,0
	bne	.loop
	blr

	.type	InputIntCode,@function
	.size	InputIntCode,$-InputIntCode
