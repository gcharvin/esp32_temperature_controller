#include "menu.h"
#include "parameters.h"


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define LINE_HEIGHT 8 // Hauteur de la ligne (taille de texte 1 = 8 pixels)
#define OLED_ADDRESS 0x3C // Adresse I2C de l'OLED

// Déclaration des broches pour l'encodeur
#define encoder0PinA  4  // Pin A de l'encodeur
#define encoder0PinB  19 // Pin B de l'encodeur
#define encoder0Press 23  // Pin du bouton de l'encodeur

volatile int menuIndex = 0;
bool menuActive = false;
bool editing = false;
int editIndex = -1;
int lastEncoderPosition = 0;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 20;
int cursorY = 0; // Position verticale du curseur sur l'écran
bool oledInitialized=false;

void knobCallback(long value) {
    unsigned long currentTime = millis();

    // if ((currentTime - lastDebounceTime) > debounceDelay) {
    //     lastDebounceTime = currentTime;

         int delta = value - lastEncoderPosition;
         lastEncoderPosition = value;

         if (delta==numParameters){
            delta=-1;
           }
         if (delta==-numParameters){
            delta=1;
           }
         

        if (editing) {
            adjustParameter(editIndex, delta > 0 ? 1 : -1);
            showSingleParameter(editIndex);
        } else if (menuActive) {
            menuIndex = value; //+= delta > 0 ? 1 : -1;

            Serial.println(menuIndex);

            //menuIndex = constrain(menuIndex, 0, numParameters);
            showMenu();
        }
   // }
}

void buttonCallback(unsigned long duration) {

  if (duration>1000) {
    return; // remove wrong calls to the function ; encoder bug
  }

  //Serial.println(duration);

    unsigned long currentTime = millis();

    if ((currentTime - lastDebounceTime) > debounceDelay) {
        lastDebounceTime = currentTime;

        // Indiquer que le bouton vient d'être pressé
       // buttonRecentlyPressed = true;

        if (!menuActive) {
            menuActive = true;
            menuIndex=0;
            showMenu();
            delay(100);
            showMenu();
        } else {
            if (!editing) {
                if (menuIndex == 0) {
                    menuActive = false;
                } else {
                    editing = true;
                    editIndex = menuIndex - 1;
                    showSingleParameter(editIndex);
                }
            } else {
                lastEncoderPosition = menuIndex;
                editing = false;
                editIndex = -1;
                showMenu();
            }
        }
    }
}

void showMenu() {
    int fontHeight = 8;
    int visibleItems = SCREEN_HEIGHT / fontHeight;
    int topItem = 0;

//Serial.println(menuIndex);

    // Ajuster topItem pour s'assurer que menuIndex est toujours visible
    if (menuIndex >= visibleItems) {
        topItem = menuIndex - visibleItems + 1;
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);

    // Afficher les éléments du menu
    for (int i = 0; i < visibleItems; i++) {
        int itemIndex = topItem + i;

        if (itemIndex > numParameters) break;

        int y = i * fontHeight;

        if (itemIndex == menuIndex) {
            display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            display.fillRect(0, y, SCREEN_WIDTH, fontHeight, SSD1306_WHITE);
        } else {
            display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
        }

        if (itemIndex == 0) {
            display.setCursor(0, y);
            display.println("Back");
        } else {
            display.setCursor(0, y);
            display.print(parameters[itemIndex - 1].name);
            display.setCursor(SCREEN_WIDTH - 50, y);
            display.print(": ");
            display.println(*(float*)parameters[itemIndex - 1].value, 2);
        }
    }

    display.display();
}

void showSingleParameter(int index) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("Set ");
    display.print(parameters[index].name);
    display.println(" ?");
    display.setTextSize(2);
    display.setCursor(0, 20);
    display.print(*(parameters[index].value), 2);
    display.display();
}

void applyUpdatedParameters() {
    // Forcer la sortie à zéro
    Output = 0;

    // Réinitialiser les paramètres PID
    myPID.SetTunings(Kp, Ki, Kd);
    myPID.SetOutputLimits(-1, 0); // artificially resests the integrator
    myPID.SetOutputLimits(0, 255);
   //myPID.SetMode(MANUAL);
   // myPID.SetMode(AUTOMATIC); // reini
    // Redémarrer le PID avec les paramètres actuels
    myPID.Start(Input, Output, Setpoint);

    // Afficher un message pour confirmer la réinitialisation
    Serial.println("PID réinitialisé avec les nouveaux paramètres.");
    Serial.print("Kp: "); Serial.print(Kp);
    Serial.print(", Ki: "); Serial.print(Ki);
    Serial.print(", Kd: "); Serial.print(Kd);
    Serial.print(", Setpoint: "); Serial.println(Setpoint);

    // Vérifier si Output est bien nul
    if (Output == 0) {
        Serial.println("Sortie PID réinitialisée à zéro.");
    } else {
        Serial.print("Avertissement : Sortie PID non nulle (");
        Serial.print(Output);
        Serial.println(")");
    }
}


void adjustParameter(int index, int direction) {
    if (index >= 0 && index < numParameters) {
        // Ajuster la valeur pointée par le paramètre
        float newValue = *(parameters[index].value) + direction * parameters[index].increment;
        newValue = constrain(newValue, parameters[index].minValue, parameters[index].maxValue);
        *(parameters[index].value) = newValue;

        // Sauvegarder la nouvelle valeur dans les préférences
        preferences.putFloat(parameters[index].name, newValue);

        // Si le paramètre modifié est l'un des paramètres PID, appliquer les changements
        if (parameters[index].name == "Kp" || parameters[index].name == "Ki" || 
            parameters[index].name == "Kd" || parameters[index].name == "Setpoint") {
            applyUpdatedParameters();
        }
    }
}

// void updateDisplay() {
//   display.clearDisplay();
//   display.setTextSize(2);
//   display.setTextColor(SSD1306_WHITE);
//   display.setCursor(0, 0);
//   display.print(Input, 1);
//  // float dhtTemperatureC = dht.readTemperature();
//   display.setCursor(70, 0);
//   if (isnan(dhtTemperatureC)) {
//     display.print("Err");
//   } else {
//     display.print(dhtTemperatureC, 1);
//   }
//   display.setCursor(0, 25);
//   display.print("Set: ");
//   display.print(Setpoint, 1);
//   int outputPercentage = (int)(Output / 255.0 * 100);
//   display.setCursor(0, 50);
//   display.print("PWM: ");
//   display.print(outputPercentage);
//   display.println("%");
//   display.display();
// }

void updateDisplay(bool error) {
      display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  if (error) {
  display.print("Err"); 
  } else {
  display.print(Input, 1);
  }

 // float dhtTemperatureC = dht.readTemperature();
  display.setCursor(70, 0);
  if (isnan(dhtTemperatureC)) {
    display.print("Err");
  } else {
    display.print(dhtTemperatureC, 1);
  }
  display.setCursor(0, 25);
  display.print("Set: ");
  display.print(Setpoint, 1);

  int outputPercentage;

   if (error) {
  outputPercentage=0;  
   } else {
  outputPercentage = (int)(Output / 255.0 * 100);
   }

  display.setCursor(0, 50);
  display.print("PWM: ");
  display.print(outputPercentage);
  display.println("%");
  display.display();
}

void displayTextLine(const char* text) {
    // Vérifie si on dépasse la hauteur de l'écran
    if (cursorY >= SCREEN_HEIGHT) {
        cursorY = 0;             // Réinitialise la position du curseur
    }

     if (cursorY ==0) {
        display.clearDisplay();  // Efface l'écran
    }

    // Affiche le texte à la position actuelle du curseur
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, cursorY);
    display.println(text);
    display.display(); // Mise à jour de l'affichage

    // Passe à la ligne suivante
    cursorY += LINE_HEIGHT;
}

bool isOLEDConnected() {
   byte error, address;
  int nDevices;

  nDevices = 0;
  for (address = 1; address < 127; address++) { // Les adresses I2C sont sur 7 bits (1-127)
    Wire.beginTransmission(address); // Commence la transmission vers cette adresse
    error = Wire.endTransmission();  // Fin de la transmission et retour du code d'erreur

    if (error == 0) {
      Serial.print("Périphérique I2C trouvé à l'adresse 0x");
      if (address < 16)
        Serial.print("0"); // Ajouter un 0 pour les adresses < 0x10
      Serial.println(address, HEX); // Afficher l'adresse en hexadécimal
      nDevices++;
    } else if (error == 4) {
      Serial.print("Erreur inconnue à l'adresse 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  }
}