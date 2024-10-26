// parameters.h

#ifndef PARAMETERS_H
#define PARAMETERS_H

struct Parameter {
    const char* name;   // Nom du paramètre
    float* value;       // Pointeur vers la valeur actuelle du paramètre
    float defaultValue; // Valeur par défaut
    float minValue;     // Valeur minimale
    float maxValue;     // Valeur maximale
    float increment;    // Valeur de l'incrément
};

// Déclaration des variables globales externes
extern float Setpoint;
extern float Kp, Ki, Kd;
extern Parameter parameters[];
extern const int numParameters;
extern double Input, Output;
extern float dhtTemperatureC;

extern volatile int menuIndex;
extern bool menuActive;
extern bool editing;
extern int editIndex;
extern int lastEncoderPosition;
extern unsigned long lastDebounceTime;
extern unsigned long debounceDelay;
extern int cursorY;
extern bool oledInitialized;
extern bool displayNeedsUpdate;

#endif
