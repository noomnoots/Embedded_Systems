.include "m328Pdef.inc"

; Data Segment (RAM Variables)
.dseg
fan_on_off_state:    .byte 1		  ; Tracks if the fan is on/off
last_nonzero_duty:   .byte 1          ; Computed PWM value (0–255)
scaled_duty:         .byte 1          ; Variable: 0 (0%) to 100 (100%)
old_pinb_snapshot:   .byte 1		  ; Stores a snapshot of the PINB register
oldAB:               .byte 1		  ; Stores past state information from two bits read from PIND 
dtxt:                .BYTE 6          ; 6-byte display buffer for "XX.X%"

; in flash memory
.cseg
.org 0x032c
FAN_ON_MSG:     .db "FAN=ON ",0
FAN_OFF_MSG:    .db "FAN=OFF",0
DC_LABEL:       .db "DC=",0

; Interrupt Vector Table

.org 0x0000
    rjmp reset					; Jumps to the reset routine.
.org 0x0006
    rjmp PCINT0_ISR				;PCINT0 is used to toggle the fan state
.org 0x000A
    rjmp PCINT2_ISR				; PCINT2 handles the rotary encoder.

; Reset Routine
reset:
	;Initializes the stack with pointer SPL and SPH with the address of the end of RAM
    ldi  r16, LOW(RAMEND)
    out  SPL, r16
    ldi  r16, HIGH(RAMEND)
    out  SPH, r16

    ; Setup LCD I/O by setting portB and portC pins
    ldi  r16, (1<<PB5) | (1<<PB3)
    out  DDRB, r16
    ldi  r16, 0x0F
    out  DDRC, r16
	; Small delay before initializing, then initializes, and clears the screen
    rcall DELAY_100MS
    rcall LCD_INIT
    rcall LCD_CLEAR

    ; Initialize variables:
	; fan off initially
    ldi  r16, 0         
    sts  fan_on_off_state, r16
    ; Set scaled_duty to 50 (i.e. 50%)
    ldi  r16, 50
    sts  scaled_duty, r16
    ; Compute last_nonzero_duty = (scaled_duty * 255) / 100
    lds  r16, scaled_duty
    ldi  r18, 255
	; r1:r0 = scaled_duty*255
    mul  r16, r18     
	; quotient in r16  
    rcall DIVIDE_100    
    sts  last_nonzero_duty, r16

	; Reads the state of PINB and PINB to initialize old_pinb_snapshot and oldAB
    in   r16, PINB           ; Read button port
    sts  old_pinb_snapshot, r16 ; Save snapshot

    in   r16, PIND        ; Read encoder pins
    andi r16, 0x18            ; Mask PD3 & PD4
    ror  r16                ; Rotate down into bits0/1
    ror  r16
    ror  r16
    sts  oldAB, r16           ; Save encoder state

    sbi  DDRD, 5              ; DDRD5 = PWM output (OC0B)

    in   r16, PORTB            ; Read PORTB
    ori  r16, 0x01              ; Enable pull?up on PB0
    out  PORTB, r16
    in   r16, DDRB           ; Read DDRB
    andi r16, 0xFE              ; Set PB0 as input
    out  DDRB, r16

    in   r16, PORTD            ; Read PORTD
    ori  r16, 0x18           ; Enable pull?ups on PD3/PD4
    out  PORTD, r16

    ldi  r16, (1<<PCIE0)|(1<<PCIE2) ; Enable PCINT0 & PCINT2
    sts  PCICR, r16
    ldi  r16, (1<<PCINT0)       ; Mask PB0
    sts  PCMSK0, r16
    ldi  r16, (1<<3)|(1<<4)     ; Mask PD3 & PD4
    sts  PCMSK2, r16

    ldi  r16, (1<<COM0B1)|(1<<WGM01)|(1<<WGM00)
                                ; Fast PWM, non?inverting OC0B
    out  TCCR0A, r16
    ldi  r16, (1<<CS01)         ; Prescaler clk/8
    out  TCCR0B, r16

    ldi  r16, 0x00             ; Duty = 0
    out  OCR0B, r16           ; Turn fan off

	; Used to show the initial message on the LCD and enable interrupts
    sei
    rcall LCD_DISPLAY_FLASH

	; Jump into main which is a endless loop
    rjmp main
main:
    rjmp main

; Interrupt Service Routines
PCINT0_ISR:
    push r16			;Saves register r16
    in   r16, PINB		;Reads PINB
    sbrs r16, 0			;Skip if button is pressed
    rjmp _toggle_fan	;jump to toggle fan
    pop  r16			;Restores r16 
    reti

_toggle_fan:
    lds  r16, fan_on_off_state	;Load the current fan state
    ldi  r17, 0x01				; load 0x01 into r17 
    eor  r16, r17				;Toggles the fan state
    sts  fan_on_off_state, r16	; store the updated state
    tst  r16					; Test if the state is off
    breq fan_off				; if the test is true, jump to fan_off

    lds  r16, last_nonzero_duty	;Load the last non-zero duty cycle
    out  OCR0B, r16				;Set duty cycle to the stored value
    rcall LCD_UPDATE_FAN_STATE	;Update LCD with fan state
    rcall update_duty_display	;Update LCD with duty cycle
    pop  r16					;Restore r16
    reti

fan_off:
    ldi  r16, 0					;Turn off fan
    out  OCR0B, r16				;Turn off PWM
    rcall LCD_UPDATE_FAN_STATE	;Update LCD with fan state
    rcall update_duty_display	;Update LCD with duty cycle
    pop  r16					;Restore r16
    reti

PCINT2_ISR:
    push r16
    push r17
    push r18
    push r19
    in   r19, SREG			; Save the status register
    rcall handle_encoder	; Call the encoder handling function
    out  SREG, r19			; Restore the status register
    pop  r19
    pop  r18
    pop  r17
    pop  r16
    reti

handle_encoder:
    in   r16, PIND          ; Read the PIND register (pins for encoder)
    andi r16, 0x18          ; Mask out irrelevant bits
    lsr  r16                ; Shift right three times to get proper bits
    lsr  r16				; Shift right three times to get proper bits
    lsr  r16				; Shift right three times to get proper bits
    lds  r17, oldAB         ; Load the previous encoder state
    sts  oldAB, r16         ; Store the new encoder state
    mov  r18, r17           ; Copy previous state to r18
    swap r18                ; Swap nibbles of r18
    andi r18, 0x0C          ; Mask the relevant bits
    or   r18, r16           ; Combine old and new states
	; Compare with clockwise patterns
	; If clockwise, branch to encoder_CW
    cpi  r18, 0x01          
    breq encoder_CW        
    cpi  r18, 0x07        
    breq encoder_CW
    cpi  r18, 0x0E         
    breq encoder_CW
    cpi  r18, 0x08          
    breq encoder_CW
	; Compare with counterclockwise patterns
	; If counterclockwise, branch to encoder_CCW
    cpi  r18, 0x02          
    breq encoder_CCW        
    cpi  r18, 0x06          
    breq encoder_CCW
    cpi  r18, 0x09          
    breq encoder_CCW
    cpi  r18, 0x0D          
    breq encoder_CCW
    ret

encoder_CW:
    rcall inc_duty	;Increase duty cycle
    ret

encoder_CCW:
    rcall dec_duty	;Decrease duty cycle
    ret


; New Duty Update Routines Using scaled_duty

; inc_duty: Increment scaled_duty by 1 (max 100), then recalc PWM duty.
inc_duty:
    lds   r16, scaled_duty	;Load the current scaled duty cycle
    ;Compare r16 with 100 and skip the next line if it equals 100
	cpi   r16, 100	
    breq  inc_done
    inc   r16
    sts   scaled_duty, r16	;Store the updated value back to scaled_duty
    mov   r20, r16        ; copy scaled_duty to r20
    ldi   r18, 255
    mul   r20, r18        ; r1:r0 = scaled_duty * 255
    rcall DIVIDE_100      ; Divide r1:r0 by 100; quotient in r16
    sts   last_nonzero_duty, r16	;Store the quotient in last_nonzero_duty
    lds   r16, last_nonzero_duty	;Load it into r16
    lds   r17, fan_on_off_state		;Load the fan state
    tst   r17		;Test if the fan is on or not
    breq  inc_skip_pwm	;If it is off skipp next line
    out   OCR0B, r16
inc_skip_pwm:
    rcall update_duty_display	;Update the LCD with the new duty cycle
inc_done:
    ret

; dec_duty: Decrement scaled_duty by 1 (min 0), then recalc PWM duty.
dec_duty:
    lds   r16, scaled_duty  ; Load the current scaled duty cycle into r16
    tst   r16                ; Test if scaled_duty is 0
    breq  dec_done           ; If it's already 0, skip the decrement 
    dec   r16                ; Decrement scaled_duty by 1
    sts   scaled_duty, r16   ; Store the updated value back to scaled_duty
    mov   r20, r16           ; Copy scaled_duty to r20
    ldi   r18, 255           ; Load 255 into r18 
    mul   r20, r18           ; Multiply r20 by 255
    rcall DIVIDE_100         ; Divide the result by 100
    sts   last_nonzero_duty, r16 ; Store the quotient in last_nonzero_duty
    lds   r16, last_nonzero_duty ; Load the calculated duty cycle into r16
    lds   r17, fan_on_off_state ; Load the fan state (on/off)
    tst   r17                ; Test if the fan is off 
    breq  dec_skip_pwm       ; If the fan is off, skip next line
    out   OCR0B, r16         ; Set the OCR0B register with the new duty cycle value
dec_skip_pwm:
    rcall update_duty_display ; Update the LCD with the new duty cycle
dec_done:
    ret                      


; LCD Subroutines 
LCD_INIT:
    cbi PORTB, PB5			;Clear RS pin
    ldi r16, 0x03
    rcall LCD_WRITE_NIBBLE	;Send the first nibble
    rcall DELAY_200MS
    rcall LCD_WRITE_NIBBLE	;Send the second nibble
    rcall DELAY_200MS
    rcall LCD_WRITE_NIBBLE	;Send the third nibble
    rcall DELAY_200MS
    ldi r16, 0x02			;Load 4 bit mode into r16
    rcall LCD_WRITE_NIBBLE	;Send the fourth nibble
    rcall DELAY_100MS

    ldi r16, 0x28              ; Set the LCD to 2 lines, 5x8 dots
    rcall LCD_WRITE_CMD        ; Send command
    rcall DELAY_100MS          
    ldi r16, 0x0C              ; Display ON, cursor OFF
    rcall LCD_WRITE_CMD        ; Send command
    ldi r16, 0x06              ; Entry mode: Increment cursor, no shift
    rcall LCD_WRITE_CMD        ; Send command
    rcall LCD_CLEAR            ; Clear the display
    ret                        

;Clear the LCD screen and resets the cursor position
LCD_CLEAR:
    ldi r16, 0x01
    rcall LCD_WRITE_CMD
    rcall DELAY_200MS
    ret

;Prepares the LCD to receive a command by clearing RS (Register Select) 
;and then calls LCD_WRITE_BYTE to send the command byte.
LCD_WRITE_CMD:
    cbi PORTB, PB5
    rcall LCD_WRITE_BYTE
    ret

;Prepares the LCD to receive a character by setting RS (Register 
;Select) and then calls LCD_WRITE_BYTE to send the character byte.
LCD_WRITE_CHAR:
    sbi PORTB, PB5
    rcall LCD_WRITE_BYTE
    ret

;Sends an 8-bit byte to the LCD in two 4-bit nibbles. It first swaps the 
;nibbles in the byte, writes the high nibble, then writes the low nibble.
LCD_WRITE_BYTE:
    push r17                   ; Save register r17
    push r18                   ; Save register r18
    mov  r17, r16              ; Copy r16 to r17
    mov  r18, r16              ; Copy r16 to r18
    swap r17                   ; Swap the nibbles in r17
    mov  r16, r17              ; Move the swapped nibbles into r16
    rcall LCD_WRITE_NIBBLE     ; Write the high nibble
    andi r18, 0x0F             ; Mask the lower nibble of r18
    mov  r16, r18              ; Move the lower nibble into r16
    rcall LCD_WRITE_NIBBLE     ; Write the low nibble
    rcall DELAY_100US         
    pop  r18                   ; Restore r18
    pop  r17                   ; Restore r17
    ret                        ; Return from subroutine

;Sends a 4-bit nibble to the LCD by combining it with the current state of 
;PORTC and writing it to the data pins, followed by toggling the 
;Enable pin (PB3).
LCD_WRITE_NIBBLE:
    in   r17, PORTC            ; Read PORTC into r17
    andi r17, 0xF0             ; Mask the high nibble of PORTC
    andi r16, 0x0F             ; Mask the low nibble of r16
    or   r16, r17              ; Combine the nibbles and send to PORTC
    out  PORTC, r16            ; Output to PORTC
    sbi  PORTB, PB3            ; Set PB3 (Enable pin high)
    rcall DELAY_100US          ; Wait for 100 microseconds
    cbi  PORTB, PB3            ; Set PB3 (Enable pin low)
    rcall DELAY_100US          ; Wait for 100 microseconds
    ret                        ; Return from subroutine

;This subroutine flashes the duty cycle and fan 
;state on the LCD with a 100ms delay between each update.
LCD_DISPLAY_FLASH:
    rcall update_duty_display	; Update the duty cycle display
    rcall DELAY_100MS
    rcall LCD_UPDATE_FAN_STATE	; Update the fan state display
    rcall DELAY_100MS
    ret

;Displays the fan state ("Fan ON" or "Fan OFF") on the second line 
;of the LCD. The text is selected based on the fan_on_off_state variable.
LCD_UPDATE_FAN_STATE:
    ldi r16, 0xC0                 ; Load 0xC0 (second line, first column) into r16
    rcall LCD_WRITE_CMD           ; Send command to set cursor to the second line
    lds r16, fan_on_off_state     ; Load the fan state into r16
    tst r16                       ; Test the fan state
    breq print_fan_off			  ; If the fan is off, jump to print_fan_off
    ldi ZL, LOW(FAN_ON_MSG << 1)  ; Load address of FAN_ON_MSG
    ldi ZH, HIGH(FAN_ON_MSG << 1)
    rjmp print_flash_msg          ; Jump to print the "fan on" message
print_fan_off:
    ldi ZL, LOW(FAN_OFF_MSG << 1) ; Load address of FAN_OFF_MSG
    ldi ZH, HIGH(FAN_OFF_MSG << 1)
print_flash_msg:
    rjmp LCD_PRINT_FLASH          ; Jump to print the message

;Loops through the characters of a message (either "Fan ON" or "Fan OFF") 
;and prints them on the LCD until the null terminator is encountered.
LCD_PRINT_FLASH:
PRINT_FLASH_LOOP:
    lpm r16, Z+                ; Load the next byte from the flash message
    cpi r16, 0x00              ; Compare with null byte 
    breq PRINT_FLASH_DONE      ; If it's the null byte end the message
    rcall LCD_WRITE_CHAR       ; Write character to LCD
    rjmp PRINT_FLASH_LOOP      ; Loop for the next character
PRINT_FLASH_DONE:
    ret


; New Dynamic String Print Subroutine (from RAM)
displayDString:
dstring_loop:
    ld   r16, Z+	;Loads the value at the memory address pointed by Z pair
    tst  r16		;Test if r16 is zero and if true go to next line otherwise skip it
    breq dstring_done
    rcall LCD_WRITE_CHAR	;Calls LCD_WRITE_CHAR to write the character in r16
    rjmp dstring_loop
dstring_done:
    ret

;is designed to store a value into memory and 
;increment the pointer (the Y register pair, r28:r29) after each call. 
copy_to_buf:
    st    Y+, r19	; Stores the value from r19 into Y
    ret

; Redefine update_duty_display with Y pointer set-up.
update_duty_display:
    ; Preserve registers including Y registers
    push r16
    push r17
    push r18
    push r19
    push r20
    push r21
    push r28
    push r29

    ; Initialize Y pointer to destination buffer
    ldi  r28, low(dtxt)
    ldi  r29, high(dtxt)

    ; Get the scaled_duty (0–100) from RAM.
    lds  r16, scaled_duty

    ; Use DIVIDE_10 to split r16 into tens and ones.
    mov  r30, r16      ; copy duty into r30
    rcall DIVIDE_10    ; r18 = tens, r17 = ones

    ; Build the display string in dtxt.
    ; dtxt = [tens][ones]['.']['0']['%'][0]
    cpi  r18, 0
    breq build_space2
    ldi  r19, '0'
    add  r19, r18
    rcall copy_to_buf        ; store tens in dtxt[0]
    rjmp build_ones2
build_space2:
    ldi  r19, ' '
    rcall copy_to_buf        ; dtxt[0] = ' '
build_ones2:
    ldi  r19, '0'
    add  r19, r17
    rcall copy_to_buf        ; dtxt[1] = ones digit
    ldi  r19, '.'
    rcall copy_to_buf        ; dtxt[2] = '.'
    ldi  r19, '0'
    rcall copy_to_buf        ; dtxt[3] = '0'
    ldi  r19, '%'
    rcall copy_to_buf        ; dtxt[4] = '%'
    ldi  r19, 0
    rcall copy_to_buf        ; dtxt[5] = 0

    ; Set LCD cursor to column 4 of line 1.
    ldi  r16, 0x83
    rcall LCD_WRITE_CMD

    ; Print the static label "DC=" from flash.
    ldi  ZL, LOW(DC_LABEL << 1)
    ldi  ZH, HIGH(DC_LABEL << 1)
    rcall LCD_PRINT_FLASH

    ; Print the dynamic string stored in dtxt.
    ldi  r30, low(dtxt)
    ldi  r31, high(dtxt)
    rcall displayDString

    ; Restore registers and return.
    pop  r29
    pop  r28
    pop  r21
    pop  r20
    pop  r19
    pop  r18
    pop  r17
    pop  r16
    ret

; Calculates last_nonzero_duty from scaled_duty and then calls update_duty_display.
update_duty_pwm:
    lds  r16, scaled_duty        ; Get scaled duty (0-100)
    ldi  r18, 255
    mul  r16, r18               ; r1:r0 = scaled_duty * 255
    rcall DIVIDE_100            ; Divide r1:r0 by 100; quotient in r16
    sts  last_nonzero_duty, r16
    out  OCR0B, r16
    ret

; Performs: r17:r16 ÷ r19:r18
; Returns: Quotient in r21:r20 (r21:r20),
;          Remainder in r23:r22 (r23:r22)
div16u:
    clr   r21         
    clr   r20      
    clr   r23       
    clr   r22         
    ldi   r24, 16          ; Bit counter = 16
div16u_loop:
    rol   r22			; Rotate left remainder (r22:r23)
    rol   r23
    rol   r16			; Shift dividend left (r16:r17)
    rol   r17
    lsl   r20			 ; Shift quotient left
    rol   r21
    cp    r22, r18  ; Compare low byte of remainder with low byte of divisor
	cpc   r23, r19  ; Compare high byte of remainder with high byte of divisor
    brlo  div16u_skip
    sub   r22, r18	; Subtract divisor from remainder
    sbc   r23, r19
    ori   r20, 0x01	; Set the corresponding bit of quotient to 1
div16u_skip:
    dec   r24
    brne  div16u_loop
    ret

; Divides the 8?bit number in r30 by 10.
; Outputs: Quotient in r18 and remainder in r17.
DIVIDE_10:
    mov   r20, r30        ; copy dividend into r20
    clr   r18             ; clear quotient (r18 = 0)
DIV_LOOP:
    cpi   r20, 10
    brlo  DIV_DONE
    subi  r20, 10
    inc   r18
    rjmp  DIV_LOOP
DIV_DONE:
    mov   r17, r20
    ret

; Divides a 16?bit number in r1:r0 by 100.
; Outputs: Quotient in r16; remainder is discarded.
DIVIDE_100:
    clr   r16             ; r16 will hold the quotient, initialize to 0
DIV100_LOOP:
    ; Set up constant 0x0064 (100) using registers that support LDI (r20 and r21, which are >= r16)
    ldi   r20, 0x00       ; high byte constant = 0
    ldi   r21, 0x64       ; low byte constant = 100 (0x64)
    cp    r0, r21         ; compare the low byte of dividend (r0) with 100
    cpc   r1, r20         ; compare the high byte (r1) with 0, taking carry into account
    brlo  DIV100_DONE    ; if dividend < 100, then done
    ; Copy the dividend from r1:r0 into a temporary pair (r25:r24)
    mov   r24, r0
    mov   r25, r1
    ; Now subtract 100 from this temporary dividend.
    subi  r24, 0x64       ; subtract 100 from r24 (r24 is >= r16 so it's allowed)
    ldi   r20, 0          ; set r20 to 0 for the next immediate subtraction
    sbc   r25, r20        ; subtract with carry from r25
    ; Copy the updated result back to the dividend (r1:r0)
    mov   r0, r24
    mov   r1, r25
    inc   r16             ; increment the quotient
    rjmp  DIV100_LOOP
DIV100_DONE:
    ret



;Prints a two-digit number with a leading zero if needed
print2digit:
    push  r18
    ldi   r18, 10
    mov   r16, r16
    clr   r17
    mov   r18, r18
    clr   r19
    rcall div16u
    ldi   r16, '0'
    add   r16, r20
    rcall LCD_WRITE_CHAR
    ldi   r16, '0'
    add   r16, r22
    rcall LCD_WRITE_CHAR
    pop   r18
    ret

; Delay Subroutines
DELAY_100US:
    ldi r20, 200 ; loads 320 w/ 200 then enters 2 nops before its decremented creating a 100 microsec delay
DELAY_100US_LOOP:
    nop
    nop
    dec r20
    brne DELAY_100US_LOOP
    ret

DELAY_100MS: ;uses timer 1 by clearing ctrl regs and configures it at 256 to create 100ms delay
    ldi r16, 0
    sts TCCR1B, r16
    sts TCCR1A, r16
    ldi r16, (1<<WGM12) | (1<<CS12)
    sts TCCR1B, r16
    ldi r16, low(3124)
    sts OCR1AL, r16
    ldi r16, high(3124)
    sts OCR1AH, r16
    ldi r16, 0
    sts TCNT1L, r16
    sts TCNT1H, r16
    sbi TIFR1, OCF1A
DELAY_100MS_WAIT:
    in r16, TIFR1
    sbrs r16, OCF1A
    rjmp DELAY_100MS_WAIT
    ldi r16, 0
    sts TCCR1B, r16
    ret

DELAY_200MS:
    ldi r16, 0
    sts TCCR1B, r16
    sts TCCR1A, r16
    ldi r16, (1<<WGM12) | (1<<CS12)
    sts TCCR1B, r16
    ldi r16, low(6249)
    sts OCR1AL, r16
    ldi r16, high(6249)
    sts OCR1AH, r16
    ldi r16, 0
    sts TCNT1L, r16
    sts TCNT1H, r16
    sbi TIFR1, OCF1A
DELAY_200MS_WAIT:
    in r16, TIFR1
    sbrs r16, OCF1A
    rjmp DELAY_200MS_WAIT
    ldi r16, 0
    sts TCCR1B, r16
    ret