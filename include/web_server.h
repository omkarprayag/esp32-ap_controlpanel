#ifndef WEB_SERVER_H
#define WEB_SERVER_H

void initWebServer();
void initWebSocket();
void handleClients();
void handle_OnConnect();
void handle_led1on();
void handle_led1off();
void handle_led2on();
void handle_led2off();
void handle_temperature();
void handle_NotFound();
void handleGPIOControl();
String SendHTML(uint8_t led1stat, uint8_t led2stat);

#endif