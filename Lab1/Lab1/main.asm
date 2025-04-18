;NM Lab 1 code

.include "m328Pdef.inc"      ; Dev def
.cseg
.org 0
; Configure PB1 and PB2
    sbi   DDRB, 1            ; PB1 => output
    sbi   DDRB, 2            ; PB2 => output
; Main loop
loop:
    ; Turn LED on PB1 OFF, LED on PB2 ON
    sbi   PORTB, 1
    cbi   PORTB, 2
    rcall delay_long ;wait
    ; Turn LED on PB1 ON, LED on PB2 OFF
    cbi   PORTB, 1
    sbi   PORTB, 2
    rcall delay_long ;waits .34s
    rjmp  loop ;return to loop so it runs forever

.equ count = 5336   ; Outer loop runs 5300 times

delay_long: ;subroutine
    ; Load outer loop counter
    ldi   r30, low(count)    ; r30 = low byte (loads lower 8 into r30)
    ldi   r31, high(count)   ; r31 = high byte (loads upper 8 into r31)
d1:
    ; Inner loop = 255 times
    ldi   r29, 0xFE
d2:
	nop						 ; 1 cycle
    dec   r29                ; 1 cycle
    brne  d2                 ; 2 cycles if taken, 1 if not
    ; Decrement outer loop (takes 1 from r30,31)
    sbiw  r31:r30, 1         ; 2 cycles
    brne  d1                 ; 2 cycles if branch taken, 1 if not

d3:
	;another inner loop
	ldi r29, 0x73
	nop
	nop
	nop
	nop
	dec r29
tune: ;adds to a fraction of a ms to "tune"
    nop
    dec   r29
    brne  tune
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
    ret

.exit

;For me to remember for later
;target = 5,443,200 cycles
;CPU freq = 16,000,000cyc/s
;eqn(ideal) = .3402s x 16,000,000 = 5,443,200cycs 
;try breaks at start of main loop and after each rcall delay long
;If you want to mess with the cycles you can change below
;