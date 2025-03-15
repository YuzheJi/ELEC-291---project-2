#include <stdio.h>
#include "../Common/Include/stm32l051xx.h"
#include "../Common/Include/serial.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "UART2.h"
#include "lcd.h"
#include "adc.h"

// LQFP32 pinout
//              ----------
//        VDD -|1       32|- VSS
//       PC14 -|2       31|- BOOT0
//       PC15 -|3       30|- PB7
//       NRST -|4       29|- PB6
//       VDDA -|5       28|- PB5
// LCD_RS PA0 -|6       27|- PB4
// LCD_E  PA1 -|7       26|- PB3
// LCD_D4 PA2 -|8       25|- PA15
// LCD_D5 PA3 -|9       24|- PA14
// LCD_D6 PA4 -|10      23|- PA13
// LCD_D7 PA5 -|11      22|- PA12
//        PA6 -|12      21|- PA11
//        PA7 -|13      20|- PA10 (Reserved for RXD)
//        PB0 -|14      19|- PA9  (Reserved for TXD)
//        PB1 -|15      18|- PA8
//        VSS -|16      17|- VDD
//              ----------

// mode: 0 - 00 no pull, 1 - 01 pull up, 2 - 10 pull down
void Set_Pin_Input(int pin, int pmode){
	uint32_t upper_bits, bottom_bits; 
	upper_bits = 1UL << pin * 2 + 1; 
	bottom_bits = 1UL << pin * 2; 

	GPIOA->MODER &= ~(bottom_bits | upper_bits);
	
	if (pmode == 0){
		GPIOA->PUPDR &= ~(bottom_bits);
		GPIOA->PUPDR &= ~(upper_bits);
	}
	else if (pmode == 1){
		GPIOA->PUPDR |= (bottom_bits);
		GPIOA->PUPDR &= ~(upper_bits);
	}
	else if (pmode == 2){
		GPIOA->PUPDR |= (bottom_bits);
		GPIOA->PUPDR |= (upper_bits);
	}
}
// 0 - Bank A, 1 - Bank B
void Set_Pin_Output(int bank, int pin, int pmode){
	uint32_t upper_bits, bottom_bits; 
	uint16_t type_bits; 
	type_bits = 1UL << pin; 
	upper_bits = 1UL << (pin * 2 + 1); 
	bottom_bits = 1UL << (pin * 2); 
	
	if (bank == 0){
		GPIOA->MODER = (GPIOA->MODER & ~(upper_bits|bottom_bits)) | bottom_bits;
		if (pmode == 0){
			GPIOA->OTYPER &= ~(type_bits);
		}	
	}
	else if (bank == 1){
		GPIOB->MODER = (GPIOB->MODER & ~(upper_bits|bottom_bits)) | bottom_bits;
		if (pmode == 0){
			GPIOB->OTYPER &= ~(type_bits);
		}
	}
}

void Configure_Pins (void)
{
	RCC->IOPENR |= BIT0; // peripheral clock enable for port A
	RCC->IOPENR |= BIT1; // peripheral clock enable for port B
	
	// Make pins PA0 to PA5 outputs (page 200 of RM0451, two bits used to configure: bit0=1, bit1=0)
    Set_Pin_Output(0,0,0);
	Set_Pin_Output(0,1,0);
	Set_Pin_Output(0,2,0);
	Set_Pin_Output(0,3,0);
	Set_Pin_Output(0,4,0);
	Set_Pin_Output(0,5,0);

	GPIOA->OSPEEDR=0xffff0000; // All pins of port A configured for very high speed! Page 201 of RM0451

    GPIOA->MODER = (GPIOA->MODER & ~(BIT27|BIT26)) | BIT26; // Make pin PA13 output (page 200 of RM0451, two bits used to configure: bit0=1, bit1=0))
	GPIOA->ODR |= BIT13; // 'set' pin to 1 is normal operation mode.

	GPIOA->MODER &= ~(BIT16 | BIT17); // Make pin PA8 input
	// Activate pull up for pin PA8:
	GPIOA->PUPDR |= BIT16; 
	GPIOA->PUPDR &= ~(BIT17);

	GPIOB->MODER |= (BIT2|BIT3);  // Select analog mode for PB1 (pin 15 of LQFP32 package)
	GPIOB->MODER |= (BIT0|BIT1);  // Select analog mode for PB0 (pin 14 of LQFP32 package)
}

void SendATCommand (char * s)
{
	char buff[40];
	printf("Command: %s", s);
	GPIOA->ODR &= ~(BIT13); // 'set' pin to 0 is 'AT' mode.
	waitms(10);
	eputs2(s);
	egets2(buff, sizeof(buff)-1);
	GPIOA->ODR |= BIT13; // 'set' pin to 1 is normal operation mode.
	waitms(10);
	printf("Response: %s", buff);
}

void ReceptionOff (void)
{
	GPIOA->ODR &= ~(BIT13); // 'set' pin to 0 is 'AT' mode.
	waitms(10);
	eputs2("AT+DVID0000\r\n"); // Some unused id, so that we get nothing in RXD1.
	waitms(10);
	GPIOA->ODR |= BIT13; // 'set' pin to 1 is normal operation mode.
	while (ReceivedBytes2()>0) egetc2(); // Clear FIFO
}


void main(void)
{  
	char buff[80];
	char lb[17];
    int timeout_cnt=0;
    int cont1=0, cont2=100;
	int nadc;
	float vx;
	float vy;


	Configure_Pins();
	LCD_4BIT();
	initADC();

	initUART2(9600);
	
	waitms(1000); // Give putty some time to start.

	ReceptionOff();
	
	//WARNING: notice that printf() of floating point numbers is not enabled in the makefile!

	SendATCommand("AT+VER\r\n");
	SendATCommand("AT+BAUD\r\n");
	SendATCommand("AT+RFID\r\n");
	SendATCommand("AT+DVID\r\n");
	SendATCommand("AT+RFC\r\n");
	SendATCommand("AT+POWE\r\n");
	SendATCommand("AT+CLSS\r\n");
	
	// We should select an unique device ID.  The device ID can be a hex
	// number from 0x0000 to 0xFFFF.  In this case is set to 0xABBA
	SendATCommand("AT+DVIDABBA\r\n");

   	// Display something in the LCD
	LCDprint("LCD 4-bit test:", 1, 1);
	LCDprint("Hello, World!", 2, 1);

	while(1)
	{
		nadc=readADC(ADC_CHSELR_CHSEL9);
		vx = (nadc*3.3)/0x1000;
		sprintf(lb,"Vx=%.3f", vx);
		LCDprint(lb,1,1);

		nadc=readADC(ADC_CHSELR_CHSEL8);
		vy = (nadc*3.3)/0x1000;
		sprintf(lb,"Vy=%.3f", vy);
		LCDprint(lb,2,1);

		sprintf(buff, "%03d,%03d\n", cont1, cont2); // Construct a test message
		eputc2('!'); // Send a message to the slave. First send the 'attention' character which is '!'
		// Wait a bit so the slave has a chance to get ready
		waitms(20); // This may need adjustment depending on how busy is the slave
		eputs2(buff); // Send the test message
		
		if(++cont1>200) cont1=0; // Increment test counters for next message
		if(++cont2>200) cont2=0;
		
		waitms(20); // This may need adjustment depending on how busy is the slave

		eputc2('@'); // Request a message from the slave
		
		timeout_cnt=0;
		while(1)
		{
			if(ReceivedBytes2()>0) break; // Something has arrived
			if(++timeout_cnt>250) break; // Wait up to 25ms for the repply
			Delay_us(100); // 100us*250=25ms
		}
		
		if(ReceivedBytes2()>0) // Something has arrived from the slave
		{
			egets2(buff, sizeof(buff)-1);
			if(strlen(buff)==6) // Check for valid message size (5 characters + new line '\n')
			{
				printf("Slave says: %s\r", buff);
			}
			else
			{
				printf("*** BAD MESSAGE ***: %s\r", buff);
			}
		}
		else // Timed out waiting for reply
		{
			printf("NO RESPONSE\r\n", buff);
		}
		
		waitms(50);  // Set the information interchange pace: communicate about every 50ms
	}
	
}
