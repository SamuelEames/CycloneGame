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
#include "FastLED.h"


///////////////////////// IO /////////////////////////
#define NUM_BTNS 			4 				// This isn't a pin - used in next line
const uint8_t btnBtnPin[NUM_BTNS] = {4, 6, 8, 20}; // Pins for buttons
// Pins for pixel strings on buttons
#define BTN_LED_PIN_0 5 			// Not sure of a better way to format this that will work
#define BTN_LED_PIN_1 7
#define BTN_LED_PIN_2 9
#define BTN_LED_PIN_3 19

// #define BEEP_PIN    	10 
#define LED_PIN     	21 			// Pin for main LED string 
#define RAND_ANALOG_PIN	10 		// Analog pin -- Leave pin floating -- used to seed random number generator


//////////////////// PIXEL SETUP ////////////////////
#define NUM_LEDS    	300 		
#define BRIGHTNESS  	255 		// Range 0-255
#define REFRESH_INT		20 			// (ms) Refresh interval of pixel strip (because updating them takes FOREVER) (40 = 25Hz)

Adafruit_NeoPixel leds(NUM_LEDS, LED_PIN, NEO_GRBW + NEO_KHZ800);

// Setup button pixels
#define NUM_BTN_LEDS 	12
CRGB btnleds[NUM_BTNS][NUM_BTN_LEDS];


const int pwmIntervals = 50;
float R;


// COLOURS -- 0xWWRRGGBB, Cyan, 	Yellow, 		Magenta		Green??
uint32_t COLOUR[] = {0x0000FFFF, 0x00FFDD00, 0x007700FF, 0x0000FF00};
uint32_t COLOUR_DIM[] = {0x00000A0A, 0x000A0700, 0x0007000A, 0x00000A00};
#define COL_BLACK 	0x00000000
#define COL_WHITE 	0xFF000000
#define COL_RED 		0x00FF0000
#define COL_GREEN 	0x0000FF00


//////////////////// MISC VARS ////////////////////

#define DEBOUNCE 			5							// (ms) Button debounce time

// State tracking
gameState GameState_Current = ST_RUN;			// State game start up into
gameState GameState_Last = ST_NULL;


uint8_t 	numPlayers = 3;							// Number of players (set during power up) <= NUM_BTNS (Not implemented)
uint16_t 	midPoint[NUM_BTNS];					// Midpoint (pixelNum) of each player zone
																			// 		This kinda doubles as score

uint8_t 	maxMarkerHWidth; 						// Max width that markers can get to
uint8_t 	def_halfWidth;  						// Default distance (pixels) from zone mid point to markers (initiated in setup())
uint8_t 	zoneSize;										// Size (pixels) of a player zone (initiated in setup())
#define 	MARKER_WIDTH 4 							// Width (pixels) of markers - Ideally keep this as an even number so maths is nicer later

#define 	IDLE_RESET_TIMER 30000			// Time (ms) before game variables are reset to original values 

#define 	METEOR_SPEED_MIN 	500				// (pixels per second) Time between lighting pixels
#define 	METEOR_SPEED_DEF	600
#define 	METEOR_SPEED_MAX 	700
#define 	METEOR_SPEED_SUPER 1200 		// If someone superWins
uint16_t 	meteorSpeed = METEOR_SPEED_DEF; 					

// Global variables - these ones update during game
uint16_t 	position = 0;  							// Current position of meteor (pixelNum)
uint8_t		halfWidth[NUM_BTNS];				// Length (in pixels) from midpoint to player markers
uint8_t 	btnPressed = NUM_BTNS;  		// Holds number of last button pressed
uint32_t 	t_lastLED_Update;  					// Time (micros) LEDs were last updated
uint32_t 	t_lastGame_Update;  				// Time (millis) last button was pressed used to reset game after 30 seconds of inactivity



void setup() 
{
	// Initialise IO
	for (uint8_t i = 0; i < NUM_BTNS; ++i)
	{
		pinMode(btnBtnPin[i], INPUT_PULLUP);
		// btnState_last[i] = 1;				// Set initial assumed button state
	}

	pinMode(RAND_ANALOG_PIN, INPUT);

	// Game Pixel Setup
	leds.begin(); 							// Initialise Pixels
	leds.show();								// Turn them all off
	leds.setBrightness(BRIGHTNESS);

	R = (pwmIntervals * log10(2))/(log10(255)); // Special number used in making nice log brightness tail of meteor

	// Button Pixel Setup - again, not sure how to make this tidier :(
	FastLED.addLeds<NEOPIXEL, BTN_LED_PIN_0>(btnleds[0], NUM_BTN_LEDS);
	FastLED.addLeds<NEOPIXEL, BTN_LED_PIN_1>(btnleds[1], NUM_BTN_LEDS);
	FastLED.addLeds<NEOPIXEL, BTN_LED_PIN_2>(btnleds[2], NUM_BTN_LEDS);
	FastLED.addLeds<NEOPIXEL, BTN_LED_PIN_3>(btnleds[3], NUM_BTN_LEDS);


	// Calculate mid points & marker widths
	maxMarkerHWidth = NUM_LEDS / (numPlayers *2);
	zoneSize = NUM_LEDS/numPlayers;											// Size of player zones in pixels

	def_halfWidth = maxMarkerHWidth / 4;

	for (uint8_t i = 0; i < numPlayers; ++i)
	{
		midPoint[i] = maxMarkerHWidth + (NUM_LEDS/numPlayers * i); 	// Calculate midpoints
		halfWidth[i] = def_halfWidth;											// Initial marker widths

		// Light button pixels their respective colours
		fill_solid(btnleds[i], NUM_BTN_LEDS, COLOUR[i]);
	}

	FastLED.show();

	// Initialise Serial debug
	#ifdef DEBUG
		Serial.begin(115200);					// Open comms line
		// while (!Serial) ; 					// Wait for serial port to be available

		Serial.println(F("Wassup?"));

		for (uint8_t i = 0; i < numPlayers; ++i)
		{
			Serial.print(F("Zone "));
			Serial.print(i, DEC);
			Serial.print(F("\t Start = "));
			Serial.print(zoneSize * i, DEC);
			Serial.print(F("\t End = "));
			Serial.println(zoneSize*i + zoneSize, DEC);
		}
	#endif

	randomSeed(analogRead(RAND_ANALOG_PIN)); 	// seed random number generator

	t_lastLED_Update = micros();
	t_lastGame_Update = millis();

	// Let the games begin!
}


void loop() 
{
	bool win = false; 												// True if button pushed within markers

	switch (GameState_Current)
	{
		case ST_RUN:
			// Check buttons - do this as much as possible
			btnPressed = checkButtons();

			// Calculate distance we moved since last iteration
			// distance = speed * time
			// position += (meteorSpeed * REFRESH_INT) / 1000;

			position += (meteorSpeed * (micros() - t_lastLED_Update)) / 1000000;
			// position = 0;
			t_lastLED_Update = micros();

			while	(position >= NUM_LEDS)					// Wrap around if we've gone off the edge of the tape
				position -= NUM_LEDS;

			drawField();
			dRamp_W(1, position, pwmIntervals); 	// Draw meteor

			leds.show();


			if (btnPressed != NUM_BTNS)
			{
				// Change state
				GameState_Current = ST_STOP;
				GameState_Last = ST_RUN;

				DPRINT(F("Button pressed = "));
				DPRINTLN(btnPressed);

				t_lastGame_Update = millis();

				// Flash result
				for (uint8_t i = 0; i < 4; ++i)
				{
					// Draw blank field
					drawField();
					leds.show();
					delay(50);

					// Draw field with meteor!
					drawField();
					dRamp_W(1, position, pwmIntervals);
					leds.show();
					delay(50);
				}
			}

			// reset to initial game state if no one has used in a while
			if (t_lastGame_Update + IDLE_RESET_TIMER < millis())
				resetGame();

			break;

		case ST_STOP:
			if (GameState_Last != ST_STOP)
			{
				// CHECK FOR WIN/LOSS
				// If button was pushed when meteor in my zone but outside markers, 0 to opponents, -1 to me
				// 		* Flash my marker zone red
				// If button was pushed when meteor was outside my zone, +1 to opponents (up to limit), -2 to me
				// 		* flash my whole zone red
				// If button pushed in my zone within markers, +1 to me!, -1 to opponents
				// 		* flash my merker zone green!

				// If button was pushed ... in my zone
				if ((position >= zoneSize * btnPressed) && (position < (zoneSize * btnPressed + zoneSize)))
				{

					// ... in my zone and within markers -- > I Win!
					if ((position >= midPoint[btnPressed] - halfWidth[btnPressed] - MARKER_WIDTH/2) && (position < midPoint[btnPressed] + halfWidth[btnPressed] + MARKER_WIDTH/2))
					{
						DPRINTLN(F("WIN"));
						leds.fill(COL_GREEN, midPoint[btnPressed] - halfWidth[btnPressed] - MARKER_WIDTH/2, halfWidth[btnPressed] *2 + MARKER_WIDTH);
						
						win = true;

						// Shift markers according to outcome
						for (uint8_t i = 0; i < numPlayers; ++i)
						{
							if (i == btnPressed)			// Reward me!
								halfWidth[i] -= 2;
							else 											// penalize opponents
								halfWidth[i] += 1;
						}
					}
					// ... in my zone but not within markers 
					else
					{
						DPRINTLN(F("Lose"));
						leds.fill(COL_RED, midPoint[btnPressed] - halfWidth[btnPressed] - MARKER_WIDTH/2, halfWidth[btnPressed] *2 + MARKER_WIDTH);
					
						// Shift markers according to outcome
						for (uint8_t i = 0; i < numPlayers; ++i)
						{
							if (i == btnPressed)			// Penalize me! - Do nothing for opponents
								halfWidth[i] += 1;
						}
					}

					DPRINT(F("Position = "));
					DPRINT(position);
					DPRINT(F("\t LMarker = "));
					DPRINT(midPoint[btnPressed] - halfWidth[btnPressed] - MARKER_WIDTH/2);
					DPRINT(F("\t UMarker = "));
					DPRINTLN(midPoint[btnPressed] + halfWidth[btnPressed] + MARKER_WIDTH/2);
				}
				// ... not within my zone
				else 
				{
					DPRINTLN(F("Really bad lose"));
					leds.fill(COL_RED, zoneSize*btnPressed, zoneSize);
					for (uint8_t i = 0; i < numPlayers; ++i)
					{
						if (i == btnPressed)					// penalize me
							halfWidth[i] += 3;
						else if (halfWidth[i] > 6)		// reward opponents - only up to half width = 4
							halfWidth[i] -= 1;
					}
				}

				// Draw meteor back on where it currently is (in case red/green drew over it)
				dRamp_W(1, position, pwmIntervals); // Draw meteor

				// Check updated player widths are within allowable bounds
				for (uint8_t i = 0; i < numPlayers; ++i)
				{
					// Don't allow markers to move outside of player zones
					if (halfWidth[i] > zoneSize/2 - MARKER_WIDTH)
					{
						if (halfWidth[i] > 250)			// Check we didn't overflow (due to going negative)
							halfWidth[i] = 0;
						else
							halfWidth[i] = zoneSize/2 - MARKER_WIDTH;
					}
				}

				DPRINT(F("Halfwidth = "));
				DPRINTLN(halfWidth[btnPressed]);

				// If markers on top of eachother - WIN!
				if (halfWidth[btnPressed] <= 2 && win)
				{
					DPRINTLN(F("WINNER"));
					// Do some sort of rainbow animation thing in winner player zone
					GameState_Current = ST_WINNER_WINNER;
					GameState_Last = ST_STOP;
					t_lastGame_Update = millis();
				}	
				else
				{
					leds.show();

					// Start new round with updated marker positions
					delay(1000);
					GameState_Current = ST_RUN;
					GameState_Last = ST_STOP;

					// Continue meteor from where we left off
					t_lastLED_Update = micros();
				}
			}

			break;


		case ST_WINNER_WINNER:
			if ((t_lastGame_Update + 500) > millis())
			{
				if (halfWidth[btnPressed] == 0)
				{
					superWin();
					meteorSpeed = METEOR_SPEED_MAX;
				}
				else
				{
					winnerWinner();
					meteorSpeed = random(METEOR_SPEED_MIN, METEOR_SPEED_MAX);
				}
			}
			else
			{
				GameState_Current = ST_RUN;
				GameState_Last = ST_WINNER_WINNER;

				// reset opponent markers if they're < default
				for (uint8_t i = 0; i < numPlayers; ++i)
				{
					if (i != btnPressed)
					{
						if (halfWidth[i] < def_halfWidth)
							halfWidth[i] = def_halfWidth;
					}
				}

				t_lastLED_Update = micros();		// Continue meteor from where we left off
			}

			break;

		default:
			break;
	}

}

void winnerWinner()
{
	static uint32_t firstPixelHue;

	// Play rainbow effect on winning zone
	for(uint16_t i = btnPressed * zoneSize; i < btnPressed * zoneSize + zoneSize; i++) 
	{
		uint32_t pixelHue = firstPixelHue + (i * 65536L / NUM_LEDS);
		leds.setPixelColor(i, leds.gamma32(leds.ColorHSV(pixelHue)));
	}

	firstPixelHue += 2000; 					// Shift colour wheel

	leds.show();

	return;
}

void superWin()
{
	// Same as winnerWinner() except has white sparkles on top
	uint16_t randomPix;
	static uint32_t firstPixelHue;

	for(uint16_t i = btnPressed * zoneSize; i < btnPressed * zoneSize + zoneSize; i++) 
	{
		uint32_t pixelHue = firstPixelHue + (i * 65536L / NUM_LEDS);
		leds.setPixelColor(i, (leds.getPixelColor(i) & COL_WHITE) | leds.gamma32(leds.ColorHSV(pixelHue, 255, 127)));
	}

	firstPixelHue += 2000; 					// Shift colour wheel

	// Set random white LED in zone to white
	randomPix = random(btnPressed * zoneSize, btnPressed * zoneSize + zoneSize);
	leds.setPixelColor(randomPix, (leds.getPixelColor(randomPix) | COL_WHITE));

	leds.show();

	return;
}


void resetGame()
{
	// Resets game variables back to default starting values
	for (uint8_t i = 0; i < numPlayers; ++i)
		halfWidth[i] = def_halfWidth;

	meteorSpeed = METEOR_SPEED_DEF;

	return;
}


void drawField()
{
	// Draws field design

	// Colour dim background
	for (uint8_t i = 0; i < numPlayers; ++i)
		leds.fill(COLOUR_DIM[i], zoneSize*i, zoneSize*i + zoneSize);

	// Fill in remainder LEDs when not even split
	leds.fill(COLOUR_DIM[numPlayers], zoneSize*numPlayers, NUM_LEDS);

	// Drawer player aim markers
	for (uint8_t i = 0; i < numPlayers; ++i)
	{
		leds.fill(COLOUR[i], (midPoint[i] - halfWidth[i] - MARKER_WIDTH), MARKER_WIDTH); 	// Draw Lower Marker
		leds.fill(COLOUR[i], (midPoint[i] + halfWidth[i]) , MARKER_WIDTH); 	// Draw Upper Marker
	}

	// Colour player buttons
	for (uint8_t i = 0; i < numPlayers; ++i) // yes I realise I have the same for loop three times
		fill_solid( btnleds[i], NUM_BTN_LEDS, COLOUR[i] << 2);

	return;
}


void dRamp_W(bool dir, uint16_t pos, uint8_t len)
{
	// Down ramp effect
	int16_t pixelNum = pos-len+1; 	 							// Track current pixel
	uint8_t intensity;

	// Ensure position is within bounds of LED string
	while (pixelNum < 0)
		pixelNum += NUM_LEDS;

	while (pixelNum > NUM_LEDS)
		pixelNum -= NUM_LEDS;

	// Draw it
	for (uint8_t i = 0; i < len; ++i)
	{
		intensity = (pow (2, (i / R)) - 1);

		// Draw meteor on top of current LED state
		leds.setPixelColor(pixelNum, (leds.getPixelColor(pixelNum) | (uint32_t) intensity<<24));

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
		btnState_now[i] = digitalRead(btnBtnPin[i]);	// Read input

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
