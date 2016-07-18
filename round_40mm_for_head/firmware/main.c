#include <avr/io.h>
#include <avr/delay.h>
#include <avr/interrupt.h>
#include <avr/sleep.h> 


#define BLUE_OFF 			(PORTA |=  (1<<PA4))
#define BLUE_ON 			(PORTA &= ~(1<<PA4))
#define GREEN_OFF 			(PORTA |=  (1<<PA3))
#define GREEN_ON 			(PORTA &= ~(1<<PA3))
#define RED_OFF  			(PORTA |=  (1<<PA2))
#define RED_ON  			(PORTA &= ~(1<<PA2))

#define LEFT_PRESSED 		(!(PINB & (1 << PB1)))
#define RIGHT_PRESSED 		(!(PINB & (1 << PB2)))

#define DRIVER_ON  			(PORTB |=  (1<<PB0))
#define DRIVER_OFF 			(PORTB &= ~(1<<PB0))

// #define LED_ON  			(PORTA |=  (1<<PA7))
// #define LED_OFF 			(PORTA &= ~(1<<PA7))


volatile uint8_t need_to_sleep, current_mode, power_mode;
volatile uint32_t seconds, d_seconds;
volatile uint16_t val=1024;

// ISR(ADC_vect){
// 	uint8_t theLowADC = ADCL;
// 	val = ADCH<<8 | theLowADC;
// 	ADCSRA |= 1<<ADSC;	//start conversion
// // TODO - round-robin between voltage and temp
// }

ISR(PCINT1_vect){
	read_button();
}

ISR(TIM1_OVF_vect){
	d_seconds+=1;
	if(d_seconds==9){
		seconds+=1;
		d_seconds=0;
		PORTA ^= (1<<PA4);
	}
	TCNT1 = 53035; //~=10Hz
}


int apply_power(void){
	if(power_mode==255){power_mode=0;}
	if(power_mode>4){power_mode=4;}
	
	if(power_mode==0){ 
		DRIVER_OFF; 
		OCR0B=0;
		TIMSK1 = 0;
		TCCR1B = 0;
	}
	else{
		DRIVER_ON; 
		set_sleep_mode(SLEEP_MODE_IDLE); 
		sleep_disable();
		TCCR1A = 0;
		TCNT1 = 53035; //~=10Hz
		TCCR1B = (1<<CS11); 
		TIMSK1 = (1<<TOIE1); //timer1 interrupt enabled
	}
	// if(power_mode==1){DRIVER_ON;  OCR0B=16;}
	// if(power_mode==2){DRIVER_ON;  OCR0B=64;}
	// if(power_mode==3){DRIVER_ON;  OCR0B=127;}
	// if(power_mode==4){DRIVER_ON;  OCR0B=255;}
	if(power_mode==1){OCR0B=1;}
	if(power_mode==2){OCR0B=2;}
	if(power_mode==3){OCR0B=3;}
	if(power_mode==4){OCR0B=4;}
	return 0;
}

void read_button(void){
	// if(power_mode>0){
	// 	// if(val<755){
	// 	if(val<100){
	// 		current_mode = 1- current_mode;
	// 		if(current_mode){
	// 			RED_ON;
	// 		}else{
	// 			RED_OFF;
	// 		}	
	// 		// TODO - перевесить светодиоды на PWM и плавно моргать
	// 		power_mode = 1;
	// 		_delay_ms(1200);
	// 		GREEN_OFF;
	// 		BLUE_OFF;
	// 	}else if(val<793){
	// 		RED_ON;
	// 		GREEN_OFF;
	// 		BLUE_OFF;
	// 	}else if(val<872){
	// 		RED_OFF;
	// 		GREEN_OFF;
	// 		BLUE_ON;
	// 	}else{
	// 		RED_OFF;
	// 		BLUE_OFF;
	// 		GREEN_ON;	
	// 	}
	// }else{
	// 	RED_OFF;
	// 	BLUE_OFF;
	// 	GREEN_OFF;
	// 	//TODO - вырубать драйвер
	// }	
	if(LEFT_PRESSED){
		RED_OFF;GREEN_OFF;BLUE_ON;
		power_mode-=1;
		apply_power();
		while(LEFT_PRESSED){};
		BLUE_OFF;
	}
	if(RIGHT_PRESSED){
		RED_OFF;GREEN_OFF;BLUE_ON;
		power_mode+=1;
		apply_power();
		while(RIGHT_PRESSED){};
		BLUE_OFF;
	}
}

int main(void){
	// PA0 - ADC IN
	// PA2 - RED
	// PA3 - GREEN
	// PA4 - BLUE
	// PA7(OC0B) - PWM output.
	// PB0 - out (EN)
	// PB1 - hall1, PB2 - hall2

	DDRA  = 0b11111110;
	// PORTA = 0xFF;
	DDRB  = 0b0001;
	PORTB  = 0b0111;

	BLUE_OFF;
	GREEN_OFF;
	RED_OFF;

	// DRIVER_ON;
	// LED_OFF;
	need_to_sleep = 1;
	power_mode=0;
	BLUE_OFF;
	RED_OFF;
	GREEN_OFF;
	DRIVER_OFF;
// LED_OFF;
// _delay_ms(1500);
// LED_ON;
// _delay_ms(1500);
RED_ON;
_delay_ms(500);
RED_OFF;
	TCCR0A = (1 << COM0B1) | (1 << WGM00);// | (1 << WGM01);  		// PWM mode
	TCCR0B = (1 << CS01);							// clock source = CLK/8, start PWM

	PCMSK1 = (1<<PCINT9) | (1<<PCINT10);

	// TCNT1H = 0xCF;
	// TCNT1L = 0x2B; //~=10Hz

	// GIFR = (1<<PCIF1); 
	GIMSK = (1<<PCIE1);//interrupt on pin change0
	// OCR0B = 0x00;
// LED_ON;

	// TCCR1A = (1 << COM0B1) | (1 << WGM00);  		// PWM mode
	// TCCR1B = (1 << CS01);							// clock source = CLK/8, start PWM
	// OCR1A = 0xFFFF;
	// OCR1B = 0xFFFF;

	// ADMUX = 0b10100010; // int reference and temp sensor
	ADMUX = 0b10000000; // ADC0 - int

	// ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0) | (1<<ADIE);	// enable ADC on 1mhz/16 with interrupt
	// ADCSRA |= (1<<ADSC);							//start conversion
	// //временно вырубил АЦП и забил значение жестко


	uint8_t initial = PINB;
	sei();

	while(1){
		// ADC8 = 330 - ~70 degrees Celcium
		// 9V =  714
		// 10V = 793
		// 11V = 872
		// 12V = 951
		// 12.6V = 999
		if(power_mode==0){
			set_sleep_mode(SLEEP_MODE_PWR_DOWN);
			sleep_enable();
			sleep_bod_disable();
			sleep_cpu();
			sleep_disable();
		}	
		
		// while(LEFT_PRESSED || RIGHT_PRESSED){}//UGLY debounce :)
		


 	// 	if(val>999){
		// 	GREEN_ON;
		// }else{
		// 	GREEN_OFF;	
		// }
		// BLUE_ON;
		// _delay_ms(5);
		// BLUE_OFF;
		// _delay_ms(250);

		// sei();
		// if(need_to_sleep){
			// sleep_mode();
		// }
		// if(LEFT_PRESSED){GREEN_OFF;BLUE_ON;}
		// if(RIGHT_PRESSED){BLUE_OFF;GREEN_ON;}
		

		// if(LEFT_PRESSED){
		// 	OCR0B=16;
		// 	BLUE_ON;
		// }else{
		// 	BLUE_OFF;
		// 	GREEN_OFF;
		// }
		// if(RIGHT_PRESSED){
		// 	OCR0B = 0;
		// 	GREEN_ON;
		// }else{
		// 	GREEN_OFF;
		// 	BLUE_OFF;
		// }
		


		// if(RIGHT_PRESSED){RED_ON;}
	}	
}