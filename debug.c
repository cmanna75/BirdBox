#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#define FOSC 9830400            // Clock frequency = Oscillator freq.
#define BAUD 9600               // UART0 baud rate
#define MYUBRR (FOSC/16/BAUD)-1   // Value for UBRR0 register


uint8_t rdata[32];
uint8_t tdata[32];
void port_init();
void serial_init();
void serial_out(char);
void serial_send_string(char*);
char serial_in();
void ultrasonic_read();

static volatile int pulse = 0;
static volatile int i = 0;


int main(){
  port_init();
  serial_init();
  

  while(1){
    char input = serial_in();
    //ultrasonic
    if(input == 'u'){
      ultrasonic_read();
    }
    else if(input == '\n'||input == '\r'){
      //if carriage returns do nothing
    }
    else{
      serial_send_string("invalid command\n");
    }
    _delay_us(10);
  }
  return 0;
}



void port_init(){
  //ultrasonic ports
  DDRD = 0xFF;
  DDRB = 0xFF;
  DDRB &= ~(1<<DDB1); //echo
  PORTB |= (1<<PORTB1); //enable pull up on PB5
  DDRB |= 1 << PB0; //trig

  PRR &= ~(1<<PRTIM1);
  TCNT1 = 0;
  TCCR1B |= (1 << CS10);
  TCCR1B |= (1 << ICES1);

  PCICR = (1 << PCIE0);
  PCMSK0 = (1 << PCINT1);
  sei();

  
  /*
  PRR &= ~(1<<PRTIM1);					// To activate timer1 module
	TCNT1 = 0;								// Initial timer value
	TCCR1B |= (1<<CS10);					// Timer without prescaller. Since default clock for atmega328p is 1Mhz period is 1uS
	TCCR1B |= (1<<ICES1);					// First capture on rising edge

	PCICR = (1<<PCIE1);						// Enable PCINT[14:8] we use pin C5 which is PCINT13
	PCMSK1 = (1<<PCINT13);					// Enable C5 interrupt
	sei();									// Enable Global Interrupts
  */
  
  //enable interrupts
    // EIMSK |= (1<<INT0);
    // EICRA |= (1<<ISC00);
    // sei();
  
}

void serial_init() {

  UBRR0H = (MYUBRR>>8);
  UBRR0L = (MYUBRR);
  UCSR0B |= (1 << TXEN0 ); // Turn on transmitter
  UCSR0B |= (1 << RXEN0 ); // Turn on receiver
  UCSR0C = (3 << UCSZ00 ); // Set for async . operation , no parity ,
                          // one stop bit , 8 data bits
}


void serial_out ( char ch )
{
  while (( UCSR0A & (1 << UDRE0 )) == 0);
  UDR0 = ch;
}

void serial_send_string (char* str)
{
  int i;
  for(i = 0; i < strlen(str);i++){
    serial_out(str[i]);
  }
}

char serial_in ()
{
  while ( !( UCSR0A & (1 << RXC0 )) );
  return UDR0;
}


void ultrasonic_read(){
    _delay_us(60);
  PORTB |= (1<< PB0);
  _delay_us(10);
  PORTB &= ~(1 << PB0);
  /*
  //wait for echo pulse
  while(!(PINB & (1 <<PB1))) {}
;
  //measure pulse duration
  uint16_t pulse_duration = 0;
  while(PINB & (1 <<PB1)) {
    pulse_duration++;
    _delay_us(2);
  }
  //caclulate distance
  //float distance_cm = pulse_duration/58.0;
  int distance_mm = pulse_duration * 1000 / 58;
  char distance_str[10];
  snprintf(distance_str, sizeof(distance_str), "%d\n", distance_mm);
  serial_send_string(distance_str);
  */
}


ISR(PCINT0_vect){
  if (bit_is_set(PINB,PB1))
  {
    TCNT1 = 0;
    PORTD |= (1 << PD0);
  }
  else
  {
    uint16_t pulse_time = TCNT1;
    uint16_t oldSREG = SREG;
    cli();
    char buffer[100];
    itoa(pulse_time/58, buffer, 10);
    serial_send_string(buffer);
    serial_out('\n');
    SREG = oldSREG;
  }
}

/*
ISR(INT0_vect)
{
  if(i == 0)
  {
    TCCR1B |= 1<<CS10;
    i = 1;
  }
  else
  {
    TCCR1B = 0;
    pulse = TCNT1;
    TCNT1 = 0;
    i = 0;
  }
}
*/
/*
ISR(PCINT1_vect) {
	if (bit_is_set(PINB,PB1)) {					// Checks if echo is high
		TCNT1 = 0;								// Reset Timer
		//PORTC |= (1<<PC3);
	} else {
		uint16_t pulse_time = TCNT1;					// Save Timer value
    int distance_mm = pulse_time * 1000 /58;
    char distance_str[10];
    snprintf(distance_str, sizeof(distance_str), "%d\n", distance_mm);
    serial_send_string(distance_str);
    /*
		uint8_t oldSREG = SREG;
		cli();									// Disable Global interrupts
		ClearDisplay();							// Clear Display and send cursor home
		Write16BitToDisplayAsDec(numuS/58);		// Write Timer Value / 58 (cm). Distance in cm = (uS of echo high) / 58
		WriteDisplay(1,0,0x20);					// Space
		WriteDisplay(1,0,0x63);					// c
		WriteDisplay(1,0,0x6d);					// m

		SREG = oldSREG;							// Enable interrupts
		PORTC &= ~(1<<PC3);						// Toggle debugging LED
    */
	//}
//}
