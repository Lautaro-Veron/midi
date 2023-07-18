#include <MIDI.h>

/*
     
     - Este Sketch lee los puertos digitales y analogicos del Arduino, envia notas midi al MIDI Control Change

*/


/////////////////////////////////////////////
// Seleciona tu placa
// Define tu placa, elija:
// "ATMEGA328" si usas ATmega328 - Uno, Mega, Nano ...
// "ATMEGA32U4" si usas ATmega32U4 - Micro, Pro Micro, Leonardo ...
// "DEBUG" solo para debuguear en monitor serie

#define ATMEGA328 1 //* coloque aqui el controlador que estas usando, como en las lineas de arriba seguidas de "1", como "ATMEGA328 1", "DEBUG 1", etc.

/////////////////////////////////////////////
// BIBLIOTECAS
// -- Define biblioteca MIDI -- //

// si usas ATmega328 - Uno, Mega, Nano ...
#ifdef ATMEGA328
#include <MIDI.h> // Use a "by Francois Best"
MIDI_CREATE_DEFAULT_INSTANCE();

// si usas ATmega32U4 - Micro, Pro Micro, Leonardo ...
#elif ATMEGA32U4
#include "MIDIUSB.h"

#endif
// ---- //

/////////////////////////////////////////////
// BOTONES
const int N_BUTTONS = 2; //*  número total de botones
const int BUTTON_ARDUINO_PIN[N_BUTTONS] = {2, 3}; //* pines de cada boton conectado diretamente al Arduino

//#define pin13 1 // descomenta si estas usando el pin 13 (el pin con led), o comente la linea sino
byte pin13index = 12; //* coloque el índice de pin 13 de la matriz buttonPin [] si lo está usando, si no, coméntelo

int buttonCState[N_BUTTONS] = {};        // almacena el valor actual del botón
int buttonPState[N_BUTTONS] = {};        // almacena el valor anterior del botón

// debounce
unsigned long lastDebounceTime[N_BUTTONS] = {0};  // la última vez que se cambió el pin de salida
unsigned long debounceDelay = 5;    //* el tiempo de rebote; aumente si la salida envía demasiadas notas a la vez
/////////////////////////////////////////////
// POTENCIOMETROS
const int N_POTS = 2; //* número total de pots (slide y rotativo)
const int POT_ARDUINO_PIN[N_POTS] = {A0, A1}; //* pines de cada pot conectado directamente al Arduino

int potCState[N_POTS] = {0}; // estado actual de la puerta analogica
int potPState[N_POTS] = {0}; // estado previo de la puerta analogica
int potVar = 0; // variacion entre el valor de estado previo y el atual de la puerta analogica

int midiCState[N_POTS] = {0}; // Estado actual del valor midi
int midiPState[N_POTS] = {0}; // Estado anterior del valor midi

const int TIMEOUT = 300; //* cantidad de tiempo en que el potenciometro se leera despues de pasar por ultrapassar o varThreshold
const int varThreshold = 10; //* threshold para la variacion de la señal del potenciometro
boolean potMoving = true; // si el potenciometro se esta moviendo
unsigned long PTime[N_POTS] = {0}; // tiempo previamente almacenado
unsigned long timer[N_POTS] = {0}; // almacena el tiempo que paso desde que se reinició el temporizador

/////////////////////////////////////////////
// midi
byte midiCh = 1; //* Canal MIDI para usar
byte note = 36; //* nota mas baja para usar
byte cc = 1; //* CC MIDI más bajo para usar

/////////////////////////////////////////////
// SETUP
void setup() {

  // Baud Rate
  // si usas ATmega328 (uno, mega, nano...)
  // 31250 para MIDI class compliant | 115200 para Hairless MIDI
  Serial.begin(115200); //*

#ifdef DEBUG
Serial.println("Debug mode");
Serial.println();
#endif

  // Buttons
  // Inicializar botones con resistencia pull up
  for (int i = 0; i < N_BUTTONS; i++) {
    pinMode(BUTTON_ARDUINO_PIN[i], INPUT_PULLUP);
  }

#ifdef pin13 // inicializa el pin 13 como una entrada
pinMode(BUTTON_ARDUINO_PIN[pin13index], INPUT);
#endif


}

/////////////////////////////////////////////
// LOOP
void loop() {

  buttons();
  potentiometers();

}

/////////////////////////////////////////////
// BOTONES
void buttons() {

  for (int i = 0; i < N_BUTTONS; i++) {

    buttonCState[i] = digitalRead(BUTTON_ARDUINO_PIN[i]);   // leer los pines del Arduino

#ifdef pin13
if (i == pin13index) {
buttonCState[i] = !buttonCState[i]; // invierta el pin 13 porque tiene una resistencia pull down en lugar de pull up
}
#endif

    if ((millis() - lastDebounceTime[i]) > debounceDelay) {

      if (buttonPState[i] != buttonCState[i]) {
        lastDebounceTime[i] = millis();

        if (buttonCState[i] == LOW) {

          // Envía la nota MIDI según la placa elegida
#ifdef ATMEGA328
// ATmega328 (uno, mega, nano...)
MIDI.sendNoteOn(note + i, 127, midiCh); // note, velocity, channel

#elif ATMEGA32U4
// ATmega32U4 (micro, pro micro, leonardo...)
noteOn(midiCh, note + i, 127);  // channel, note, velocity
MidiUSB.flush();

#elif TEENSY
// Teensy
usbMIDI.sendNoteOn(note + i, 127, midiCh); // note, velocity, channel

#elif DEBUG
Serial.print(i);
Serial.println(": button on");
#endif

        }
        else {

          // Envía la nota MIDI OFF según la tarjeta elegida
#ifdef ATMEGA328
// ATmega328 (uno, mega, nano...)
MIDI.sendNoteOn(note + i, 0, midiCh); // note, velocity, channel

#elif ATMEGA32U4
// ATmega32U4 (micro, pro micro, leonardo...)
noteOn(midiCh, note + i, 0);  // channel, note, velocity
MidiUSB.flush();

#elif TEENSY
// Teensy
usbMIDI.sendNoteOn(note + i, 0, midiCh); // note, velocity, channel

#elif DEBUG
Serial.print(i);
Serial.println(": button off");
#endif

        }
        buttonPState[i] = buttonCState[i];
      }
    }
  }
}

/////////////////////////////////////////////
// POTENTIOMETERS
void potentiometers() {

 /* para que solo se lean los puertos analógicos cuando se usan, sin perder resolución,
    Es necesario establecer un "umbral" (varThreshold), un valor mínimo que tienen que mover los puertos
    para empezar a leer. Después de eso, se crea una especie de "puerta", una puerta que se abre y permite
    que los puertos analógicos se lean sin interrupción durante un tiempo determinado (TIMEOUT). Cuando el temporizador es menor que TIMEOUT
    significa que el potenciómetro se movió muy recientemente, lo que significa que probablemente todavía se está moviendo,
    por lo tanto, uno debe mantener abierta la "puerta"; si el temporizador es mayor que TIMEOUT significa que no se ha movido por un tiempo,
    entonces la puerta debe estar cerrada. Para que esta lógica tenga lugar, el temporizador debe reiniciarse (líneas 99 y 100) cada vez que el puerto analógico
    variar más que el varThreshold establecido.
  */


  //Debug solamente
  //    for (int i = 0; i < nPots; i++) {
  //      Serial.print(potCState[i]); Serial.print(" ");
  //    }
  //    Serial.println();

  for (int i = 0; i < N_POTS; i++) { // Hace loop todos los potenciometros

    potCState[i] = analogRead(POT_ARDUINO_PIN[i]);

    midiCState[i] = map(potCState[i], 0, 1023, 0, 127); // Mapea la lectura del potCState a un valor utilizable midi

    potVar = abs(potCState[i] - potPState[i]); // Calcula el valor absoluto de la diferencia entre el estado actual y el anterior del pot

    if (potVar > varThreshold) { // Abre la puerta si la variación del potenciómetro es mayor que el umbral (varThreshold)
      PTime[i] = millis(); // Almacena la hora anterior
    }

    timer[i] = millis() - PTime[i]; // Resetea el timer 11000 - 11000 = 0ms

    if (timer[i] < TIMEOUT) { // Si el temporizador es menor que el tiempo máximo permitido, significa que el potenciómetro todavía se está moviendo.
      potMoving = true;
    }
    else {
      potMoving = false;
    }

    if (potMoving == true) { // Si el potenciómetro aún se está moviendo, envíe el cambio de control
      if (midiPState[i] != midiCState[i]) {

        // Envia al MIDI CC de acuerdo con la placa elegida
#ifdef ATMEGA328
// ATmega328 (uno, mega, nano...)
MIDI.sendControlChange(cc + i, midiCState[i], midiCh); // cc number, cc value, midi channel

#elif ATMEGA32U4
// ATmega32U4 (micro, pro micro, leonardo...)
controlChange(midiCh, cc + i, midiCState[i]); //  (channel, CC number,  CC value)
MidiUSB.flush();

#elif TEENSY
// Teensy
usbMIDI.sendControlChange(cc + i, midiCState[i], midiCh); // cc number, cc value, midi channel

#elif DEBUG
Serial.print("Pot: ");
Serial.print(i);
Serial.print(" ");
Serial.println(midiCState[i]);
//Serial.print("  ");
#endif

        potPState[i] = potCState[i]; // Almacena la lectura actual del potenciómetro para compararla con la siguiente
        midiPState[i] = midiCState[i];
      }
    }
  }
}

/////////////////////////////////////////////
// si usas ATmega32U4 (micro, pro micro, leonardo ...)
#ifdef ATMEGA32U4

// Arduino (pro)micro midi functions MIDIUSB Library
void noteOn(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOn = {0x09, 0x90 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOn);
}

void noteOff(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOff = {0x08, 0x80 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOff);
}

void controlChange(byte channel, byte control, byte value) {
  midiEventPacket_t event = {0x0B, 0xB0 | channel, control, value};
  MidiUSB.sendMIDI(event);
}

#endif
