// 'Cyclone Arcade Game'


/* NOTES

		* Hold button when powering on to set how many buttons (players) will be used
				* Max 4 buttons
				* Default ??
		* Speed increases with each success, and decreases with each failed attempt
		* OR make it lihg the actual program and just implement a win %



	Program Mechanics

	* Updating the LEDs takes a long time so we can't do this every time LEDs change
	* Instead, update on time intervals (30fps or something) and recalculate where light is based on speed (m/s)
	* 


*/

//////////////////// DEBUG SETUP ////////////////////
// #define DEBUG   //If you comment this line, the DPRINT & DPRINTLN lines are defined as blank.
#ifdef DEBUG
	#define DPRINT(...)   Serial.print(__VA_ARGS__)   //DPRINT is a macro, debug print
	#define DPRINTLN(...) Serial.println(__VA_ARGS__) //DPRINTLN is a macro, debug print with new line
#else
	#define DPRINT(...)                       //now defines a blank line
	#define DPRINTLN(...)                     //now defines a blank line
#endif



//////////////////// LIBRARIES ////////////////////
#include <Adafruit_NeoPixel.h>
#include "ENUMVars.h"


///////////////////////// IO /////////////////////////
#define NUM_BTNS 		4
const uint8_t Button[NUM_BTNS] = {7, 8, 9, 10}; // Pins for buttons

#define BEEP_PIN    	5
#define LED_PIN     	6

//////////////////// PIXEL SETUP ////////////////////
#define NUM_LEDS    	300
#define BRIGHTNESS  	255 				// Range 0-255
#define REFRESH_INT	20 				// (ms) Refresh interval of pixel strip (because updating them takes FOREVER) (40 = 25Hz)

Adafruit_NeoPixel leds(NUM_LEDS, LED_PIN, NEO_GRBW + NEO_KHZ800);



const int pwmIntervals = 50;
float R;


//////////////////// MISC VARS ////////////////////

#define DEBOUNCE 		5								// (ms) Button debounce time
uint16_t meteorSpeed = 300; 						// (pixels per second) Time between lighting pixels

// State tracking
gameStates currentState = ST_RUN;			// State game start up into
gameStates lastState = ST_NULL;


uint8_t numPlayers = NUM_BTNS;								// Number of players (set during power up) <= NUM_BTNS


void setup() 
{
	// Initialise IO
	for (uint8_t i = 0; i < NUM_BTNS; ++i)
	{
		pinMode(Button[i], INPUT_PULLUP);
		// btnState_last[i] = 1;				// Set initial assumed button state
	}

	// Pixel Setup
	leds.begin(); 							// Initialise Pixels
	leds.show();								// Turn them all off
	leds.setBrightness(BRIGHTNESS);

	R = (pwmIntervals * log10(2))/(log10(255));


	// Initialise Serial debug
	#ifdef debugMSG
		Serial.begin(115200);				// Open comms line
		while (!Serial) ; 					// Wait for serial port to be available

		Serial.println(F("Wassup?"));
	#endif

	// Let the games begin!
}


// strip.setPixelColor(n, red, green, blue, white);    // Raw setting of colours
// strip.setPixelColor(n, color);
// uint32_t greenishwhite = strip.Color(0, 64, 0, 64); // Setup colour variable
// strip.fill(color, first, count); 										// Set a bunch of LEDs in a row to the same colour
// strip.clear(); 																			// Set LEDs to black

void loop() 
{

	static uint16_t position = 0;
	static uint32_t lastLED_Update = 0;
	static uint32_t lastGame_Update = 0;

	// Check buttons - do this as much as possible
		// If button press
			// Work out where meteor was when button pressed
			// If it's close enough to button, WIN!
			// If else, fail
			// But also throw in some gambling logic to balance wins/losses 


	// Update LEDs - to this at 30fps - this is also 
	// if (millis() - lastLED_Update > REFRESH_INT)
	// {

		// Calculate distance we moved since last iteration
		// distance = speed * time
		// position += (meteorSpeed * REFRESH_INT) / 1000;

		position += (meteorSpeed * (micros() - lastLED_Update)) / 1000000;
		lastLED_Update = micros();

		while	(position >= NUM_LEDS)		// Wrap around if we've gone off the edge of the tape
			position -= NUM_LEDS;

		dRamp_W(1, position++, pwmIntervals);

		leds.show();
	// }




	// Update game state ??

	// if (millis() - lastGame_Update > meteorSpeed)
	// {
	// 	lastGame_Update = millis();

	// 	switch (currentState) 
	// 	{
	// 		case ST_RUN: 	
	// 			// leds.clear();
	// 			dRamp_W(1, position++, pwmIntervals);
	// 			// delay(1);


	// 			break;

	// 		case ST_STOP:
	// 			break;

	// 		default:
	// 			break;
	// 	}

	// 	if (position >= NUM_LEDS)
	// 		position = 0;

	// drawPlayers();


		// update LEDs

	// }

}


void drawField()
{
	// Draws field design

	return;
}

void drawPlayers()
{
	// Adds players to pixel buffer

	return;
}


void dRamp_W(bool dir, uint16_t pos, uint8_t len)
{
	// Down ramp effect

	uint16_t pixelNum = pos; 	 							// Track current pixel
	uint8_t intensity = 255;
	uint8_t dimStep = intensity/len;


	for (uint8_t i = 0; i < len; ++i)
	{
		intensity = pow (2, (i / R)) - 1;
		leds.setPixelColor(pixelNum, 0, 0, 255, intensity);

		// intensity -=  dimStep;

		Serial.println(intensity);

		// Go to next LED & loop around if going off edge of pixel strip
		if (dir)
		{
			if (++pixelNum >= NUM_LEDS)
				pixelNum = 0;
		}
		else
		{
			if (pixelNum == 0)
				pixelNum = NUM_LEDS;
			
			pixelNum--;
		}


	}

	return;
}



uint8_t checkButtons()
{
	// Checks button state
	// If button just pressed returns that button number, else returns NUM_BTNS

	// Button state arrays
	static uint8_t btnState_last[NUM_BTNS];			// State of buttons on previous iteration
	uint8_t btnState_now[NUM_BTNS];							// Last read state of buttons


	// Debounce buttons
	static uint32_t lasttime;

	if (millis() < lasttime) 										// Millis() wrapped around - restart timer
		lasttime = millis();

	if ((lasttime + DEBOUNCE) > millis())				// Debounce timer hasn't elapsed
		return NUM_BTNS; 
	
	lasttime = millis();												// Debouncing complete; record new time & continue

	// Record button state
	for (uint8_t i = 0; i < NUM_BTNS; ++i)
	{
		btnState_now[i] = digitalRead(Button[i]);	// Read input

		if (btnState_now[i] != btnState_last[i]) 	// If button state changed 
		{
			btnState_last[i] = btnState_now[i];			// Record button state

			if (!btnState_now[i])
			{
				// beepNow();
				return i;															// Return number of button that was just pressed
			}
		}
	}

	return NUM_BTNS;														// Return NUM_BTNS if no buttons pressed
}
