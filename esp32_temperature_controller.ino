
#define DEBUG true

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <math.h>
#include <PID_v2.h>
#include <Preferences.h>
#include <ESP32RotaryEncoder.h>
#include "menu.h"
#include "parameters.h"  // Inclusion de parameters.h

// https://www.upesy.fr/blogs/tutorials/esp32-pinout-reference-gpio-pins-ultimate-guide

// Stockage en mémoire
Preferences preferences;

// OLED display size
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C // Adresse I2C de l'OLED
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// DHT22 sensor settings
#define DHTPIN 18
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// PWM settings
const int mosfetPin = 5;
const int pwmChannel = 0;
const int pwmFrequency = 100; // with 5000Hz I had interference and poor thermistor reading !
const int pwmResolution = 8;

// Thermistor settings
int thermistorPin = 15;
float divisorValue = 8400.0; // recalibrated using DHT22
float Bvalue = 3920.0; // extracted from datsheet values : https://www.ovenind.com/pdf/datasheets/DS-TR136.pdf

// Déclaration des paramètres
float Setpoint;
float Kp, Ki, Kd;
float resistorValue;
double Input, Output;
float dhtTemperatureC;

// Tableau unique contenant tous les paramètres
Parameter parameters[] = {
    {"Setpoint", &Setpoint, 32.0, 20.0, 40.0, 0.1},
    {"Kp", &Kp, 40.0, 0.0, 100.0, 0.1},
    {"Ki", &Ki, 8.0, 0.0, 50.0, 0.1},
    {"Kd", &Kd, 0.0, 0.0, 10.0, 0.1},
    {"resistorValue", &resistorValue, 15000.0, 1000.0, 30000.0, 500.0}
};

// Nombre total de paramètres
const int numParameters = sizeof(parameters) / sizeof(Parameter);

// PID controller
PID_v2 myPID(Kp, Ki, Kd, PID::Direct);

// Déclaration des broches pour l'encodeur
#define encoder0PinA  4  // Pin A de l'encodeur
#define encoder0PinB  19 // Pin B de l'encodeur
#define encoder0Press 23  // Pin du bouton de l'encodeur

RotaryEncoder rotaryEncoder(encoder0PinA, encoder0PinB, encoder0Press);

unsigned long lastPIDTime = 0;
unsigned long lastDisplayTime = 0;
const long PIDInterval = 500;
const long DisplayInterval = 500;
bool displayNeedsUpdate=true;

// Function declarations
void setupPWM();
void setupOLED();
void setupPID();
void readThermistor();
void updatePID();
void debugPrint();

bool initSuccess = true;

void setup() {
  Serial.begin(9600);
  debugPrint("Init components setup");

  // Setup OLED
  setupOLED();


  debugPrint("Get parameters");
  preferences.begin("my-app", false);
  for (int i = 0; i < numParameters; i++) {
    *(parameters[i].value) = preferences.getFloat(parameters[i].name, parameters[i].defaultValue);
  }

  debugPrint("Init PID");
  myPID.SetTunings(Kp, Ki, Kd);
  myPID.SetOutputLimits(0, 255);
  readThermistor();
  myPID.Start(Input, 0, Setpoint);

  debugPrint("Setup Encoder");

    pinMode(encoder0Press, INPUT_PULLUP);  // Assurez-vous que la résistance pull-up interne est activée pour le bouton
    rotaryEncoder.setEncoderType(EncoderType::HAS_PULLUP);
    rotaryEncoder.setBoundaries(0, numParameters, true); // Limite le range de 0 à numParameters
    rotaryEncoder.onTurned(&knobCallback);
    rotaryEncoder.onPressed(&buttonCallback);
    rotaryEncoder.begin();

  // rotaryEncoder.setEncoderType(EncoderType::HAS_PULLUP);
  // rotaryEncoder.setBoundaries(0, 5, true);
  // rotaryEncoder.onTurned(&knobCallback);
  // rotaryEncoder.onPressed(&buttonCallback);
  // rotaryEncoder.begin();
  debugPrint("Encoder OK");

  setupPWM();

  debugPrint("DHT22 sensor setup");
  dht.begin();
  float temperature = dht.readTemperature();
  if (isnan(temperature)) {
    debugPrint("DHT sensor failed!");
  } else {
    debugPrint("DHT sensor OK");
  }

  debugPrint("Init procedure OK");
  delay(1000);


   updateDisplay();
}


void loop() {
  
  if (!initSuccess) return;
  unsigned long currentTime = millis();

  // Lire la thermistance et mettre à jour le PID, sauf si en mode d'édition
  if (!menuActive || (menuActive && !editing)) {
    
    if (currentTime - lastPIDTime >= PIDInterval) {
      lastPIDTime = currentTime;
      readThermistor();
      updatePID();
      dhtTemperatureC = dht.readTemperature();
    }
  }

  // Gérer l'affichage et le menu
  if (menuActive) {
    if (editing) {
      // Afficher le paramètre en cours d'édition
       if (displayNeedsUpdate) {
      showSingleParameter(editIndex);
      displayNeedsUpdate = false;
       }
    } else {
      // Afficher le menu
       if (displayNeedsUpdate) {
      showMenu();
      displayNeedsUpdate = false;
       }
    }
  } else {
    // Affichage principal lorsque le menu est inactif
    if (currentTime - lastDisplayTime >= DisplayInterval) {
      lastDisplayTime = currentTime;
      updateDisplay();
    }
  }
}

void debugPrint(const char* message) {
  if (DEBUG) {
    Serial.println(message);
  }

  if(oledInitialized) {
      displayTextLine(message);
      delay(500);
  }
}


void setupPWM() {
  if (DEBUG) {
    Serial.println("Init PWM");
  }
  ledcAttach(mosfetPin, pwmFrequency, pwmResolution);
}

void setupOLED() {
 
debugPrint("Init OLED");

 bool error= display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS); 
 
  if (error) {
        // Essayer d'écrire puis lire quelque chose sur l'écran pour vérifier
        display.clearDisplay();
        display.drawPixel(0, 0, SSD1306_WHITE); // Dessiner un pixel en haut à gauche
        display.display();
        delay(10); // Attendre un instant pour donner le temps d'afficher

        // Essayer de vérifier si l'écran répond correctement en lisant un état
        if (Wire.requestFrom(OLED_ADDRESS, 1) == 1) {
            oledInitialized = true;
            debugPrint("OLED setup successful");
           
        } else {
            oledInitialized = false;
            debugPrint("OLED not detected!");
        }
    } else {
        oledInitialized = false;
        debugPrint("OLED initi failed!");
    }

}

void setupPID() {
  if (DEBUG) {
    Serial.println("Init PID");
  }
  myPID.SetOutputLimits(0, 255);
  myPID.Start(25, 0, Setpoint);
}


#define NUM_SAMPLES 10  // Nombre de mesures à utiliser pour la moyenne

float tempSamples[NUM_SAMPLES] = {0}; // Tableau pour stocker les dernières mesures
int sampleIndex = 0;                  // Index pour insérer la prochaine mesure
bool samplesFilled = false;           // Indicateur que le tableau est rempli

void readThermistor() {
    int adcValue = analogRead(thermistorPin);

    float voltage = adcValue * (3.3 / 4095.0);
    
    if (voltage != 0) {
        float thermistorResistance = divisorValue * (3.3 / voltage - 1.0);

       // Serial.println(thermistorResistance);
        
        if (thermistorResistance > 0) {
            float temp = 1.0 / (log(thermistorResistance / resistorValue) / Bvalue + 1.0 / 298.15) - 273.15;
            
            if (temp>0) {
            Input=temp;
        }
              // // Ajouter la nouvelle mesure dans le tableau
              // tempSamples[sampleIndex] = temp;
              // sampleIndex = (sampleIndex + 1) % NUM_SAMPLES;  // Incrémente l'index circulaire

              // // Vérifie si le tableau est rempli au moins une fois
              // if (sampleIndex == 0) {
              //     samplesFilled = true;
              // }

              // // Calculer la moyenne des mesures
              // float sum = 0;
              // int count = samplesFilled ? NUM_SAMPLES : sampleIndex;  // Si tableau non plein, utilise sampleIndex
              // for (int i = 0; i < count; i++) {
              //     sum += tempSamples[i];
              // }
              // Input = sum / count;  // Met à jour Input avec la moyenne
        }
    } else {
        if (DEBUG) {
            Serial.println("Thermistor: Tension est 0, veuillez vérifier !");
        }
    }
}




void updatePID() {
  Output = myPID.Run(Input);
  ledcWrite(mosfetPin, Output);
  if (DEBUG) {
    Serial.print("Setpoint: ");
    Serial.print(Setpoint);
    Serial.print(" Input: ");
    Serial.print(Input);
    Serial.print(" Output: ");
    Serial.println(Output);
  }
}

