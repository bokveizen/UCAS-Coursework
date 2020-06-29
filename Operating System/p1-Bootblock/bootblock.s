	.text
	.globl main

main:
	# check the offset of main
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	
	lui $8,0x8007
	ori $8,$8,0xb1a8
	
	lui $9,0xa080
	ori $9,$9,0x0004
	
	lw $4,($9)
	
	lui $9,0xa080
	ori $9,$9,0x0000

	lw $5,($9)

	lui $9,0xa080
	ori $9,$9,0x000c

	lw $6,($9)

	jal $8
		
	lui $9,0xa080
	ori $9,$9,0x0008	
	
	lw $8,($9)

	jal $8
	
	jr $31	
	
		
	#need add code
	#read kernel

