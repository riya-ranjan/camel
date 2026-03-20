#include "rpi-asm.h"

@ Exception vector table for GPIO interrupt handling.
@ Only IRQ is forwarded to our handler; all others use
@ the default trampolines from libpi/staff-src/unhandled-exception.S.
.align 5
.globl interrupt_vec
interrupt_vec:
  ldr pc, =unhandled_reset
  ldr pc, =unhandled_undefined_instruction
  ldr pc, =unhandled_swi
  ldr pc, =unhandled_prefetch_abort
  ldr pc, =unhandled_data_abort
  ldr pc, =interrupt_asm

@ IRQ trampoline: save state, call C handler, restore and return.
interrupt_asm:
  mov sp, #INT_STACK_ADDR
  sub   lr, lr, #4
  push  {r0-r12,lr}
  mov   r0, lr              @ pass interrupted PC
  bl    interrupt_vector    @ call C handler
  pop   {r0-r12,lr}
  movs    pc, lr            @ return from exception (restores CPSR)
