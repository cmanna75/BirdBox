#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "SWseriale.h"



#define FOSC 9830400            // Clock frequency = Oscillator freq.
#define BAUD 9600               // UART0 baud rate
//#define BAUD 115200
#define MYUBRR (FOSC/16/BAUD)-1   // Value for UBRR0 register


uint8_t rdata[32];
uint8_t tdata[32];
void init();
void serial_out(char);
void serial_send_string(char*);
char serial_in();
void ultrasonic_read();
void send_wifi();

int main(){
  init();

  while(1){
    char input = serial_in();
    //ultrasonic
    if(input == 'u'){
      ultrasonic_read();
    }
    else if(input == 's'){
      send_wifi();
    }
    else if(input == 't'){
      serial_send_string('test');
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



void init(){
  //ultrasonic ports
  DDRD = 0xFF;
  DDRB = 0xFF;
  DDRB &= ~(1<<DDB1); //echo
  PORTB |= (1<<PORTB1); //enable pull up on PB1
  DDRB |= 1 << PB0; //trig


  //interupt and timer stuff
  PRR &= ~(1<<PRTIM1); //ensures timer1 enabled
  TCNT1 = 0; 
  TCCR1B |= (1 << CS10); //no clock prescaling
  TCCR1B |= (1 << ICES1); //capture positive edge

  PCICR = (1 << PCIE0); //pin change interupt 0 enabled PCINT7-0
  PCMSK0 = (1 << PCINT1); //PB1 is trigger for interrupt
  sei();

  //serial stuff
  UBRR0H = (MYUBRR>>8);
  UBRR0L = (MYUBRR);
  UCSR0B |= (1 << TXEN0 ); // Turn on transmitter
  UCSR0B |= (1 << RXEN0 ); // Turn on receiver
  UCSR0C = (3 << UCSZ00 ); // Set for async . operation , no parity ,
                          // one stop bit , 8 data bits
  //softserial
  SWseriale_begin();

  //wifi stuff


  
}

//sends char
void serial_out ( char ch )
{
  while (( UCSR0A & (1 << UDRE0 )) == 0);
  UDR0 = ch;
}

//sends char string
void serial_send_string (char* str)
{
  int i;
  for(i = 0; i < strlen(str);i++){
    serial_out(str[i]);
  }
}

//reads in 1 char
char serial_in ()
{
  while ( !( UCSR0A & (1 << RXC0 )) );
  return UDR0;
}


void ultrasonic_read(){
  //send trig pulse for 10 us
    _delay_us(60);
  PORTB |= (1<< PB0);
  _delay_us(10);
  PORTB &= ~(1 << PB0);
}

//interrupt for Ultrasonic sensor
ISR(PCINT0_vect){
  if (bit_is_set(PINB,PB1)) //if rising edge 
  {
    TCNT1 = 0; //set timer to 0
    PORTD |= (1 << PD0);
  }
  else
  {
    uint16_t pulse_time = TCNT1;
    uint16_t oldSREG = SREG; 
    cli(); //disable interrupt
    char buffer[100];
    itoa(pulse_time/58, buffer, 10);
    serial_send_string(buffer); //send ultrasonic distance
    serial_out('\n');
    SREG = oldSREG; //enable interrupt
  }
}


void send_wifi(){
  SWseriale_write("AT",2);
  while (SWseriale_available()){ // Checks if any character has been received
	    uint8_t temp = SWseriale_read(); // Reads one character from SWseriale received data buffer
      char buffer = (char)temp;
	    serial_out(buffer); // Send one character using SWseriale
    }
}
