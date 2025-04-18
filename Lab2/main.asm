.include "m328pdef.inc"

; Bit definitions
.equ SER_BIT = 0; PB0 = SER 
.equ SRCLK_BIT = 1; PB1 = SRCLK 
.equ RCLK_BIT = 2; PB2 = RCLK 
.equ BTN_BIT = 3; PB3 -> pushbuttn
.equ DP_BIT  = 7; Decimal point (DP) indicator

; Seven-segment display patterns (DP used separately)
.equ DIGIT0 = 0b00111111; '0'
.equ DIGIT1 = 0b00000110; '1'
.equ DIGIT2 = 0b01011011; '2'
.equ DIGIT3 = 0b01001111; '3'
.equ DIGIT4 = 0b01100110; '4'
.equ DIGIT5 = 0b01101101; '5'
.equ DIGIT6 = 0b01111101; '6'
.equ DIGIT7 = 0b00000111; '7'
.equ DIGIT8 = 0b01111111; '8'
.equ DIGIT9 = 0b01101111; '9'
.equ DIGITA = 0b01110111; 'A'
.equ DIGITB = 0b01111100; 'B'
.equ DIGITC = 0b00111001; 'C'
.equ DIGITD = 0b01011110; 'D'
.equ DIGITE = 0b01111001; 'E'
.equ DIGITF = 0b01110001; 'F'

.org 0x0000; Reset vector
  rjmp init; Jump to initialization

init:
  ; Configure PB0, PB1, PB2 as outputs
  ldi  r16, (1<<SER_BIT)|(1<<SRCLK_BIT)|(1<<RCLK_BIT)
  out  DDRB, r16
  
  ; Configure PB3 as input with internal pullup (active low button)
  cbi  DDRB, BTN_BIT
  sbi  PORTB, BTN_BIT
  
  ; Initialize counter (r17 = 0) and mode flag (r19 = 0 ? increment mode)
  ldi  r17, 0
  clr  r19
  
  ; Display initial "0" with DP off
  ldi  r16, DIGIT0
  cbr  r16, (1<<DP_BIT)
  rcall display
  
  rjmp main

main:
wait_for_press:
  ; Wait for button press (active low)
  sbic PINB, BTN_BIT
  rjmp wait_for_press
  ; rcall debounce  ; (Not needed if hardware debounce is present)
  
  ; Begin measuring press duration in r18 (~10ms intervals)
  ldi  r18, 0
measure_press:
  in   r20, PINB; Read PINB (check button state)
  sbic PINB, BTN_BIT; If button is released, skip next instruction
  rjmp button_released; and jump to processing on release
  ; If count is less than 200, add delay; otherwise, saturate at 200.
  cpi  r18, 200        
  brsh skip_delay      
  rcall delay_10ms; ~10ms delay 
  inc  r18
skip_delay:
  rjmp measure_press; Continue until button is released

button_released:
  ; Decide action based on duration measured in r18.
  cpi  r18, 100; r18 < 100 (<1.00s): short press
  brlo short_press
  cpi  r18, 200; 100 <= r18 < 200 (1.00s - 1.99s): medium press
  brlo toggle_mode
  rjmp reset_action; r18 >= 200 (>=2.00s): long press

short_press:
  ; Short press: Increment (mode=0) decrement (mode=1)
  tst  r19
  breq increment_counter
  rjmp decrement_counter

toggle_mode:
  ; Medium press: Toggle mode and update DP indicator
  ldi  r20, 0x01
  eor  r19, r20; Toggle mode flag
  rcall use_digit; Get digit pattern for current counter (r17) into r16
  tst  r19
  breq set_dp_off; If mode now 0, ensure DP off
  sbr  r16, (1<<DP_BIT)  ; If mode is 1, set DP on
  rjmp update_display

set_dp_off:
  cbr  r16, (1<<DP_BIT)

update_display:
  rcall display
  rcall wait_release
  rjmp main

reset_action:
  ; Long press: Reset counter to 0 and force increment mode.
  ldi  r17, 0
  clr  r19
  clr  r18
  rcall use_digit
  cbr  r16, (1<<DP_BIT)
  rcall display
  rcall wait_release
  rjmp main

increment_counter:
  inc  r17 ;increments r17 by one
  cpi  r17, 16 ; makes sure it hasn't exceeded max val
  brlo update_after_counter ;branches if between 0-F. goes to sub if F
  ldi  r17, 0       ; Wrap from F to 0
update_after_counter:
  rcall use_digit; calls sub
  tst  r19; ANDS r19 to check if dp is on or off
  breq update_dp_off2 ; r19 = 0, then dp stays off
  sbr  r16, (1<<DP_BIT)
  rjmp update_after
update_dp_off2:
  cbr  r16, (1<<DP_BIT)
update_after:
  rcall display
  rcall wait_release
  rjmp main

decrement_counter:
  tst  r17
  breq wrap_to_F
  dec  r17
  rjmp update_after_counter
wrap_to_F:
  ldi  r17, 15; Wrap from 0 to F
  rjmp update_after_counter

use_digit:
  ; Load digit pattern for current value in r17 into r16.
  ldi  r16, DIGIT0
  cpi  r17, 0
  breq use_done
  cpi  r17, 1
  breq use_digit_1
  cpi  r17, 2
  breq use_digit_2
  cpi  r17, 3
  breq use_digit_3
  cpi  r17, 4
  breq use_digit_4
  cpi  r17, 5
  breq use_digit_5
  cpi  r17, 6
  breq use_digit_6
  cpi  r17, 7
  breq use_digit_7
  cpi  r17, 8
  breq use_digit_8
  cpi  r17, 9
  breq use_digit_9
  cpi  r17, 10
  breq use_digit_A
  cpi  r17, 11
  breq use_digit_B
  cpi  r17, 12
  breq use_digit_C
  cpi  r17, 13
  breq use_digit_D
  cpi  r17, 14
  breq use_digit_E
  cpi  r17, 15
  breq use_digit_F
use_done:
  ret
use_digit_1:
  ldi  r16, DIGIT1
  ret
use_digit_2:
  ldi  r16, DIGIT2
  ret
use_digit_3:
  ldi  r16, DIGIT3
  ret
use_digit_4:
  ldi  r16, DIGIT4
  ret
use_digit_5:
  ldi  r16, DIGIT5
  ret
use_digit_6:
  ldi  r16, DIGIT6
  ret
use_digit_7:
  ldi  r16, DIGIT7
  ret
use_digit_8:
  ldi  r16, DIGIT8
  ret
use_digit_9:
  ldi  r16, DIGIT9
  ret
use_digit_A:
  ldi  r16, DIGITA
  ret
use_digit_B:
  ldi  r16, DIGITB
  ret
use_digit_C:
  ldi  r16, DIGITC
  ret
use_digit_D:
  ldi  r16, DIGITD
  ret
use_digit_E:
  ldi  r16, DIGITE
  ret
use_digit_F:
  ldi  r16, DIGITF
  ret

display:
  ; Shift out 8 bits to the 74HC595
  ldi  r18, 8         ; 8 bits to shift
shift_loop:
  rol  r16            ; Rotate left; MSB to Carry
  brcs set_ser        ; If Carry set, branch to set_ser
  cbi  PORTB, SER_BIT  ; Otherwise, clear SER
  rjmp clk_pulse
set_ser:
  sbi  PORTB, SER_BIT  ; Set SER if Carry is set
clk_pulse:
  sbi  PORTB, SRCLK_BIT  ; Pulse shift clock high
  cbi  PORTB, SRCLK_BIT  ; Then low
  dec  r18 ; decrements bit counter
  brne shift_loop ; if not all shifted out, loop back
  sbi  PORTB, RCLK_BIT    ; Pulse latch clock high
  cbi  PORTB, RCLK_BIT    ; Then low
  ret

delay_10ms:
    ldi r20, 0xDC; Outer loop: 220
d10ms_loop_outer:
    ldi r21, 0xFF; Inner loop: 255
d10ms_loop_inner:
    dec r21;decrement r21
    brne d10ms_loop_inner;loop until r21 is 0
    dec r20;after innner complete, loop until 20 is 0
    brne d10ms_loop_outer;continues until 20 is 0
    ret

wait_release:
  sbis PINB, BTN_BIT; Wait until button is released (active high)
  rjmp release_ready ;when released jump to release routine
  rjmp wait_release; keep checking for release
release_ready:
  ret





