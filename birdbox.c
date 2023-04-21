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
#define SQUIRREL_WEIGHT_THRESHOLD 300
#define FOOD_WEIGHT_THRESHOLD 300
#define OPEN_DOOR 0
#define CLOSE_DOOR 45 

#define FOSC 9830400            // Clock frequency = Oscillator freq.
#define BAUD 9600               // UART0 baud rate
#define MYUBRR (FOSC/16/BAUD)-1   // Value for UBRR0 register

#define NIGHTMODE 0
#define DAY_DOOR_OPEN 1
#define DAY_DOOR_CLOSED 2


void init();
void serial_out(char);
void serial_send_string(char*);
char serial_in();
void check_ultrasonic();
void check_servo();
void debug();
int debug_flag = 0;
void check_weight();
int state;
int main(){
  init();
  while(1){
    if(PINC & (1 << PINC1)) { //if lid switch is closed, operate normally
      check_servo();
      if(state == DAY_DOOR_OPEN){
          check_ultrasonic();
        }
      if(state != NIGHTMODE){
        check_weight();
      }
    }
    //debug(); //allows serial commands
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
  DDRD |= (1<<DDD7); //make PD7 an output for trigger bird is there
  PORTD &= ~(1<<PD7); //make low at first
 
  //Servo stuff
  pwm_init();
  servo_set(CLOSE_DOOR,180);

  //phototransistor
  DDRB &= ~(1<<DDB2);
  PORTB &= ~(1<<PB2);

  
  //feed tube weight input
  DDRC &= ~(1<<DDC2);
  PORTC &= ~(1<<PC2);

  //feed tube weight output
  DDRC |= (1<<DDC3);
  PORTC &= ~(1<<PC3);

  //squirrel weight input
  DDRC &= ~(1<<DDC4);
  PORTC &= ~(1<<PC4);

  //squirrel weight output
  DDRC |= (1<<DDC5);
  PORTC &= ~(1<<PC5);

  //night mode output
  DDRD |= (1<<DDD2);
  PORTD &= ~(1<<PD2);


  //ADC
  // Enable ADC and set prescaler to 128
  ADCSRA |= (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

  // Set AVCC as the reference voltage
  ADMUX |= (1 << REFS0);

  state = DAY_DOOR_OPEN;

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
  if( (UCSR0A & (1 << RXC0))){
    return UDR0;
  }
  else return '\n';
}


void check_ultrasonic(){
  //send trig pulse for 10 us
    _delay_us(60);
  PORTD |= (1<< PD5);
  _delay_us(10);
  PORTD &= ~(1 << PD5);
}

void debug(){
  char input = serial_in();
  //ultrasonic
  if(input == 'u'){
    debug_flag = 1;
    check_ultrasonic();
  }
  //open servo
  else if(input == 'c'){
    servo_set(CLOSE_DOOR,180);
  }
  //close servo
  else if(input == 'o'){
    servo_set(OPEN_DOOR,180);
  }
  else if(input == 't'){
    serial_send_string("test\n");
  }
  else if(input == '\n'||input == '\r'){
    //if carriage returns do nothing
  }
  else if(input == 'l'){
    PORTC |= 1 << PC0;      // Turns on RED debug LED
    _delay_ms(400);
    PORTC &= ~(1 << 0);
    _delay_ms(400);
    
  }
  else{
    serial_send_string("invalid command\n");
  }
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
    if(debug_flag){
      char buffer[100];
      itoa(pulse_time, buffer, 10);
      serial_send_string(buffer); //send ultrasonic distance
      serial_out('\n');
      debug_flag = 0;
    }
    //if ultrasonic is below distance output the camera
    if(pulse_time <= ULTRASONIC_DISTANCE_THRESHOLD){
      PORTD |= (1 << PD7);
    }
    else{
      PORTD &= ~(1 << PD7);
    }
    SREG = oldSREG; //enable interrupt
  }
}

void check_servo(){
  if( (PINB & (1<<PINB2))){ //if sunlight
    //make night mode pin 0
    PORTD &= ~(1<<PD2);
    if( !(PINB & (1<< PINB0))){
      servo_set(OPEN_DOOR,180);
      state = DAY_DOOR_OPEN;
    }
    else{
      servo_set(CLOSE_DOOR,180);
      state = DAY_DOOR_CLOSED;
    }  
  }
  else{ //else night time close door
        //make night mode pin high
        PORTD |= (1<<PD2);
        servo_set(CLOSE_DOOR,180);
        state = NIGHTMODE;
      }
}

void check_weight(){

  unsigned short squirrel_adc;
  unsigned short food_adc;
 
  //food polling
  //ADMUX &= 0xF0; // clear the ADC channel selection bits
  ADMUX &= ~(1 << MUX3) & ~(1 << MUX2) & ~(1 << MUX1) & ~(1 << MUX0);
  ADMUX |= (1 << MUX1); // select the ADC channel for PC3
  ADCSRA |= (1 << ADSC); // start the conversion
  while (ADCSRA & (1 << ADSC)); // wait for the conversion to complete
  food_adc = ADC; // read the ADC result
  //feed tube weight low make output high
  if(food_adc > FOOD_WEIGHT_THRESHOLD){
    PORTC |= (1<<PC3);
  }
  else{
    PORTC &= ~(1<<PC3);
  }


  //squirrel polling
  //ADMUX &= 0xF0; // clear the ADC channel selection bits
  ADMUX &= ~(1 << MUX3) & ~(1 << MUX2) & ~(1 << MUX1) & ~(1 << MUX0);
  ADMUX |= (1 << MUX2); // select the ADC channel for PC5
  ADCSRA |= (1 << ADSC); // start the conversion
  while (ADCSRA & (1 << ADSC)); // wait for the conversion to complete
  squirrel_adc = ADC; // read the ADC result
  //squirrel weight sensor
  if( (squirrel_adc > SQUIRREL_WEIGHT_THRESHOLD)){
    PORTC |= (1<<PC5);
  }
  else{
    PORTC &= ~(1<<PC5);
  }

}
