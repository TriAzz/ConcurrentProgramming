//Initialize global variables for sensor states and interrupt handling
volatile byte r, g, b = 0;           
volatile bool interruptActive = false; 
volatile unsigned int interruptTimer = 0;
const unsigned int interruptTimeout = 5;

// Debug variables
volatile bool sensorStateChanged = false;
volatile bool timerStateChanged = false;

void setup() {
  
  	//Initialize serial monitoring
    Serial.begin(9600);
    Serial.println("=== Debug Started ===");
  
    //Set pins 10-12 as outputs
    DDRB |= 0b00011100;  //Set bits 2,3,4 of DDRB (pins 10,11,12)
  	Serial.println("LED pins 10-12 configured as outputs");
    
    //Enable Pin Change Interrupts
    PCICR = 0b00000100;   //Enable pin change interrupts for PCINT2 group
    PCMSK2 = 0b00011100;  //Enable interrupts pins 2, 3, 4
  	Serial.println("Pin change interrupts enabled for pins 2, 3, 4");
    
    //Configure Timer1 for periodic interrupts
    TCCR1A = 0;
    
    //Set Timer1 to CTC mode
    TCCR1B = (1 << WGM12) | (1 << CS12) | (1 << CS10); 
    
    //Set compare value
    OCR1A = 16000000/1024 - 1;
  	Serial.print("Timer1 configured - OCR1A value: ");
    Serial.println(OCR1A);
    
    //Enable Timer1 compare match A interrupt
    TIMSK1 = (1 << OCIE1A);
  	Serial.println("Timer1 interrupt enabled");
    
    //Initialize LEDs to current sensor states
    updateLED();
  	
  	//Inform serial monitor entire setup function is completed
  	Serial.println("Setup complete");
    Serial.println("Monitoring sensor changes and timer events...");
    Serial.println();
}

void loop() {
    //Loops function will solely focus on debugging
    if (sensorStateChanged) {
        sensorStateChanged = false;
        Serial.print("SENSOR MODE: R: ");
        Serial.print(r ? "ON" : "OFF");
        Serial.print(" G: ");
        Serial.print(g ? "ON" : "OFF");
        Serial.print(" B: ");
        Serial.print(b ? "ON" : "OFF");
        Serial.print(" | Active: ");
        Serial.println(interruptActive ? "YES" : "NO");
    }
    
    if (timerStateChanged) {
        timerStateChanged = false;
        Serial.print("TIMER MODE: R:");
        Serial.print(r ? "ON" : "OFF");
        Serial.print(" G:");
        Serial.print(g ? "ON" : "OFF");
        Serial.print(" B:");
        Serial.println(b ? "ON" : "OFF");
    }
}

//Function to update LED states based on sensor values
void updateLED() {
	PORTB &= 0b11100011;
	if (g) PORTB |= 0b00000100;
	if (b) PORTB |= 0b00001000;
	if (r) PORTB |= 0b00010000;
}

//Interrupt Service Routine for pin change interrupts
ISR(PCINT2_vect) {
    interruptActive = true;  //Mark that sensors are currently active
    interruptTimer = 0;      //Reset timeout counter
    
    //Read sensor states from input pins
    r = PIND & 0b00010000;   //Read the red sensor at pin 4
    g = PIND & 0b00000100;   //Read the green sensor at pin 2
    b = PIND & 0b00001000;    //Read the blue sensor at pin 3
    
    updateLED();  //Update LED display
  	sensorStateChanged = true; //Let debug loop know to write update
}

//Interrupt Service Routine for Timer1 compare match
ISR(TIMER1_COMPA_vect) {
    if (interruptActive) {
        //If sensors are active, increment timeout counter
        interruptTimer++;
        if (interruptTimer >= interruptTimeout) {
            //Interrupt ended, return to flashing yellow
            interruptActive = false;
            interruptTimer = 0;
        }
    } else {
        //Flash yellow by toggling red and green LEDs
        r = !r;
        g = !g;
        b = 0;
        updateLED();
        timerStateChanged = true; //Let debug loop know to write update
    }
}