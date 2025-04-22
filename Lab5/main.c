/*
 * Nick/Andre – Lab 5
 */

#define F_CPU 16000000UL //Defined that Arduin runs at 16MHZ
#include <avr/io.h>
#include <util/delay.h> //Libraries
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define BAUD     9600UL
#define MYUBRR   ((F_CPU)/(16UL*BAUD) - 1)

// MAX517 fixed address (A0=A1=0)
#define MAX517_SLA_W 0x58     // 7?bit 0x58 with write bit //

//declarations cause I don't like the warnings.
void usartInit(uint16_t ubrr);
void usartSendChar(char c);
void usartSendString(const char *s);
char usartReceiveChar(void);
void adcInit(void);
uint16_t adcRead(uint8_t ch);

// I2C helper
void i2cInit(void);          // initialize TWI @100 kHz
void i2cStart(void);         // send START
void i2cWrite(uint8_t d);    // write byte, wait for ACK
void i2cStop(void);          // send STOP

//USART
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

//ADC
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

//I2C funcs
void i2cInit(void)
{
    TWSR = 0;                                     // prescaler = 1
    TWBR = ((F_CPU / 100000UL) - 16) / 2;         // ?100 kHz
    TWCR = (1 << TWEN);
}
void i2cStart(void)
{
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
}
void i2cWrite(uint8_t d)
{
    TWDR = d;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
}
void i2cStop(void)
{
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);
}

//helpers
static void delaySeconds(uint8_t sec)
{
    while (sec--) { _delay_ms(1000); }
}

//Main---------------------------------------------------------------------------------
int main(void)
{
    usartInit(MYUBRR);
    adcInit();
    i2cInit();

    usartSendString(
        "Ready.\r\n"
        "  G            – get single voltage\r\n"
        "  M,n,dt       – n readings, dt seconds apart\r\n"
        "  S,c,v        – set DAC voltage\r\n"
    );

    char  cmdBuf[32];
    uint8_t idx = 0;

    while (1)
    {
        char c = usartReceiveChar();

        //ending lines
        if (c == '\r' || c == '\n')
        {
            cmdBuf[idx] = '\0';
            idx = 0;
            if (cmdBuf[0] == '\0') continue;

            //G logic-----------------------------------------------------
            if ((cmdBuf[0] == 'G' || cmdBuf[0] == 'g') && cmdBuf[1] == '\0')
            {
                uint16_t raw = adcRead(0);
                float v = raw * 5.0 / 1023.0;
                char vStr[8];
                char out[32];
                dtostrf(v, 5, 3, vStr);
                snprintf(out, sizeof out, "v=%s V\r\n", vStr);
                usartSendString(out);
                continue;
            }

            // M,n,dt logic 
            if (cmdBuf[0] == 'M' || cmdBuf[0] == 'm')
            {
                char *tok;
                uint8_t n = 0, dt = 0;
                tok = strtok(cmdBuf + 1, ",");
                if (tok) n = (uint8_t)atoi(tok);
                tok = strtok(NULL, ",");
                if (tok) dt = (uint8_t)atoi(tok);

                if (n < 2 || n > 20 || dt < 1 || dt > 10)
                {
                    usartSendString("ERROR: range n=2?20, dt=1?10\r\n");
                    continue;
                }

                char header[24];
                snprintf(header, sizeof header, "M,%u,%u\r\n", n, dt);
                usartSendString(header);

                for (uint8_t i = 0; i < n; ++i)
                {
                    uint16_t raw = adcRead(0);
                    float v = raw * 5.0 / 1023.0;

                    char vStr[8];
                    char line[40];
                    dtostrf(v, 5, 3, vStr);
                    snprintf(line, sizeof line,
                             "t=%u s, v=%s V\r\n", (unsigned)(i * dt), vStr);
                    usartSendString(line);

                    if (i != n - 1) delaySeconds(dt);
                }
                continue;
            }

            //S,c,v logic
            if (cmdBuf[0] == 'S' || cmdBuf[0] == 's')
            {
	            char *tok;
	            uint8_t chan = 2;
	            float volts = 0.0f;

	            tok = strtok(cmdBuf + 1, ",");
	            if (tok) chan = (uint8_t)atoi(tok);
	            tok = strtok(NULL, ",");
	            if (tok) volts = atof(tok);

	            if ((chan > 1) || volts < 0.0f || volts > 5.0f)
	            {
		            usartSendString("ERROR: S,c,v  c=0|1  v=0?5\r\n");
		            continue;
	            }

	            uint8_t code = (uint8_t)lroundf(volts * 255.0f / 5.0f);
	            uint8_t cmdByte = chan & 0x01;          // PD=0 RST=0 A0=chan

	            i2cStart();               uint8_t s1 = TWSR & 0xF8;
	            i2cWrite(MAX517_SLA_W);   uint8_t s2 = TWSR & 0xF8;
	            i2cWrite(cmdByte);        uint8_t s3 = TWSR & 0xF8;
	            i2cWrite(code);           uint8_t s4 = TWSR & 0xF8;
	            i2cStop();

// 	            char dbg[32];
// 	            snprintf(dbg, sizeof dbg, "STS:%02X %02X %02X %02X\r\n", s1, s2, s3, s4);
// 	            usartSendString(dbg);

	            char resp[48];
	            snprintf(resp, sizeof resp,
	            "DAC channel %u set to %.2f V (%u)\r\n",
	            chan, volts, code);
	            usartSendString(resp);
	            continue;
            }


            usartSendString("ERROR: unknown command\r\n");
        }
        else if (idx < sizeof cmdBuf - 1)
        {
            cmdBuf[idx++] = c;
        }
    }
}





//cd "C:\users\n3400\documents\atmel studio\7.0\EmbeddedSystems"
//
//git status
//
//git add .
//
//git commit -m "explain what you changed here"
//
//git push


//Remember not to continuously write null character
