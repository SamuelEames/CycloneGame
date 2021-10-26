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



		SCORING
			Because I'm not going to spit out tickets I've decided to score this differently.
			If a player successfully captures the meteor within their markers,
			their markers take two steps in and their opponents markers go one step out.

			At some point the meteor speed increases / decreases... maybe randomly per round?


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


// COLOURS -- 0xWWRRGGBB, Cyan, 	Yellow, 		Magenta		Green
uint32_t COLOUR[] = {0x0000FFFF, 0x00FFDD00, 0x007700FF, 0x0000FF00};
uint32_t COLOUR_DIM[] = {0x00000A0A, 0x000A0700, 0x0007000A, 0x00000A00};
uint32_t COL_BLACK = 0x00000000;
uint32_t COL_WHITE = 0xFF000000;


//////////////////// MISC VARS ////////////////////

#define DEBOUNCE 		5								// (ms) Button debounce time
uint16_t meteorSpeed = 600; 						// (pixels per second) Time between lighting pixels

// State tracking
gameState GameState_Current = ST_RUN;			// State game start up into
gameState GameState_Last = ST_NULL;


uint8_t numPlayers = 3;								// Number of players (set during power up) <= NUM_BTNS
uint16_t midPoint[NUM_BTNS];					// Midpoint (pixelNum) of each player zone
uint8_t	playerHWidth[NUM_BTNS];				// Length (in pixels) from midpoint to player markers
																			// This kinda doubles as score

uint8_t maxMarkerHWidth; 							// Max width that markers can get to
uint8_t zoneSize = 0;


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


	// Calculate mid points & marker widths
	maxMarkerHWidth = NUM_LEDS / (numPlayers *2);
	zoneSize = NUM_LEDS/numPlayers;																// Size of player zones in pixels


	for (uint8_t i = 0; i < numPlayers; ++i)
	{
		midPoint[i] = maxMarkerHWidth + (NUM_LEDS/numPlayers * i); 	// Calculate midpoints
		playerHWidth[i] = maxMarkerHWidth / 4;											// Initial marker widths
	}



	// Initialise Serial debug
	#ifdef DEBUG
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
	static uint8_t btnPressed = NUM_BTNS;


	switch (GameState_Current)
	{
		case ST_RUN:
			// Check buttons - do this as much as possible
			btnPressed = checkButtons();

			// Calculate distance we moved since last iteration
			// distance = speed * time
			// position += (meteorSpeed * REFRESH_INT) / 1000;

			position += (meteorSpeed * (micros() - lastLED_Update)) / 1000000;
			lastLED_Update = micros();

			while	(position >= NUM_LEDS)		// Wrap around if we've gone off the edge of the tape
				position -= NUM_LEDS;

			drawField();
			dRamp_W(1, position++, pwmIntervals); // Draw meteor

			leds.show();


			if (btnPressed != NUM_BTNS)
			{
				// Change state
				GameState_Current = ST_STOP;
				GameState_Last = ST_RUN;

				
				// Flash result
				for (uint8_t i = 0; i < 4; ++i)
				{
					// Draw blank field
					drawField();
					leds.show();
					delay(60);

					// Draw field with meteor!
					drawField();
					dRamp_W(1, position++, pwmIntervals);
					leds.show();
					delay(60);
				}

			}

			break;

		case ST_STOP:
			if (GameState_Last != ST_STOP)
			{
				// CHECK FOR WIN/LOSS
				// If button was pushed when meteor was outside my zone, +1 to opponents (up to limit), -2 to me
				// 		* flash my whole zone red

				if ((position >= zoneSize * btnPressed) && (position <= zoneSize * btnPressed + zoneSize))
				{
					leds.fill(0x0000FF00, zoneSize*btnPressed, zoneSize*btnPressed + zoneSize);
				}
				else
					leds.fill(0x00FF0000, zoneSize*btnPressed, zoneSize*btnPressed + zoneSize);

					leds.show();
				// If button was pushed when meteor in my zone but outside markers, 0 to opponents, -1 to me
				// 		* Flash my marker zone red
				// If button pushed in my zone within markers, +1 to me!, -1 to opponents
				// 		* flash my merker zone green!


				// Start new round with updated marker positions

				// 
				



			}

			break;

		default:
			break;
	}




		// If button press
			// Work out where meteor was when button pressed
			// If it's close enough to button, WIN!
			// If else, fail
			// But also throw in some gambling logic to balance wins/losses 


	// Update LEDs


}


void drawField()
{
	// Draws field design

	// uint8_t zoneSize = NUM_LEDS/numPlayers;
	uint8_t markerWidth = 3;

	// Colour dim background
	for (uint8_t i = 0; i < numPlayers; ++i)
		leds.fill(COLOUR_DIM[i], zoneSize*i, zoneSize*i + zoneSize);

	// Fill in remainder LEDs when not even split
	leds.fill(COLOUR_DIM[numPlayers], zoneSize*numPlayers, NUM_LEDS);

	// Drawer player aim markers
	for (uint8_t i = 0; i < numPlayers; ++i)
	{
		leds.fill(COLOUR[i], (midPoint[i] - playerHWidth[i] - markerWidth), markerWidth); 	// Draw Lower Marker
		leds.fill(COLOUR[i], (midPoint[i] + playerHWidth[i]) , markerWidth); 	// Draw Upper Marker
	}

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

		// Draw meteor on top of current LED state
		leds.setPixelColor(pixelNum, (leds.getPixelColor(pixelNum) | (uint32_t) intensity<<24));

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
