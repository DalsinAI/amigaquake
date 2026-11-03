##
## Quake for AMIGA
##
## fconstPPC.s
##
## Includes some frequently used floating point constants for the
## r_#?PPC.s and d_#?PPC.s PowerPC assembler modules.
##

.include "macrosPPC.i"

.ifdef	WOS
	.tocd
.else
	.rodata
.endif


glab c0
	.float	0.0
obj c0

glab c0_5
	.float	0.5
obj c0_5

glab c2
	.float	2.0
obj c2

glab c65536
	.float	65536.0
obj c65536

glab INT2DBL_0
	.long	0x43300000,0x80000000
obj INT2DBL_0


.ifdef	WOS
	.globl	@___ReciprocTable
@___ReciprocTable:
	.long	__ReciprocTable
	.data
.endif

glab _ReciprocTable
	.long	0,0,0,1431655765,0,858993459,715827883,613566757,536870912
	.long	477218588,429496729,390451572,357913941,330382100,306783378
	.long	286331153,268435456,252645135,238609294,226050910,214748365
	.long	204522252,195225786,186737709,178956971,171798692,165191050
	.long	159072863,153391689,148102321,143165577,138547332
obj _ReciprocTable
