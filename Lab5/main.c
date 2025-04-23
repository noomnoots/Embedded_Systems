/*
 * Nick/Andre – Lab 5
 */

#define F_CPU 16000000UL //Defined that Arduino runs at 16MHz
#include <avr/io.h> //Defines all of the AVR register(PORTS, UDR0, ADMUX,TWDR)
#include <util/delay.h>  //uses delay_ms() for blocking
#include <stdlib.h> //atoi(), atof(), lroundf()
#include <stdio.h> //print funcs
#include <string.h> //string manipulation
#include <math.h> //rounding functions

#define BAUD     9600UL //Buad rate (bits sent and received per second
#define MYUBRR   ((F_CPU)/(16UL*BAUD) - 1) //UBRR = F_CPU/(16*Baud)-1

// MAX517 fixed address (A0=A1=0) (0b10110000)
#define MAX517_SLA_W 0x58     // 7-bit 0x58 with write bit

// declarations
void usartInit(uint16_t ubrr);
void usartSendChar(char c);
void usartSendString(const char *s);
char usartReceiveChar(void);
void adcInit(void);
uint16_t adcRead(uint8_t ch);

// I2C help
void i2cInit(void);          // initialize TWI @100 kHz
void i2cStart(void);         // send START
void i2cWrite(uint8_t d);    // write byte, wait for ACK
void i2cStop(void);          // send STOP

// USART-----------------------------------------------------------
void usartInit(uint16_t ubrr) //ubrr 16bit divisor
{
    UBRR0H = (uint8_t)(ubrr >> 8); //top 8 bits
    UBRR0L = (uint8_t) ubrr; //bottom 8 bits, (shifted out to hardware)
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);   // set TX/RX, in control reg UCSr0B 
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // 8-N-1
} //Once done oprates at 9600bps with 8bit frames

void usartSendChar(char c)
{   //waiting for USART(UCSR0A status reg) data reg to be empty
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = c; //Once empty it writes char into data reg
}   //UDR0 transmit data reg

void usartSendString(const char *s) //takes C string, pointer to null-terminated array of chars
{
    while (*s) usartSendChar(*s++); //tests current char is non-zero
}//Calls usartSendChar that blocks until the UART is ready then writes it out on PD1

char usartReceiveChar(void)
{ //RXC0 (receive complete) becomes 1 when the Uart hardware has received the char into UDR0
    while (!(UCSR0A & (1 << RXC0))); //runs until char is on RXD(PD0)
    return UDR0;//read and return UDR0, and clear the RXC0 flag, prepares for next byte
}

// ADC----------------------------------------------------------

void adcInit(void)
{
    ADMUX  = (1 << REFS0); //REFS0 =1 selects AVcc (voltage reference for conversions) 
    ADCSRA = (1 << ADEN)  //brings power to the ADC module
				| (1 << ADPS2) 
				| (1 << ADPS1); //divides 16MHz system clock by 64, giving ~250Khz for ADC clock
}//when done ADC is ready and conversions can start

uint16_t adcRead(uint8_t ch) 
{
    ADMUX  = (ADMUX & 0xF0) | (ch & 0x0F);//selects analog input
	//Masks low 4 bits of ADMUX, then OR in
	//makes ADC multiplexer so the next conversion reads ADC[ch]
	
    ADCSRA |= (1 << ADSC);//starts conversion in setting ADSC
	//hardware samples selected pin, charging capacitor, then performs approximation conversion
	
    while (ADCSRA & (1 << ADSC));//waits for conversion to finish
    return ADC;//Reads and returns the 10bit result when 
	//ADC is 16bit reg that combines ADCL and ADCH. holds raw 0-1023 value
}

// I2C-----------------------------------------------------------------
void i2cInit(void)
{
    TWSR = 0; //sets a prescaler of 1 [TWPS = 00 = 1]
    TWBR = ((F_CPU / 100000UL) - 16) / 2; //sets the bit rate 
	//TWBR = (16MHz/100kHz -16)/2 = 72 (Determines SCL speed)
	
    TWCR = (1 << TWEN);//sets enable on TWI (I2C) hardware
	
}// when ran, the Two wire inerface pins generate bus signals at ~100kHz

void i2cStart(void)
{//sending start conditions to bus
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
	//In order; Clears interrupt flag, Transmits the start condition, Makes sure TWI is enabled
    while (!(TWCR & (1 << TWINT)));
	//waits for the srart to complete
}

void i2cWrite(uint8_t d)
{
    TWDR = d;//Loads the data/address byte into TWI data reg
	//put 'd' onto SDA to shift out
	
    TWCR = (1 << TWINT) | (1 << TWEN);//Clear TWINT + maek sure TWEN so hardware begins transmitting
	// In order; Clears interrupt flag to start, Keeps TWI enabled
	
    while (!(TWCR & (1 << TWINT)));//Blocks all bytes sent until the ACK/NACK is received 
}

void i2cStop(void)
{//issues stop condition to bus
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);
	//TWSTO = 1 -SDA goes high while SCA is high
	//TWINT = 1 -clears pending interrupt
	//Twen = 1 -keeps hardware enabled
}

// helpers
static void delaySeconds(uint8_t sec)
{
    while (sec--) { _delay_ms(1000); } //decrements one sec per iteration 
}//used to control M,n,dt so we can get the time pauses we want

// Main----------------------------------------------------------------------
int main(void)
{
	//sets up the UART at the baud rate specified by MYUBRR
	usartInit(MYUBRR);
	//Configures ADC to take analog voltage values
	adcInit();
	//Set up the i2c connection
	i2cInit();

	//Sends these strings on startup as the instructions
	usartSendString(
	"Ready.\r\n"
	"  G            - get single voltage\r\n"
	"  M,n,dt       - n readings, dt seconds apart\r\n"
	"  S,c,v        - set DAC voltage\r\n"
	);

	//Stores the characters typed until the user hits enter
	char    cmdBuf[32];
	//idx tracks how many characters have been typed
	uint8_t idx = 0;

	//Will continue indefinitley
	while (1)
	{
		//This will wait until a new command is entered
		char c = usartReceiveChar();

		//If the user pressed enter
		if (c == '\r' || c == '\n')
		{
			//Null terminate the input string
			cmdBuf[idx] = '\0';
			// reset the index
			idx = 0;
			//  skips further processing if the user pressed Enter without typing anything
			if (cmdBuf[0] == '\0') continue;

			// G logic
			// Will check if the user types in G or g
			if ((cmdBuf[0] == 'G' || cmdBuf[0] == 'g') && cmdBuf[1] == '\0')
			{
				//This command will read one voltage value from ADC channel 0 which is connected to the potentiometer
				uint16_t raw = adcRead(0);
				//Will do math to take the value given from 0-255 and change it to the corresponding voltage value from 0-5V
				float    v   = raw * 5.0 / 1023.0;
				//Convert the float to a string with 3 decimal points
				char     vStr[8];
				dtostrf(v, 5, 3, vStr);
				//Format the output string
				char     out[32];
				snprintf(out, sizeof out, "v=%s V\r\n", vStr);
				//Send result back to UART
				usartSendString(out);
				continue;
			}

			// M,n,dt logic
			if (cmdBuf[0] == 'M' || cmdBuf[0] == 'm')
			{
				//Tokenize the imput
				char    *tok;
				uint8_t  n  = 0, dt = 0;

				//Parse the n value
				tok = strtok(cmdBuf + 1, ",");
				if (tok) n  = (uint8_t)atoi(tok);

				//Parse the dt value
				tok = strtok(NULL, ",");
				if (tok) dt = (uint8_t)atoi(tok);

				//Check if the values are in the given ranges
				if (n < 2 || n > 20 || dt < 1 || dt > 10)
				{
					usartSendString("ERROR: range n=2-20, dt=1-10\r\n");
					continue;
				}

				//Echo back the command
				char header[24];
				snprintf(header, sizeof header, "M,%u,%u\r\n", n, dt);
				usartSendString(header);

				//Loop for n readings
				for (uint8_t i = 0; i < n; ++i)
				{
					//Do what was done like G
					uint16_t raw = adcRead(0);
					float    v   = raw * 5.0 / 1023.0;
					char     vStr[8];
					char     line[40];
					dtostrf(v, 5, 3, vStr);
					//Print the time since the loop started in s and what the voltage is currently
					snprintf(line, sizeof line,
					"t=%u s, v=%s V\r\n", (unsigned)(i * dt), vStr);
					usartSendString(line);

					//If n does not = -1 do a delay for dt seconds
					if (i != n - 1) delaySeconds(dt);
				}
				continue;
			}

            // S,c,v logic
            if (cmdBuf[0] == 'S' || cmdBuf[0] == 's') //takes input for S command
            {
                char    *tok; //pointer for command buffer
                uint8_t  chan  = 2; //Init at 2 so if parsing fails, range check will catch it
                float    volts = 0.0f; //Initializes at 0V for safe default

                tok = strtok(cmdBuf + 1, ","); //Skips first char in Cmdbuf, then finds substring to first comma
                if (tok) chan = (uint8_t)atoi(tok); //converts string "0" to integer 0
                tok = strtok(NULL, ","); //starts where strtok leaves off and gets next comma
                if (tok) volts = atof(tok); //converts value (Ex. "3.45") to float 3.45

                if ((chan > 1) || volts < 0.0f || volts > 5.0f) //If statement to catch if user screwed up input
                {
                    usartSendString("ERROR: S,c,v  c=0|1  v=0-5\r\n");
                    continue;
                }

                uint8_t code    = (uint8_t)lroundf(volts * 255.0f / 5.0f);
				//Scales to 8-bit digital value (518 needs int from 0-255)
                uint8_t cmdByte = chan & 0x01;  // PowerDown=0 Reset=0 A0= which DAC channel we're loading
				
				//calling functions  to use 
                i2cStart();//Tells bus I'm the master
                i2cWrite(MAX517_SLA_W); //Shifts out DAC 7bit addr and the write bit
				//Max518 sees addr and pulls SDA on low on the 9th clock as ACK
				
                i2cWrite(cmdByte); //Sends the command byte that tells DAC which channel to use
                i2cWrite(code); //Sends 8-bit scaled voltage code. When received, DAC updates
                i2cStop();//Generates stop condition, triggers MAX to transfer input latch onto
				//output amp, fianlly physically changes the voltage on outputs

                // build response with dtostrf (printf %f not supported)
                char  vStr2[8];
                char  resp[48];
                dtostrf(volts, 5, 2, vStr2);//Converts foat into, 2dec ASCII string
                snprintf(resp, sizeof resp, //takes Channel #, voltage string and, code into resp
                         "DAC channel %u set to %s V (%u)\r\n",
                         chan, vStr2, code);
                usartSendString(resp);//sends info back over serial link to see:
                continue;
            }

            usartSendString("ERROR: unknown command\r\n"); //error message for unknown command
        }
        else if (idx < sizeof cmdBuf - 1) //checks if incoming char 'c' was not a line terminator
        { //stuff in else statement makes sure that we never go to the end of the array, but leaves room for \0 terminator
            cmdBuf[idx++] = c;
			//receives char at current index, then increments idx by one, for next char
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
