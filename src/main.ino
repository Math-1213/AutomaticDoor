#include <SD.h>           // Interage com o Cartão SD
#include <SPI.h>          // Interage com dispositivos SPI
#include <ESP32Servo.h>   // Controla os Servos
#include <WiFi.h>         // Permite Conexão WIFI
#include <HTTPClient.h>   // Permite Requisições HTTP
#include <PubSubClient.h> // Permite Requisições MQTT
#include <ArduinoJson.h>  // Para parsear o JSON
#include <MFRC522.h>      // Biblioteca para RFID

// ---------------- Define os Pinos ---------------- //

// Pinos SD Card Reader
#define SD_CS_PIN 15      // Pino CS (Chip Select) do cartão SD

// Pinos RFID
#define RFID_CS_PIN 5     // Pino CS (Chip Select) para o RFID
#define RFID_RST_PIN 22   // Pino RST (Reset) para o RFID

// Pinos Comuns RFID e SD
#define MOSI_PIN 23      // Pino MOSI (Master Out Slave In)
#define MISO_PIN 19      // Pino MISO (Master In Slave Out)
#define SCK_PIN 18       // Pino SCK (Clock)

// Pinos Outros
#define BUTTON_PIN_IN 32   // Botão Interno
#define BUTTON_PIN_OUT 33  // Botão Externo
#define BUZZER_PIN 2       // Buzzer
#define LED_PIN 25         // LED 
#define SERVO_PIN_1 4      // Servo Interno
#define SERVO_PIN_2 21     // Servo Externo
#define ECHO_PIN 35         // Echo - Sensor de Presença
#define TRIGGER_PIN 26     // Trig - Sensor de Presença

//Objetos e variaveis globais
Servo servoIN;
Servo servoOUT;
File logFile;
MFRC522 rfid(RFID_CS_PIN, RFID_RST_PIN); // Inicializa o objeto RFID
bool isTagRegistrationMode = false; // Controle do modo de cadastro de tag
String currentTag = "";             // Tag atual sendo lida ou cadastrada

// Configuração Wi-Fi
const char* ssid = "Made_In_Heaven";
const char* password = "Verdao123!";

// Configuração MQTT
const char* mqtt_server = "37caacba90b842b38a69fae005a025e2.s1.eu.hivemq.cloud";  // Endereço do broker MQTT
const int mqtt_port = 8883;                          // Porta MQTT
const char* mqtt_user = "usuario";                   // Usuário (se necessário)
const char* mqtt_password = "Senha12.";                 // Senha (se necessário)
const char* mqtt_topic = "home/doors";               // Tópico MQTT

WiFiClient espClient;   // Cliente Wi-Fi para o MQTT
PubSubClient mqttClient(espClient);  // Cliente MQTT

// ------------ Classe LOG ------------ //

// Classe Log
class Log {
  private:
    String Data;            // Data e hora
    String Login;           // Pessoa/Objeto 
    String Method;          // Origem
    String AdditionalInfo;  // Informações adicionais

  public:
    // Construtor
    Log(String data, String login, String method, String additionalInfo) {
      this->Data = data;
      this->Login = login;
      this->Method = method;
      this->AdditionalInfo = additionalInfo;
    }

    // Formatar log como string
    String formatLog() {
      return Data + "/" + Login + "/" + String(Method) + "/" + AdditionalInfo;
    }

    // ---------------- Funções para o SD Card ---------------- //

    // Gravar log no SD Card
    void saveToSD() {
        String log = formatLog();
        File logFile = SD.open("/log.txt", FILE_WRITE); // Abre o arquivo para escrita
        if (!logFile) {
            Serial.println("Falha ao abrir o arquivo para gravação!");
            return;
        }

        // Escreve o log no arquivo
        logFile.println(log);
        logFile.close(); // Fecha o arquivo
        Serial.println("Log gravado no SD Card.");
    }

    // Ler log do SD Card
    static String readFromSD() {
        String log = "";
        File logFile = SD.open("/log.txt", FILE_READ); // Abre o arquivo para leitura
        if (!logFile) {
            Serial.println("Falha ao abrir o arquivo para leitura!");
            return "";
        }

        // Lê o conteúdo do arquivo
        while (logFile.available()) {
            log += (char)logFile.read();
        }
        logFile.close(); // Fecha o arquivo
        return log;
    }
};

// Função para gerar a data e hora a partir de uma requisição HTTP
String getCurrentDateTime() {
  // URL para pegar a data e hora de São Paulo (fuso horário: America/Sao_Paulo)
  String url = "http://worldtimeapi.org/api/timezone/America/Sao_Paulo";

  // Realiza a requisição HTTP
  HTTPClient http;
  http.begin(url);
  int httpResponseCode = http.GET();

  String formattedTime = "Erro ao obter hora";  // Valor padrão caso a requisição falhe

  if (httpResponseCode == 200) {  // Se a resposta for bem-sucedida
    String payload = http.getString();  // Pega o corpo da resposta

    // Parseia o JSON retornado
    StaticJsonDocument<512> doc;
    deserializeJson(doc, payload);

    // Extrai as informações de data e hora
    String dateTime = doc["datetime"];  // Exemplo: "2025-01-07T10:45:32.123456-03:00"

    // Extrai a parte da data (YYYY-MM-DD) e da hora (HH:MM:SS)
    String date = dateTime.substring(0, 10);  // "2025-01-07"
    String time = dateTime.substring(11, 19); // "10:45:32"

    // Formata a data para o formato DIA/MES/ANO
    String day = date.substring(8, 10);   // "07"
    String month = date.substring(5, 7);  // "01"
    String year = date.substring(0, 4);   // "2025"

    // Concatena para a string final
    formattedTime = day + "/" + month + "/" + year + " " + time;
  }

  http.end();  // Finaliza a requisição HTTP
  return formattedTime;  // Retorna a data e hora formatada
}

// Função para criar e registrar um log
void createLogger(String login, String method, String additionalInfo) {
  // Criar o log
  Log log(getCurrentDateTime(), login, method, additionalInfo);
  
  // Salvar o log no SD
  log.saveToSD();  // Grava no SD Card
}

// Função para inicializar o SD
bool initializeSD() {
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("Falha ao inicializar o cartão SD.");
    return false;
  }
  return true;
}

void printSDLogs() {
    String logs = Log::readFromSD();
    if (logs.length() > 0) {
        Serial.println(logs);
    } else {
        Serial.println("Nenhum log encontrado.");
    }
}

void setupWiFi() {
  Serial.print("Conectando-se à rede Wi-Fi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("Conectado ao Wi-Fi!");
}

void setupMQTT() {
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
}

void openInternalDoor();
void closeInternalDoor();
void openExternalDoor();
void closeExternalDoor();

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  bool debug = false;
  String msg = "";
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  // Verifica o tópico e processa a mensagem
  if (topic == "home/doors/openIN" && msg == "1") {
    // Abrir a porta interna via MQTT
    openInternalDoor();
  } else if (topic == "home/doors/openOUT" && msg == "1") {
    // Abrir a porta externa via MQTT
    openExternalDoor();
  } else if (topic == "home/doors/closeIN" && msg == "1") {
    // Fechar a porta interna via MQTT
    closeInternalDoor();
  } else if (topic == "home/doors/closeOUT" && msg == "1") {
    // Fechar a porta externa via MQTT
    closeExternalDoor();
  } else if (topic == "home/doors/registerTag" && msg == "1") {
    // Ativar o modo de cadastro de tag
    isTagRegistrationMode = true;
    if (debug) {
      Serial.println("Modo de cadastro de tag ativado.");
    }
  } else if (topic == "home/doors/stopRegisterTag" && msg == "1") {
    // Desativar o modo de cadastro de tag
    isTagRegistrationMode = false;
    if (debug) {
      Serial.println("Modo de cadastro de tag desativado.");
    }
  }
}

void reconnectMQTT() {
  bool debug = false;
  while (!mqttClient.connected()) {
    if (mqttClient.connect("EspClient")) {
      mqttClient.subscribe("home/doors/openIN");
      mqttClient.subscribe("home/doors/openOUT");
      mqttClient.subscribe("home/doors/registerTag");
      mqttClient.subscribe("home/doors/stopRegisterTag");
    } else {
      delay(5000);
    }
  }
}

void initRFID() {
  SPI.begin();  // Inicia o barramento SPI
  rfid.PCD_Init();  // Inicializa o MFRC522
  Serial.println("Leitor RFID pronto.");
}

// Define as Funções
void Lights();
void Doors();

void setup() {
  // Inicializa o Serial
    Serial.begin(115200);

    // Inicializa o SD Card
    if (!initializeSD()) {
        return;
    }
    Serial.println("SD Card inicializado.");

    setupWiFi();

    // Inicializa outros dispositivos (RFID, Servo, etc.)
    initRFID();
    
    // Inicializa outros componentes, como os servos
    servoIN.attach(SERVO_PIN_1);
    servoOUT.attach(SERVO_PIN_2);

    // Configuração do MQTT
    mqttClient.setServer("37caacba90b842b38a69fae005a025e2.s1.eu.hivemq.cloud", 8883);
    mqttClient.setCallback(mqttCallback);


    // Define as Entradas e Saídas
    pinMode(BUTTON_PIN_IN, INPUT_PULLUP);
    pinMode(BUTTON_PIN_OUT, INPUT_PULLUP);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(TRIGGER_PIN, OUTPUT);
  
    servoIN.attach(SERVO_PIN_1);
    servoOUT.attach(SERVO_PIN_2);
  
    digitalWrite(BUZZER_PIN, LOW);  // Buzzer off
    digitalWrite(LED_PIN, LOW);     // LED off
    servoIN.write(0);  // Porta interna fechada
    servoOUT.write(0);  // Porta Externa fechada

  // Conecta-se ao MQTT
  while (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();
    // Cria as tasks
    xTaskCreate(Lights, "LightsTask", 2048, NULL, 1, NULL);
    xTaskCreate(Doors, "DoorsTask", 1000, NULL, 1, NULL);
}

void Lights(void *parameter) {
  // Variável de depuração
  bool debug = true;

  // Configurações
  int distanciaMinima = 30; // Distância mínima para acionar a luz
  int luzTempo = 3000;      // Tempo que a luz ficará acesa após detectar presença (em ms)
  unsigned long luzUltimoAcionamento = 0;
  unsigned long luzUltimoLog = 0; // Para garantir que o log não seja repetido

  while (true) {
    digitalWrite(TRIGGER_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIGGER_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIGGER_PIN, LOW);

    int duracao = pulseIn(ECHO_PIN, HIGH);
    int distancia = duracao * 0.034 * 0.5;

    if (distancia < distanciaMinima) { // Presença detectada
      if (debug) {
        Serial.print("Presença detectada! Distância: ");
        Serial.println(distancia);
      }
      luzUltimoAcionamento = millis(); // Atualiza o último acionamento
    }

    // Mantém a luz acesa se estiver dentro do período configurado
    if (millis() - luzUltimoAcionamento <= luzTempo) {
      digitalWrite(LED_PIN, HIGH); // Acende o LED
    } else {
      digitalWrite(LED_PIN, LOW); // Apaga o LED
      // Verifica se já passou tempo suficiente para registrar o log
      if (millis() - luzUltimoLog > 5000) { // Garante que o log só será registrado a cada 5 segundos
        createLogger("Sensor de Presença", "Sensor de Presença", "Luz Apagada");
        luzUltimoLog = millis(); // Atualiza o tempo do último log
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS); // Aguarda 100ms antes de verificar novamente
  }
}

void Doors(void *parameter) {
  // Variável de depuração
  bool debug = true;

  // Configurações
  int tempo = 3000;                    // Tempo de abertura das portas
  const unsigned long debounceDelay = 200; // 200ms para debounce

  // Variáveis de controle
  unsigned long lastPressIN = 0;
  unsigned long lastPressOUT = 0;
  unsigned long servoCloseTimeIN = 0;
  unsigned long servoCloseTimeOUT = 0;
  bool isExtLast = false;
  bool isServoOpenIN = false;
  bool isServoOpenOUT = false;

  while (true) {
    unsigned long currentMillis = millis();

    // Verifica se o botão interno foi pressionado
    handleButtonPress(BUTTON_PIN_IN, lastPressIN, currentMillis, debounceDelay, true, tempo, isServoOpenIN, servoCloseTimeIN, servoIN, isExtLast, debug);

    // Verifica se o botão externo foi pressionado
    handleButtonPress(BUTTON_PIN_OUT, lastPressOUT, currentMillis, debounceDelay, false, tempo, isServoOpenOUT, servoCloseTimeOUT, servoOUT, isExtLast, debug);

    // Fecha a porta interna automaticamente após o tempo definido
    autoClosePort(isServoOpenIN, currentMillis, servoCloseTimeIN, servoIN, "Porta interna fechada.", debug);

    // Fecha a porta externa automaticamente após o tempo definido
    autoClosePort(isServoOpenOUT, currentMillis, servoCloseTimeOUT, servoOUT, "Porta externa fechada.", debug);

    // Verifica a Tag
    handleTagPress(currentMillis, lastPressOUT, debounceDelay, tempo, isExtLast, isServoOpenIN, servoIN, servoCloseTimeIN, isServoOpenOUT, servoOUT, servoCloseTimeOUT, debug);

    vTaskDelay(50 / portTICK_PERIOD_MS); // Aguarda 50ms antes de verificar novamente
  }
}

void handleButtonPress(int buttonPin, unsigned long &lastPress, unsigned long currentMillis, const unsigned long debounceDelay, bool isInternal, int tempo, bool &isServoOpen, unsigned long &servoCloseTime, Servo &servo, bool &isExtLast, bool debug) {
  if (digitalRead(buttonPin) == LOW && currentMillis - lastPress > debounceDelay) {
    lastPress = currentMillis; // Atualiza o tempo do último evento

    if (debug) {
      String buttonType = isInternal ? "interno" : "externo";
      Serial.println("Botão " + buttonType + " pressionado.");
    }

    // Abre a porta (interna ou externa)
    servo.write(90);
    isServoOpen = true;
    servoCloseTime = currentMillis + tempo; // Define o tempo para fechar a porta

    isExtLast = !isInternal; // Inverte o estado para a tag identificar a última porta aberta

    if (debug) {
      String portType = isInternal ? "interna" : "externa";
      Serial.println("Porta " + portType + " aberta.");
    }
  }
}

void autoClosePort(bool &isServoOpen, unsigned long currentMillis, unsigned long servoCloseTime, Servo &servo, String message, bool debug) {
  if (isServoOpen && currentMillis > servoCloseTime) {
    servo.write(0);
    isServoOpen = false;
    if (debug) {
      Serial.println(message);
    }
  }
}

void handleTagPress(unsigned long currentMillis, unsigned long &lastPressOUT, const unsigned long debounceDelay, int tempo, bool &isExtLast, bool &isServoOpenIN, Servo &servoIN, unsigned long &servoCloseTimeIN, bool &isServoOpenOUT, Servo &servoOUT, unsigned long &servoCloseTimeOUT, bool debug) {

    // Abre a porta oposta à última aberta
    if (isExtLast) {
      servoOUT.write(90);
      isServoOpenOUT = true;
      servoCloseTimeOUT = currentMillis + tempo; // Define o tempo para fechar a porta

      if (debug) {
        Serial.println("Porta externa aberta.");
      }
    } else {
      servoIN.write(90);
      isServoOpenIN = true;
      servoCloseTimeIN = currentMillis + tempo; // Define o tempo para fechar a porta

      if (debug) {
        Serial.println("Porta interna aberta.");
    }
  }
}

// MQTT DOORS CONTROL
bool debugMqttDoors = false;

void openInternalDoor() {
  servoIN.write(90);
  if (debugMqttDoors) {
    Serial.println("Porta interna aberta via MQTT.");
  }
}

void openExternalDoor() {
  servoOUT.write(90);
  if (debugMqttDoors) {
    Serial.println("Porta externa aberta via MQTT.");
  }
}

void closeInternalDoor() {
  servoIN.write(0);
  if (debugMqttDoors) {
    Serial.println("Porta interna fechada via MQTT.");
  }
}

void closeExternalDoor() {
  servoOUT.write(0);
  if (debugMqttDoors) {
    Serial.println("Porta externa fechada via MQTT.");
  }
}

void loop() {
  //LOOP VAZIO :)
}
