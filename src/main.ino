#include <SD.h>            // Interage com o Cartão SD
#include <SPI.h>           // Interage com dispositivos SPI
#include <ESP32Servo.h>    // Controla os Servos
#include <WiFi.h>          // Permite Conexão WIFI
#include <HTTPClient.h>    // Permite Requisições HTTP
#include <PubSubClient.h>  // Permite Requisições MQTT
#include <ArduinoJson.h>   // Para parsear o JSON
//Bibliotecas do RFID
#include <MFRC522v2.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522DriverPinSimple.h>
#include <MFRC522Debug.h>

// ---------------- Define os Pinos ---------------- //

// Pinos SD Card Reader
#define SD_CS_PIN 15  // Pino CS (Chip Select) do cartão SD

// Pinos RFID
#define RFID_CS_PIN 5    // Pino CS (Chip Select) para o RFID
#define RFID_RST_PIN 21  // Pino RST (Reset) para o RFID

// Pinos Comuns RFID e SD
#define MOSI_PIN 23  // Pino MOSI (Master Out Slave In)
#define MISO_PIN 19  // Pino MISO (Master In Slave Out)
#define SCK_PIN 18   // Pino SCK (Clock)

// Pinos Outros
#define BUTTON_PIN_IN 32   // Botão Interno
#define BUTTON_PIN_OUT 33  // Botão Externo
#define BUZZER_PIN 2       // Buzzer
#define LED_PIN 25         // LED
#define SERVO_PIN_1 14     // Servo Interno
#define SERVO_PIN_2 12     // Servo Externo
#define ECHO_PIN 35        // Echo - Sensor de Presença
#define TRIGGER_PIN 26     // Trig - Sensor de Presença

//Objetos e variaveis globais
#define FILENAME "/registros.txt"
Servo servoIN;
Servo servoOUT;
File logFile;
MFRC522DriverPinSimple ss_pin(5);
MFRC522DriverSPI driver{ ss_pin };   // Create SPI driver.
MFRC522 mfrc522{ driver };           // Create MFRC522 instance.
bool isTagRegistrationMode = false;  // Controle do modo de cadastro de tag
String currentTag = "";              // Tag atual sendo lida ou cadastrada

//TODO - Colocar as TAGS No Cartão SD
String uids[10]; 

// Configuração Wi-Fi
const char *ssid = "CLARO_RAMOS_EXT";
const char *password = "02072017";

// Configuração MQTT
const char *mqtt_server = "broker.hivemq.com";  // Endereço do broker MQTT
const int mqtt_port = 1883;                     // Porta MQTT
const char *mqtt_user = "EspClient";            // Usuário 
const char *mqtt_password = "ifspEsp32";        // Senha 

WiFiClient espClient;                // Cliente Wi-Fi para o MQTT
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
    File logFile = SD.open("/log.txt", FILE_WRITE);  // Abre o arquivo para escrita
    if (!logFile) {
      Serial.println("Falha ao abrir o arquivo para gravação!");
      return;
    }

    // Escreve o log no arquivo
    logFile.println(log);
    logFile.close();  // Fecha o arquivo
    Serial.println("Log gravado no SD Card.");
  }

  // Ler log do SD Card
  static String readFromSD() {
    String log = "";
    File logFile = SD.open("/log.txt", FILE_READ);  // Abre o arquivo para leitura
    if (!logFile) {
      Serial.println("Falha ao abrir o arquivo para leitura!");
      return "";
    }

    // Lê o conteúdo do arquivo
    while (logFile.available()) {
      log += (char)logFile.read();
    }
    logFile.close();  // Fecha o arquivo
    return log;
  }
};


// ---------- Funções ---------------- //
// Função para gerar a data e hora a partir de uma requisição HTTP
String getCurrentDateTime() {
  // URL para pegar a data e hora de São Paulo (fuso horário: America/Sao_Paulo)
  String url = "http://worldtimeapi.org/api/timezone/America/Sao_Paulo";

  // Realiza a requisição HTTP
  HTTPClient http;
  http.begin(url);
  int httpResponseCode = http.GET();

  String formattedTime = "Erro ao obter hora";  // Valor padrão caso a requisição falhe

  if (httpResponseCode == 200) {        // Se a resposta for bem-sucedida
    String payload = http.getString();  // Pega o corpo da resposta

    // Parseia o JSON retornado
    StaticJsonDocument<512> doc;
    deserializeJson(doc, payload);

    // Extrai as informações de data e hora
    String dateTime = doc["datetime"];  // Exemplo: "2025-01-07T10:45:32.123456-03:00"

    // Extrai a parte da data (YYYY-MM-DD) e da hora (HH:MM:SS)
    String date = dateTime.substring(0, 10);   // "2025-01-07"
    String time = dateTime.substring(11, 19);  // "10:45:32"

    // Formata a data para o formato DIA/MES/ANO
    String day = date.substring(8, 10);   // "07"
    String month = date.substring(5, 7);  // "01"
    String year = date.substring(0, 4);   // "2025"

    // Concatena para a string final
    formattedTime = day + "/" + month + "/" + year + " " + time;
  }

  http.end();            // Finaliza a requisição HTTP
  return formattedTime;  // Retorna a data e hora formatada
}

// Função para criar e registrar um log
void createLogger(String login, String method, String additionalInfo) {
  // Criar o log
  Log log(getCurrentDateTime(), login, method, additionalInfo);

  // Salvar o log no SD
  //log.saveToSD();  // Grava no SD Card
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

void carregarRegistros() {
  if (!SD.exists(FILENAME)) {
    Serial.println("Arquivo de registros não encontrado, criando novo...");
    File file = SD.open(FILENAME, FILE_WRITE);
    if (file) {
      file.println("[]"); // Arquivo começa vazio como um array JSON
      file.close();
    } else {
      Serial.println("Falha ao criar o arquivo de registros!");
    }
    return;
  }

  Serial.println("Registros carregados com sucesso!");
}

void salvarRegistros(JsonArray registros) {
  File file = SD.open(FILENAME, FILE_WRITE);
  if (file) {
    file.seek(0); // Certifique-se de sobrescrever o arquivo
    serializeJson(registros, file);
    file.close();
    Serial.println("Registros salvos com sucesso!");
  } else {
    Serial.println("Erro ao abrir o arquivo para salvar!");
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

// Função para processar mensagem de registro
void processarMensagemRegistro(String payload) {
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.println("Erro ao parsear JSON!");
    return;
  }

  String metodo = doc["metodo"]; // APAGAR, ATUALIZAR, INSERIR
  String uid = doc["uid"];
  String nome = doc["nome"];
  String addInfo = doc["addInfo"];

  File file = SD.open(FILENAME, FILE_READ);
  if (!file) {
    Serial.println("Erro ao abrir arquivo para leitura!");
    return;
  }

  StaticJsonDocument<2048> registrosDoc;
  DeserializationError loadError = deserializeJson(registrosDoc, file);
  file.close();

  if (loadError) {
    Serial.println("Erro ao carregar registros!");
    return;
  }

  JsonArray registros = registrosDoc.as<JsonArray>();

  if (metodo == "APAGAR") {
    // Apagar registro por UID
    bool apagado = false;
    for (JsonArray::iterator it = registros.begin(); it != registros.end(); ++it) {
      if ((*it)["uid"] == uid) {
        registros.remove(it);
        apagado = true;
        break;
      }
    }
    if (apagado) {
      Serial.println("Registro apagado com sucesso!");
      salvarRegistros(registros);
    } else {
      Serial.println("UID não encontrado!");
    }
  } else if (metodo == "ATUALIZAR") {
    // Atualizar registro por UID
    bool atualizado = false;
    for (JsonObject registro : registros) {
      if (registro["uid"] == uid) {
        registro["nome"] = nome;
        registro["addInfo"] = addInfo;
        atualizado = true;
        break;
      }
    }
    if (atualizado) {
      Serial.println("Registro atualizado com sucesso!");
      salvarRegistros(registros);
    } else {
      Serial.println("UID não encontrado para atualização!");
    }
  } else if (metodo == "INSERIR") {
    // Inserir novo registro
    JsonObject novoRegistro = registros.createNestedObject();
    novoRegistro["uid"] = uid;
    novoRegistro["nome"] = nome;
    novoRegistro["addInfo"] = addInfo;
    salvarRegistros(registros);
    Serial.println("Registro inserido com sucesso!");
  } else {
    Serial.println("Método desconhecido!");
  }
}


void mqttCallback(char *topic, byte *payload, unsigned int length) {
  bool debug = true;
  String msg = "";
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  if(debug){
    Serial.print("Tópico: ");
    Serial.println(topic);
    Serial.print("Mensagem: ");
    Serial.println(msg);
  }

  // Verifica o tópico e processa a mensagem
  if (String(topic) == "home/doors/openIN" && msg == "1") {
    // Abrir a porta interna via MQTT
    openInternalDoor();
  } else if (String(topic) == "home/doors/openOUT" && msg == "1") {
    // Abrir a porta externa via MQTT
    openExternalDoor();
  } else if (String(topic) == "home/doors/closeIN" && msg == "1") {
    // Fechar a porta interna via MQTT
    closeInternalDoor();
  } else if (String(topic) == "home/doors/closeOUT" && msg == "1") {
    // Fechar a porta externa via MQTT
    closeExternalDoor();
  } else if (String(topic) == "home/doors/registerTag" && msg == "1") {
    // Ativar o modo de cadastro de tag
    isTagRegistrationMode = true;
    if (debug) {
      Serial.println("Modo de cadastro de tag ativado.");
    }
  } else if (String(topic) == "home/doors/stopRegisterTag" && msg == "1") {
    // Desativar o modo de cadastro de tag
    isTagRegistrationMode = false;
    if (debug) {
      Serial.println("Modo de cadastro de tag desativado.");
    }
  } else if (String(topic) == "home/doors/Registro") { // METODO(APAGAR,ATUALIZAR,INSERIR) //UID //NOME //ADDINFO
    processarMensagemRegistro(msg);
    /* EXEMPLO DE MENSAGEM de Registro:
    *
    *   {
    *   "metodo": "INSERIR",
    *   "uid": "12345678",
    *   "nome": "Joao",
    *   "addInfo": "Acesso Principal"
    *}                                  */
  }
}

void reconnectMQTT() {
  bool debug = false;
  while (!mqttClient.connected()) {
    if (mqttClient.connect("EspClient")) {
      mqttClient.subscribe("home/doors/openIN");
      mqttClient.subscribe("home/doors/openOUT");
      mqttClient.subscribe("home/doors/closeIN");
      mqttClient.subscribe("home/doors/closeOUT");
      mqttClient.subscribe("home/doors/registerTag");
      mqttClient.subscribe("home/doors/stopRegisterTag");
      mqttClient.subscribe("home/doors/Registro");
    } else {
      delay(500);
      Serial.println("Erro na conexão MQTT");
    }
  }
}

// Funções do RFID
void initRFID() {
  SPI.begin();  // Inicia o barramento SPI
  mfrc522.PCD_Init();
  Serial.println("Leitor RFID pronto.");
}

bool readRFID() {
  // Verifica se há uma nova tag presente
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return false;
  }

  // Select one of the cards.
  if (!mfrc522.PICC_ReadCardSerial()) {
    return false;
  }

  // Exibe o UID no monitor serial e salva em currentTag
  currentTag = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    // Formata o byte para garantir que tenha 2 dígitos hexadecimais
    if (mfrc522.uid.uidByte[i] < 0x10) {
      currentTag += "0";  // Adiciona um "0" antes de números de 1 dígito
    }

    // Adiciona o byte formatado (em HEX) à String global currentTag
    currentTag += String(mfrc522.uid.uidByte[i], HEX);
  }

  // Agora a variável currentTag contém o UID completo do cartão
  Serial.println(currentTag);  // Imprime o UID completo no Monitor Serial

  // Emite o som no buzzer
  digitalWrite(BUZZER_PIN, HIGH);  // Liga o buzzer
  delay(100);                      // Tempo do buzzer ligado (100ms)
  digitalWrite(BUZZER_PIN, LOW);   // Desliga o buzzer

  // Finaliza a comunicação com a tag
  mfrc522.PICC_HaltA();

  return true;  // Leitura bem-sucedida
}

bool isRFIDValid(){
  for (byte i = 0; i < 10; i++) {
    if (uids[i] == currentTag) {  // Se encontrar um UID igual, retorna true
      return true;
    }
  }
  // Se não encontrar nenhum UID igual, retorna false
  return false;
}


// Define as Funções
void Lights();
void Doors();

void setup() {
  // Variavel de TESTE
  uids[0] = ("a32a9013");
  uids[1] = ("a32a9013");
  uids[2] = ("a32a9013");
  uids[3] = ("a32a9013");
  uids[4] = ("a32a9013");
  uids[5] = ("a32a9013");
  uids[6] = ("a32a9013");
  uids[7] = ("a32a9013");
  uids[8] = ("a32a9013");
  uids[9] = ("a32a9013");

  // Inicializa o Serial
  Serial.begin(115200);

  // Inicializa o SD Card
  if (!initializeSD()) {
    //    return;
  }
  Serial.println("SD Card inicializado.");

  //carregarRegistros();

  setupWiFi();

  // Inicializa outros dispositivos (RFID, Servo, etc.)
  initRFID();

  // Inicializa outros componentes, como os servos
  servoIN.attach(SERVO_PIN_1);
  servoOUT.attach(SERVO_PIN_2);
  Serial.println("Servos Conectados");

  // Configuração do MQTT
  Serial.println("Configurando MQTT");
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
  Serial.println("Conexão MQTT Configurado");


  // Define as Entradas e Saídas
  pinMode(BUTTON_PIN_IN, INPUT_PULLUP);
  pinMode(BUTTON_PIN_OUT, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(TRIGGER_PIN, OUTPUT);


  digitalWrite(BUZZER_PIN, LOW);  // Buzzer off
  digitalWrite(LED_PIN, LOW);     // LED off
  servoIN.write(0);               // Porta interna fechada
  servoOUT.write(0);              // Porta Externa fechada

  // Conecta-se ao MQTT
  while (!mqttClient.connected()) {
    Serial.println("Conectando MQTT");
    reconnectMQTT();
  }
  Serial.println("MQTT Conectado");

  Serial.println("Iniciado com Sucesso");
  // Cria as tasks
  //xTaskCreate(Lights, "LightsTask", 4096, NULL, 1, NULL);
  xTaskCreate(Doors, "DoorsTask", 4096, NULL, 1, NULL);

  mqttClient.loop();


  //TESTE
    Serial.print("UID ");
    Serial.print(1);  // Para mostrar o número do UID
    Serial.print(": ");
    Serial.println(uids[0]);  // Imprime o UID

}

void Lights(void *parameter) {
  // Variável de depuração
  bool debug = false;

  // Configurações
  int distanciaMinima = 30;  // Distância mínima para acionar a luz
  int luzTempo = 3000;       // Tempo que a luz ficará acesa após detectar presença (em ms)
  unsigned long luzUltimoAcionamento = 0;
  unsigned long luzUltimoLog = 0;  // Para garantir que o log não seja repetido

  while (true) {
    digitalWrite(TRIGGER_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIGGER_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIGGER_PIN, LOW);

    int duracao = pulseIn(ECHO_PIN, HIGH);
    int distancia = duracao * 0.034 * 0.5;

    if (distancia < distanciaMinima && distancia > 0) {  // Presença detectada
      if (debug) {
        Serial.print("Presença detectada! Distância: ");
        Serial.println(distancia);
      }
      luzUltimoAcionamento = millis();  // Atualiza o último acionamento
    }

    // Mantém a luz acesa se estiver dentro do período configurado
    if (millis() - luzUltimoAcionamento <= luzTempo) {
      digitalWrite(LED_PIN, HIGH);  // Acende o LED
    } else {
      digitalWrite(LED_PIN, LOW);  // Apaga o LED
      // Verifica se já passou tempo suficiente para registrar o log
      if (millis() - luzUltimoLog > 5000) {  // Garante que o log só será registrado a cada 5 segundos
        createLogger("Sensor de Presença", "Sensor de Presença", "Luz Apagada");
        luzUltimoLog = millis();  // Atualiza o tempo do último log
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);  // Aguarda 100ms antes de verificar novamente
  }
}

//CONSTANTES
int tempo = 3000;                         // Tempo de abertura das portas
const unsigned long debounceDelay = 200;  // 200ms para debounce

void Doors(void *parameter) {
  // Variável de depuração
  bool debug = true;

  // Variáveis de controle
  unsigned long lastPressIN = 0;
  unsigned long lastPressOUT = 0;
  unsigned long servoCloseTimeIN = 0;
  unsigned long servoCloseTimeOUT = 0;
  bool isExtLast = false;
  bool isServoOpenIN = false;
  bool isServoOpenOUT = false;
  bool inRegisterMode = isTagRegistrationMode;

  while (true) {
    unsigned long currentMillis = millis();

    //Mantem a conexão MQTT
    if (!mqttClient.connected()) {
    reconnectMQTT();  // Garantir reconexão com o MQTT se desconectado
    }
    mqttClient.loop();

    // Verifica se os botões forão pressionados
    handleButtonPress(BUTTON_PIN_IN, lastPressIN, currentMillis, true, isServoOpenIN, servoCloseTimeIN, servoIN, isExtLast, debug);
    handleButtonPress(BUTTON_PIN_OUT, lastPressOUT, currentMillis, false, isServoOpenOUT, servoCloseTimeOUT, servoOUT, isExtLast, debug);

    // Fecha as portas automaticamente após o tempo definido
    autoClosePort(isServoOpenIN, currentMillis, servoCloseTimeIN, servoIN, "Porta interna fechada.", debug);
    autoClosePort(isServoOpenOUT, currentMillis, servoCloseTimeOUT, servoOUT, "Porta externa fechada.", debug);

    // Verifica a Tag
    if (readRFID() && !isTagRegistrationMode) {
      if(isRFIDValid()){
      handleTagPress(currentMillis, lastPressOUT, isExtLast, isServoOpenIN, servoIN, servoCloseTimeIN, isServoOpenOUT, servoOUT, servoCloseTimeOUT, debug);
      } else if (isTagRegistrationMode) {
        //Enviar tag por lida via mqtt
        if(!isRFIDValid()){
          if(debug) Serial.println("Tag Enviada para Cadastro:" + currentTag);
          mqttClient.publish("home/doors/RFID", currentTag.c_str());
        }
      }
    }

    //Manda uma resposta se trocar de modo:
    if(isTagRegistrationMode != inRegisterMode){
      inRegisterMode = isTagRegistrationMode;
      mqttClient.publish("home/doors/inRegMode", inRegisterMode ? "true" : "false");
    }

    vTaskDelay(50 / portTICK_PERIOD_MS);  // Aguarda 50ms antes de verificar novamente
  }
}

void handleButtonPress(int buttonPin, unsigned long &lastPress, unsigned long currentMillis, bool isInternal, bool &isServoOpen, unsigned long &servoCloseTime, Servo &servo, bool &isExtLast, bool debug) {
  if (digitalRead(buttonPin) == LOW && currentMillis - lastPress > debounceDelay) {
    lastPress = currentMillis;  // Atualiza o tempo do último evento
    String buttonType = isInternal ? "interno" : "externo";

    if (debug) {
      Serial.println("Botão " + buttonType + " pressionado.");
    }

    // Abre a porta (interna ou externa)
    servo.write(90);
    isServoOpen = true;
    servoCloseTime = currentMillis + tempo;  // Define o tempo para fechar a porta

    isExtLast = !isInternal;  // Inverte o estado para a tag identificar a última porta aberta

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

void handleTagPress(unsigned long currentMillis, unsigned long &lastPressOUT, bool &isExtLast, bool &isServoOpenIN, Servo &servoIN, unsigned long &servoCloseTimeIN, bool &isServoOpenOUT, Servo &servoOUT, unsigned long &servoCloseTimeOUT, bool debug) {

  if(!isTagRegistrationMode){
    // Abre a porta oposta à última aberta
    if (isExtLast) {
      servoOUT.write(90);
      isServoOpenOUT = true;
      servoCloseTimeOUT = currentMillis + tempo;  // Define o tempo para fechar a porta
      mqttClient.publish("home/doors/statusOUT", "Aberta");
      if (debug) {
        Serial.println("Porta externa aberta. TAG");
      }
      delay(servoCloseTimeOUT);
      mqttClient.publish("home/doors/statusOUT", "Fechada");
      if (debug) {
        Serial.println("Porta externa fechada. TAG");
      }
    } else {
      servoIN.write(90);
      isServoOpenIN = true;
      servoCloseTimeIN = currentMillis + tempo;  // Define o tempo para fechar a porta
      mqttClient.publish("home/doors/statusIN", "Aberta");
      if (debug) {
        Serial.println("Porta interna aberta. TAG");
      }
      delay(servoCloseTimeIN);
      mqttClient.publish("home/doors/statusIN", "Fechada");
      if (debug) {
        Serial.println("Porta interna fechada. TAG");
      }
    }
    isExtLast = !isExtLast;
  }
}

// MQTT DOORS CONTROL
bool debugMqttDoors = false;

void openInternalDoor() {
  servoIN.write(90);
  if (debugMqttDoors) {
    Serial.println("Porta interna aberta via MQTT.");
    mqttClient.publish("home/doors/statusIN", "Aberta");
  }
}

void openExternalDoor() {
  servoOUT.write(90);
  if (debugMqttDoors) {
    Serial.println("Porta externa aberta via MQTT.");
    mqttClient.publish("home/doors/statusOUT", "Aberta");
  }
}

void closeInternalDoor() {
  servoIN.write(0);
  if (debugMqttDoors) {
    Serial.println("Porta interna fechada via MQTT.");
    mqttClient.publish("home/doors/statusIN", "Fechada");
  }
}

void closeExternalDoor() {
  servoOUT.write(0);
  if (debugMqttDoors) {
    Serial.println("Porta externa fechada via MQTT.");
    mqttClient.publish("home/doors/statusOUT", "Fechada");
  }
}

void loop() {
  //LOOP VAZIO :)
}


/*     Lista de Tópicos home/doors/:
* openIN		: APP manda abrir a porta IN
* openOUT		: APP manda abrir a porta OUT
* closeIN		: APP manda fechar a porta IN
* closeOUT	: APP manda fechar a porta OUT
* registerTag	: APP manda entrar no modo de registro
* stopRegisterTag	: APP manda sair do modo de registro
* Registro	: APP Manda o registro da tag para ser adicionado
* RFID		: ESP32 Manda a uid para ser registrada no APP
* inRegMode	: ESP32 Manda o se está em modo de registro (1 Para sim, 0 para não)
* statusOUT	: ESP32 Manda para o APP o estado da porta OUT (1 para aberta, 0 para fechada)
* statusIN	: ESP32 Manda para o APP o estado da porta IN (1 para aberta, 0 para fechada)
*/
