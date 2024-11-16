#ifndef MENU_H
#define MENU_H

#include <Adafruit_SSD1306.h>
#include <ESP32RotaryEncoder.h>
#include "parameters.h"  // Inclusion de parameters.h pour accéder à la structure Parameter
#include <Preferences.h>
#include <PID_v2.h> 

// Déclaration des paramètres externes qui seront utilisés dans le menu
extern float Setpoint;
extern float Kp, Ki, Kd;
extern double Input, Output;
extern const int numParameters;
extern Adafruit_SSD1306 display;
extern RotaryEncoder rotaryEncoder;
extern Preferences preferences;
extern PID_v2 myPID;

// Prototypes de fonctions
void knobCallback(long value);
void buttonCallback(unsigned long duration);
void showMenu();
void showSingleParameter(int index);
void adjustParameter(int index, int direction);
void applyUpdatedParameters();
void updateDisplay(bool error);
void displayTextLine(const char* text);
bool isOLEDConnected();

#endif
