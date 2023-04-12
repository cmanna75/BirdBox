#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "timer1_hal.h"

#define SERVO_MIN_ANGLE 0
#define SERVO_MAX_ANGLE 180
#define SERVO_MIN_PULSE_WIDTH 50u
#define SERVO_MAX_PULSE_WIDTH 250u
#define ULTRASONIC_DISTANCE_THRESHOLD 7

#define FOSC 9830400            // Clock frequency = Oscillator freq.
#define BAUD 9600               // UART0 baud rate
#define MYUBRR (FOSC/16/BAUD)-1   // Value for UBRR0 register

void init();
void serial_out(char);
void serial_send_string(char*);
char serial_in();
void check_ultrasonic();
int check_servo();

int main(){
  while(1){
    if(PINC & (1 << PINC1)) { //if lid switch is closed, operate normally
      if(check_servo()){ //returns false if night time
        check_ultrasonic();
      }
    }
    _delay_ms(10);
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
  //wifi stuff
  DDRB &= ~(1<<DDB0); //make PB0 an input for open/close servo
  DDRD |= (1<<DDD7);
 
  //Servo stuff
  pwm_init();
  servo_set(0,180);

  //phototransistor
  DDRB &= ~(1<<DDB2);

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


void check_ultrasonic(){
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
    //if ultrasonic is below distance output the camera
    if(pulse_time <= ULTRASONIC_DISTANCE_THRESHOLD){
      PORTD |= (1 << PD7);
    }
    SREG = oldSREG; //enable interrupt
  }
}

int check_servo(){
  if( (PINB & (1<<PINB2))){ //if sunlight
        /*
        if(PINB & (1<< PINB0)){
        servo_set(50,180);
        }
        else{
          servo_set(0,180);
        }  
        */
        servo_set(50,80);
        return 1;
  }
  else{ //else night time close door
        servo_set(0,180);
        return 0;
      }
  }
