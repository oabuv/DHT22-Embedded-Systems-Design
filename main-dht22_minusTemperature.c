/*3.2 solved
DHT22 connected to PA6 (D12)
*/

/* Includes */
#include <stddef.h>
#include "stm32l1xx.h"
#include "nucleo152start.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
/* Private typedef */
/* Private define  */
/* Private macro */
/* Private variables */
/* Private function prototypes */
/* Private functions */
void USART2_Init(void);
void USART2_write(char data);
void delay_Ms(int delay);
void read_dht22_humidity_and_temperature(int *hum, int *temp, int *checks);
int check(int x, int y);
void delay_us(unsigned long delay);
void delay_Us(int delay);

/**
**===========================================================================
**
**  Abstract: main program
**
**===========================================================================
*/
int main(void)
{
  /* Configure the system clock to 32 MHz and update SystemCoreClock */
  SetSysClock();
  SystemCoreClockUpdate();
  USART2_Init();
  /* TODO - Add your application code here */
  RCC->AHBENR|=1; //GPIOA ABH bus clock ON. p154
  GPIOA->MODER|=0x400; //GPIOA pin 5 to output. p184
  /* Infinite loop */
  int hum=0;
  int temp=0;
  int checks = 0;
  int temp_degree=0;
  int temp_decimals=0;
  int hum_int=0;
  int hum_decimals=0;

  char buf2[30]="";
  char buf[100] = "";
  int checkcal;
  while (1)
  {
	//GPIOA->ODR|=0x20; //0010 0000 set bit 5. p186
	GPIOA->ODR|=0x20; //0010 0000 set bit 5. p186
	delay_Ms(1500);
	GPIOA->ODR&=~0x20; //0000 0000 clear bit 5. p186
	delay_Ms(1500);
	read_dht22_humidity_and_temperature(&hum,&temp,&checks);

	checkcal = check(hum, temp);	// to compare the calculated checksum and the transfered checksum from dht22

	/* Print out the temperature and humidity */
	if((temp>>15))	temp_degree = (temp / 10)*-1;
	else temp_degree = temp / 10;
	temp_decimals = abs((int)temp % 10);
	hum_decimals = abs((int)hum % 10);
	hum_int = hum / 10;

		
	sprintf(buf,"checks: %d, checkcalulated: %d\n\r temperature: %d.%d Celsius\n\r humidity: %d.%d%%\n\r checksum: %d\n\r",checks, checkcal,temp_degree, temp_decimals,hum_int, hum_decimals,checks);
	for(int i=0;i<strlen(buf);i++){
		USART2_write(buf[i]);
	}

	if(checkcal == checks){
		sprintf(buf2,"The sersor is working well\n\r\n\r\n\r");
		for(int i=0;i<strlen(buf2);i++){
			USART2_write(buf2[i]);
		}
	}

  }
  return 0;
}

void USART2_Init(void)
{
	RCC->APB1ENR|=0x00020000; 	//set bit 17 (USART2 EN)
	RCC->AHBENR|=0x00000001; 	//enable GPIOA port clock bit 0 (GPIOA EN)
	GPIOA->AFR[0]=0x00000700;	//GPIOx_AFRL p.188,AF7 p.177
	GPIOA->AFR[0]|=0x00007000;	//GPIOx_AFRL p.188,AF7 p.177
	GPIOA->MODER|=0x00000020; 	//MODER2=PA2(TX) to mode 10=alternate function mode. p184
	GPIOA->MODER|=0x00000080; 	//MODER2=PA3(RX) to mode 10=alternate function mode. p184

	USART2->BRR = 0x00000D05;	//9600 BAUD and crystal 32MHz. p710, D05
	USART2->CR1 = 0x00000008;	//TE bit. p739-740. Enable transmit
	USART2->CR1 |= 0x00000004;	//RE bit. p739-740. Enable receiver
	USART2->CR1 |= 0x00002000;	//UE bit. p739-740. Uart enable
}

void USART2_write(char data)
{
	//wait while TX buffer is empty
	while(!(USART2->SR&0x0080)){} 	//TXE: Transmit data register empty. p736-737
		USART2->DR=(data);		//p739
}

void delay_Ms(int delay)
{
	int i=0;
	for(; delay>0;delay--)
		for(i=0;i<2460;i++); //measured with oscilloscope
}

void delay_Us(int delay)
{
	for(int i=0;i<(delay*2);i++) //accurate range 10us-100us
	{
		asm("mov r0,r0");
	}
}

void delay_us(unsigned long delay)
{
	unsigned long i=0;
	RCC->APB2ENR|=(1<<4); 	//TIM11EN: Timer 11 clock enable. p160
	TIM11->PSC=1; 			//32 000 000 MHz / 32 = 1 000 000 Hz. p435
	TIM11->ARR=1;	 		//TIM11 counter. 1 000 000 Hz / 1000 = 1000 Hz ~ 1ms. p435
	TIM11->CNT=0;			//counter start value = 0
	TIM11->CR1=1; 			//TIM11 Counter enabled. p421

	  while(i<delay)
	  {
		  while(!((TIM11->SR)&1)){} //Update interrupt flag. p427
		  i++;
		  TIM11->SR &= ~1; 	//flag cleared. p427
		  TIM11->CNT=0;	  	//counter start value = 0
	  }
	  TIM11->CR1=0; 		//TIM11 Counter disabled. p421
}

void read_dht22_humidity_and_temperature(int *hum, int *temp, int *checks)
{
	unsigned int humidity=0, i=0,temperature=0, checksum = 0;
	unsigned long long mask=0x8000000000;		// mask will be shifted from left to right 40 times

	//RCC->AHBENR|=1; //GPIOA ABH bus clock ON. p154
	GPIOA->MODER|=0x1000; //GPIOA pin 6 to output. p184
	GPIOA->ODR|=0x40; //0100 0000 set bit 6. p186
	delay_Ms(10);
	GPIOA->ODR&=~0x40; //low-state at least 500 us (host sends start signal)
	delay_Ms(1);
	GPIOA->ODR|=0x40; //pin 6 high state and sensor gives this 20us-40us
	GPIOA->MODER&=~0x3000; //GPIOA pin 6 to input. p184

	//response from sensor
	while((GPIOA->IDR & 0x40)){}
	while(!(GPIOA->IDR & 0x40)){}
	while((GPIOA->IDR & 0x40)){}

	//read values from sensor
	while(i<40)
	{
		while(!(GPIOA->IDR & 0x40)){}	// run in while() until the high bit appears

		delay_Us(35);
		/* 	The bits sent from the left to the right that means the humidity is the first 16 bits and checksum is the last
		/	8 bits transfered from dht22 to output register of stm32nucleo
			And the first 8-high bits are transfered first then 8-low bits came next in the 16 bits data resolution
		 */
		if((GPIOA->IDR & 0x40)&& i<16)
		{
			humidity=humidity|(mask>>24);	// the high bit is right shifted by 24 positions, because 16 bits of Humidity are sent first by dht22
		}
		if((GPIOA->IDR & 0x40)&& i>=16 && i<32)
		{
			temperature=temperature|(mask>>8);	// the next 16 bits are shifted by 8 positions to get the value of temperature
		}
		if((GPIOA->IDR & 0x40)&& i>=32)
		{
			checksum=checksum|mask;			// checksum is the last 8 bits of sum. 
											// Sum = Humidity(first 8 bits + last 8 bits) + Temperature(first 8 bits + last 8 bits)
		}
		mask=(mask>>1);						// mask is right shifted 1 position each loops
		i++;

		while((GPIOA->IDR & 0x40)){}		// wait for the low data-bus to start transmitting next 1 bit data
	}
	// cast the local variables to integer to match with the passed arguments
	*hum=(int)humidity;
	*temp=(int)temperature;
	*checks=(int)checksum;
}

int check(int x, int y)
{
    int xlow1 = x & 0xff;
    int xhigh1 = (x >> 8);
    int ylow2 = y & 0xff;
    int yhigh2 = (y >> 8);
    int sum=xlow1+xhigh1+ylow2+yhigh2;
    int checksum=(sum&0xff);
    return checksum;
}

