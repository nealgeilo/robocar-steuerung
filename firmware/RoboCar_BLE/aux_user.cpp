/*
 * ===================================================================
 *  aux_user.cpp – HIER DÜRFT IHR EURE EIGENEN FUNKTIONEN EINBAUEN
 * ===================================================================
 *
 * Ideen: Kicker (Servo), Licht, Greifarm, Hupe, Sensoren ...
 *
 * Die App hat vier zusätzliche Kanäle AUX 1 bis AUX 4. Jeder Kanal liefert
 * einen Wert von 0 bis 255:
 *   - Taster:   0 = losgelassen, 255 = gedrückt
 *   - Schalter: 0 = aus, 255 = an
 *   - Schieber: 0 .. 255 stufenlos
 * Welcher Kanal Taster, Schalter oder Schieber ist, stellt ihr in der App
 * ein (Datei docs/controls.js, ganz oben).
 *
 * Immer wenn sich ein Wert ändert, ruft das Hauptprogramm onAux(kanal, wert)
 * auf. Dort schreibt ihr hinein, was passieren soll.
 *
 * Freie Pins für eure Erweiterungen:  GPIO 4, 16, 17, 18, 19, 21, 22, 23
 * Die Motor-Pins (14, 25, 26, 27, 32, 33) und die LED (2) sind belegt!
 *
 * Achtung Strom: Ein ESP32-Pin liefert nur 3,3 V und höchstens ca. 20 mA.
 * Das reicht für eine LED mit Vorwiderstand, aber NICHT für Servos oder
 * Motoren. Die brauchen eine eigene 5-V-Versorgung, vom ESP32 kommt nur
 * das Steuersignal. Für den Servo ist das ein Step-down-Modul am Akku,
 * das vorher auf 5,0 V eingestellt wird (Anleitung "Programm anpassen
 * und flashen", Kapitel Servo-Kicker). GND (Minus) immer verbinden!
 *
 * Unten stehen zwei fertige Beispiele. Zum Einschalten in der Zeile
 * "#define BEISPIEL_... 0" die 0 durch eine 1 ersetzen und hochladen.
 *
 * Tipp: Im Seriellen Monitor (Werkzeuge > Serieller Monitor, 115200 Baud)
 * seht ihr, welche AUX-Werte ankommen.
 *
 * Wichtig: Nirgends delay() verwenden! Sonst reagiert das Auto während
 * der Wartezeit nicht mehr auf das Handy.
 */


#include "aux_user.h"
/*
 * aux_user.cpp – Zwei Klapparme mit Kick
 *
 * AUX 1 = Ausklappen  (beide Arme 0° -> 90°)
 * AUX 2 = Einklappen  (beide Arme 90° -> 0°)
 * AUX 3 = Kick links  (nur wenn ausgeklappt: 90° -> 180° -> 90°)
 * AUX 4 = Kick rechts (nur wenn ausgeklappt: 90° -> 180° -> 90°)
 *
 * Servo links: Signal an GPIO 4 (D4), Servo rechts: Signal an GPIO 18 (D18).
 * Strom (rot/braun) vom Step-down (5,0 V), OUT- mit GND am ESP32 verbinden.
 */

const int PIN_LINKS  = 4;
const int PIN_RECHTS = 18;


const bool RECHTS_SPIEGELN = false; //nur auf true setzen, wenn der der Servo "falsch" eingebaut ist, also um 180 grad drehen

//Alle drei Winkel-Positionen
const int GRAD_EIN  = 0;    // eingeklappt
const int GRAD_AUS  = 90;   // ausgeklappt                                                                                            //Veränderbare Werte
const int GRAD_KICK = 180;  // Kick

const unsigned long KICK_DAUER_MS = 250;  // so lange bleibt der Arm in der "Kick-Stellung"

// Pulslänge für 0° und 180°. Falls der Servo 180° nicht erreicht, MAX erhöhen
const int PULS_MIN_US = 500;
const int PULS_MAX_US = 2400;

bool armeAus = false; //Sind die Arme ausgefahren?
bool kickLinks = false; // Kickzustand?
bool kickRechts = false; // Kickzustand?
unsigned long kickLinksStart = 0;
unsigned long kickRechtsStart = 0;
uint8_t letzterWert[5] = {0, 0, 0, 0, 0};  // Index 1..4 = AUX 1..4 

void servoGrad(int pin, int grad) {
  grad = constrain(grad, 0, 180); // Begrenzt den Winkel auf 180 grad
  int pulsUs = PULS_MIN_US + (long)grad * (PULS_MAX_US - PULS_MIN_US) / 180;
  int duty = (long)pulsUs * 16383 / 20000; 
  ledcWrite(pin, duty);
}

void servoLinks(int grad) {
  servoGrad(PIN_LINKS, grad);
}

void servoRechts(int grad) {
  if (RECHTS_SPIEGELN) grad = 180 - grad;
  servoGrad(PIN_RECHTS, grad);
}

void userSetup() {
  ledcAttach(PIN_LINKS, 50, 14);   
  ledcAttach(PIN_RECHTS, 50, 14); // 50 Hz (alle 20 ms ein Puls)
  servoLinks(GRAD_EIN);
  servoRechts(GRAD_EIN);
}

void onAux(uint8_t kanal, uint8_t wert) {
  Serial.printf("AUX %u = %u\n", kanal, wert);
  if (kanal < 1 || kanal > 4) return;


  bool gedrueckt = (wert > 127) && (letzterWert[kanal] <= 127); //Regaiert bei Drücken, beim Loslassen wird die Funktion sofort beendet
  letzterWert[kanal] = wert;
  if (!gedrueckt) return; 

  if (kanal == 1) {            // Ausklappen
    armeAus = true;
    kickLinks = false;
    kickRechts = false;
    servoLinks(GRAD_AUS);
    servoRechts(GRAD_AUS);
  }
  else if (kanal == 2) {       // Einklappen
    armeAus = false;
    kickLinks = false;
    kickRechts = false;
    servoLinks(GRAD_EIN);
    servoRechts(GRAD_EIN);
  }
  else if (kanal == 3) {       // Kick links
    if (armeAus && !kickLinks) {
      servoLinks(GRAD_KICK);
      kickLinks = true;
      kickLinksStart = millis();
    }
  }
  else if (kanal == 4) {       // Kick rechts
    if (armeAus && !kickRechts) {
      servoRechts(GRAD_KICK);
      kickRechts = true;
      kickRechtsStart = millis();
    }
  }
}

void userLoop() { //Checkt ob sich der der Servo in "Kick" position befinden und ggf. zurücksetzt
  if (kickLinks && millis() - kickLinksStart >= KICK_DAUER_MS) { 
    servoLinks(GRAD_AUS);
    kickLinks = false;
  }
  if (kickRechts && millis() - kickRechtsStart >= KICK_DAUER_MS) {
    servoRechts(GRAD_AUS);
    kickRechts = false;
  }
}

void onFailsafe() {
  //Wenn ein Kick beendet wurde, dann wird der Servo auf 90 Grad zurükcgesetzt und den Tastenwert auf 0
  if (kickLinks)  servoLinks(GRAD_AUS);
  if (kickRechts) servoRechts(GRAD_AUS);
  kickLinks = false;
  kickRechts = false;
  letzterWert[1] = letzterWert[2] = letzterWert[3] = letzterWert[4] = 0;
}
