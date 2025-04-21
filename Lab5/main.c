/*
 * Nick/Andre – Lab 5
 */

#define F_CPU 16000000UL //Defined that Arduin runs at 16MHZ
#include <avr/io.h>
#include <util/delay.h> //Libraries
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BAUD     9600UL
#define MYUBRR   ((F_CPU)/(16UL*BAUD) - 1)

//Declarations because I don't like the warnings
void usartInit(uint16_t ubrr);
void usartSendChar(char c);
void usartSendString(const char *s);
char usartReceiveChar(void);
void adcInit(void);
uint16_t adcRead(uint8_t ch);

//USART stuff
void usartInit(uint16_t ubrr)
{
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t) ubrr;
    UCSR0B = (1 << RXEN0) | (1 << TXEN0); // set TX/RX for shift regs
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);   // 8?N?1
}
void usartSendChar(char c)
{
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = c;
}
void usartSendString(const char *s)
{
    while (*s) usartSendChar(*s++);
}
char usartReceiveChar(void)
{
    while (!(UCSR0A & (1 << RXC0)));
    return UDR0;
}

//Initializing ADC for use later
void adcInit(void)
{
    ADMUX  = (1 << REFS0);                       // AVcc ref, ADC0 
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1); // clk/64
}
uint16_t adcRead(uint8_t ch)
{
    ADMUX  = (ADMUX & 0xF0) | (ch & 0x0F);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADC;
}

//Helper delay function
static void delaySeconds(uint8_t sec)
{
    while (sec--) { _delay_ms(1000); } //Prolly gotta tweak this
}


int main(void)
{
    usartInit(MYUBRR);
    adcInit();
    usartSendString(
        "Ready.\r\n"
        "  G            – get single voltage\r\n"
        "  M,n,dt       – n readings, dt seconds apart\r\n"
    );

    char  cmdBuf[32];
    uint8_t idx = 0;

    while (1)
    {
        char c = usartReceiveChar();

        /* line termination? */
        if (c == '\r' || c == '\n')
        {
            cmdBuf[idx] = '\0';          // null?terminate
            idx = 0;                     // reset for next command

            // G user input loogic
            if ((cmdBuf[0] == 'G' || cmdBuf[0] == 'g') && cmdBuf[1] == '\0')
            {
                uint16_t raw = adcRead(0);
                float v = raw * 5.0 / 1023.0;
                char vStr[8];
                char out[32];
                dtostrf(v, 5, 3, vStr);          // e.g., "1.234"
                snprintf(out, sizeof out, "v=%s V\r\n", vStr);
                usartSendString(out);
                continue;
            }

           // M, n, dt logic :)
            if (cmdBuf[0] == 'M' || cmdBuf[0] == 'm')
            {
                //parse for M
                char *tok;
                uint8_t n=0, dt=0;
                tok = strtok(cmdBuf+1, ",");     
                if (tok) n  = (uint8_t)atoi(tok);
                tok = strtok(NULL, ",");
                if (tok) dt = (uint8_t)atoi(tok);

                //Making sure n/dt are in defined range
                if (n < 2 || n > 20 || dt < 1 || dt > 10)
                {
                    usartSendString("ERROR: range n=2?20, dt=1?10\r\n");
                    continue;
                }

                //header 
                char header[24];
                snprintf(header, sizeof header, "M,%u,%u\r\n", n, dt);
                usartSendString(header);

                //Loop to send out information to Serial monitor
                for (uint8_t i = 0; i < n; ++i)
                {
                    uint16_t raw = adcRead(0);
                    float v = raw * 5.0 / 1023.0;

                    char vStr[8];
                    char line[40];
                    dtostrf(v, 5, 3, vStr);
                    snprintf(line, sizeof line,
                             "t=%u s, v=%s V\r\n", (unsigned)(i*dt), vStr);
                    usartSendString(line);

                    if (i != n-1) delaySeconds(dt);
                }
                continue;
            }

            
            usartSendString("ERROR: unknown command\r\n");
        }
        else if (idx < sizeof cmdBuf - 1)     // store char
        {
            cmdBuf[idx++] = c;
        }
        
    }
}





//cd "C:\users\n3400\documents\atmel studio\7.0\EmbeddedSystems\Lab5"
//
//git status
//
//git add .
//
//git commit -m "explain what you changed here"
//
//git push


//Remember not to continuously write null character