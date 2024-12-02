#include <Ethernet.h>
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>
#include <DHT.h>

// Configuración DHT11
#define DHTPIN 3
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Configuración Ethernet y dirección de Base de Datos
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
EthernetClient clienteWeb;
IPAddress serverDB(150, 136, 254, 73); // IP del servidor MySQL
EthernetServer serverWeb(80);

char usuario[] = "user1";
char pass[] = "1234";
char db_name[] = "Arduino";

MySQL_Connection conn((Client *)&clienteWeb);

unsigned long lastMillis = 0; // Para el control de tiempo de inserción

// Configuración de motores
int motorA1 = 8;
int motorA2 = 9;
int motorB1 = 11;
int motorB2 = 12;

void setup() {
  Serial.begin(9600);
  dht.begin(); // Inicializa el sensor DHT11

  // Intentar obtener IP mediante DHCP repetidamente
  int max_reintentos = 10; // Número máximo de intentos
  int intentos = 0;
  while (Ethernet.begin(mac) == 0) {
    Serial.println("Fallo al obtener IP mediante DHCP. Reintentando...");
    intentos++;
    if (intentos >= max_reintentos) {
      Serial.println("Máximo de intentos alcanzado. Verifique su conexión.");
      while (true); // Detener si no se logra obtener una IP tras varios intentos
    }
    delay(5000); // Esperar 5 segundos antes de intentar nuevamente
  }

  serverWeb.begin(); // Inicia el servidor para el control del carro

  Serial.print("IP asignada: ");
  Serial.println(Ethernet.localIP());

  // Configuración de pines para motores
  pinMode(motorA1, OUTPUT);
  pinMode(motorA2, OUTPUT);
  pinMode(motorB1, OUTPUT);
  pinMode(motorB2, OUTPUT);
  detenerMotores();
}

void loop() {
  // Control de movimiento del carro
  atenderClienteWeb();

  // Control de temperatura y subida de datos
  if (millis() - lastMillis >= 30000 || millis() < lastMillis) { // Envía los datos cada 30 segundos
    lastMillis = millis();

    // Lee la temperatura del DHT11
    float temperatura = dht.readTemperature();

    if (isnan(temperatura)) {
      Serial.println("Error al leer el DHT11.");
    } else {
      subirDatosBD(temperatura); // Enviar datos a la base de datos
    }
  }
}

void atenderClienteWeb() {
  EthernetClient cliente = serverWeb.available();
  if (cliente) {
    boolean currentLineIsBlank = true;
    String request = "";

    while (cliente.connected()) {
      if (cliente.available()) {
        char c = cliente.read();
        Serial.write(c); // Para depuración
        request.concat(c);

        int posicion = request.indexOf("GET /");
        if (posicion != -1) {
          String comando = request.substring(posicion + 5, request.indexOf(" ", posicion + 5));

          // Control de motores basado en comando
          if (comando == "A") {
            moverDerecha();
          } else if (comando == "R") {
            moverIzquierda();
          } else if (comando == "D") {
            moverDerechaIzquierda();
          } else if (comando == "I") {
            moverIzquierdaDerecha();
          } else if (comando == "N") {
            detenerMotores();
          }
        }

        if (c == '\n' && currentLineIsBlank) {
          cliente.println("HTTP/1.1 200 OK");
          cliente.println("Content-Type: text/html");
          cliente.println();
          cliente.println("<html>");
          cliente.println("<head>");
          cliente.println("<title>Control de Carro</title>");
          cliente.println("<script>");
          cliente.println("function enviarComando(comando) {");
          cliente.println("  var xhr = new XMLHttpRequest();");
          cliente.println("  xhr.open('GET', '/' + comando, true);");
          cliente.println("  xhr.send();");
          cliente.println("}");
          cliente.println("function detener() {");
          cliente.println("  enviarComando('N');");
          cliente.println("}");
          cliente.println("</script>");
          cliente.println("</head>");
          cliente.println("<body>");
          cliente.println("<h1>Control de Carro</h1>");
          cliente.println("<div style='text-align:center;'>");
          cliente.println("<button onmousedown=\"enviarComando('A')\" onmouseup=\"detener()\" ontouchstart=\"enviarComando('A')\" ontouchend=\"detener()\">Adelante</button>");
          cliente.println("<button onmousedown=\"enviarComando('R')\" onmouseup=\"detener()\" ontouchstart=\"enviarComando('R')\" ontouchend=\"detener()\">Atras</button>");
          cliente.println("<button onmousedown=\"enviarComando('D')\" onmouseup=\"detener()\" ontouchstart=\"enviarComando('D')\" ontouchend=\"detener()\">Derecha</button>");
          cliente.println("<button onmousedown=\"enviarComando('I')\" onmouseup=\"detener()\" ontouchstart=\"enviarComando('I')\" ontouchend=\"detener()\">Izquierda</button>");
          cliente.println("</div>");
          cliente.println("</body>");
          cliente.println("</html>");
          break;
        }

        if (c == '\n') {
          currentLineIsBlank = true;
        } else if (c != '\r') {
          currentLineIsBlank = false;
        }
      }
    }
    delay(1);
    cliente.stop();
  }
}

void subirDatosBD(float temperatura) {
  // Conectar a la base de datos
  Serial.println("Conectando a la base de datos...");
  if (conn.connect(serverDB, 3306, usuario, pass)) {
    Serial.println("Conexión exitosa.");
    MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
    char use_db[50];
    sprintf(use_db, "USE %s", db_name);
    cur_mem->execute(use_db);
    delete cur_mem;

    // Enviar datos
    char query[128];
    char tempStr[10];
    dtostrf(temperatura, 5, 2, tempStr);
    sprintf(query, "INSERT INTO TemperaturaHora (lugar, temperatura, fecha_hora) VALUES ('Torreon', %s, NOW())", tempStr);

    Serial.print("Insertando Datos: ");
    Serial.println(tempStr);

    cur_mem = new MySQL_Cursor(&conn);
    cur_mem->execute(query);
    delete cur_mem;

    conn.close(); // Cerrar la conexión después de enviar datos
    Serial.println("Desconectado de la base de datos.");
  } else {
    Serial.println("Error al conectar a la base de datos.");
  }
}

void detenerMotores() {
  digitalWrite(motorA1, LOW);
  digitalWrite(motorA2, LOW);
  digitalWrite(motorB1, LOW);
  digitalWrite(motorB2, LOW);
}

void moverDerecha() {
  digitalWrite(motorA1, HIGH);
  digitalWrite(motorA2, LOW);
  digitalWrite(motorB1, HIGH);
  digitalWrite(motorB2, LOW);
}

void moverIzquierda() {
  digitalWrite(motorA1, LOW);
  digitalWrite(motorA2, HIGH);
  digitalWrite(motorB1, LOW);
  digitalWrite(motorB2, HIGH);
}

void moverDerechaIzquierda() {
  digitalWrite(motorA1, HIGH);
  digitalWrite(motorA2, LOW);
  digitalWrite(motorB1, LOW);
  digitalWrite(motorB2, HIGH);
}

void moverIzquierdaDerecha() {
  digitalWrite(motorA1, LOW);
  digitalWrite(motorA2, HIGH);
  digitalWrite(motorB1, HIGH);
  digitalWrite(motorB2, LOW);
}

