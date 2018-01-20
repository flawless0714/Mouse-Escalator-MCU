#include <c8051f330.h>
#include <stdio.h>

#define SYSCLK 24500000
#define BAUDRATE 9600
#define TIMER_PRESCALER 12
#define TIMER_TICKS_PER_MS SYSCLK/TIMER_PRESCALER/75000
#define AUX1 TIMER_TICKS_PER_MS
#define AUX2 -AUX1
#define TIMER3_RELOAD -SYSCLK/TIMER_PRESCALER/103
#define TIMER2_RELOAD AUX2

sfr16 TMR2RL = 0xCA;
sfr16 TMR2 = 0xCC;
sfr16 TMR3RL = 0x92;
sfr16 TMR3 = 0x94;

sbit PULSE_38K = P2^0;

unsigned short speed;
unsigned short speedcount = 0;

void SYSCLK_Init (void);
void IDA0_Init (void);
void UART0_Init (void);
void PORT_Init (void);
void Timer3_Init (void);
void Timer2_Init (void) ;
void Ext_Interrupt_Init (void);

void main (void) 
{
	unsigned char state = 0;
	PCA0MD &= ~0x40;                // WDTE = 0 (Disable Watchdog timer)
	PORT_Init();
	SYSCLK_Init();
	IDA0_Init();
	UART0_Init();
	Ext_Interrupt_Init();
	Timer2_Init();
	Timer3_Init();
	EA = 1;
	while(1)
	{
		unsigned char read = _getkey();
		if(state == 0)
		{
			if(read == 0xDD) 
			{
				state = 1;
			}
		}
		else if(state == 1)
		{
			if(read == 0xDD) state = 2;
			else state = 0;
		}
		else if(state == 2)
		{
			IDA0H = read;
			state = 3;
		}
		else if(state == 3)
		{
			IDA0L = read;
			state = 0;
		}
	}
}

void UART0_Init (void)     
{
	SCON0 = 0x10;
	TH1 = -(SYSCLK / BAUDRATE / 2 / 12);
	CKCON &= ~0x0B;
	TL1 = TH1;
	TMOD &= ~0xF0;
	TMOD |=  0x20;
	TR1 = 1;
	TI0 = 1;
}

void PORT_Init (void)
{
	P0SKIP = 0x02;
	XBR0 = 0x01;
	XBR1 = 0x42;
	P0MDOUT |= 0xC0;
    P1MDOUT = 0xFF;
	P2MDOUT |= 0x01;
}

void SYSCLK_Init (void)
{
	OSCICN = 0x83;                  // Configure internal oscillator for its maximum frequency
	RSTSRC  = 0x04;                 // Enable missing clock detector
}

void IDA0_Init (void)   
{
   REF0CN = 0x03;                      // Enable internal VREF generator
   IDA0CN = 0xF2;                      // Enable IDAC0 for 2.0 mA full-scale
}

//-----------------------------------------------------------------------------
// Ext_Interrupt_Init
//-----------------------------------------------------------------------------
// Pin 0.6 
// Use this to count motor speed
//-----------------------------------------------------------------------------
void Ext_Interrupt_Init (void)
{
	TCON |= 0x05;                   //INT 0 and INT 1 are edge triggered
	IT01CF = 0x06;                  //INT0 active low, INT0 on P0.6;
	EX0 = 1;                        //Enable INT0 interrupts
} 	  

void INT0_ISR (void) interrupt 0
{
	speedcount++;
}

//-----------------------------------------------------------------------------
// Timer2_Init
//-----------------------------------------------------------------------------
// Interrupt per 13.2us
//-----------------------------------------------------------------------------
void Timer2_Init(void)
{
	CKCON &= ~0x60;                 // Timer2 uses SYSCLK/12
	TMR2CN &= ~0x01;
	TMR2RL = TIMER2_RELOAD;         // Reload value to be used in Timer2
	TMR2 = TMR2RL;                  // Init the Timer2 register
	TMR2CN = 0x04;                  // Enable Timer2 in auto-reload mode
	ET2 = 1;                        // Timer2 interrupt enabled
}

void Timer2_ISR (void) interrupt 5
{
	PULSE_38K = ~PULSE_38K;
	TF2H = 0;
}

//-----------------------------------------------------------------------------
// Timer3_Init
//-----------------------------------------------------------------------------
// Interrupt per 1ms
//-----------------------------------------------------------------------------
void Timer3_Init(void)
{
   TMR3RL = TIMER3_RELOAD;          // Reload value to be used in Timer3
   TMR3 = TMR3RL;                   // Init the Timer3 register

   TMR3CN = 0x04;                   // Enable Timer3 in auto-reload mode
   EIE1 |= (1 << 7);                // Timer3 interrupt enabled
}

void Timer3_ISR (void) interrupt 14
{
	static unsigned int count1 = 0, count2 = 0;
	count1++;
	if(count1 == 10)
	{
		count1 = 0;
		count2++;
		if(count2 == 10)
		{
			count2 = 0;
			speed = speedcount;
			speedcount = 0;
		}
		putchar(0xFF); // packet start byte 0
		putchar(0xFF); // packet start byte 1
		putchar((speed >> 8) & 0xFF); //speed value high byte
		putchar(speed & 0xFF); //speed value low byte
		putchar((((~P0^7) << 1) & 0x02) | ((~P0^6) & 0x01));
		putchar(~P1);
	}
	TMR3CN &= ~(1 << 7);
}	