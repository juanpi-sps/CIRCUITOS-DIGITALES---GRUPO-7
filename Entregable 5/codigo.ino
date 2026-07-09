#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <SoftwareSerial.h>

// ==========================================
// CONFIGURACIÓN DE PINES
// ==========================================
// Cambiamos la velocidad interna a 9600 para máxima estabilidad
SoftwareSerial sim800(2, 3); 
LiquidCrystal_I2C lcd(0x27, 16, 2);  

#define LED_AZUL A0
#define LED_ROJO A1
#define LED_VERDE A2
#define LED_AMARILLO A3

const byte FILAS = 4; 
const byte COLUMNAS = 4; 
char keys[FILAS][COLUMNAS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte pinesFilas[FILAS] = {4, 5, 6, 7};       
byte pinesColumnas[COLUMNAS] = {8, 9, 10, 11}; 
Keypad teclado = Keypad(makeKeymap(keys), pinesFilas, pinesColumnas, FILAS, COLUMNAS);

// ==========================================
// VARIABLES GLOBALES
// ==========================================
String numeroCelular = "";
String ultimoMensaje = "";
String agenda[10]; 
bool hayMensajeNuevo = false; 
bool redConectada = false;

int modoActual = 1;   
int pasoModo2 = 0;    
int memoriaTemp = -1; 

unsigned long tiempoAnteriorBlink = 0;
bool estadoBlink = false;

// ==========================================
// PROTOTIPOS DE FUNCIONES
// ==========================================
void actualizarPantalla();
void enviarSMSEmergencia(String numero);
void verificarRed();
void procesarModo1(char tecla);
void procesarModo2(char tecla);
void procesarModo3(char tecla);

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200); // PC a 115200
  sim800.begin(9600);   // ¡SIM800L cambiado a 9600 para estabilidad!
  
  pinMode(LED_AZUL, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_AMARILLO, OUTPUT);
  
  lcd.init();
  lcd.backlight();
  
  Serial.println(F("\n=================================="));
  Serial.println(F("   INICIANDO CENTRAL DE ALARMAS   "));
  Serial.println(F("=================================="));
  Serial.println(F("[SISTEMA] Configurando modulo SIM800L..."));

  lcd.setCursor(0, 0);
  lcd.print(F("Iniciando red..."));
  
  // Forzamos al módulo a sincronizarse a 9600 baudios
  sim800.println(F("AT")); 
  delay(500);
  sim800.println(F("AT+IPR=9600")); // Comando para fijar 9600 baudios permanentes
  delay(500);
  sim800.println(F("AT+CMGF=1")); 
  delay(500);
  sim800.println(F("AT+CNMI=2,2,0,0,0")); 
  delay(1000);

  verificarRed(); 
  Serial.println(F("\n[SISTEMA] Listo. Presione '#' para cambiar de modo."));
  actualizarPantalla(); 
}

// ==========================================
// LOOP PRINCIPAL
// ==========================================
void loop() {
  // 1. Control de LEDs
  unsigned long tiempoActual = millis();
  if (tiempoActual - tiempoAnteriorBlink >= 500) {
    tiempoAnteriorBlink = tiempoActual;
    estadoBlink = !estadoBlink;

    if (modoActual == 1) digitalWrite(LED_AMARILLO, estadoBlink);
    else if (modoActual == 2) digitalWrite(LED_AMARILLO, LOW);
    else if (modoActual == 3) digitalWrite(LED_AMARILLO, HIGH);

    if (hayMensajeNuevo) digitalWrite(LED_ROJO, estadoBlink);
    else digitalWrite(LED_ROJO, LOW);

    if (redConectada) digitalWrite(LED_AZUL, HIGH);
    else digitalWrite(LED_AZUL, estadoBlink);
  }

  // 2. LECTURA DEL TECLADO
  char tecla = teclado.getKey(); 
  
  if (tecla) {
    if (tecla == '#') { 
      modoActual++;
      if (modoActual > 3) modoActual = 1;
      numeroCelular = "";
      pasoModo2 = 0;
      
      Serial.println(F("\n----------------------------------"));
      Serial.print(F("[TECLADO] Cambio de Modo -> MODO "));
      Serial.println(modoActual);
      Serial.println(F("----------------------------------"));
      
      actualizarPantalla();
    }
    else if (tecla == 'A') {
      Serial.println(F("\n[TECLADO] Boton 'A' presionado (Bandeja)"));
      if (hayMensajeNuevo) {
        Serial.print(F("[MENSAJE] Leyendo: "));
        Serial.println(ultimoMensaje);
        lcd.clear();
        lcd.setCursor(0, 0); lcd.print(F("SMS RECIBIDO:"));
        lcd.setCursor(0, 1); lcd.print(ultimoMensaje.substring(0, 16)); 
        delay(5000); 
        hayMensajeNuevo = false; 
        actualizarPantalla(); 
      } else {
        Serial.println(F("[MENSAJE] Bandeja vacia."));
        lcd.clear();
        lcd.setCursor(0, 0); lcd.print(F("Bandeja vacia"));
        delay(2000); actualizarPantalla();
      }
    }
    else if (tecla == 'B') {
      Serial.println(F("\n[TECLADO] Boton 'B' presionado (Senal)"));
      verificarRed(); 
    }
    else {
      if (modoActual == 1) procesarModo1(tecla);
      else if (modoActual == 2) procesarModo2(tecla);
      else if (modoActual == 3) procesarModo3(tecla);
    }
  }

  // 3. LECTURA DE SMS EN SEGUNDO PLANO
  if (sim800.available()) {
    String respuestaSIM = sim800.readString();
    
    // Imprime TODO lo que responda el módulo en el monitor para ver qué pasa en vivo
    Serial.print(F("[SIM800 RAW]: ")); 
    Serial.println(respuestaSIM); 

    if (respuestaSIM.indexOf(F("+CMT:")) != -1) {
      int saltoDeLinea = respuestaSIM.indexOf('\n', respuestaSIM.indexOf(F("+CMT:")));
      if (saltoDeLinea != -1) {
        ultimoMensaje = respuestaSIM.substring(saltoDeLinea + 1);
        ultimoMensaje.trim(); 
        hayMensajeNuevo = true; 
        Serial.println(F("\n[ALERTA] ¡NUEVO SMS DETECTADO! Presione 'A' para leerlo en el LCD."));
        actualizarPantalla(); 
      }
    }
  }
}

// ==========================================
// FUNCIONES DE LOS MODOS
// ==========================================
void procesarModo1(char tecla) {
  if (tecla >= '0' && tecla <= '9') {
    if (numeroCelular.length() < 9) { numeroCelular += tecla; actualizarPantalla(); }
  } else if (tecla == '*') { 
    numeroCelular = ""; actualizarPantalla();
  } else if (tecla == 'D') {
    if (numeroCelular.length() == 9) { 
      enviarSMSEmergencia(numeroCelular);
      numeroCelular = ""; actualizarPantalla();
    }
  }
}

void procesarModo2(char tecla) {
  if (pasoModo2 == 0) { 
    if (tecla >= '0' && tecla <= '9') {
      if (numeroCelular.length() < 9) { numeroCelular += tecla; actualizarPantalla(); }
    } else if (tecla == '*') { 
      numeroCelular = ""; actualizarPantalla();
    } else if (tecla == 'D' && numeroCelular.length() == 9) {
      pasoModo2 = 1; 
      Serial.println(F("[INFO] Elija ranura (1-9) para guardar."));
      actualizarPantalla();
    }
  } 
  else if (pasoModo2 == 1) { 
    if (tecla >= '1' && tecla <= '9') {
      int espacio = tecla - '0'; 
      agenda[espacio] = numeroCelular; 
      
      Serial.print(F("[GUARDADO] Numero ")); Serial.print(numeroCelular);
      Serial.print(F(" en ranura ")); Serial.println(espacio);
      
      lcd.clear();
      lcd.setCursor(0,0); lcd.print(F("Guardado en:"));
      lcd.setCursor(0,1); lcd.print(F("Espacio ")); lcd.print(espacio);
      delay(2000);
      numeroCelular = "";
      pasoModo2 = 0;
      modoActual = 3; 
      actualizarPantalla();
    } else if (tecla == '*') {
      pasoModo2 = 0; actualizarPantalla();
    }
  }
}

void procesarModo3(char tecla) {
  if (tecla >= '1' && tecla <= '9') {
    int espacio = tecla - '0';
    if (agenda[espacio].length() == 9) {
      memoriaTemp = espacio; 
      numeroCelular = agenda[espacio];
      Serial.print(F("[MEMORIA] Ranura ")); Serial.print(espacio); Serial.print(F(" cargada: ")); Serial.println(numeroCelular);
      actualizarPantalla();
    } else {
      lcd.clear(); lcd.setCursor(0,0); lcd.print(F("Espacio vacio")); delay(1500); actualizarPantalla();
    }
  } else if (tecla == '*') {
    numeroCelular = ""; memoriaTemp = -1; actualizarPantalla();
  } else if (tecla == 'D') {
    if (numeroCelular.length() == 9) {
      enviarSMSEmergencia(numeroCelular);
      numeroCelular = ""; memoriaTemp = -1;
      actualizarPantalla();
    }
  }
}

// ==========================================
// FUNCIONES AUXILIARES
// ==========================================
void actualizarPantalla() {
  lcd.clear();
  if (hayMensajeNuevo) {
    lcd.setCursor(0, 0); lcd.print(F("SMS NUEVO! (A)")); 
  } else {
    if (modoActual == 1) { lcd.setCursor(0, 0); lcd.print(F("M1:Escriba Num:")); } 
    else if (modoActual == 2) {
      if (pasoModo2 == 0) { lcd.setCursor(0, 0); lcd.print(F("M2:Num a guardar")); }
      else { lcd.setCursor(0, 0); lcd.print(F("M2:Elija slot 1-9")); }
    } else if (modoActual == 3) { lcd.setCursor(0, 0); lcd.print(F("M3:SOS Rapido")); }
  }
  lcd.setCursor(0, 1);
  lcd.print(numeroCelular); 
}

void enviarSMSEmergencia(String numero) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(F("Enviando SMS..."));

  sim800.println(F("AT+CMGF=1")); delay(300);
  sim800.print(F("AT+CMGS=\"+51")); sim800.print(numero); sim800.println(F("\"")); delay(1000);
  sim800.print(F("ALERTA S.O.S: Mensaje de emergencia.")); delay(500);
  sim800.write(26); 
  
  for(int i=0; i<6; i++){
    digitalWrite(LED_VERDE, HIGH); delay(400);
    digitalWrite(LED_VERDE, LOW); delay(400);
  }      

  digitalWrite(LED_VERDE, HIGH);
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(F("SMS ENVIADO"));
  delay(2000);
  digitalWrite(LED_VERDE, LOW); 
}

void verificarRed() {
  sim800.println(F("AT+CREG?"));
  delay(500);
  if (sim800.available()) {
    String res = sim800.readString();
    if (res.indexOf(F("0,1")) != -1 || res.indexOf(F("0,5")) != -1) {
      redConectada = true;
      Serial.println(F("[RED] OK - Conectado."));
    } else {
      redConectada = false;
      Serial.println(F("[RED] ERROR - Sin senal."));
    }
  }
}