;NM and AM


;Bit definitions for PORTB

.include "m328pdef.inc"

.equ SER_BIT   = 0 ;serial data output
.equ SRCLK_BIT = 1 ;shift reg clock
.equ RCLK_BIT  = 2 ;register clock
.equ BTN_BIT   = 3 ;Button input
.equ RPGB_BIT  = 4 ; RPGB input
.equ RPGA_BIT  = 5 ; RPGA input

.def zero_reg  = r22 ; a register set to zero for operations
.def btn_prev  = r19 ; previous button state
.def temp_reg  = r25 ; a temp reg 

.dseg ;SRAM
EnteredCode:   .byte 5 ; Array to store inputted code
CodeIndex:     .byte 1 ; Indexing the code array
t0_ovf_count:  .byte 1 ; Overflow counter

;definitions for our group's code
.equ SECRET0 = 3
.equ SECRET1 = 8
.equ SECRET2 = 2
.equ SECRET3 = 6
.equ SECRET4 = 12


.org 0x0500 ;flash mem
digit_table: ;lookup table 
  .db 0b00111111, 0b00000110
  .db 0b01011011, 0b01001111
  .db 0b01100110, 0b01101101
  .db 0b01111101, 0b00000111
  .db 0b01111111, 0b01101111
  .db 0b01110111, 0b01111100
  .db 0b00111001, 0b01011110
  .db 0b01111001, 0b01110001
  .db 0b01000000, 0b00001000

;reset vector
.cseg
.org 0x0000
rjmp init

init:
    ;setting/loading high/low bytes in RAM/stack pointers
    ldi  r17, high(RAMEND)
    out  SPH, r17
    ldi  r17, low(RAMEND)
    out  SPL, r17

    clr  zero_reg

    ;Configuring Inputs (BTN, RPGA/B)
    ldi  r17, (1<<SER_BIT)|(1<<SRCLK_BIT)|(1<<RCLK_BIT)
    out  DDRB, r17
    cbi  DDRB, BTN_BIT
    sbi  PORTB, BTN_BIT
    cbi  DDRB, RPGB_BIT
    sbi  PORTB, RPGB_BIT
    cbi  DDRB, RPGA_BIT
    sbi  PORTB, RPGA_BIT

    ;setting up Timer0 for delays and button press measurements
    ldi  r16, 0x00
    out  TCCR0A, r16
    ldi  r16, 0x05
    out  TCCR0B, r16
    in   r16, TIFR0
    sbr  r16, (1<<TOV0)
    out  TIFR0, r16
    ldi  r16, 0
    out  TCNT0, r16

    ;initialize display
    ldi  r17, 16
    rcall use_digit
    rcall display

    clr  btn_prev;clear flag

    ;Initialize codeIndex at 0
    ldi  r16, 0
    sts  CodeIndex, r16
    rjmp main



main:

    ;reads port state, skips if high, if low switch to button logic
    in   r20, PINB
    sbrs r20, BTN_BIT
    rjmp button_is_pressed

button_is_not_pressed:

    ; Tests tests button, if btn = 0 switch to rpg logic
    tst  btn_prev
    breq do_rpg_logic_near
    jmp  skip_branch

do_rpg_logic_near:
    ;jump to routine
    jmp  do_rpg_logic

skip_branch:

    ;clears flag, jumps to btn release
    clr  btn_prev
    rjmp button_released_bridge

; button_released_bridge: Just forward to button release handling
button_released_bridge:
    rjmp button_released

; button_is_pressed: Handle a button press event
button_is_pressed:
    tst  btn_prev
    brne handle_held
    lds  r16, t0_ovf_count
    ldi  r16, 0
    sts  t0_ovf_count, r16
    in   r16, TIFR0
    sbr  r16, (1<<TOV0)
    out  TIFR0, r16
    ldi  btn_prev, 1

; handle_held: Process a held button press
handle_held:
    in   r16, TIFR0
    sbrc r16, TOV0
    rcall timer0_overflow
    rjmp do_rpg_logic

; button_released: Decide if the press was short or long when released
button_released:
    lds  r16, t0_ovf_count
    ldi  r18, 122
    cp   r16, r18
    brlo short_press
    rjmp long_press

; short_press: Store the current digit for a short press
short_press:
    lds  r16, CodeIndex
    push r16
    ldi  r26, low(EnteredCode)
    ldi  r27, high(EnteredCode)
    add  r26, r16
    adc  r27, zero_reg
    mov  r0, r17
    st   X, r0
    pop  r16
    inc  r16
    sts  CodeIndex, r16
    cpi  r16, 5
    brlo short_done
    rcall compare_entered_code
    brne code_success
    rjmp code_failure

; short_done: Continue if less than 5 digits have been entered
short_done:
    rjmp do_rpg_logic

; long_press: Reset the code on a long button press
long_press:
    rcall reset_entry
    rjmp do_rpg_logic

; compare_entered_code: Compare entered digits with the secret code
compare_entered_code:
    ldi  r16, 1
    ldi  r26, low(EnteredCode)
    ldi  r27, high(EnteredCode)
    ld   r17, X+
    cpi  r17, SECRET0
    brne fail_cmp
    ld   r17, X+
    cpi  r17, SECRET1
    brne fail_cmp
    ld   r17, X+
    cpi  r17, SECRET2
    brne fail_cmp
    ld   r17, X+
    cpi  r17, SECRET3
    brne fail_cmp
    ld   r17, X
    cpi  r17, SECRET4
    brne fail_cmp
done_cmp:
    ret
fail_cmp:
    clr  r16
    rjmp done_cmp

; code_success: Show success display and wait a bit
code_success:
    ldi  r17, 0b10000000
    rcall display_immediate
    ldi  r16, 4
    rcall wait_seconds_8bit
    rcall reset_entry
    rjmp do_rpg_logic

; code_failure: Show failure display and wait longer
code_failure:
    ldi  r17, 0b00001000
    rcall display_immediate
    ldi  r16, 7
    rcall wait_seconds_8bit
    rcall reset_entry
    rjmp do_rpg_logic

; reset_entry: Clear the display and code index for a fresh start
reset_entry:
    ldi  r17, 16
    rcall use_digit
    rcall display
    ldi  r16, 0
    sts  CodeIndex, r16
    ret

; display_immediate: Show the current display pattern right away
display_immediate:
    mov  r16, r17
    rcall display
    ret

; wait_seconds_8bit: Wait for a set number of seconds using Timer0
wait_seconds_8bit:
    mov  r24, r16
wait_sec_outer:
    ldi  r16, 61
    sts  t0_ovf_count, r16
wait_sec_inner:
wait_sec_loop:
    in   r16, TIFR0
    sbrc r16, TOV0
    rcall dec_overflow_count
    lds  r16, t0_ovf_count
    tst  r16
    brne wait_sec_loop
    dec  r24
    brne wait_sec_outer
    ret

; dec_overflow_count: Decrement Timer0 overflow count by one
dec_overflow_count:
    in   r16, TIFR0
    sbr  r16, (1<<TOV0)
    out  TIFR0, r16
    lds  r16, t0_ovf_count
    dec  r16
    sts  t0_ovf_count, r16
    ret

; timer0_overflow: Increase the Timer0 overflow counter
timer0_overflow:
    lds  r16, t0_ovf_count
    inc  r16
    sts  t0_ovf_count, r16
    in   r16, TIFR0
    sbr  r16, (1<<TOV0)
    out  TIFR0, r16
    ret

; do_rpg_logic: Handle input from the rotary encoder
do_rpg_logic:
    sbic PINB, RPGA_BIT
    rjmp check_rpgb
    sbis PINB, RPGB_BIT
    rjmp increment_counter
    rjmp decrement_counter

; check_rpgb: Check the encoder’s channel B to decide action
check_rpgb:
    sbic PINB, RPGB_BIT
    rjmp main
    rjmp increment_counter

; increment_counter: Bump up the digit value (roll from off to 0)
increment_counter:
    cpi  r17, 16
    breq from_dash_to_zero
    cpi  r17, 15
    breq update_after
    inc  r17
    rjmp update_after

; from_dash_to_zero: Convert "off" state to 0
from_dash_to_zero:
    ldi  r17, 0
    rjmp update_after

; decrement_counter: Lower the digit value (with roll handling)
decrement_counter:
    cpi  r17, 16
    breq from_dash_to_F
    cpi  r17, 0
    breq update_after
    dec  r17
    rjmp update_after

; from_dash_to_F: Roll over to max (15) from "off"
from_dash_to_F:
    ldi  r17, 15
    rjmp update_after

; update_after: Refresh display after changing the digit
update_after:
    rcall use_digit
    rcall display
    rcall wait_for_release
    rjmp main

; wait_for_release: Wait until both rotary encoder inputs are released
wait_for_release:
wait_loop:
    sbis PINB, RPGA_BIT
    rjmp wait_loop
    sbis PINB, RPGB_BIT
    rjmp wait_loop
    ret

; use_digit: Grab the display pattern for the current digit from digit_table
use_digit:
    ldi  r30, 0x00
    ldi  r31, 0x0A
    add  r30, r17
    adc  r31, zero_reg
    lpm  r16, Z
    clr  r31
    clr  r30
    ret

; display: Shift out the 8-bit pattern to the display
display:
    ldi  r18, 8
shift_loop:
    rol  r16
    brcs set_ser
    cbi  PORTB, SER_BIT
    rjmp clk_pulse
set_ser:
    sbi  PORTB, SER_BIT
clk_pulse:
    sbi  PORTB, SRCLK_BIT
    cbi  PORTB, SRCLK_BIT
    dec  r18
    brne shift_loop
    sbi  PORTB, RCLK_BIT
    cbi  PORTB, RCLK_BIT
    ret
