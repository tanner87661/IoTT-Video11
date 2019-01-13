#include <Mux.h>
#include <FastLED.h>

#define TestChip

// How many leds in your strip?
#ifdef TestChip
  #define NUM_LEDS 16
#else
  #define NUM_LEDS 108
#endif

// For led chips like Neopixels, which have a data line, ground, and power, you just
// need to define DATA_PIN.  For led chipsets that are SPI based (four wires - data, clock,
// ground, and power), like the LPD8806 define both DATA_PIN and CLOCK_PIN
#ifdef TestChip
  #define DATA_PIN 19
#else
  #define DATA_PIN 12
#endif
#ifdef TestChip
  #define numPorts 1
  #define numTouchButtons 16
#else
  #define numPorts 1
  #define numTouchButtons 16
#endif
const int touchThreshold = 12;
#define shortPressThreshold 200
#define longPressThreshold 1000
#define analogCountThreshold 5 //number of analog reads between pressed and released status to suppress digital messaging
#define noTouchMax 5
#define noConnectMax 30
const float avgBase = 5;
const int numCalibRuns = 100;

typedef struct 
{
  uint8_t  touchData = 0;
  uint16_t analogData = 0;
  uint8_t  touchLow = touchThreshold - 2;
  uint8_t  touchHigh = 0;
  float    touchAvg = 0.0;
  uint16_t prevData = 0;
  uint8_t  btnStatus = 0x83; //low nibble: internal  high nibble: external  bit 7: Active
  uint8_t  btnTouchThreshold = touchThreshold;
  uint32_t lastStatChg = millis(); 
  uint8_t  analogCount = 0;
} touchData;

CRGB leds[NUM_LEDS];

//Mux mux(2,3,4,5,A0); // initialise on construct... 
Mux mux; // ...or construct now, and call setup later

#ifdef TestChip
  int analogPins[] = {numPorts,13,12,14,27}; //byte 0 is number of defined pins to follow
#else
  int analogPins[] = {numPorts,4}; //byte 0 is number of defined pins to follow
#endif

float avgMultiplier = ((avgBase - 1) / avgBase);
float avgNewMult = (1 / avgBase);

void setup(){
  touch_pad_init();
  Serial.begin(115200);
  mux.setup(18,5,17,16,&analogPins[0]); // initialise on setup d0, d1, d2, d3, analog, enable
  FastLED.addLeds<WS2811, DATA_PIN, GRB>(leds, NUM_LEDS);
  for(int i = 0; i < NUM_LEDS; i++)
    leds[i] = 0x050500;
//  calibrateButtons();
}

int numRead = 5;
touchData touchArray[16*numPorts];

void buttonPressed(int buttonNr)
{
  Serial.print("Short pressed ");
  Serial.println(buttonNr);
  leds[buttonNr] = 0x070000;
}

void buttonReleased(int buttonNr)
{
  Serial.print("Short released ");
  Serial.println(buttonNr);
  leds[buttonNr] = 0x000007;
}

void buttonClicked(int buttonNr)
{
  Serial.print("Clicked ");
  Serial.println(buttonNr);
}

void buttonLongPressed(int buttonNr)
{
  Serial.print("Long pressed ");
  Serial.println(buttonNr);
}

void buttonLongReleased(int buttonNr)
{
  Serial.print("Long released ");
  Serial.println(buttonNr);
}

void analogSensorInput(int buttonNr, int analogValue)
{
  Serial.printf("Analog Value of Button %d is %d \n", buttonNr, analogValue);
  uint16_t luxVal = (touchArray[buttonNr].analogData & 0x0FF0) << 4;
  leds[buttonNr] = luxVal; //0x000700; //((luxVal & 0x00FF) << 8);
}

void processDigitalButton(int btnNr)
{
  int bounceDelay = millis() - touchArray[btnNr].lastStatChg;
  if (bounceDelay > longPressThreshold)
    touchArray[btnNr].btnStatus |= 0x01; //low nibble: internal
  else
    if (bounceDelay > shortPressThreshold)
      touchArray[btnNr].btnStatus |= 0x02;
  byte eventMask = ((touchArray[btnNr].btnStatus & 0x03) ^ ((touchArray[btnNr].btnStatus >> 4) & 0x03)); //high nibble: Public Status
  if (eventMask > 0)
  {
//    touchArray[btnNr].buttonPublicStatus = touchArray[btnNr].buttonInternalStatus;
    touchArray[btnNr].btnStatus = (touchArray[btnNr].btnStatus & 0x8F) | ((touchArray[btnNr].btnStatus & 0x03)<<4); // save internal status to published status

    if (touchArray[btnNr].analogCount < analogCountThreshold)
    {
      if ((eventMask & 0x02) > 0) //short click event
      {
        if ((touchArray[btnNr].btnStatus & 0x80) > 0)
          buttonPressed(btnNr); //used pressed/released for level dependent stuff
        else
        {
          buttonReleased(btnNr);
          buttonClicked(btnNr); //use clicked for toggle button
        }
      }   
      if ((eventMask & 0x01) > 0) //long click event
      {
        if ((touchArray[btnNr].btnStatus & 0x80) > 0)
          buttonLongPressed(btnNr); //use longpressed for two button commands
        else
          buttonLongReleased(btnNr); //not needed
        touchArray[btnNr].analogCount = 0;
      }
    }
  }   
}

void readTouchButtons()
{
  int lineNr;
  int portNr;
  byte hlpVal;
  for (int btnCtr = 0; btnCtr < numTouchButtons; btnCtr++)
  {
    lineNr = btnCtr % 16;
    portNr = trunc(btnCtr/16);
    touchArray[btnCtr].touchData = 0;
    touchArray[btnCtr].analogData = 0;
    for (int i = 0; i < numRead; i++)
    {
      hlpVal = (mux.readTouch(lineNr, portNr) & 0x00FF);
      touchArray[btnCtr].touchData += hlpVal;
      if ((hlpVal > touchArray[btnCtr].touchHigh) && (hlpVal > noTouchMax))
        touchArray[btnCtr].touchHigh = hlpVal;
      if ((hlpVal < touchArray[btnCtr].touchLow) && (hlpVal > noTouchMax))
        touchArray[btnCtr].touchLow = hlpVal;
      touchArray[btnCtr].btnTouchThreshold = (touchArray[btnCtr].touchLow + touchArray[btnCtr].touchHigh)>>1;
      touchArray[btnCtr].analogData += mux.readAnalog(lineNr, portNr);
    }
    touchArray[btnCtr].touchData = round(touchArray[btnCtr].touchData/numRead);
    touchArray[btnCtr].analogData = round(touchArray[btnCtr].analogData/numRead);
    touchArray[btnCtr].touchAvg = (touchArray[btnCtr].touchAvg * avgMultiplier) + ((float)touchArray[btnCtr].touchData * avgNewMult);
    if ((btnCtr == 2) || (btnCtr == 10))
    {
//      byte addrSettings = (digitalRead(18)<<3) + (digitalRead(5)<<2) + (digitalRead(17)<<1) + (digitalRead(16));
//      Serial.printf("B: %d V: %f IO: %d \n", btnCtr, touchArray[btnCtr].touchAvg, addrSettings);
    }
  }
  for (int btnCtr = 0; btnCtr < numTouchButtons; btnCtr++)
  {
    if (touchArray[btnCtr].touchAvg > noTouchMax) //is this a touch button?
    {
      touchArray[btnCtr].analogCount = 0;
      bool hlpActive = touchArray[btnCtr].touchAvg < touchArray[btnCtr].btnTouchThreshold;
      bool oldActive = (touchArray[btnCtr].btnStatus & 0x80) > 0;
      if (oldActive != hlpActive) //digital value and not the same as before
      {
        if (hlpActive)
          touchArray[btnCtr].btnStatus = 0x80; //Set Active Status, clear other status flags
        else  
          touchArray[btnCtr].btnStatus = 0x00; //Clear Active Status, clear other status flags
        touchArray[btnCtr].lastStatChg = millis(); //start bounce / long press timer
      }
      processDigitalButton(btnCtr);
    }
    else //process analog / digital button
    {
      int valSpan = abs(touchArray[btnCtr].analogData - touchArray[btnCtr].prevData);
      if ((valSpan > (0.005 * 4095)) || ((valSpan > 0) && ((touchArray[btnCtr].analogData == 0) || (touchArray[btnCtr].analogData == 4095))))   //react at 0.5% deviation or when MIN or MAX
      {
        analogSensorInput(btnCtr, touchArray[btnCtr].analogData);
        if (touchArray[btnCtr].analogCount < 255)
          touchArray[btnCtr].analogCount += 1;
        touchArray[btnCtr].prevData = touchArray[btnCtr].analogData;
      }
      bool hlpActive = touchArray[btnCtr].analogData <= (0.01 * 4095); //1%
      bool hlpPassive = touchArray[btnCtr].analogData >= (0.99 * 4095); //99%
      bool oldActive = (touchArray[btnCtr].btnStatus & 0x80) > 0;
//      Serial.printf("V: %d A: %d P: %d O: %d \n", touchArray[btnCtr].analogData, hlpActive, hlpPassive, oldActive);
      if ((hlpActive != hlpPassive) && (oldActive != hlpActive)) //digital value and not the same as before
      {
        if (hlpActive)
          touchArray[btnCtr].btnStatus = 0x80; //Set Active Status, clear other status flags
        else  
          touchArray[btnCtr].btnStatus = 0x00; //Clear Active Status, clear other status flags
        touchArray[btnCtr].lastStatChg = millis(); //start bounce / long press timer
      }
      if (hlpActive != hlpPassive)
        processDigitalButton(btnCtr);
    }
  }
  FastLED.show();
}

void loop()
{
  readTouchButtons();
}


