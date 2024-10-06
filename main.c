//------------------------------------------- main.c CODE STARTS ---------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include "NUC100Series.h"
#include "MCU_init.h"
#include "SYS_init.h"
#include "LCD.h"

#define BOUNCING_DLY 200000
volatile uint8_t key = 0;
int index = 1;
volatile countTMR3 = 0;
volatile int pattern[] = {
	//   gedbaf_dot_c
	0b10000010,  //Number 0          // ---a----
	0b11101110,  //Number 1          // |      |
	0b00000111,  //Number 2          // f      b
	0b01000110,  //Number 3          // |      |
	0b01101010,  //Number 4          // ---g----
	0b01010010,  //Number 5          // |      |
	0b00010010,  //Number 6          // e      c
	0b11100110,  //Number 7          // |      |
	0b00000010,  //Number 8          // ---d----
	0b01000010,   //Number 9
	0b11111111   //Blank LED
};
volatile int game_started = 0;
volatile int game_over = 0;
volatile int changing_x = 1;
volatile int changing_y = 0;
volatile int x = 0; // 0-based
volatile int y = 0; // 0-based. TODO: Check if this is still needed.
volatile int num_shot = 0;
volatile int x_showing = 1;
volatile int y_showing = 1;
volatile int isShow7Segment = 0;
int N = 8;
volatile int ships_sunk = 0;
volatile int map[8][8]; 
volatile int iLM = 0;
volatile int jLM = 0; 
int isLoadedMap = 0;
volatile int hit_map[8][8] = {
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0}
};

void System_Config(void);
void UART0_Config(void);
void UART0_SendChar(int ch);
char UART0_GetChar();
void SPI3_Config(void);

void LCD_start(void);
void LCD_command(unsigned char temp);
void LCD_data(unsigned char temp);
void LCD_clear(void);
void LCD_SetAddress(uint8_t PageAddr, uint8_t ColumnAddr);
void KeyPadEnable(void);
uint8_t KeyPadScanning(void);


//Choose 7 segment
void turn_on_digit(int digit){
	switch(digit){
		case 1: // First 7-segment
			PC->DOUT |= (1<<7);     //Logic 1 to turn on the digit
			PC->DOUT &= ~(1<<6);		//SC3
			PC->DOUT &= ~(1<<5);		//SC2
			PC->DOUT &= ~(1<<4);		//SC1
			break;
		case 2: // Second 7-segment
			PC->DOUT |= (1<<6);     //Logic 1 to turn on the digit
			PC->DOUT &= ~(1<<7);		//SC3
			PC->DOUT &= ~(1<<5);		//SC2
			PC->DOUT &= ~(1<<4);
			break;
		case 3: // Third 7-segment
			PC->DOUT |= (1<<5);     //Logic 1 to turn on the digit
			PC->DOUT &= ~(1<<6);		//SC3
			PC->DOUT &= ~(1<<7);		//SC2
			PC->DOUT &= ~(1<<4);
			break;
		case 4: // Last 7-segment
			PC->DOUT |= (1<<4);     //Logic 1 to turn on the digit
			PC->DOUT &= ~(1<<6);		//SC3
			PC->DOUT &= ~(1<<7);		//SC2
			PC->DOUT &= ~(1<<5);
			break;
		case 5: // Last 7-segment
			PC->DOUT &= ~(1<<4);     //Logic 1 to turn on the digit
			PC->DOUT &= ~(1<<6);		//SC3
			PC->DOUT &= ~(1<<7);		//SC2
			PC->DOUT &= ~(1<<5);
			break;
	}
}
void resetArray(int arr[8][8]){
	for(int i = 0;i<8;i++){
		for(int j = 0; j< 8;j++){
			arr[i][j] = 0;
		}			
	}
}
/*
====================== SYSTEM REQUIREMENTS ====================
a.	After reset, the LCD shows a welcome screen. You can flexibly to choose the texts and images to show on this screen. 
b.	Then, if you send a map file from your laptop/PC to the Arduino board. You need to write an embedded program to
capture and store this map temporarily. You only need to do this one time before the game starts. Once the sent
complete, the screen should display ?Map Loaded Successfully? right in the middle of the screen.
c.	If you press SW_INT (GP15) the game starts. We will use External Interrupt in this system.
d.	Select coordinate X (ranging from 1 to 8) by using press the corresponding Key 1 to Key 8 from the Key matrix.
This current coordinate X also displays on the 7segment U11.
e.	Press Key 9 will allow us to change coordinate X
f.	Repeat step d to select coordinate Y. This current coordinate Y also displays on the 7segment U11. If you
press the Key 9 button again, it will change back to coordinate X selection.
g.
?	Once XY is defined, we can press the SW_INT (GP15) to shoot.
?	Your program then compares the entered coordinates vs the map earlier and then indicates if there is a Hit or Miss.
?	If there is a Hit, we will flash the LED5 (GPC12) three times. You can choose the suitable flashing frequency here.
    If there is a hit, the LCD updates to indicate the hit. For example, X display the hit.

h.	The system also displays in real-time the number of shots fired via U13 and U14:
i.	If the player successfully sinks all five ships on a map (Note that a ship is sunk if two hits are at two adjacent
dots) or has fired more than 16 shots but could not sink all ships, the game will be over. Couple indications for this:
?	The LCD will display Game over message.
?	Buzzer will go off 5 times one time only.
?	All the keys will stop responding.
j.	If you press the SW_INT (GP15) now, the game will be replay from the welcome screen

====================== OTHER REQUIREMENTS ====================
The system must include the following game features:
?	Check if a ship is shrunk when two adjacent dots are hit.
?	Indicate if the same position is hit twice. For this, the number of shots will go up by one still.
?	Count maximum shots allowed for a player; if he runs out of 16 shots, then the game is over.
In terms of technical requirements, your system must use:
?	Program is developed using registered based method. All configurations must be clearly explained (via code comments).
Only standard drivers for LCD display (i.e., functions in LCD.c and Draw2D.c libraries) can be used in project code.
?	Design with FSM
?	UART Interrupt to receive text file sent from PC. You can decide the baud rate and UART format.
?	Timer Interrupt for scanning rate. You can use this to define the delay time between the display of each digit.
?	External Interrupt for SW_INT (GP15) button
?	Use C functions to optimize the code.
?	Put meaningful and clean comments to explain your design
?	The entire system is built properly on a breadboard and ready to be checked.
*/

// a) After reset, the LCD shows a welcome screen. You can flexibly to choose the texts and images to show on this screen.
void show_welcome_screen() {
	// TODO: LOL
			printS_5x7(0,20,"Welcome");
		//CLK_SysTickDelay(100000);
}

// b.	Then, if you send a map file from your laptop/PC to the Arduino board. You need to write an embedded program to
// capture and store this map temporarily. You only need to do this one time before the game starts. Once the sent
// complete, the screen should display ?Map Loaded Successfully? right in the middle of the screen.

void printMap(){
		for(int i = 1;i<=8;i++){
				for(int j = 1; j<= 8 ;j++){
					char c ;
					if(hit_map[i-1][j-1] == 0) c = '-';
					else	c = 'x';
					printC_5x7(10*j,i*7,c);
				}
		}
}



void check_game_over() {
	// Count the number of sunk ships after the shot.
	int num_sunks = 0;
	for (int i = 0; i < N; i++) {
		for (int j = 0; j < N; j++) {
			if (hit_map[i][j] == 1 && map[i][j] == 1) {
				num_sunks += 1;
			}
		}
	}

	// i.	If the player successfully sinks all five ships on a map (Note that a ship is sunk if two hits are at two adjacent
	// dots) or has fired more than 16 shots but could not sink all ships, the game will be over. Couple indications for this:
	// ?	The LCD will display Game over message.
	// ?	Buzzer will go off 5 times one time only.
	// ?	All the keys will stop responding.
	if (num_sunks >= 10 && num_shot <= 16) {
		game_over = 1;

		// TODO: Game Over display stuff
    LCD_clear();
    printS_5x7(10,20,"You WIN :)");
		for(int i = 0;i<10;i++){
			PB->DOUT ^= (1<<11);
			CLK_SysTickDelay(2000000);
		}
	}
	if(num_sunks < 10 && num_shot > 16){
		game_over = 1;
		LCD_clear();
    	printS_5x7(10,20,"You LOSE :(");
		for(int i = 0;i<10;i++){
			PB->DOUT ^= (1<<11);
			CLK_SysTickDelay(4000000);
		}
	}
}

void check_toggle() {
	if (changing_x == 1) {
		y_showing = 1;
		x_showing = abs(1- x_showing);
	} else {
		x_showing = 1 ;
		y_showing = (1-y_showing);
	}
}

void display7segment(){
	int dozens = 0;
	int units = 0;
	if(index == 1){
		turn_on_digit(1);
		if (x_showing == 1) {
			PE->DOUT = pattern[x];	
		} else {
			PE->DOUT = pattern[10];
		}
		//PE->DOUT = pattern[0];
		index = 2;
	}
	else if(index == 2){
		turn_on_digit(2);
		if (y_showing == 1) {
		PE->DOUT = pattern[y];	
		} else {
			PE->DOUT = pattern[10];
		}
		//PE->DOUT = pattern[0];
		index = 3;
	}
	else if(index == 3){
			dozens = num_shot/10;
			turn_on_digit(3);
			PE->DOUT = pattern[dozens];
			index = 4;
	}
	else if(index == 4){
			units = num_shot%10;
			turn_on_digit(4);
			PE->DOUT = pattern[units];
			index = 1;
	}
	//TIMER3->TISR |= (1 << 0);
//}
}

void shoot(int x, int y) {
	
	// Update shot count.
	num_shot += 1;
	// h.	The system also displays in real-time the number of shots fired via U13 and U14:
	// TODO: Update LED U13, U14
	int hit = 0;
	//for (int i = 0; i < N; i++) {
		//for (int j = 0; j < N; j++) {
	if (map[y][x] == 1) {
		// if(x == 2 && y == 1) PC->DOUT ^= (1<<13);
		hit = 1;
	}

	//      If there is a hit, the LCD updates to indicate the hit. For example, X display the hit.
	if (hit == 1) {
		if(hit_map[y][x] == 0){
			hit_map[y][x] = 1;
			//If there is a Hit, we will flash the LED5 (GPC12) three times. You can choose the suitable flashing frequency here.
			for(int i = 0;i<6;i++){
				PC->DOUT ^= (1<<12);
				CLK_SysTickDelay(200000);
			}
			//Update LCD
			LCD_clear();
			printMap();
		}
	}

	check_game_over();
}

void sw_int_15_pressed() {
	// j.	If you press the SW_INT (GP15) now, the game will be replay from the welcome screen
	if (game_over == 1) {
		game_over = 0;
		game_started = 0;
		num_shot = 0;
		x = 0;
		y = 0;
		resetArray(hit_map);
		LCD_clear();
		show_welcome_screen();
		turn_on_digit(5);
		return;
		// TODO: Restart-related stuff.
	}

	if (game_started == 1) {
		// g) Once XY is defined, we can press the SW_INT (GP15) to shoot
		if (x > 0 && y > 0) {
			
			shoot(x-1, y-1);
		}
		//PC->DOUT &= ~(1<<13);
		//display7segment();
	} else {
		if(isLoadedMap == 1){
			//PC->DOUT &= ~(1<<13);
			// c) If you press SW_INT (GP15) the game starts. We will use External Interrupt in this system.
			game_started = 1;
			x = 0;
			y = 0;
			changing_x = 1;
			changing_y = 0;
			resetArray(hit_map);
			LCD_clear();
			printMap();
		}
		else{
			LCD_clear();
			printS_5x7(0,0,"Please load the map first");
		}
	}
}

// d.	Select coordinate X (ranging from 1 to 8) by using press the corresponding Key 1 to Key 8 from the Key matrix.
// This current coordinate X also displays on the 7segment U11.
void change_x(int number) {
	if (changing_x == 0) {
		return;
	}

	x = number;
}
void change_y(int number) {
	if (changing_y == 0) {
		return;
	}

	y = number;

}
// e.	Press Key 9 will allow us to change coordinate X
void key_9_pressed() {
	changing_x = abs(1-changing_x);
	changing_y = abs(1-changing_y);
}

void key_pressed() {
	// i. If game over:
	// ?	All the keys will stop responding.
	if (game_over == 1) {

		return;
	}

	key = KeyPadScanning();
	
	// e.	Press Key 9 will allow us to change coordinate X
	if (key == 9) {
		key_9_pressed();
		CLK_SysTickDelay(200000);
		return;
	}
	if (key > 0){
		if (changing_x == 1) {
			// d.	Select coordinate X (ranging from 1 to 8) by using press the corresponding Key 1 to Key 8 from the Key matrix.
			// This current coordinate X also displays on the 7segment U11.
			change_x(key);
		} else {
			// f.	Repeat step d to select coordinate Y. This current coordinate Y also displays on the 7segment U11. If you
			// press the Key 9 button again, it will change back to coordinate X selection.
			change_y(key);
		}
		CLK_SysTickDelay(200000);
	}
}


// f.	Repeat step d to select coordinate Y. This current coordinate Y also displays on the 7segment U11. If you
// press the Key 9 button again, it will change back to coordinate X selection.

int main(void)
{
	
    //GPIO_SetMode(PC,BIT12,GPIO_MODE_OUTPUT);
	//--------------------------------
	//System initialization
	//--------------------------------
	System_Config();
	SPI3_Config();
	UART0_Config();	
	GPIO_SetMode(PC,BIT13,GPIO_MODE_OUTPUT);
	//--------------------------------
	//SPI3 initialization
	//--------------------------------
	//--------------------------------
	//LCD initialization
	//--------------------------------
	LCD_start();
	LCD_clear();

	//--------------------------------
	//LCD static content
	//--------------------------------


	//--------------------------------
	//LCD dynamic content
	//--------------------------------
	show_welcome_screen();
	while (1) {
		//key_pressed();
		// PC->DOUT |= (1<<13);
		//  switch(game_started){
		//  	case 1:
		//  		PC->DOUT |= (1<<13);
		//  		//if (TIMER0->TISR & (1 << 0)){
		//  		//	check_toggle();
		//  			//TIMER0->TISR |= (1 << 0);
		// 		//}
		//  		//key_pressed();
		//  		break;
		//  	case 0:
		//  		turn_on_digit(5);
		//  		//loadMap();
		//  		//PC->DOUT &= ~(1<<15);
		//  		break;
		//  }
	}
}

// Interrupt Service Rountine of GPIO port B pin 15
void EINT1_IRQHandler(void) {
	sw_int_15_pressed();
	CLK_SysTickDelay(250000);
	PB->ISRC |= (1 << 15);
}

void UART02_IRQHandler(){
	//PC->DOUT &= ~(1<<13);
	//PC->DOUT &= ~(1<<12);
	if(game_started == 0){
		//PC->DOUT ^= (1<<13);
			char ReceivedByte = UART0->DATA;
			if (ReceivedByte != ' ' && ReceivedByte != '\n' && ReceivedByte != '\r') 	{
					map[iLM][jLM] = ReceivedByte - '0';
					jLM++;
			}
			else if(ReceivedByte == '\n'){
				iLM++;
				jLM = 0;
			}
			if(iLM == 7 && jLM ==7){
				if(map[6][0] == 1){
					//PC->DOUT &= ~(1<<12);
				}
				isLoadedMap = 1;
				LCD_clear(); 	
				printS_5x7(0,0,"Map Loaded Successfully");
				UART0->IER &= ~(1<<0);
				}
		}
		UART0->ISR |= (1<<0);
}

void TMR3_IRQHandler(){
	if(game_started == 1){
		display7segment();
		countTMR3++;
		if(countTMR3 == 120){
			check_toggle(); 	
			countTMR3 = 0;
		}
		key_pressed();
	}
	TIMER3->TISR |= (1<<0);
}
//------------------------------------------------------------------------------------------------------------------------------------
// Functions definition
//------------------------------------------------------------------------------------------------------------------------------------
void System_Config(void) {
	SYS_UnlockReg(); // Unlock protected registers
	CLK->PWRCON |= (0x01 << 0);
	while (!(CLK->CLKSTATUS & (1 << 0)));

	//PLL configuration starts
	CLK->PLLCON &= ~(1 << 19); //0: PLL input is HXT
	CLK->PLLCON &= ~(1 << 16); //PLL in normal mode
	CLK->PLLCON &= (~(0x01FF << 0));
	CLK->PLLCON |= 48;
	CLK->PLLCON &= ~(1 << 18); //0: enable PLLOUT
	while (!(CLK->CLKSTATUS & (0x01ul << 2)));
	//PLL configuration ends

	//clock source selection
	CLK->CLKSEL0 &= (~(0x07 << 0));
	CLK->CLKSEL0 |= (0x02 << 0);
	//clock frequency division
	CLK->CLKDIV &= (~0x0F << 0);

	//enable clock of SPI3
	CLK->APBCLK |= 1 << 15;
	
	//UART0 Clock selection and configuration
	CLK->CLKSEL1 |= (0b11 << 24); // UART0 clock source is 22.1184 MHz
	CLK->CLKDIV &= ~(0xF << 8); // clock divider is 1
	CLK->APBCLK |= (1 << 16); // enable UART0 clock

	//Timer 3  and 0 initialization start--------------
	//TM3 Clock selection and configuration
	CLK->CLKSEL1 &= ~(0b111 << 20);
	CLK->APBCLK |= (1 << 5);
	
	CLK->CLKSEL1 &= ~(0b111 << 8);
	CLK->APBCLK |= (1 << 2);

	//Pre-scale = 11
	TIMER3->TCSR &= ~(0xFF << 0);
	TIMER3->TCSR |= 11<< 0;
	//Pre-scale = 11
	// TIMER0->TCSR &= ~(0xFF << 0);
	// TIMER0->TCSR |= 11<< 0;
	//reset Timer 3 and 0 
	TIMER3->TCSR |= (1 << 26);
	//TIMER0->TCSR |= (1 << 26);
	
	//define Timer 3 operation mode
	TIMER3->TCSR &= ~(0b11 << 27);
	TIMER3->TCSR |= (0b01 << 27);
	TIMER3->TCSR &= ~(1 << 24);
	//define Timer 0 operation mode
	// TIMER0->TCSR &= ~(0b11 << 27);
	// TIMER0->TCSR |= (0b01 << 27);
	// TIMER0->TCSR &= ~(1 << 24);
	
	//TDR to be updated continuously while timer counter is counting
	TIMER3->TCSR |= (1 << 16);
	//TIMER0->TCSR |= (1 << 16);
	
	//Enable TE bit (bit 29) of TCSR
	//The bit will enable the timer interrupt flag TIF
	TIMER3->TCSR |= (1 << 29);
	//TIMER0->TCSR |= (1 << 29);

	//TimeOut = 5ms --> Counter's TCMPR =2-1 
	TIMER3->TCMPR = 4999;
	//TIMER0->TCMPR = 699999;
	
	//start counting
	TIMER3->TCSR |= (1 << 30);
	//TIMER0->TCSR |= (1 << 30);
	
	SYS_LockReg();  // Lock protected registers
	
	//LED display via GPIO-C12 to indicate main program execution
	PC->PMD &= (~(0x03 << 24));
	PC->PMD |= (0x01 << 24);
	PC->PMD &= (~(0x03 << 26));
	PC->PMD |= (0x01 << 26);
	//PC->PMD &= (~(0x03 << 30));
	//PC->PMD |= (0x01 << 30);
	//BUZZER to indicate interrupt handling routine
	PB->PMD &= (~(0x03 << 22));
	PB->PMD |= (0x01 << 22);
	
	//Configure GPIO for 7 segment
	//Set mode for PC4 to PC7 
	PC->PMD &= (~(0xFF<< 8));		//clear PMD[15:8] 
	PC->PMD |= (0b01010101 << 8);   //Set output push-pull for PC4 to PC7
	
	//Set mode for PE0 to PE7
	PE->PMD &= (~(0xFFFF<< 0));		//clear PMD[15:0] 
	PE->PMD |= 0b0101010101010101<<0;   //Set output push-pull for PE0 to PE7	
	
	//GPIO Interrupt configuration. GPIO-B15 is the interrupt source
	PB->PMD &= (~(0x03 << 30));
	PB->IMD &= (~(1 << 15));
	PB->IEN |= (1 << 15);

	//NVIC interrupt configuration for GPIO-B15 interrupt source
	NVIC->ISER[0] |= 1 << 3;
	NVIC->IP[0] &= (~(3<<30));
	NVIC->IP[0] |= (1 << 30);
	
	//NVIC interrupt configuration for UART02 interrupt source
	NVIC->ISER[0] |= 1<<12;
	//Set priority (0->3: highest to lowest)
	NVIC->IP[3] &= (~(3<<6));
	
	//NVIC interrupt configuration for TIMER3 interrupt source
	NVIC->ISER[0] |= (1<<11);
	NVIC->IP[2] &= (~(3<<30));
	//Set priority (0->3: highest to lowest)
	NVIC->IP[2] |= (2 << 30);
}
void UART0_Config(void) {
	// UART0 pin configuration. PB.1 pin is for UART0 TX
	PB->PMD &= ~(0b11 << 2);
	PB->PMD |= (0b01 << 2); // PB.1 is output pin
	SYS->GPB_MFP |= (1 << 1); // GPB_MFP[1] = 1 -> PB.1 is UART0 TX pin
	
	SYS->GPB_MFP |= (1 << 0); // GPB_MFP[0] = 1 -> PB.0 is UART0 RX pin	
	PB->PMD &= ~(0b11 << 0);	// Set Pin Mode for GPB.0(RX - Input)
	
	
	// UART0 operation configuration
	UART0->LCR |= (0b11 << 0); // 8 data bit
	UART0->LCR &= ~(1 << 2); // one stop bit	
	UART0->LCR &= ~(1 << 3); // no parity bit
	UART0->FCR |= (1 << 1); // clear RX FIFO
	UART0->FCR |= (1 << 2); // clear TX FIFO
	UART0->FCR &= ~(0xF << 16); // FIFO Trigger Level is 1 byte]
	
	//Baud rate config: BRD/A = 1, DIV_X_EN=0
	//--> Mode 0, Baud rate = UART_CLK/[16*(A+2)] = 22.1184 MHz/[16*(70+2)]= 19200 bps
	UART0->BAUD &= ~(0b11 << 28); // mode 0
	UART0->BAUD &= ~(0xFFFF << 0);
	UART0->BAUD |= 70;
	UART0->IER |= (1<<0);
}

void UART0_SendChar(int ch){
	while(UART0->FSR & (0x01 << 23)); //wait until TX FIFO is not full
	UART0->DATA = ch;
	if(ch == '\n'){								// \n is new line
		while(UART0->FSR & (0x01 << 23));
		UART0->DATA = '\r';						// '\r' - Carriage return
	}
}
void SPI3_Config(void) {
	SYS->GPD_MFP |= 1 << 11; //1: PD11 is configured for alternative function
	SYS->GPD_MFP |= 1 << 9; //1: PD9 is configured for alternative function
	SYS->GPD_MFP |= 1 << 8; //1: PD8 is configured for alternative function

	SPI3->CNTRL &= ~(1 << 23); //0: disable variable clock feature
	SPI3->CNTRL &= ~(1 << 22); //0: disable two bits transfer mode
	SPI3->CNTRL &= ~(1 << 18); //0: select Master mode
	SPI3->CNTRL &= ~(1 << 17); //0: disable SPI interrupt
	SPI3->CNTRL |= 1 << 11; //1: SPI clock idle high
	SPI3->CNTRL &= ~(1 << 10); //0: MSB is sent first
	SPI3->CNTRL &= ~(3 << 8); //00: one transmit/receive word will be executed in one data transfer

	SPI3->CNTRL &= ~(31 << 3); //Transmit/Receive bit length
	SPI3->CNTRL |= 9 << 3;     //9: 9 bits transmitted/received per data transfer

	SPI3->CNTRL |= (1 << 2);  //1: Transmit at negative edge of SPI CLK
	SPI3->DIVIDER = 0; // SPI clock divider. SPI clock = HCLK / ((DIVIDER+1)*2). HCLK = 50 MHz
}

void LCD_start(void)
{
	// a) Welcome screen

	LCD_command(0xE2); // Set system reset
	LCD_command(0xA1); // Set Frame rate 100 fps
	LCD_command(0xEB); // Set LCD bias ratio E8~EB for 6~9 (min~max)
	LCD_command(0x81); // Set V BIAS potentiometer
	LCD_command(0xA0); // Set V BIAS potentiometer: A0 ()
	LCD_command(0xC0);
	LCD_command(0xAF); // Set Display Enable
}

void LCD_command(unsigned char temp)
{
	SPI3->SSR |= 1 << 0;
	SPI3->TX[0] = temp;
	SPI3->CNTRL |= 1 << 0;
	while (SPI3->CNTRL & (1 << 0));
	SPI3->SSR &= ~(1 << 0);
}

void LCD_data(unsigned char temp)
{
	SPI3->SSR |= 1 << 0;
	SPI3->TX[0] = 0x0100 + temp;
	SPI3->CNTRL |= 1 << 0;
	while (SPI3->CNTRL & (1 << 0));
	SPI3->SSR &= ~(1 << 0);
}

void LCD_clear(void)
{
	int16_t i;
	LCD_SetAddress(0x0, 0x0);
	for (i = 0; i < 132 * 8; i++)
	{
		LCD_data(0x00);
	}
}

void LCD_SetAddress(uint8_t PageAddr, uint8_t ColumnAddr)
{
	LCD_command(0xB0 | PageAddr);
	LCD_command(0x10 | (ColumnAddr >> 4) & 0xF);
	LCD_command(0x00 | (ColumnAddr & 0xF));
}

void KeyPadEnable(void) {
	GPIO_SetMode(PA, BIT0, GPIO_MODE_QUASI);
	GPIO_SetMode(PA, BIT1, GPIO_MODE_QUASI);
	GPIO_SetMode(PA, BIT2, GPIO_MODE_QUASI);
	GPIO_SetMode(PA, BIT3, GPIO_MODE_QUASI);
	GPIO_SetMode(PA, BIT4, GPIO_MODE_QUASI);
	GPIO_SetMode(PA, BIT5, GPIO_MODE_QUASI);
}

uint8_t KeyPadScanning(void) {
	PA0 = 1; PA1 = 1; PA2 = 0; PA3 = 1; PA4 = 1; PA5 = 1;
	if (PA3 == 0) return 1;
	if (PA4 == 0) return 4;
	if (PA5 == 0) return 7;
	PA0 = 1; PA1 = 0; PA2 = 1; PA3 = 1; PA4 = 1; PA5 = 1;
	if (PA3 == 0) return 2;
	if (PA4 == 0) return 5;
	if (PA5 == 0) return 8;
	PA0 = 0; PA1 = 1; PA2 = 1; PA3 = 1; PA4 = 1; PA5 = 1;
	if (PA3 == 0) return 3;
	if (PA4 == 0) return 6;
	if (PA5 == 0) return 9;
	return 0;
}
//------------------------------------------- main.c CODE ENDS ---------------------------------------------------------------------------

                                                                                                                                                                                                