#ifndef MAIN_H
#define MAIN_H


const char* hostname = "backdoor";
const char* ssid = "SSID";
const char* password = "PASSWORD";

void ev_handler(struct mg_connection *c, int ev, void *ev_data);
void WSConnect(struct mg_connection *c, int ev, void *ev_data);
void WSDisconnect(struct mg_connection *c, int ev, void *ev_data);
void WSMessage(struct mg_connection *c, int ev, void *ev_data);

void ProcessCommand(struct mg_connection *c, JsonDocument doc);

#endif
