#include <HardwareSerial.h>

// Broches validées
#define RXD_PIN 16
#define TXD_PIN 17
#define BUTTON_PIN 22 // Bouton entre GPIO 22 et GND

// Numéro destinataire
const char DEST_PHONE_NUMBER[] = "+243893921134"; 

HardwareSerial sim868(1);

String sendAT(String cmd, int timeout = 2000) {
  while (sim868.available()) sim868.read();
  sim868.println(cmd);
  
  String resp = "";
  long t = millis();
  while ((millis() - t) < timeout) {
    while (sim868.available()) {
      resp += (char)sim868.read();
    }
  }
  Serial.print(">> "); Serial.println(cmd);
  Serial.print("<< "); Serial.println(resp);
  return resp;
}

void sendLocationSMS() {
  Serial.println("\n[+] Bouton pressé : Recherche de position...");

  // 1. Réactivation rapide du canal de données GPRS
  sendAT("AT+SAPBR=1,1", 2000);

  // 2. Interrogation de la position LBS
  String lbsResp = sendAT("AT+CLBS=1,1", 4000);

  String googleMapsLink = "";
  bool locationFound = false;

  // Analyse de la réponse LBS (Exemple : +CLBS: 0,-4.321456,15.312456,50)
  if (lbsResp.indexOf("+CLBS: 0,") != -1) {
    int p1 = lbsResp.indexOf(',');
    int p2 = lbsResp.indexOf(',', p1 + 1);
    int p3 = lbsResp.indexOf(',', p2 + 1);

    if (p1 != -1 && p2 != -1 && p3 != -1) {
      String lat = lbsResp.substring(p1 + 1, p2);
      String lon = lbsResp.substring(p2 + 1, p3);
      lat.trim();
      lon.trim();

      googleMapsLink = "https://maps.google.com/?q=" + lat + "," + lon;
      locationFound = true;
    }
  }

  // 3. Secours automatique par Antenne Réseau (Si la LBS échoue)
  if (!locationFound) {
    Serial.println("[⚠️] LBS indisponible, bascule sur le mode antenne...");
    sendAT("AT+CREG=2", 1000);
    String cregResp = sendAT("AT+CREG?", 2000);

    int q1 = cregResp.indexOf('"');
    int q2 = cregResp.indexOf('"', q1 + 1);
    int q3 = cregResp.indexOf('"', q2 + 1);
    int q4 = cregResp.indexOf('"', q3 + 1);

    if (q1 != -1 && q4 != -1) {
      String lacHex = cregResp.substring(q1 + 1, q2);
      String cidHex = cregResp.substring(q3 + 1, q4);
      long lac = strtol(lacHex.c_str(), NULL, 16);
      long cid = strtol(cidHex.c_str(), NULL, 16);

      googleMapsLink = "https://opencellid.org/#zoom=16&lat=&lon=&mcc=630&mnc=2&lac=" + String(lac) + "&cellid=" + String(cid);
      locationFound = true;
    }
  }

  // 4. Envoi du SMS si une position a été générée
  if (locationFound) {
    String smsMessage = "🚨 ALERTE LOCALISATION 🚨\n\n";
    smsMessage += "Position du dispositif :\n";
    smsMessage += googleMapsLink;

    Serial.println("\n[+] Envoi du SMS...");
    sendAT("AT+CMGF=1", 1000);
    
    sim868.print("AT+CMGS=\"");
    sim868.print(DEST_PHONE_NUMBER);
    sim868.println("\"");
    delay(1000);
    
    sim868.print(smsMessage);
    delay(500);
    sim868.write(26); // Touche CTRL+Z
    delay(5000);

    Serial.println("\n[✔] SMS envoyé avec succès !");
  } else {
    Serial.println("\n[❌] Échec total de géolocalisation. Assurez-vous que la SIM a du crédit et du réseau.");
  }
}

void setup() {
  Serial.begin(115200);
  sim868.begin(115200, SERIAL_8N1, RXD_PIN, TXD_PIN);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  delay(2000);
  Serial.println("\n===========================================");
  Serial.println("    SYSTÈME PRÊT ! APPUYEZ SUR LE BOUTON   ");
  Serial.println("===========================================");
  
  sendAT("AT", 1000);

  // Configuration du profil GPRS Orange RDC
  sendAT("AT+SAPBR=3,1,\"Contype\",\"GPRS\"", 1000);
  sendAT("AT+SAPBR=3,1,\"APN\",\"orange\"", 1000);
  sendAT("AT+SAPBR=1,1", 3000);
}

void loop() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(50);
    if (digitalRead(BUTTON_PIN) == LOW) {
      sendLocationSMS();
      while (digitalRead(BUTTON_PIN) == LOW) delay(10);
      delay(1000);
    }
  }
}