
// Example sketch which shows how to display some patterns
// on a 64x32 LED matrix
//

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <AnimatedGIF.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>


#define PANEL_RES_X 64 // Number of pixels wide of each INDIVIDUAL panel module.
#define PANEL_RES_Y 64 // Number of pixels tall of each INDIVIDUAL panel module.
#define PANEL_CHAIN 2  // Total number of panels chained one to another


//Your Domain name with URL path or IP address with path
String wordpressRating = "https://fluent-support-iot.vercel.app/api/wordpressrating";
String fluentSupport = "https://fluent-support-iot.vercel.app/api/iot";

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastTime = 0;
// Timer set to 10 minutes (600000)
//unsigned long timerDelay = 600000;
// Set timer to 5 seconds (5000)
unsigned long timerDelay = 5000;

//MatrixPanel_I2S_DMA dma_display;
MatrixPanel_I2S_DMA *dma_display = nullptr;

uint16_t myBLACK = dma_display->color565(0, 0, 0);
uint16_t myWHITE = dma_display->color565(255, 255, 255);
uint16_t myRED = dma_display->color565(255, 0, 0);
uint16_t myGREEN = dma_display->color565(0, 0, 255);
uint16_t myBLUE = dma_display->color565(0, 255, 0);

TaskHandle_t Task1;
TaskHandle_t Task2;

uint8_t wheelval = 0;

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
// From: https://gist.github.com/davidegironi/3144efdc6d67e5df55438cc3cba613c8
uint16_t colorWheel(uint8_t pos)
{
  if (pos < 85)
  {
    return dma_display->color565(pos * 3, 255 - pos * 3, 0);
  }
  else if (pos < 170)
  {
    pos -= 85;
    return dma_display->color565(255 - pos * 3, 0, pos * 3);
  }
  else
  {
    pos -= 170;
    return dma_display->color565(0, pos * 3, 255 - pos * 3);
  }
}

String lines[] = {"TICKET:045", "REPLY :030", "REVIEW:009"};
int scrollShift = 0;
String scollingText = "";

void drawText(int colorWheelOffset)
{
  dma_display->fillRect(0,0, 128, dma_display->height(), 0);
  // draw text with a rotating colour
  dma_display->setTextSize(2);     // size 1 == 8 pixels high
  dma_display->setTextWrap(false); //  Don't wrap at end of line - will do ourselves

  dma_display->setCursor(5, 5); // start at top left, with 8 pixel of spacing
  uint8_t w = 0;

  String str = lines[0];
  for (w = 0; w < str.length(); w++)
  {
    dma_display->setTextColor(myBLUE);
    dma_display->print(str[w]);
  }
  dma_display->println();
  dma_display->setCursor(5, 10+16); // start at top left, with 8 pixel of spacing
  String secondLine = lines[1];
  for (w = 0; w < secondLine.length(); w++)
  {
    dma_display->setTextColor(myGREEN);
    dma_display->print(secondLine[w]);
  }
  dma_display->setCursor(5, 5+16+10+16); // start at top left, with 8 pixel of spacing
  String thirdLine = lines[2];
  for (w = 0; w < thirdLine.length(); w++)
  {
    dma_display->setTextColor(myRED);
    dma_display->print(thirdLine[w]);
  }

  dma_display->println();
  dma_display->setTextSize(1);
  String fouthLine = scollingText.substring(scrollShift);
  for (w = 0; w < fouthLine.length(); w++)
  {
    dma_display->fillRect(0,dma_display->height()-18, 128, 18, 0);
    dma_display->setTextColor(colorWheel((w * 32) + colorWheelOffset));
    dma_display->print(fouthLine[w]);
  }
}

AnimatedGIF gif[2]; // we need 4 independent instances of the class to animate the 4 digits simultaneously
typedef struct my_private_struct
{
  int xoff, yoff; // corner offset
} PRIVATE;

// Draw a line of image directly on the LED Matrix
void GIFDraw(GIFDRAW *pDraw)
{
  uint8_t *s;
  uint16_t *d, *usPalette, usTemp[320];
  int x, y, iWidth;

  iWidth = pDraw->iWidth;
  if (iWidth > MATRIX_WIDTH)
    iWidth = MATRIX_WIDTH;

  usPalette = pDraw->pPalette;
  y = pDraw->iY + pDraw->y; // current line

  s = pDraw->pPixels;
  if (pDraw->ucDisposalMethod == 2) // restore to background color
  {
    for (x = 0; x < iWidth; x++)
    {
      if (s[x] == pDraw->ucTransparent)
        s[x] = pDraw->ucBackground;
    }
    pDraw->ucHasTransparency = 0;
  }
  // Apply the new pixels to the main image
  if (pDraw->ucHasTransparency) // if transparency used
  {
    uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
    int x, iCount;
    pEnd = s + pDraw->iWidth;
    x = 0;
    iCount = 0; // count non-transparent pixels
    while (x < pDraw->iWidth)
    {
      c = ucTransparent - 1;
      d = usTemp;
      while (c != ucTransparent && s < pEnd)
      {
        c = *s++;
        if (c == ucTransparent) // done, stop
        {
          s--; // back up to treat it like transparent
        }
        else // opaque
        {
          *d++ = usPalette[c];
          iCount++;
        }
      }           // while looking for opaque pixels
      if (iCount) // any opaque pixels?
      {
        for (int xOffset = 0; xOffset < iCount; xOffset++)
        {
          dma_display->drawPixel(x + xOffset, y, usTemp[xOffset]); // 565 Color Format
        }
        x += iCount;
        iCount = 0;
      }
      // no, look for a run of transparent pixels
      c = ucTransparent;
      while (c == ucTransparent && s < pEnd)
      {
        c = *s++;
        if (c == ucTransparent)
          iCount++;
        else
          s--;
      }
      if (iCount)
      {
        x += iCount; // skip these
        iCount = 0;
      }
    }
  }
  else // does not have transparency
  {
    s = pDraw->pPixels;
    // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
    for (x = 0; x < pDraw->iWidth; x++)
    {
      dma_display->drawPixel(x, y, usPalette[*s++]); // color 565
    }
  }
} /* GIFDraw() */

const char* ssid     = "Hr Delwar";
const char* password = "1234567809";

void setup()
{

  Serial.begin(9600);
  delay(10);

  Serial.println("done...");

  // We start by connecting to a WiFi network

    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());


  // Module configuration
  HUB75_I2S_CFG mxconfig(
      PANEL_RES_X, // module width
      PANEL_RES_Y, // module height
      PANEL_CHAIN  // Chain length
  );

  mxconfig.gpio.e = 32;
  mxconfig.clkphase = false;
  // mxconfig.driver = HUB75_I2S_CFG::FM6126A;

  // Display Setup
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(150); //0-255
  dma_display->clearScreen();


  // create a task that will be executed in the Task1code() function, with priority 1 and executed on core 0
  // xTaskCreatePinnedToCore(
  //     Task1code, /* Task function. */
  //     "Task1",   /* name of task. */
  //     10000,     /* Stack size of task */
  //     NULL,      /* parameter of the task */
  //     1,         /* priority of the task */
  //     &Task1,    /* Task handle to keep track of created task */
  //     0);        /* pin task to core 0 */
  // delay(500);

  //create a task that will be executed in the Task2code() function, with priority 1 and executed on core 1
  // xTaskCreatePinnedToCore(
  //     Task2code, /* Task function. */
  //     "Task2",   /* name of task. */
  //     10000,     /* Stack size of task */
  //     NULL,      /* parameter of the task */
  //     1,         /* priority of the task */
  //     &Task2,    /* Task handle to keep track of created task */
  //     1);        /* pin task to core 1 */
  // delay(500);
}

//Task1code: blinks an LED every 1000 ms
void Task1code(void *pvParameters)
{
  for (;;)
  {
    
    delay(500);
  }
}

//Task2code: blinks an LED every 700 ms
void Task2code(void *pvParameters)
{
  Serial.print("Task2 running on core ");
  Serial.println(xPortGetCoreID());

  for (;;)
  {
    // animate by going through the colour wheel for the first two lines
    scrollText(wheelval);
    wheelval += 1;
    delay(200);
  }
}

void scrollText(int colorWheelOffset)
{

  String text = "** Ticket Interaction: 45              ** Ticket Replied: 30         ** Happy Customers: 09 **       "; // sample text

  const int width = 21; // width of the marquee display (in characters)

  // Loop once through the string
  for (int offset = 0; offset < text.length(); offset++)
  {
    // Construct the string to display for this iteration

    String t = "";

    for (int i = 0; i < width; i++)
    {
      t += text.charAt((offset + i) % text.length());
    }

    dma_display->fillRect(0,dma_display->height()-12, dma_display->width(), 12, 0);
    dma_display->setCursor(1, dma_display->height() - 12);
    dma_display->setTextSize(1);
    dma_display->setTextWrap(false);
    dma_display->setTextColor(myGREEN);
    dma_display->print(t);

    // Short delay so the text doesn't move too fast
    delay(200);
  }
}

// Allocate the JSON document
  //
  // Inside the brackets, 200 is the capacity of the memory pool in bytes.
  // Don't forget to change this value to match your JSON document.
  // Use arduinojson.org/v6/assistant to compute the capacity.
StaticJsonDocument<200> doc;
String sensorReadings;

void loop()
{
  if(scrollShift > scollingText.length()){
    scrollShift = 0;
  }

  drawText(wheelval);
  // scrollText(wheelval);
  wheelval += 1;

  // if ((millis() - lastTime) > timerDelay) {
    //Check WiFi connection status
    if(WiFi.status()== WL_CONNECTED){
      Serial.println("getting data...");
      sensorReadings = httpGETRequest(fluentSupport.c_str());
      // Deserialize the JSON document
      DeserializationError error = deserializeJson(doc, sensorReadings);

       // Test if parsing succeeds.
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
      } else {
        // Fetch values.
        //
        // Most of the time, you can rely on the implicit casts.
        // In other case, you can do doc["time"].as<long>();
        long interactions = doc["interactions"];
        long responses = doc["responses"];

        // lines[0] = "TI: " + String(interactions);
        // lines[1] = "TR: " + String(responses);

        // Print values.
        Serial.println(interactions);
        Serial.println(responses);
      }
    } else {
      Serial.println("WiFi Disconnected");
    }
  // }
  
  lastTime = millis();

  delay(20);
}

String httpGETRequest(const char* serverName) {
  HTTPClient http;
    
  // Your Domain name with URL path or IP address with path
  http.begin(serverName);
  
  // Send HTTP POST request
  int httpResponseCode = http.GET();
  
  String payload = "{}"; 
  
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
    Serial.println(payload);
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}
