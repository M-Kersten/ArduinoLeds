#ifdef __AVR__
  #include <avr/power.h>
#endif

// bibliotheek om ledstips aan te sturen
#include <Adafruit_NeoPixel.h>

// bibliotheek voor multitasking
#include <Arduino_FreeRTOS.h>

// bibliotheek voor het gebruiken van meerdere serial ports
#include <SoftwareSerial.h>

// bluetooth module serial ports
// adres van bleutooth module == 98d3:32:216a0d
// naam van bluetooth module == LetinkLED
// wachtwoord van bluetooth module == letink
#define rxPin 52
#define txPin 50
SoftwareSerial customPort(rxPin, txPin); // RX, TX

const uint8_t maxLedsInStrip = 12;
const uint8_t numSectors = 4;

// non-static variabelen
uint8_t pulseWaitTime = 10;
uint8_t brightness = 100;
int currentBuildings = 0;
int currentLed;
bool first;
uint8_t activeSector;
uint8_t finishedSector;
uint8_t ledFactor = 1;
// deze array is voor de helderheid van het licht uit de leds, zo ziet de overgang er natuurlijker uit.
byte ledGamma[] =
{
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255
};

// voorbeeldstrip voor kleuren kiezen, dit is een functie in de neopixel bibliotheek die je aanspreekt met: strip.Color(r,g,b,w)
Adafruit_NeoPixel strip = Adafruit_NeoPixel();

// ledstrips zijn afgeleid van de neopixel leds maar dan via een eigen bibliotheek, op deze manier kun je ze in arrays zetten.
Adafruit_NeoPixel sectorStrips[]
{
  // de ledstrip heeft een pin en een aantal leds toegewezen
  Adafruit_NeoPixel(30, 2)
};
Adafruit_NeoPixel pulseStrips[]
{
  //Adafruit_NeoPixel(11, 2)
};

// bepaal hier hoeveel gebouwen in de opstelling komen te staan, waar ze beginnen in de strip en hoeveel leds ze bevatten. dit is nog niet ideaal maar wel efficient qua tijd.
// startposities (led op de strip) per gebouw
uint8_t buildingsStartPositions0[] = {0, 12, 15};
uint8_t buildingsStartPositions1[] = {0, 3, 20, 10};
uint8_t buildingsStartPositions2[] = {0, 5, 15, 20, 26, 32};
uint8_t buildingsStartPositions3[] = {0, 6, 16, 20, 27};
// aantal leds per gebouw
uint8_t buildingsLeds0[] = {12, 3, 3};
uint8_t buildingsLeds1[] = {3, 4, 3, 3};
uint8_t buildingsLeds2[] = {3, 5, 5, 6, 6, 5};
uint8_t buildingsLeds3[] = {6, 6, 6, 7, 7};

// de sector taak stuurt de tafel aan, de pulse taak de ledstrips op de grond.
void FillSectorTask( void *pvParameters );
void BlinkSectorTask( void *pvParameters );
void PulseTask ( void *pvParameters );
void startSectorLed(int pin){
  sectorStrips[pin].setBrightness(100);
  sectorStrips[pin].begin();
  sectorStrips[pin].show();
}
void startPulseLed(int pin){
  pulseStrips[pin].setBrightness(100);
  pulseStrips[pin].begin();
  pulseStrips[pin].show();
}

void setup() {
  // Bluetooth communicatie openen
  pinMode(rxPin, INPUT);
  pinMode(txPin, OUTPUT);
  //customPort.begin(4800);
  Serial.begin(9600);
  Serial1.begin(9600);    
  // Serial.println("starting up...");
  // Deze trinket gebruiken we niet zover ik weet, ik laat het toch staan gezien dat de standaard is.
  // This is for Trinket 5V 16MHz, you can remove these three lines if you are not using a Trinket
  #if defined (__AVR_ATtiny85__)
    if (F_CPU == 16000000) clock_prescale_set(clock_div_1);
  #endif

  // Ledstrips aanzetten en resetten
  startSectorLed(0);
  startPulseLed(0);

  // Tasks in taskmanager laden, dit zijn void loops die tegerlijkertijd draaien.
  xTaskCreate(
   FillSectorTask
   ,  (const portCHAR *)"TafelBelichting"
   ,  128   // This stack size can be checked & adjusted by reading the Stack Highwater
   ,  NULL
   ,  2     // Prioriteit: min 0, max. 3. Lager getal gaat voor
   ,  NULL );

  xTaskCreate(
    BlinkSectorTask
    ,  (const portCHAR *)"TafelBelichting2"
    ,  128    // This stack size can be checked & adjusted by reading the Stack Highwater
    ,  NULL
    ,  2      // Prioriteit: min 0, max. 3. Lager getal gaat voor
    ,  NULL );

  xTaskCreate(
    PulseTask
    ,  (const portCHAR *) "VloerBelichting"
    ,  128   // Stack size
    ,  NULL
    ,  2     // Prioriteit: min 0, max. 3 Lager getal gaat voor
    ,  NULL );
}

// dit laten staan anders compiled het programma niet.
void loop(){}

void FillSectorTask(void *pvParameters) {
  (void) pvParameters;
  const TickType_t xDelay = 1800 / portTICK_PERIOD_MS;
   for( ;; ){    
    if(activeSector < 4){
      first = true;
      lightSector(activeSector, 2);
    }
    if(activeSector == 4){
      vTaskDelay( xDelay );
      for(int i = 0; i < 4; i++){
      fullColor(i, 0);}
      for(int i = 0; i < 15; i++){
      lightRandomBuilding(15);}
      activeSector = 0;
    }
   }
}

void BlinkSectorTask(void *pvParameters) {
  (void) pvParameters;
  for( ;; ){
    Serial.println(activeSector);    
    if(activeSector < 4){
    lightSector2(activeSector, 2);
    }
  }
}

void PulseTask(void *pvParameters) {
  (void) pvParameters;
  for (;;){
    doubleWipeRed(0, strip.Color(255,0,0), 30, 2);
  }
}

// start het belichten van een sector.
void lightSector(int sector, int wipeSpeed) {
  currentBuildings = 0;
  first = true;  
  Serial1.write(activeSector);
  if(sector == 0) {
    for(int buildings = 0; buildings < sizeof(buildingsStartPositions0); buildings++) {
      currentBuildings = buildings;
      for(uint16_t i = buildingsStartPositions0[buildings]; i < buildingsStartPositions0[buildings] + buildingsLeds0[buildings]; i++) {
        currentLed++;
        for(int j = 0; j < 256 ; j += wipeSpeed) {
          sectorStrips[sector].setPixelColor(i, sectorStrips[sector].Color(ledGamma[j],0,0, ledGamma[j]));
          sectorStrips[sector].show();
        }
      }
    }
    activeSector = 1;
  }
  if(sector == 1) {
    for(int buildings = 0; buildings < sizeof(buildingsStartPositions1); buildings++) {
      currentBuildings = buildings;
      for(uint16_t i = buildingsStartPositions1[buildings]; i < buildingsStartPositions1[buildings] + buildingsLeds1[buildings]; i++) {
        currentLed++;
        for(int j = 0; j < 256 ; j+=wipeSpeed) {
          sectorStrips[sector].setPixelColor(i, sectorStrips[sector].Color(ledGamma[j],0,0, ledGamma[j]));
          sectorStrips[sector].show();
        }
      }
    }
    activeSector = 2;
  }
  if(sector == 2) {
    for(int buildings = 0; buildings < sizeof(buildingsStartPositions2); buildings++) {
      currentBuildings = buildings;
      currentLed++;
      for(uint16_t i = buildingsStartPositions2[buildings]; i < buildingsStartPositions2[buildings] + buildingsLeds2[buildings]; i++) {
        for(int j = 0; j < 256 ; j+=wipeSpeed) {
          sectorStrips[sector].setPixelColor(i, sectorStrips[sector].Color(ledGamma[j],0,0, ledGamma[j]));
          sectorStrips[sector].show();
        }
      }
    }
    activeSector = 3;
  }
  if(sector == 3) {
    for(int buildings = 0; buildings < sizeof(buildingsStartPositions3); buildings++) {
      currentBuildings = buildings;
      currentLed++;
      for(uint16_t i = buildingsStartPositions3[buildings]; i < buildingsStartPositions3[buildings] + buildingsLeds3[buildings]; i++) {
        for(int j = 0; j < 256 ; j+=wipeSpeed) {
          sectorStrips[sector].setPixelColor(i, sectorStrips[sector].Color(ledGamma[j],0,0, ledGamma[j]));
          sectorStrips[sector].show();
        }
      }
    }    
    activeSector = 4;
    Serial1.write(activeSector);
    currentLed = 0;
    for(int i = 0; i < 4; i++) {
      fullColor(i, 0);
    }
  } 
}

void lightSector2(int sector, int blinkSpeed) {
  //fade uit met j
  // deze variabele verandert de snelheid van de blink, dit is nodig omdat de arduino trager wordt wanneer het meer informatie doorstuurt.
  ledFactor = 1 + (((sector + 1) * 12) - 7) + (currentLed / 5) * blinkSpeed;
  //Serial.println(ledFactor);
  if(sector == 0){
  for(int j = 255*100; j >= 150*100 ; j -= ledFactor) {
    for(int i = 0; i < buildingsStartPositions0[currentBuildings]; i++) {
        sectorStrips[sector].setPixelColor(i, sectorStrips[sector].Color(ledGamma[j/100],0,0, ledGamma[j/100]));
    }  
    for(int s = 0; s < sector; s++) {
      for(uint16_t i=0; i < sectorStrips[s].numPixels(); i++) {
        sectorStrips[s].setPixelColor(i, sectorStrips[s].Color(ledGamma[j/100],0,0, ledGamma[j/100]));
      }
    sectorStrips[s].show();
    }
  }
  }
  if(sector == 1){
  for(int j = 255*100; j >= 150*100 ; j -= ledFactor) {
    for(int i = 0; i < buildingsStartPositions1[currentBuildings]; i++) {
        sectorStrips[sector].setPixelColor(i, sectorStrips[sector].Color(ledGamma[j/100],0,0, ledGamma[j/100]));
    }  
    for(int s = 0; s < sector; s++) {
      for(uint16_t i=0; i < sectorStrips[s].numPixels(); i++) {
        sectorStrips[s].setPixelColor(i, sectorStrips[s].Color(ledGamma[j/100],0,0, ledGamma[j/100]));
      }
    sectorStrips[s].show();
    }
  }
  }
  if(sector == 2){
  for(int j = 255*100; j >= 150*100 ; j -= ledFactor) {
    for(int i = 0; i < buildingsStartPositions2[currentBuildings]; i++) {
        sectorStrips[sector].setPixelColor(i, sectorStrips[sector].Color(ledGamma[j/100],0,0, ledGamma[j/100]));
    }  
    for(int s = 0; s < sector; s++) {
      for(uint16_t i=0; i < sectorStrips[s].numPixels(); i++) {
        sectorStrips[s].setPixelColor(i, sectorStrips[s].Color(ledGamma[j/100],0,0, ledGamma[j/100]));
      }
    sectorStrips[s].show();
    }
  }
  }
  if(sector == 3){
  for(int j = 255*100; j >= 150*100 ; j -= ledFactor) {
    for(int i = 0; i < buildingsStartPositions3[currentBuildings]; i++) {
        sectorStrips[sector].setPixelColor(i, sectorStrips[sector].Color(ledGamma[j/100],0,0, ledGamma[j/100]));
    }  
    for(int s = 0; s < sector; s++) {
      for(uint16_t i=0; i < sectorStrips[s].numPixels(); i++) {
        sectorStrips[s].setPixelColor(i, sectorStrips[s].Color(ledGamma[j/100],0,0, ledGamma[j/100]));
      }
    sectorStrips[s].show();
    }
  }
  }
  
  //fade in met j
  for(int j = 150*100; j < 256*100 ; j += ledFactor) {
    //check bij welk huis het licht is (uit andere task, kijken of dit past)
    if(sector == 0){
    for(int i = 0; i < buildingsStartPositions0[currentBuildings]; i++) {
        sectorStrips[sector].setPixelColor(i, sectorStrips[sector].Color(ledGamma[j/100],0,0, ledGamma[j/100]));
    }
    //blink de vorige led aan    
    for(int s = 0; s < sector; s++){
      for(uint16_t i = 0; i < sectorStrips[s].numPixels(); i++) {
        sectorStrips[s].setPixelColor(i, sectorStrips[s].Color(ledGamma[j/100],0,0, ledGamma[j/100]));
      }
      sectorStrips[s].show();
    }
  }
  
  if(sector == 1){
    for(int i = 0; i < buildingsStartPositions1[currentBuildings]; i++) {
        sectorStrips[sector].setPixelColor(i, sectorStrips[sector].Color(ledGamma[j/100],0,0, ledGamma[j/100]));
    }
    //blink de vorige led aan    
    for(int s = 0; s < sector; s++){
      for(uint16_t i = 0; i < sectorStrips[s].numPixels(); i++) {
        sectorStrips[s].setPixelColor(i, sectorStrips[s].Color(ledGamma[j/100],0,0, ledGamma[j/100]));
      }
      sectorStrips[s].show();
    }
  }
  
  if(sector == 2){
    for(int i = 0; i < buildingsStartPositions2[currentBuildings]; i++) {
        sectorStrips[sector].setPixelColor(i, sectorStrips[sector].Color(ledGamma[j/100],0,0, ledGamma[j/100]));
    }
    //blink de vorige led aan    
    for(int s = 0; s < sector; s++){
      for(uint16_t i = 0; i < sectorStrips[s].numPixels(); i++) {
        sectorStrips[s].setPixelColor(i, sectorStrips[s].Color(ledGamma[j/100],0,0, ledGamma[j/100]));
      }
      sectorStrips[s].show();
    }
  }
  
  if(sector == 3){
    for(int i = 0; i < buildingsStartPositions3[currentBuildings]; i++) {
        sectorStrips[sector].setPixelColor(i, sectorStrips[sector].Color(ledGamma[j/100],0,0, ledGamma[j/100]));
    }
    //blink de vorige led aan    
    for(int s = 0; s < sector; s++){
      for(uint16_t i = 0; i < sectorStrips[s].numPixels(); i++) {
        sectorStrips[s].setPixelColor(i, sectorStrips[s].Color(ledGamma[j/100],0,0, ledGamma[j/100]));
      }
      sectorStrips[s].show();
    }
  }
 }
}

// zet willekeurig gebouw aan in het rood
void lightRandomBuilding(uint8_t fadeSpeed) {
  uint8_t randSector = random(0, 3);
  lightBuilding(randSector, -1, fadeSpeed);
}

// zet een specifiek gebouw aan in het rood
void lightBuilding(uint8_t sector, int building, uint8_t fadeSpeed) {
  if(sector == 0) {
    if(building == -1) {
      building = random(0, sizeof(buildingsStartPositions0));
    }
    uint16_t ledsToLight = buildingsLeds0[building];
      for(int j = 255*10; j >= 0 ; j-= fadeSpeed * ledsToLight){
        for(int i = 0; i < buildingsLeds0[building]; i++) {
          sectorStrips[sector].setPixelColor(buildingsStartPositions0[building]+i, ledGamma[j/10],0,0,ledGamma[j/10]);
          sectorStrips[sector].show();
        }
      }
    }
  if(sector == 1) {
    if(building == -1) {
      building = random(0, sizeof(buildingsStartPositions1));
    }
    uint16_t ledsToLight = buildingsLeds1[building];
        for(int j = 255*10; j >= 0 ; j-= fadeSpeed * ledsToLight){
          for(int i = 0; i < buildingsLeds1[building]; i++) {
            sectorStrips[sector].setPixelColor(buildingsStartPositions1[building]+i, ledGamma[j/10],0,0,ledGamma[j/10]);
            sectorStrips[sector].show();
          }
      }
  }
  if(sector == 2) {
    if(building == -1) {
      building = random(0, sizeof(buildingsStartPositions2));
    }
    uint16_t ledsToLight = buildingsLeds2[building];
        for(int j = 255*10; j >= 0 ; j-= fadeSpeed * ledsToLight) {
          for(int i = 0; i < buildingsLeds2[building]; i++) {
            sectorStrips[sector].setPixelColor(buildingsStartPositions2[building]+i, ledGamma[j/10],0,0,ledGamma[j/10]);
            sectorStrips[sector].show();
          }
      }
  }
  if(sector == 3) {
    if(building == -1) {
      building = random(0, sizeof(buildingsStartPositions3));
    }
    uint16_t ledsToLight = buildingsLeds3[building];
        for(int j = 255; j >= 0 ; j-= fadeSpeed * ledsToLight) {
          for(int i = 0; i < buildingsLeds3[building]; i++) {
           sectorStrips[sector].setPixelColor(buildingsStartPositions3[building]+i, ledGamma[j],0,0,ledGamma[j]);
           sectorStrips[sector].show();
          }
      }
  }
}

// dit is de pulse op de grond die vloeiend voortbeweegt in het rood
void doubleWipeRed(int sector, uint32_t c, uint8_t pulseSpeed, int spread)
{
  for(uint16_t i = 0; i < pulseStrips[sector].numPixels()+spread; i++) {
      for(int j = 0; j < 256*10 ; j += pulseSpeed) {
        pulseStrips[sector].setPixelColor(i, pulseStrips[sector].Color(ledGamma[j/10],0,0, ledGamma[j/10]));
        pulseStrips[sector].setPixelColor(i-spread, pulseStrips[sector].Color(ledGamma[(j/10)*-1],0,0, ledGamma[(j/10)*-1]));
        pulseStrips[sector].show();
      }
    }
}

// omstebeurt opvullen van een ledstrip in rood
void colorWipe(int sector, uint8_t wipeSpeed) {
  for(uint16_t i=0; i < sectorStrips[sector].numPixels(); i++) {
    for(int j = 0; j < 256 ; j+=wipeSpeed) {
      sectorStrips[sector].setPixelColor(i, sectorStrips[sector].Color(ledGamma[j],0,0, ledGamma[j]));
      sectorStrips[sector].show();
    }
  }
}

//leds gaan om de zoveelste aan in verplaatsen zich naar voren.
void theaterCrawl(int pulseStrip, uint32_t c, uint8_t wait, uint8_t fadeWait, int cycles) {
  for (int j=0; j < cycles; j++) {
    for (int q=0; q < 9; q++) {
      for (uint16_t i=0; i < sectorStrips[pulseStrip].numPixels(); i=i+9) {
        for(int j = 0; j < 256 ; j+=4) {
          sectorStrips[pulseStrip].setPixelColor(i+q-1,   strip.Color(ledGamma[j],0,0,     ledGamma[j] ));
          sectorStrips[pulseStrip].setPixelColor(i+q,     strip.Color(ledGamma[j],0,0,     ledGamma[j] ));
          sectorStrips[pulseStrip].setPixelColor(i+q+1,   strip.Color(ledGamma[j],0,0,     ledGamma[j] ));
          sectorStrips[pulseStrip].show();
        }
      }
      for (uint16_t i=0; i < sectorStrips[pulseStrip].numPixels(); i=i+9) {
        for(int j = 255; j >= 0 ; j-=4) {
             //turn every i'th pixel on
          sectorStrips[pulseStrip].setPixelColor(i+q-1,   strip.Color(ledGamma[j],0,0,     ledGamma[j] ));
          sectorStrips[pulseStrip].setPixelColor(i+q,   strip.Color(ledGamma[j],0,0,     ledGamma[j] ));
          sectorStrips[pulseStrip].setPixelColor(i+q+1,   strip.Color(ledGamma[j],0,0,     ledGamma[j] ));
          sectorStrips[pulseStrip].show();
        }
      }
    }
  }
}
// knippert de hele strip rood
void blinkRed(int sector, int cycles, uint8_t blinkSpeed, bool startFull) {
  if(startFull) {
    //geleidelijk naar zwart
    for(int j = 255*10; j >= 150*10 ; j -= blinkSpeed) {
        for(uint16_t i=0; i < sectorStrips[sector].numPixels(); i++) {
            sectorStrips[sector].setPixelColor(i, sectorStrips[sector].Color(ledGamma[j/10],0,0, ledGamma[j/10] ) );
        }
        sectorStrips[sector].show();
    }
  }
  //van zwart naar kleur en terug.
  if(sector == -1) {
    for(int s = 0; s < activeSector; s++) {
      for(int j = 150*10; j < 256*10 ; j += blinkSpeed) {
        for(uint16_t i=0; i < sectorStrips[s].numPixels(); i++) {
          sectorStrips[s].setPixelColor(i, sectorStrips[s].Color(ledGamma[j/10],0,0, ledGamma[j/10]));
        }
        sectorStrips[s].show();
      }
      for(int j = 255*10; j >= 150*10 ; j -= blinkSpeed) {
        for(uint16_t i=0; i < sectorStrips[s].numPixels(); i++) {
          sectorStrips[s].setPixelColor(i, sectorStrips[s].Color(ledGamma[j/10],0,0, ledGamma[j/10]));
        }
      sectorStrips[s].show();
      }
    }
  }
  else{
    for(int i = 0; i < cycles; i++) {
      for(int j = 150*10; j < 256*10 ; j += blinkSpeed) {
        for(uint16_t i=0; i < sectorStrips[sector].numPixels(); i++) {
          sectorStrips[sector].setPixelColor(i, sectorStrips[sector].Color(ledGamma[j/10],0,0, ledGamma[j/10]));
        }
        sectorStrips[sector].show();
      }
      for(int j = 255*10; j >= 150*10 ; j -= blinkSpeed){
        for(uint16_t i=0; i < sectorStrips[sector].numPixels(); i++) {
          sectorStrips[sector].setPixelColor(i, sectorStrips[sector].Color(ledGamma[j/10],0,0, ledGamma[j/10]));
        }
        sectorStrips[sector].show();
      }
    }
  }
}

//fullcolor voor het testen van kleuren
void fullColor(int sector, uint32_t c) {
  if(sector == -1){
      for(int j = 0; j < 4; j++){
        for(uint16_t i=0; i < sectorStrips[j].numPixels(); i++) {
          sectorStrips[j].setPixelColor(i, c);
        }
      }
  }
  else{
    for(uint16_t i=0; i < sectorStrips[sector].numPixels(); i++) {
        sectorStrips[sector].setPixelColor(i, c);
    }
  }
  sectorStrips[sector].show();
}
