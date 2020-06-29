#include "regs.h"
	.text
	.align	2
	.globl  get_timer	
	.ent	get_timer
	.type	get_timer, @function
get_timer:
	mfc0 $2,$9
    nop
    j       ra
    nop
	.align	2
	.globl  kernel_entry	
	.ent	kernel_entry
	.type	kernel_entry, @function
kernel_entry:
	addiu sp, sp, -24
	sw ra, 0(sp)
	bnez $4, 1f
	nop

	jal do_yield
	nop
	beqz $0,2f
	nop

1:
	jal do_exit
	nop
2:
	lw ra, 0(sp)
	addiu sp, sp, 24
	jr ra
	nop

	.align	2
	.globl  scheduler_entry	
	.ent	scheduler_entry
	.type	scheduler_entry, @function
scheduler_entry:
	# call scheduler, which will set new current process
	# need student add
	jal scheduler
	nop

	lw      k0, current_running

	lw      $1, 4(k0)    
	lw      $2, 8(k0)    
	lw      $3, 12(k0)    
	lw      $4, 16(k0)    
	lw      $5, 20(k0)    
	lw      $6, 24(k0)    
	lw      $7, 28(k0)    
	lw      $8, 32(k0)    
	lw      $9, 36(k0)    
	lw      $10, 40(k0)    
	lw      $11, 44(k0)    
	lw      $12, 48(k0)    
	lw      $13, 52(k0)    
	lw      $14, 56(k0)    
	lw      $15, 60(k0)    
	lw      $16, 64(k0)    
	lw      $17, 68(k0)    
	lw      $18, 72(k0)    
	lw      $19, 76(k0)    
	lw      $20, 80(k0)    
	lw      $21, 84(k0)    
	lw      $22, 88(k0)    
	lw      $23, 92(k0)    
	lw      $24, 96(k0)    
	lw      $25, 100(k0)    
	#lw      $26, 104(k0)    
	lw      $27, 108(k0)     
	lw      $28, 112(k0)
	lw		$29, 116(k0)
    lw		$30, 120(k0)
    lw		$31, 124(k0)

    jr     	ra
    nop
	
	.align	2
	.globl  save_pcb	
	.ent	save_pcb
	.type	save_pcb, @function
save_pcb:
	# save the pcb of the currently running process

    lw      k0, current_running
    lw		k1, 16(sp) #ra in thread
    sw		k1, 124(k0)#ra in thread -> PCB reg[31]

    addiu 	sp, sp, 24 #move back sp


    #save regs
    sw      $1, 4(k0)
    sw      $2, 8(k0)
    sw      $3, 12(k0)
    sw      $4, 16(k0)
    sw      $5, 20(k0)
    sw      $6, 24(k0)
    sw      $7, 28(k0)
    sw      $8, 32(k0)
    sw      $9, 36(k0)
    sw      $10, 40(k0)
    sw      $11, 44(k0)
    sw      $12, 48(k0)
    sw      $13, 52(k0)
    sw      $14, 56(k0)
    sw      $15, 60(k0)
    sw      $16, 64(k0)
    sw      $17, 68(k0)
    sw      $18, 72(k0)
    sw      $19, 76(k0)
    sw      $20, 80(k0)
    sw      $21, 84(k0)
    sw      $22, 88(k0)
    sw      $23, 92(k0)
    sw      $24, 96(k0)
    sw      $25, 100(k0)
    #sw      $26, 104(k0)
    #sw      $27, 108(k0)
    sw      $28, 112(k0)
    sw		$29, 116(k0)
    sw		$30, 120(k0)
    #sw		$31, 124(sp)#saved before

    jr      ra
    nop

	
	 
	# need student add
