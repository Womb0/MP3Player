//Nicholas DiGiovacchino | mp3PlayerMain.c

#include <MKL25Z4.H>
#include <stdlib.h>
#include "p02_0609.h"

#define SPIPCS0_MASK 0x00000010	//4
#define RESET_MASK 0x00000004		//2
#define RAIL3V3_MASK 0x00000002	//1
#define PLAY_MASK 0x00000400		//10
#define NEXT_MASK 0x00000800		//11
#define PREV_MASK 0x00001000		//12

#define FATBASEADDR 0x00002492
#define DATABASEADDR 0x00006000

volatile uint8_t playPressed = 0, nextPressed = 0, prevPressed = 0, playSent = 0, nextSent = 0, prevSent = 0, playing = 0;
uint8_t dataBuffer[512];
uint32_t fatBuffer[128];

typedef struct node {
	uint32_t cluster;
	volatile struct node *next;
	volatile struct node *prev;
}node;	
			
volatile node* create(uint32_t data, volatile node *next, volatile node *prev) {
	volatile node *newNode = malloc(sizeof(node));
		if (newNode == 0)
			while(1) {}
	newNode->cluster = data;
	newNode->next = next;
	newNode->prev = prev;

	return newNode;	
}
volatile node* append(uint32_t data, volatile node *head) {
	volatile node *temp = head;
	while (temp->next != head)
		temp = temp->next;
	
	volatile node *newNode = create(data, head, temp);
	temp->next = newNode;
	head->prev = newNode;
	
	return head;
}

volatile struct node *head, *curr;

void hold(unsigned long loop) {
	for(int i = loop; i!=0; --i)
		__asm("nop");
}

void io_init (void) {
	// enable clocks on port B and C, SPI0 and I2C0
	SIM->SCGC5 |= (SIM_SCGC5_PORTB_MASK | SIM_SCGC5_PORTC_MASK);
	SIM->SCGC4 |= (SIM_SCGC4_I2C0_MASK | SIM_SCGC4_SPI0_MASK);
	
	//initialize port parameters
	PORTC->PCR[4] = 0x00000100; //GPIO out, SPI PCS0
	PORTC->PCR[5] = 0x00000200; //SPI SCK
	PORTC->PCR[6] = 0x00000200; //SPI MOSI
	PORTC->PCR[7] = 0x00000200; //SPI MISO
	PORTC->PCR[8] = 0x00000200; //I2C SCL
	PORTC->PCR[9] = 0x00000200; //I2C SDA
	PORTC->PCR[10] = 0x00000103; //GPIO in, play
	PORTC->PCR[11] = 0x00000103; //GPIO in, next
	PORTC->PCR[12] = 0x00000103; //GPIO in, previous
	
	PORTB->PCR[1] = 0x00000140; //GPIO out, 3.3v Rail
	PORTB->PCR[2] = 0x00000100; //GPIO out, RESET
	
	//clock stabilization
	hold(50);
	
	// initialize PDDR,0 (default) is input,1 output
	PTB->PDDR |= (RESET_MASK | RAIL3V3_MASK);
	PTC->PDDR |= SPIPCS0_MASK;
	
	PTC->PCOR |= SPIPCS0_MASK; //clear active low SPIPCS0 (for init)
	PTB->PSOR |= RAIL3V3_MASK; //enable rail power (sta013,12c,sd)
	PTB->PCOR |= RESET_MASK; //clear active low sta013 into reset
	
	//initialize i2c
	I2C0_F = 0x2A; //sets transfer speed to ~50kbps
	I2C0_C1 = 0x80; //enable I2C0
}
uint8_t sd_cmd0() {
	volatile uint8_t junk;
	PTC->PCOR |= SPIPCS0_MASK; //cs active low
	
	while((SPI0_S & SPI_S_SPTEF_MASK)==0) {__asm("nop");}
	SPI0_D = 0x40; // cmd index
	
  //dummy read after every write	
	while((SPI0_S & SPI_S_SPRF_MASK)==0) {__asm("nop");}
	junk = SPI0_D;
	
	//arg
	for( int i=4; i!=0; i--) {
		while((SPI0_S & SPI_S_SPTEF_MASK)==0) {__asm("nop");}
		SPI0_D = 0x00;
	
		while((SPI0_S & SPI_S_SPRF_MASK)==0) {__asm("nop");}
		junk = SPI0_D;
	}
	
	//crc
	while((SPI0_S & SPI_S_SPTEF_MASK)==0) {__asm("nop");}
	SPI0_D = 0x95;
	
	//dummy read after every write	
	while((SPI0_S & SPI_S_SPRF_MASK)==0) {__asm("nop");}
	junk = SPI0_D;
	
	while(1){
		//read response
		if((SPI0_S & SPI_S_SPRF_MASK)!=0) {
			PTC->PSOR |= SPIPCS0_MASK; //cs inactive high			
			return SPI0_D;
		} 
		else { //else idle by sending 0xFF repeatedly
			while((SPI0_S & SPI_S_SPTEF_MASK)==0) {__asm("nop");}
			SPI0_D = 0xFF;
		}
	}
}

uint16_t sd_cmd8() {
	uint8_t response[4] = {0x01};
	volatile uint8_t junk;
	
	PTC->PCOR |= SPIPCS0_MASK; //cs active low

	while((SPI0_S & SPI_S_SPTEF_MASK)==0) {__asm("nop");}
	SPI0_D = 0x48; // cmd index
	
	//dummy read after every write	
	while((SPI0_S & SPI_S_SPRF_MASK)==0) {__asm("nop");}
	junk = SPI0_D;
	
	//arg
	while((SPI0_S & SPI_S_SPTEF_MASK)==0) {__asm("nop");}
	SPI0_D = 0x00;	
	while((SPI0_S & SPI_S_SPRF_MASK)==0) {__asm("nop");}
	junk = SPI0_D;
	
	while((SPI0_S & SPI_S_SPTEF_MASK)==0) {__asm("nop");}
	SPI0_D = 0x00;	
	while((SPI0_S & SPI_S_SPRF_MASK)==0) {__asm("nop");}
	junk = SPI0_D;
	
	while((SPI0_S & SPI_S_SPTEF_MASK)==0) {__asm("nop");}
	SPI0_D = 0x01;	
	while((SPI0_S & SPI_S_SPRF_MASK)==0) {__asm("nop");}
	junk = SPI0_D;
		
	while((SPI0_S & SPI_S_SPTEF_MASK)==0) {__asm("nop");}
	SPI0_D = 0xAA;	
	while((SPI0_S & SPI_S_SPRF_MASK)==0) {__asm("nop");}
	junk = SPI0_D;
	
	while((SPI0_S & SPI_S_SPTEF_MASK)==0) {__asm("nop");}
	SPI0_D = 0x87; // crc
	while((SPI0_S & SPI_S_SPRF_MASK)==0) {__asm("nop");}
	junk = SPI0_D;
	
	while(1){
		//read response
		if((SPI0_S & SPI_S_SPRF_MASK)!=0) {			
			response[0] = SPI0_D;
			if (response[0] == 0x00){ //valid r1 response, read ocr
				for (int i=0; i<4; i++) {
					while((SPI0_S & SPI_S_SPRF_MASK)==0){}
					response[i] = SPI0_D;
				}
			}
			PTC->PSOR |= SPIPCS0_MASK; //cs inactive high
			uint16_t value = response[2];
			value = value << 8;
			value |= response[3];
			return value;
		} 
		else { //else idle by sending 0xFF repeatedly
			while((SPI0_S & SPI_S_SPTEF_MASK)==0) {}
			SPI0_D = 0xFF;
		}
	}
}
uint8_t sd_acmd41() {
	volatile uint8_t response;
	
	PTC->PCOR |= SPIPCS0_MASK; //cs active low
	
	//send cmd55 as part of acmd41 first
	while((SPI0_S & SPI_S_SPTEF_MASK)==0) {__asm("nop");}
	SPI0_D = 0x77; // cmd index
	//dummy read after every write
	while((SPI0_S & SPI_S_SPRF_MASK)==0) {__asm("nop");}
	response = SPI0_D;
	
	//arg
	for( int i=4; i!=0; i--) {
		while((SPI0_S & SPI_S_SPTEF_MASK)==0) {__asm("nop");}
		SPI0_D = 0x00;
		while((SPI0_S & SPI_S_SPRF_MASK)==0) {__asm("nop");}
		response = SPI0_D;
	}
	
	//dummy crc
	while((SPI0_S & SPI_S_SPTEF_MASK)==0) {__asm("nop");}
	SPI0_D = 0x95;
	while((SPI0_S & SPI_S_SPRF_MASK)==0) {__asm("nop");}
	response = SPI0_D;
	
	while(1){
		//read response, infinite loop on fail
		if((SPI0_S & SPI_S_SPRF_MASK)!=0) {
			response = SPI0_D;	
			PTC->PSOR |= SPIPCS0_MASK; //cs inactive high
			break;
		} 
		else { //else idle by sending 0xFF repeatedly
			while((SPI0_S & SPI_S_SPTEF_MASK)==0) {__asm("nop");}
			SPI0_D = 0xFF;
		}
	}
	
	//now send acmd41
	PTC->PCOR |= SPIPCS0_MASK; //cs active low
	
	while((SPI0_S & SPI_S_SPTEF_MASK)==0) {__asm("nop");}
	SPI0_D = 0x69; //cmd index
	while((SPI0_S & SPI_S_SPRF_MASK)==0) {__asm("nop");}
	response = SPI0_D;
	
	//arg
	while((SPI0_S & SPI_S_SPTEF_MASK)==0) {__asm("nop");}
	SPI0_D = 0x40;
	while((SPI0_S & SPI_S_SPRF_MASK)==0) {__asm("nop");}
	response = SPI0_D;

	for(int i=3; i!=0; --i) {
		while((SPI0_S & SPI_S_SPTEF_MASK)==0) {__asm("nop");}
		SPI0_D = 0x00;
		while((SPI0_S & SPI_S_SPRF_MASK)==0) {__asm("nop");}
		response = SPI0_D;
	}
	while((SPI0_S & SPI_S_SPTEF_MASK)==0) {__asm("nop");}
	SPI0_D = 0x95; //dummy crc
	while((SPI0_S & SPI_S_SPRF_MASK)==0) {__asm("nop");}
	response = SPI0_D;
	
	while(1){
		//read response
		if((SPI0_S & SPI_S_SPRF_MASK)!=0) {
			PTC->PSOR |= SPIPCS0_MASK; //cs inactive high
			return SPI0_D;
		} 
		else { //else idle by sending 0xFF repeatedly
			while((SPI0_S & SPI_S_SPTEF_MASK)==0) {__asm("nop");}
			SPI0_D = 0xFF;
		}
	}
}
void sd_cmd17(uint32_t address) {
	//read sector at address (physical sector)
	volatile uint8_t response;
	uint8_t addr0, addr1, addr2, addr3;
	
	addr0 = (address & 0xFF000000) >> 24;
	addr1 = (address & 0x00FF0000) >> 16;
	addr2 = (address & 0x0000FF00) >> 8;
	addr3 = (address & 0x000000FF);
	
	PTC->PCOR |= SPIPCS0_MASK; //cs active low
	while((SPI0_S & SPI_S_SPTEF_MASK)==0) {}
	SPI0_D = 0x51; //cmd index
	//dummy read after every write
	while((SPI0_S & SPI_S_SPRF_MASK)==0) {__asm("nop");}
	response = SPI0_D;
	
	//arg
	while((SPI0_S & SPI_S_SPTEF_MASK)==0) {}
	SPI0_D = addr0;
	while((SPI0_S & SPI_S_SPRF_MASK)==0) {__asm("nop");}
	response = SPI0_D;
	
	while((SPI0_S & SPI_S_SPTEF_MASK)==0) {}
	SPI0_D = addr1;
	while((SPI0_S & SPI_S_SPRF_MASK)==0) {__asm("nop");}
	response = SPI0_D;
		
	while((SPI0_S & SPI_S_SPTEF_MASK)==0) {}
	SPI0_D = addr2;
	while((SPI0_S & SPI_S_SPRF_MASK)==0) {__asm("nop");}
	response = SPI0_D;
		
	while((SPI0_S & SPI_S_SPTEF_MASK)==0) {}
	SPI0_D = addr3;
	while((SPI0_S & SPI_S_SPRF_MASK)==0) {__asm("nop");}
	response = SPI0_D;
			
	//dummy crc
	while((SPI0_S & SPI_S_SPTEF_MASK)==0) {}
	SPI0_D = 0x95;
	while((SPI0_S & SPI_S_SPRF_MASK)==0) {__asm("nop");}
	response = SPI0_D;
	
	while(1){
		//read response
		if((SPI0_S & SPI_S_SPRF_MASK)!=0) {
			response = SPI0_D;
			if (response == 0x00) {
				while(1){ //got valid cmd response, wait for data
					//read next response
					if((SPI0_S & SPI_S_SPRF_MASK)!=0) {
						response = SPI0_D;
						if (response == 0xFE) { //ready to read data (512 bytes)
							for (int i=0; i<512;i++) {
								while((SPI0_S & SPI_S_SPRF_MASK)==0){}
								dataBuffer[i] = SPI0_D;
							}
							while((SPI0_S & SPI_S_SPRF_MASK)==0){}
							response = SPI0_D;//dummy crc read
								
							while((SPI0_S & SPI_S_SPRF_MASK)==0){}
							response = SPI0_D;//dummy crc read
							
							PTC->PSOR |= SPIPCS0_MASK; //cs inactive high
							return;
						}
					} 
					else { //else idle by sending 0xFF repeatedly
						while((SPI0_S & SPI_S_SPTEF_MASK)==0) {}
						SPI0_D = 0xFF;
					}
				}
			}
		} 
		else { //else idle by sending 0xFF repeatedly
			while((SPI0_S & SPI_S_SPTEF_MASK)==0) {}
			SPI0_D = 0xFF;
		}
	}
}
void spi_sd_init(void) {
	//spi0 init
	SPI0_C1 = 0x50; //enable, mode0, master
	SPI0_C2 = 0x00; //0x24 for dma
	SPI0_BR = 0x72; //transfer on ~330KHz
	
	//sta013 spi data init 
	//
	
	//start sd init
	//wait 1.5ms
	hold(40000);
	
	//send dummy clocks, 0xFF for 74+ cycles, cs high
	PTC->PSOR |= SPIPCS0_MASK; //cs high
	for(int i=10; i!=0; i--) {
		while((SPI0_S & SPI_S_SPTEF_MASK)==0) {}
			SPI0_D = 0xFF;
	}
	
	//reset
	uint8_t response8;
	do {response8 = sd_cmd0();} while (response8 != 0x01);
	
	uint16_t response16;
	do {response16 = sd_cmd8();} while (response16 != 0x01AA);
	
	//init
	do {response8 = sd_acmd41();} while (response8 != 0x00);
	
	//
	SPI0_BR = 0x00; //transfer on ~10MHz
}
uint8_t i2c_read_ident(void) {
	uint8_t data;
	
	//i2c start 
	I2C0_C1 |= I2C_C1_TX_MASK;
	I2C0_C1 |= I2C_C1_MST_MASK;
	
	//i2c send slave address
	I2C0_D = 0x86;
	//i2c wait for complete
	while((I2C0_S & I2C_S_IICIF_MASK)==0) {}
	I2C0_S |= I2C_S_IICIF_MASK;
	
	//i2c send register address
	I2C0_D = 0x01;
	while((I2C0_S & I2C_S_IICIF_MASK)==0) {}
	I2C0_S |= I2C_S_IICIF_MASK;
		
	//repeat start
	I2C0_C1 |= I2C_C1_RSTA_MASK;
	
	//i2c send slave address with read
	I2C0_D = 0x87;
	while((I2C0_S & I2C_S_IICIF_MASK)==0) {}
	I2C0_S |= I2C_S_IICIF_MASK;
	
	//put in rx mode
	I2C0_C1 &= (~I2C_C1_TX_MASK);
	
	//turn off ACK per standard
	I2C0_C1 |= I2C_C1_TXAK_MASK;
	
	//dummy read (why?)
	data = I2C0_D;
	while((I2C0_S & I2C_S_IICIF_MASK)==0) {}
	I2C0_S |= I2C_S_IICIF_MASK;
	
	//i2c stop
	I2C0_C1 &= ~(I2C_C1_MST_MASK);
	I2C0_C1 &= ~(I2C_C1_TX_MASK);
	
	hold(10);
	//read
	data = I2C0_D;
	
	return data;
}

void i2c_write(uint8_t address, uint8_t data) {
	//i2c start 
	I2C0_C1 |= I2C_C1_TX_MASK;
	I2C0_C1 |= I2C_C1_MST_MASK;
	
	//i2c send slave address
	I2C0_D = 0x86;
	//i2c wait for complete
	while((I2C0_S & I2C_S_IICIF_MASK)==0) {}
	I2C0_S |= I2C_S_IICIF_MASK;
	
	//i2c send register address
	I2C0_D = address;
	while((I2C0_S & I2C_S_IICIF_MASK)==0) {}
	I2C0_S |= I2C_S_IICIF_MASK;
	
	//i2c send data
	I2C0_D = data;
	while((I2C0_S & I2C_S_IICIF_MASK)==0) {}
	I2C0_S |= I2C_S_IICIF_MASK;
	
	//i2c stop
	I2C0_C1 &= ~(I2C_C1_MST_MASK);
	I2C0_C1 &= ~(I2C_C1_TX_MASK);
	
	hold(10);
}

void sta_config(void) {
	PTB->PSOR |= RESET_MASK;
	hold(50);
	
	//confirm communications
	uint8_t data = i2c_read_ident();
	if (data != 0xac) {
		//do some error shit
		while(1) {}
	}
	
	//send bin
	int j;
	for(j=0; j<(COMMANDSIZE-1); j+=2)
		i2c_write(commands[j],commands[j+1]);
	
	if (j!=COMMANDSIZE) {
		//command pair mismatch, odd number of values
		//error stuff
		while(1);
	}
}
void SysTick_Handler(void) {
	// sw debouncer
	// check play status
	if(PTC->PDIR & PLAY_MASK) {
		//is currently pressed
		if(playPressed && (playSent == 0)) {
			//pressed and bounced, send command
			if(playing) {
				i2c_write(0x13,0x00);
				playing = 0;
			}
			else {
				i2c_write(0x13,0x01);
				playing = 1;
			}
			playSent = 1;
		}
		playPressed = 1;
	}
	else {
		playPressed = 0;
		playSent = 0;
	}
	
	// check next status
	if(PTC->PDIR & NEXT_MASK) {
		//is currently pressed
		if(nextPressed && (nextSent == 0)) {
			//pressed and bounced, next stuff
			nextSent = 1;
			curr = curr->next;
		}
		nextPressed = 1;
	}
	else {
		nextPressed = 0;
		nextSent = 0;
	}
	
	// check prev status
	if(PTC->PDIR & PREV_MASK) {
		//is currently pressed
		if(prevPressed && (prevSent == 0)) {
			//pressed and bounced, prev stuff
			prevSent = 1;
			curr = curr->prev;
		}
		prevPressed = 1;
	}
	else {
		prevPressed = 0;
		prevSent = 0;
	}
}

void createTrackList() {
		
	uint32_t fat = 2;
	uint8_t temp[4];
	uint32_t address = 0;
	 
	head = create(0,head,head);
	
	//read directory, at sector 24576 0x6000
	sd_cmd17(DATABASEADDR + 64*(fat-2));
	
	//create track list for navigation
	int j = 0;
	for (int i = 11;; i+=32) {
		if (i >= 512) {
			i = 11;
			j++;
			
			if (j<63) {
				sd_cmd17(DATABASEADDR + (64*(fat-2)) + j);
			}
			else {
				//set up on next cluster,read FAT, check directory's FAT entry FAT[2], sector 9362 0x2492
				sd_cmd17(FATBASEADDR + fat/128);
				
				temp[0] = dataBuffer[((fat%128)*4)+3];
				temp[1] = dataBuffer[((fat%128)*4)+2];
				temp[2] = dataBuffer[((fat%128)*4)+1];
				temp[3] = dataBuffer[(fat%128)*4];
				
				address = (temp[0] << 24) | (temp[1] << 16) | (temp[2] << 8) | (temp[3]);
				address = address & 0x0FFFFFFF;
				fat = address;
				
				if (address != 0x0FFFFFF8 || address != 0x0FFFFFFF){ //not last address
					sd_cmd17(DATABASEADDR+(64*(fat-2)));
				}
				else {
					break;
				}
				j = 0;
			}
		}
		if (dataBuffer[i] != 0x0F) {
			if (dataBuffer[i] != 0x00) {
				//track found, save its first cluster address
				
				temp[0] = dataBuffer[i+10];
				temp[1] = dataBuffer[i+9];
				temp[2] = dataBuffer[i+16];
				temp[3] = dataBuffer[i+15];
				
				address = (temp[0] << 24) | (temp[1] << 16) | (temp[2] << 8) | (temp[3]);
				address = address & 0x0FFFFFFF;
				
				if (head->cluster == 0) {
					head->cluster = address;
				}
				else {
					head = append(address, head);
				}
			}
			break;
		}
	}
	curr = head;
}
int main (void) {
	hold(50);
	// set clock and bus to 20971520 Hz
	SystemCoreClockUpdate(); 
	
	io_init();
	spi_sd_init(); // assumes sd card init parameters
	//file stuff
	sta_config();
	SysTick_Config(314573); //15ms
	//ready to do stuff

	createTrackList(); // uses precalculated sd card parameters, init on head and curr
	
	//control loop, act based on datareq, on next/prev check at end of data buffer
	while(1) {
	}
}
