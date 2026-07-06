#include <SoftwareSerial.h>
#include <Keypad.h>

//================= MACROS COMANDOS AT =================
#define AT_CMD_CHECK     "AT"
#define AT_CMD_SIGNAL    "AT+CSQ"      
#define AT_CMD_NETWORK   "AT+CREG?"    
#define AT_CMD_OPERATOR  "AT+COPS?"    
#define AT_CMD_SMS_MODE  "AT+CMGF=1"   

//================= ASIGNACIÓN DE PINES LED =================
#define LED_ROJO         A0  // Estado: SIN SEÑAL (No encenderá por la simulación)
#define LED_AMARILLO     A1  // Estado: CON SEÑAL (Siempre encendido por simulación)
#define LED_VERDE        A2  // Acción: SMS ENVIADO CORRECTAMENTE (2 Segundos)
#define LED_AZUL         A3  // Modo: ENVIAR MENSAJE (Encendido en este modo)

//================= TECLADO =================
const byte FILAS = 4;
const byte COLUMNAS = 4;

char mapaTeclas[FILAS][COLUMNAS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte pinesFilas[FILAS] = {4,5,6,7};
byte pinesColumnas[COLUMNAS] = {8,9,10,11};

Keypad teclado = Keypad(makeKeymap(mapaTeclas), pinesFilas, pinesColumnas, FILAS, COLUMNAS);

//================= SIM800L =================
SoftwareSerial mySerial(2,3);

//================= DEFINICIÓN DE MODOS =================
enum ModoSistema {
  MODO_AGREGAR,      // Modo 1: Para registrar números del 1 al 9
  MODO_ENVIO         // Modo 2: Para disparar SMS de "EMERGENCIA"
};

ModoSistema modoActual = MODO_AGREGAR; // Inicia en Modo Agregar

enum EstadoRegistro {
  MENU_ESPERA,
  INGRESANDO_DIGITOS,
  SELECCIONANDO_SLOT
};

EstadoRegistro estadoRegistro = MENU_ESPERA;

// Variables globales
char numeroTemporal[10];
byte posNumero = 0;
char numeroActivo[13];
char numerosRegistrados[10][13]; // Slots del 1 al 9 vacíos al arrancar

bool conectadoRed = true; // SIMULACIÓN: Empieza asumido como conectado

//================= PROTOTIPOS ==============
void leerRespuestaSIM(char* buffer, size_t maxLen, unsigned long timeoutMs);
void verificarModuloResponde();
bool intentarConexionRed();
void mostrarSenal();
void procesarTecla(char tecla);
void manejarModoAgregar(char tecla);
void manejarModoEnvio(char tecla);
void enviarSMSAlerta(const char* numero);
void imprimirEstadoModo();
void mostrarMenuCompleto();
void actualizarLEDsEstado();

//================= SETUP ===================
void setup() {
  Serial.begin(9600);
  mySerial.begin(9600);

  // Configurar LEDs
  pinMode(LED_ROJO, OUTPUT);
  pinMode(LED_AMARILLO, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_AZUL, OUTPUT);

  // Estado inicial
  digitalWrite(LED_ROJO, LOW);
  digitalWrite(LED_AMARILLO, LOW);
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_AZUL, LOW);

  // MEMORIA DE NÚMEROS 100% LIMPIA (No hay nada pre-agregado)
  for(int i = 0; i < 10; i++) {
    numerosRegistrados[i][0] = '\0';
  }

  delay(1000);

  verificarModuloResponde();
  intentarConexionRed(); // Corre el chequeo pero forzará red OK
  mostrarMenuCompleto();
  imprimirEstadoModo();
}

//================= LEE RESPUESTA DEL SIM =================
void leerRespuestaSIM(char* buffer, size_t maxLen, unsigned long timeoutMs) {
  size_t idx = 0;
  buffer[0] = '\0';
  unsigned long inicio = millis();

  while(millis() - inicio < timeoutMs) {
    while(mySerial.available() && idx < maxLen - 1) {
      buffer[idx++] = mySerial.read();
    }
  }
  buffer[idx] = '\0';
}

//================= VERIFICA MODULO =================
void verificarModuloResponde() {
  char resp[48];
  Serial.println(F("Verificando hardware SIM800L..."));
  mySerial.println(F(AT_CMD_CHECK));
  leerRespuestaSIM(resp, sizeof(resp), 1000);

  if(strstr(resp, "OK") != NULL) {
    Serial.println(F("SIM800L OK"));
  } else {
    Serial.println(F("SIM800L NO RESPONDE (Falta energía o cruce de TX/RX, el programa continuará igual)"));
  }
  Serial.println();
}

//================= REGISTRO EN RED (CON SIMULACIÓN) =================
bool intentarConexionRed() {
  char resp[48];
  mySerial.println(F(AT_CMD_NETWORK));
  leerRespuestaSIM(resp, sizeof(resp), 1000);

  // --- TRUCO DE SIMULACIÓN ---
  // No importa la respuesta real del chip, forzamos conexión para desarrollo continuo.
  conectadoRed = true; 
  Serial.println(F("Red Celular: OK (SIMULADO - Listo para enviar)"));
  return true;
}

void mostrarSenal() {
  char resp[64];
  Serial.println(F("\n--- DIAGNÓSTICO DE RED (REAL) ---"));
  mySerial.println(F(AT_CMD_SIGNAL));
  leerRespuestaSIM(resp, sizeof(resp), 1000);
  Serial.print(F("Señal (CSQ): ")); Serial.println(resp);

  mySerial.println(F(AT_CMD_OPERATOR));
  leerRespuestaSIM(resp, sizeof(resp), 1000);
  Serial.print(F("Operador: ")); Serial.println(resp);
  Serial.println(F("---------------------------\n"));
}

//================= CONTROL DE ESTADO DE LEDs =================
void actualizarLEDsEstado() {
  // Como está simulado, Amarillo se enciende fijo y Rojo queda apagado
  if (conectadoRed) {
    digitalWrite(LED_AMARILLO, HIGH); 
    digitalWrite(LED_ROJO, LOW);       
  } else {
    digitalWrite(LED_AMARILLO, LOW);  
    digitalWrite(LED_ROJO, HIGH);      
  }

  // LED de Modo (Azul se enciende SOLO en MODO ENVIAR MENSAJE)
  if (modoActual == MODO_ENVIO) {
    digitalWrite(LED_AZUL, HIGH);      
  } else {
    digitalWrite(LED_AZUL, LOW);       
  }
}

//================= LOOP PRINCIPAL =================
void loop() {
  actualizarLEDsEstado();

  char tecla = teclado.getKey();

  if (tecla) {
    if (tecla == '*') {
      Serial.println(F("Actualizando estado de red..."));
      intentarConexionRed();
      mostrarSenal(); // Te deja ver los valores de señal reales por monitor serie
      return;
    }
    procesarTecla(tecla);
  }

  while (mySerial.available()) {
    Serial.write(mySerial.read());
  }
}

//================= PROCESAMIENTO DE TECLAS =================
void procesarTecla(char tecla) {
  // Cambiar de modo con la tecla A
  if (tecla == 'A') {
    modoActual = (modoActual == MODO_AGREGAR) ? MODO_ENVIO : MODO_AGREGAR;
    estadoRegistro = MENU_ESPERA; // Resetea sub-estados de guardado
    posNumero = 0;
    imprimirEstadoModo();
    return;
  }

  if (modoActual == MODO_AGREGAR) {
    manejarModoAgregar(tecla);
  } else {
    manejarModoEnvio(tecla);
  }
}

//================= MODO 1: AGREGAR (CONFIGURACIÓN) =================
void manejarModoAgregar(char tecla) {
  switch (estadoRegistro) {
    case MENU_ESPERA:
      if (tecla == 'B') {
        Serial.println(F("\n--- NUEVO REGISTRO ---"));
        Serial.println(F("Ingresa los 9 digitos del celular:"));
        posNumero = 0;
        memset(numeroTemporal, 0, sizeof(numeroTemporal));
        estadoRegistro = INGRESANDO_DIGITOS;
      } else {
        Serial.println(F("[Modo Agregar] Presiona 'B' para registrar un nuevo numero."));
      }
      break;

    case INGRESANDO_DIGITOS:
      if (tecla >= '0' && tecla <= '9') {
        if (posNumero < 9) {
          numeroTemporal[posNumero++] = tecla;
          numeroTemporal[posNumero] = '\0';
          Serial.print(F("Digitando: ")); Serial.println(numeroTemporal);
        }
        if (posNumero == 9) {
          Serial.println(F("-> 9 Digitos completados. Presiona '#' para validar o '*' para cancelar."));
        }
      }
      else if (tecla == '#') {
        if (posNumero == 9) {
          snprintf(numeroActivo, sizeof(numeroActivo), "+51%s", numeroTemporal);
          Serial.print(F("Numero Validado: ")); Serial.println(numeroActivo);
          Serial.println(F("¿En que casillero lo guardas? Presiona un numero del [1 al 9]:"));
          estadoRegistro = SELECCIONANDO_SLOT;
        } else {
          Serial.println(F("Error: El numero debe tener exactamente 9 digitos."));
        }
      }
      else if (tecla == '*') {
        Serial.println(F("Registro cancelado."));
        estadoRegistro = MENU_ESPERA;
      }
      break;

    case SELECCIONANDO_SLOT:
      if (tecla >= '1' && tecla <= '9') {
        int slot = tecla - '0';
        strcpy(numerosRegistrados[slot], numeroActivo);
        
        Serial.print(F("¡Guardado exitoso! Numero asociado a la tecla: ")); Serial.println(slot);
        Serial.println(F("Volviendo al menu de espera..."));
        estadoRegistro = MENU_ESPERA;
      } else {
        Serial.println(F("Casillero invalido. Elige una tecla del 1 al 9."));
      }
      break;
  }
}

//================= MODO 2: ENVIAR MENSAJE =================
void manejarModoEnvio(char tecla) {
  if (tecla >= '1' && tecla <= '9') {
    int slot = tecla - '0';

    if (strlen(numerosRegistrados[slot]) > 0) {
      enviarSMSAlerta(numerosRegistrados[slot]);
    } else {
      Serial.print(F("La tecla '")); Serial.print(tecla); Serial.println(F("' no tiene ningun numero guardado aun."));
    }
  } else {
    Serial.println(F("[Modo Envio] Presiona teclas del 1 al 9 para mandar la alerta."));
  }
}

//================= ENVÍO DE SMS (ALERTAS GENERALES) =================
void enviarSMSAlerta(const char* numero) {
  char resp[64];
  Serial.print(F("Despachando mensaje hacia: ")); Serial.println(numero);

  mySerial.println(F(AT_CMD_SMS_MODE));
  leerRespuestaSIM(resp, sizeof(resp), 1000);

  mySerial.print(F("AT+CMGS=\""));
  mySerial.print(numero);
  mySerial.println(F("\""));
  leerRespuestaSIM(resp, sizeof(resp), 1500);

  // TEXTO UNIFICADO: Ahora todos los números reciben exactamente el mismo mensaje.
  mySerial.print(F("EMERGENCIA"));

  delay(300);
  mySerial.write(26); // Fin del mensaje (Ctrl+Z)

  Serial.println(F("Esperando confirmacion de transmision..."));
  leerRespuestaSIM(resp, sizeof(resp), 5000);
  Serial.println(resp);

  // Si el módulo responde con éxito, o si prefieres confiar en el intento simulado:
  if (strstr(resp, "+CMGS") != NULL || strstr(resp, "OK") != NULL) {
    Serial.println(F(">>> SMS ENVIADO CORRECTAMENTE <<<"));
  } else {
    Serial.println(F("Comandos enviados. (Revisa saldo o antena si el mensaje no llega físicamente)"));
  }
  
  // ACCIÓN LED VERDE: Destella por 2 segundos indicando que el proceso terminó
  digitalWrite(LED_VERDE, HIGH);
  delay(2000); 
  digitalWrite(LED_VERDE, LOW);
}

//================= INTERFAZ EN MONITOR SERIAL =================
void imprimirEstadoModo() {
  Serial.println(F("\n========================================"));
  if (modoActual == MODO_AGREGAR) {
    Serial.println(F("   TITULO: MODO 1 - AGREGAR NUMEROS"));
  } else {
    Serial.println(F("   TITULO: MODO 2 - ENVIAR MENSAJE"));
  }
  Serial.println(F("========================================"));
  
  if (modoActual == MODO_AGREGAR) {
    Serial.println(F("-> Estado: Memoria editable. Presiona 'B' para capturar digitos."));
  } else {
    Serial.println(F("-> Estado: Listo para disparar. Presiona del 1 al 9 para enviar 'EMERGENCIA'."));
  }
  Serial.println();
}

void mostrarMenuCompleto() {
  Serial.println(F("========================================="));
  Serial.println(F("      SISTEMA CONTROLADOR SIM800L        "));
  Serial.println(F("========================================="));
  Serial.println(F(" Tecla A = Cambiar entre Modos de operacion"));
  Serial.println(F(" Tecla * = Comprobar Nivel de Señal Real"));
  Serial.println(F("-----------------------------------------"));
  Serial.println(F(" ESTADO DE LUCES LED:" ));
  Serial.println(F(" A0 (Rojo)     -> Sin Señal (Inactivo por Simulacion)" ));
  Serial.println(F(" A1 (Amarillo) -> Señal OK (Activo Fijo)" ));
  Serial.println(F(" A2 (Verde)    -> Disparo Realizado (Prende 2s)" ));
  Serial.println(F(" A3 (Azul)     -> Encendido en [MODO ENVIAR MENSAJE]" ));
  Serial.println(F("=========================================\n"));
}