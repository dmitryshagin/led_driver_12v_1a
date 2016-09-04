#include <avr/io.h>
#include <avr/delay.h>
#include <avr/interrupt.h>
#include <avr/sleep.h> 
#include <avr/eeprom.h>

#define MODE_OFF			0
#define MODE_IDLE			1
#define MODE_OVERTEMP		2
#define MODE_LOW_VOLTAGE	3
#define MODE_LOW			4
#define MODE_HALF			5
#define MODE_FULL			6


#define LIMIT_9V  714
#define LIMIT_10V 793
#define LIMIT_11V 872
#define LIMIT_12V 951

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


volatile uint8_t power_mode, prev_power_mode, unlock_stage, d_seconds, adc_channel, vals_pos=0;
volatile uint16_t vals[4];
volatile uint32_t val;
volatile uint32_t seconds;

uint32_t nv_seconds_on[] EEMEM = {0,0,0,0,0,0,0};
uint16_t nv_times_on[]   EEMEM = {0,0,0,0,0,0,0};
uint16_t nv_max_on[]     EEMEM = {0,0,0,0,0,0,0};

uint32_t seconds_on[7];
uint16_t times_on[7];
uint16_t max_on[7];

ISR(ADC_vect){
	uint8_t theLowADC = ADCL;
	val = ADCH<<8 | theLowADC;
	// vals[vals_pos]=val;
	// vals_pos+=1;
	// if(vals_pos>4){
	// 	vals_pos=0;
	// };
	// val = (vals[0]+vals[1]+vals[2]+vals[3])/4;
	ADCSRA |= 1<<ADSC;	//start conversion
}

ISR(PCINT1_vect){
	read_button();
}

ISR(TIM1_OVF_vect){
	d_seconds+=1;
	if(d_seconds==10){
		seconds+=1;
		d_seconds=0;
	}
	TCNT1 = 53035; //~=10Hz
}


void all_off(){
	stop_timer();
	unlock_stage = 0;
	seconds = 0;
	d_seconds = 0;
	RED_OFF;GREEN_OFF;BLUE_OFF;
	power_mode = MODE_OFF;
	apply_power();
	DRIVER_OFF;
	OCR0B=0;
	disable_adc();
}

void start_timer(){
	TCCR1A = 0;
	TCNT1 = 53035; 		 //~=10Hz
	TCCR1B = (1<<CS11); 
	TIMSK1 = (1<<TOIE1); //timer1 interrupt enabled
	enable_adc();
	ADCSRA |= (1<<ADSC); //start ADC conversion
}

void stop_timer(){
	TIMSK1 = 0;
	TCCR1B = 0;
}

int apply_power(void){
	if(unlock_stage>0){
		RED_ON;
	}
	if(unlock_stage==3){ //unlock finished
		power_mode = MODE_IDLE;
		unlock_stage = 0;
	}
	if(power_mode==255){power_mode=MODE_IDLE;}
	if(power_mode>MODE_FULL){power_mode=MODE_FULL;}
	
	if(power_mode==MODE_OFF){ 
		DRIVER_OFF; 
		OCR0B=0;
	}else{
		DRIVER_ON; 
		set_sleep_mode(SLEEP_MODE_IDLE); 
		sleep_disable();
		// stop_timer();
		start_timer();
	}
	// if(power_mode==MODE_IDLE)		{OCR0B=0;  }
	// if(power_mode==MODE_LOW_VOLTAGE){OCR0B=0; }
	// if(power_mode==MODE_OVERTEMP)	{OCR0B=0;  }
	// if(power_mode==MODE_LOW)		{OCR0B=16; }
	// if(power_mode==MODE_HALF)		{OCR0B=0;}
	// if(power_mode==MODE_FULL)		{OCR0B=0;}
	if(power_mode==MODE_IDLE)		{OCR0B=0;  }
	if(power_mode==MODE_LOW_VOLTAGE){OCR0B=16; }
	if(power_mode==MODE_OVERTEMP)	{OCR0B=8;  }
	if(power_mode==MODE_LOW)		{OCR0B=64; }
	if(power_mode==MODE_HALF)		{OCR0B=127;}
	if(power_mode==MODE_FULL)		{OCR0B=255;}

	if(prev_power_mode!=power_mode){
		seconds_on[prev_power_mode] += seconds;
		times_on[prev_power_mode] += 1;
		if(max_on[prev_power_mode] < seconds){
			max_on[prev_power_mode] = seconds;
		}		
		if(power_mode==MODE_LOW_VOLTAGE||power_mode==MODE_OFF){
			//will store data only on power off or low batt to save eeprom
			eeprom_write_block(&seconds_on, &nv_seconds_on, sizeof(seconds_on));
			eeprom_write_block(&times_on,   &nv_times_on,   sizeof(times_on) );
			eeprom_write_block(&max_on,     &nv_max_on,     sizeof(max_on)   );
		}
		prev_power_mode=power_mode;
		seconds=0;
	}
	
	return 0;
}

void read_button(void){
	_delay_ms(250);
	uint32_t lock_timer=0;
	start_timer();
	if(LEFT_PRESSED){
		RED_OFF;GREEN_OFF;BLUE_ON;
		if(power_mode==MODE_OFF){
			if(unlock_stage==0 || unlock_stage==2){
				unlock_stage+=1;
			}
		}else{
			if(power_mode>MODE_LOW){
				power_mode-=1;
			}else{
				power_mode=1;
			}	
		}	
		apply_power();
		while(LEFT_PRESSED){lock_timer+=1;if(lock_timer==100000){all_off();}};
		BLUE_OFF;
		_delay_ms(200);
		return;
	}
	if(RIGHT_PRESSED){
		RED_OFF;GREEN_OFF;BLUE_ON;
		if(power_mode==MODE_OFF){
			if(unlock_stage==1){
				unlock_stage+=1;
			}
		}else{	
			if(power_mode>=MODE_LOW){ // проскакиваем аварийные режимы
				power_mode+=1;
			}else{
				power_mode=MODE_LOW;
			}	
		}	
		apply_power();
		while(RIGHT_PRESSED){};
		BLUE_OFF;
		_delay_ms(200);
	}

}

void enable_adc(){
	ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0) | (1<<ADIE);	// enable ADC on 1mhz/16 with interrupt
}

void disable_adc(){
	ADCSRA = 0;
}

int main(void){
	// PA0 - ADC IN
	// PA2 - RED
	// PA3 - GREEN
	// PA4 - BLUE
	// PA7(OC0B) - PWM output.
	// PB0 - out (EN)
	// PB1 - hall1, PB2 - hall2

	DDRA   = 0b11111110;
	PORTA  = 0b00000000;
	
	DDRB   = 0b0001;
	PORTB  = 0b0110;

	TCCR0A = (1 << COM0B1) | (1 << WGM00);// | (1 << WGM01);  		// PWM mode
	TCCR0B = (1 << CS01);							// clock source = CLK/8, start PWM

	PCMSK1 = (1<<PCINT9) | (1<<PCINT10);
	GIMSK = (1<<PCIE1);//interrupt on pin change0

	ADMUX = 0b10000000; // ADC0 - int
	enable_adc();
	ADCSRA |= (1<<ADSC);							//start conversion


	all_off();

	RED_ON;
	DRIVER_ON;
	OCR0B = 8;
	_delay_ms(500);
	// while(1){};
	all_off();

	eeprom_read_block(&seconds_on, &nv_seconds_on, sizeof(seconds_on));
	eeprom_read_block(&times_on,   &nv_times_on,   sizeof(times_on)  );
	eeprom_read_block(&max_on,     &nv_max_on,     sizeof(max_on)    );
	prev_power_mode = MODE_OFF;
	sei();

	while(1){
		// ADC8 = 330 - ~70 degrees Celcium
		// 9V =  714
		// 10V = 793
		// 11V = 872
		// 12V = 951
		// 12.6V = 999
		if(power_mode==MODE_OFF && unlock_stage==0){
			seconds = 0;
			d_seconds = 0;
			stop_timer();
			disable_adc();
			set_sleep_mode(SLEEP_MODE_PWR_DOWN);
			sleep_enable();
			sleep_bod_disable();
			sleep_cpu();
			sleep_disable();
		}	
		if(seconds>1 && power_mode==MODE_OFF){
			all_off();
		}
		if(unlock_stage==0 && power_mode!=MODE_OFF){
			if(d_seconds==0){
				adc_channel = 0;
				ADMUX = 0b10000000; // ADC0 - int
			}	
			if(d_seconds==5){	
				adc_channel = 1;
				ADMUX = 0b10100010; // int reference and temp sensor
			}

			if(seconds%2==0){
				if(power_mode==MODE_LOW_VOLTAGE){
					RED_ON;GREEN_OFF;BLUE_OFF;
				}
				if(power_mode==MODE_OVERTEMP){
					RED_ON;GREEN_ON;BLUE_OFF;
				}
			}else{
				if(power_mode==MODE_OVERTEMP||power_mode==MODE_LOW_VOLTAGE){
					RED_OFF;GREEN_OFF;BLUE_OFF;
				}
			}


			// if((seconds%5==0) && ADMUX == 0b10000000){ //measure voltage in active mode
			if(adc_channel ==0 && d_seconds == 4){ //measure voltage in active mode
				if(val<LIMIT_9V){
					all_off();//completely discharged. turning off
				}
				if(power_mode!=MODE_OVERTEMP && power_mode!=MODE_LOW_VOLTAGE && power_mode>=MODE_IDLE){ //show voltage always, except overtemp
					if(val<((LIMIT_9V+LIMIT_10V)/2)){
						RED_OFF;
						GREEN_OFF;
						BLUE_OFF;
						if(power_mode>=MODE_LOW){ // усвловие нужно, чтобы не задалбывать функцию - в ней будут логи
							power_mode = MODE_LOW_VOLTAGE;//low voltage
							apply_power();
						}	
					}else if(val<LIMIT_10V){
						RED_ON;
						GREEN_OFF;
						BLUE_OFF;
					}else if(val<LIMIT_11V){
						RED_OFF;
						GREEN_OFF;
						BLUE_ON;
					}else{
						RED_OFF;
						BLUE_OFF;
						GREEN_ON;	
					}
				}	
			}

			if(adc_channel ==1 && d_seconds == 8){ //measure temp in active mode
				if(val>330 && power_mode>=MODE_LOW ){// усвловие нужно, чтобы не задалбывать функцию - в ней будут логи
					power_mode = MODE_OVERTEMP;
					apply_power(); //overtemp. reduce current cunsumption
				}
			}

			if(power_mode==MODE_IDLE && seconds>=7200){ //выключение через два часа из режима простоя
				all_off();
			}

			// if(seconds==0 && d_seconds<5 && power_mode>=MODE_LOW){
			// 	GIMSK = 0;//вырубим прерывания на первые 500мс, чтобы не кнопать кнопкой сильно часто
			// }else{
			// 	GIMSK = (1<<PCIE1);//interrupt on pin change0
			// }
		}	
	}	
}