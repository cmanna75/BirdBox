#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "SWseriale.h"
#include "timer1_hal.h"

#define SERVO_MIN_ANGLE 0
#define SERVO_MAX_ANGLE 180
#define SERVO_MIN_PULSE_WIDTH 50u
#define SERVO_MAX_PULSE_WIDTH 250u

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
void setServoAngle(uint8_t);

int main(){
  init();

  while(1){
    //Toggle Switch
    while (PINC & (1 << PINC1)) { // If PC1 is HIGH
      char input = serial_in();
      //ultrasonic
      if(input == 'u'){
        ultrasonic_read();
      }
      //open servo
      else if(input == 'c'){
        servo_set(0,180);
      }
      //close servo
      else if(input == 'o'){
        servo_set(50,180);
      }
      else if(input == 's'){
        send_wifi();
      }
      else if(input == 't'){
        serial_send_string("test\n");
      }
      else if(input == '\n'||input == '\r'){
        //if carriage returns do nothing
      }
      else{
        serial_send_string("invalid command\n");
      }
      _delay_us(10);
    }
    PORTC |= 1 << PC0;      // Turns on RED debug LED
    _delay_ms(400);
    PORTC &= ~(1 << 0);
    _delay_ms(400);
  }
  return 0;
}



void init(){
  //ultrasonic ports
  DDRD &= ~(1<<DDD6); //echo PD6
  PORTD |= (1<<PORTD6); //enable pull up on PD6
  DDRD |= 1 << PD5; //trig is PD5
  PORTD &= ~(1 << PD5);
  PRR &= ~(1<<PRTIM0); //ensures timer0 is enabled
  TCNT0 = 0;
  TCCR0B |= (1 << CS02)|(1<<CS00); //n8 prescae

  //interupt for ultrasonic
  PCICR = (1 << PCIE2); //PCINT16-23
  PCMSK2 = (1<< PCINT22); //PD6 is trigger for interuppt
  
  
  sei();

  //toggle switch ports 
  DDRC &= ~(1 << DDC1); // Set PC1 as an input
  DDRC |= (1<<DDC0); //set PC0 as an output

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

  
  //Servo stuff
  pwm_init();
  servo_set(0,180);

  
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
  PORTD |= (1<< PD5);
  _delay_us(10);
  PORTD &= ~(1 << PD5);
}


//interrupt for Ultrasonic sensor
ISR(PCINT2_vect){
  if (bit_is_set(PIND,PD6)) //if rising edge 
  {
    TCNT0 = 0; //set timer to 0
    PORTD |= (1 << PD0);
  }
  else
  {
    uint8_t pulse_time = TCNT0;
    uint8_t oldSREG = SREG; 
    cli(); //disable interrupt
    char buffer[100];
    itoa(pulse_time, buffer, 10);
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
