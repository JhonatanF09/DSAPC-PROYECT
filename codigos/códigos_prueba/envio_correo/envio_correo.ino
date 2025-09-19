#include <Arduino.h>
#include <WiFi.h>
#include <ESP_Mail_Client.h>

#define WIFI_SSID "iPhone de Jhonatan"
#define WIFI_PASSWORD "xxxx"

#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465

/* Datos de acceso a cuenta. */
#define AUTHOR_EMAIL "xxxxx@gmail.com"
#define AUTHOR_PASSWORD "xxx xxx xxxx xxxx"

/* Correo electrÃ³nico del recipiente*/
#define RECIPIENT_EMAIL "xxxx@gmail.com"

/* Objeto SMTP para enviar el correo electrÃ³nico */
SMTPSession smtp;

/* Busca el estado del correo enviado. */
void smtpCallback(SMTP_Status status);

void setup(){
  Serial.begin(115200);
  Serial.println();
  Serial.print("Conectando a punto de acceso");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(200);
  }
  Serial.println("");
  Serial.println("Conectado a WiFi.");
  Serial.println("DirecciÃ³n IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  /** Enable the debug via Serial port
   * none debug or 0
   * basic debug or 1
  */
  smtp.debug(1);

  /* Solicita resultados de envÃ­o */
  smtp.callback(smtpCallback);

  /* Configura datos de sesiÃ³n */
  ESP_Mail_Session session;

  /* Configura la sesiÃ³n */
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_EMAIL;
  session.login.password = AUTHOR_PASSWORD;
  session.login.user_domain = "";

  /* Declara la clase del mensaje */
  SMTP_Message message;

  /* Configura cabecera del mensaje */
  message.sender.name = "Tu ESP32";
  message.sender.email = AUTHOR_EMAIL;
  message.subject = "Correo de prueba ESP";
  message.addRecipient("Ingeniero", RECIPIENT_EMAIL);
 
  //Manda texto
  String textMsg = "Hola Mundo. - Estamos Domando Ingenieria desde la tarjeta ESP32. Â¡Like, comparte y suscribete!";
  message.text.content = textMsg.c_str();
  message.text.charSet = "us-ascii";
  message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  
  message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;
  message.response.notify = esp_mail_smtp_notify_success | esp_mail_smtp_notify_failure | esp_mail_smtp_notify_delay;

  /* Configura cabecera personalizada */
  //message.addHeader("Message-ID: <abcde.fghij@gmail.com>");

  /* Conecta al servidor */
  if (!smtp.connect(&session))
    return;

  /* Manda correo y cierra sesiÃ³n */
  if (!MailClient.sendMail(&smtp, &message))
    Serial.println("Error sending Email, " + smtp.errorReason());
}

void loop(){

}

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status){
  /* Print the current status */
  Serial.println(status.info());

  /* Print the sending result */
  if (status.success()){
    Serial.println("----------------");
    ESP_MAIL_PRINTF("Message sent success: %d\n", status.completedCount());
    ESP_MAIL_PRINTF("Message sent failled: %d\n", status.failedCount());
    Serial.println("----------------\n");
    struct tm dt;

    for (size_t i = 0; i < smtp.sendingResult.size(); i++){
      /* Get the result item */
      SMTP_Result result = smtp.sendingResult.getItem(i);
      time_t ts = (time_t)result.timestamp;
      localtime_r(&ts, &dt);

      ESP_MAIL_PRINTF("Message No: %d\n", i + 1);
      ESP_MAIL_PRINTF("Status: %s\n", result.completed ? "success" : "failed");
      ESP_MAIL_PRINTF("Date/Time: %d/%d/%d %d:%d:%d\n", dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
      ESP_MAIL_PRINTF("Recipient: %s\n", result.recipients);
      ESP_MAIL_PRINTF("Subject: %s\n", result.subject);
    }
    Serial.println("----------------\n");
  }
}
