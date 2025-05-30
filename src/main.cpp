//Libs
#include <Arduino.h>

#include <WiFi.h>

#include <Preferences.h>

#include <StrokeEngine.h>

#include <ArduinoJson.h>

//Local Headers
#include "main.h"
#include "config.h"

#include "mongoose.h"
#include "mongoose_config.h"  // Include the custom Mongoose config


#include "index_html.h"
#include "script_js.h"



//StrokeEngine
static motorProperties servoMotor {
  .maxSpeed = MAX_SPEED,                // Maximum speed the system can go in mm/s
  .maxAcceleration = MAX_ACCEL,         // Maximum linear acceleration in mm/s²
  .stepsPerMillimeter = STEP_PER_MM,    // Steps per millimeter 
  .invertDirection = false,             // One of many ways to change the direction,  
                                        // should things move the wrong way
  .enableActiveLow = true,              // Polarity of the enable signal      
  .stepPin = STEPPER_PULSE,             // Pin of the STEP signal
  .directionPin = STEPPER_DIR,          // Pin of the DIR signal
  .enablePin = STEPPER_ENABLE,          // Pin of the enable signal
};

static machineGeometry strokingMachine = {
  .physicalTravel = PHYSICAL_TRAVEL,            // Real physical travel from one hard endstop to the other
  .keepoutBoundary = KEEPOUT_DISTANCE           // Safe distance the motion is constrained to avoiding crashes
};

// Configure Homing Procedure
static endstopProperties endstop = {
  .homeToBack = true,                 // Endstop sits at the rear of the machine
  .activeLow = true,                  // switch is wired active low
  .endstopPin = STEPPER_HOME,         // Pin number
  .pinMode = INPUT                    // pinmode INPUT with external pull-up resistor
};

static String stringState[] = {
  "Servo disabled",
  "Servo ready",
  "Servo pattern running",
  "Servo setup depth",
  "Servo position streaming"
};

StrokeEngine Stroker;

struct mg_mgr mgr;

Preferences settings;

struct UserLimit {
    int min;
    int max;
};

struct StrokerUserLimits {
    UserLimit speed;
    UserLimit depth;
    UserLimit stroke;
    UserLimit sensation;
};

StrokerUserLimits strokerUserLimits;

IPAddress DeviceIP;
bool APEnabled = false;

void setup() {
    // Start Serial For Debug
    Serial.begin(9600);
    delay(2000);

    settings.begin("BackdoorMachine", false); 
    String ssid = settings.getString("ssid", ""); 
    String password = settings.getString("password", "");
    settings.end();
    
    // Connect To Wifi
    WiFi.setHostname(hostname);

    Serial.println("Connecting to WiFi (STA Mode)...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    unsigned long startTime = millis();

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        if (millis() - startTime > 15000) {
            Serial.println("\nConnection failed.");
            WiFi.disconnect(true);
            delay(500);
            APEnabled = true;
            break;
        }
    }

    if (!APEnabled) {
        Serial.println("\nConnected!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        DeviceIP = WiFi.localIP();
    } else {
        Serial.println("Setting up AP Mode...");
        WiFi.mode(WIFI_AP);
        WiFi.softAP("Backdoor_Machine");
        delay(500);
        Serial.print("IP: ");
        Serial.println(WiFi.softAPIP());
        DeviceIP = WiFi.softAPIP();
    }



    // Initial User Limits
    strokerUserLimits.speed = {0, 200};
    strokerUserLimits.depth = {0, 200};
    strokerUserLimits.stroke = {0, 200};
    strokerUserLimits.sensation = {-100, 100};

    Stroker.begin(&strokingMachine, &servoMotor);

    
    mg_mgr_init(&mgr);

    // Create HTTP listener
    mg_http_listen(&mgr, "http://0.0.0.0:80", ev_handler, NULL);
    if (APEnabled) {
        mg_listen(&mgr, "udp://0.0.0.0:53", ev_handler_dns, NULL);
    }
}

void loop() {
    mg_mgr_poll(&mgr, 1500);
}

static bool isIp(const String &str) {
  for (size_t i = 0; i < str.length(); i++) {
    int c = str.charAt(i);
    if (c != '.' && (c < '0' || c > '9')) {
      return false;
    }
  }
  return true;
}

void ev_handler(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;
        struct mg_str *host_header = mg_http_get_header(hm, "Host");
        String host_addr(host_header->buf, host_header->len);

        if (host_addr.indexOf(F("jackhammer.lan")) >= 0 || isIp(host_addr) || !APEnabled)
        {
            if (mg_match(hm->uri, mg_str("/"), NULL)) {
                mg_http_reply(c, 200, "Content-Type: text/html\r\n", index_html);
            } else if (mg_match(hm->uri, mg_str("/script.js"), NULL)) {
                mg_http_reply(c, 200, "Content-Type: text/javascript\r\n", script_js);
            } else if (mg_match(hm->uri, mg_str("/bmws"), NULL)) {
                mg_ws_upgrade(c, hm, NULL);
            } else {
                mg_http_reply(c, 404, "Content-Type: text/html\r\n", "<h1>Not Found</h1>");
            }
        } else {
            char redirect_header[100];
            snprintf(redirect_header, sizeof(redirect_header), "Location: http://%s/\r\nContent-Length: 0\r\n", "jackhammer.lan");
            mg_http_reply(c, 302, redirect_header,"");
        }
    } else if (ev == MG_EV_WS_OPEN) {
        WSConnect(c, ev, ev_data);
    } else if (ev == MG_EV_CLOSE) {
        if (c->is_websocket) {
            WSDisconnect(c, ev, ev_data);
        }
    } else if (ev == MG_EV_WS_MSG) {
        WSMessage(c, ev, ev_data);
    }
}



uint8_t answer[] = {
    0xc0, 0x0c,          // Point to the name in the DNS question
    0,    1,             // 2 bytes - record type, A
    0,    1,             // 2 bytes - address class, INET
    0,    0,    0, 120,  // 4 bytes - TTL
    0,    4,             // 2 bytes - address lengthpersistance
    192,168,4,1          // 4 bytes - IP address
};

static void ev_handler_dns(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_OPEN) {
        //c->is_hexdumping = 1;
    } else if (ev == MG_EV_READ) {
    struct mg_dns_rr rr;  // Parse first question, offset 12 is header size
    size_t n = mg_dns_parse_rr(c->recv.buf, c->recv.len, 12, true, &rr);
    if (n > 0) {
        char buf[512];
        struct mg_dns_header *h = (struct mg_dns_header *) buf;
        memset(buf, 0, sizeof(buf));  // Clear the whole datagram
        h->txnid = ((struct mg_dns_header *) c->recv.buf)->txnid;  // Copy tnxid
        h->num_questions = mg_htons(1);  // We use only the 1st question
        h->num_answers = mg_htons(1);    // And only one answer
        h->flags = mg_htons(0x8400);     // Authoritative response
        memcpy(buf + sizeof(*h), c->recv.buf + sizeof(*h), n);  // Copy question
        memcpy(buf + sizeof(*h) + n, answer, sizeof(answer));   // And answer
        mg_send(c, buf, 12 + n + sizeof(answer));               // And send it!
    }
    mg_iobuf_del(&c->recv, 0, c->recv.len);
    }
}

void WSConnect(struct mg_connection *c, int ev, void *ev_data) {
    Serial.println("Client WS Connected");

    struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;

    JsonDocument StrokerState;

    StrokerState["type"] = "batch";
    StrokerState["speed"] = Stroker.getSpeed();
    StrokerState["speedMin"] = strokerUserLimits.speed.min;
    StrokerState["speedMax"] = strokerUserLimits.speed.max;
    StrokerState["depth"] = Stroker.getDepth();
    StrokerState["depthMin"] = strokerUserLimits.depth.min;
    StrokerState["depthMax"] = strokerUserLimits.depth.max;
    StrokerState["stroke"] = Stroker.getStroke();
    StrokerState["strokeMin"] = strokerUserLimits.stroke.min;
    StrokerState["strokeMax"] = strokerUserLimits.stroke.max;
    StrokerState["sensation"] = Stroker.getSensation();
    StrokerState["sensationMin"] = strokerUserLimits.sensation.min;
    StrokerState["sensationMax"] = strokerUserLimits.sensation.max;

    switch (Stroker.getPattern())
    {
    case 0:
        StrokerState["pattern"] = "SimpleStroke";
        break;
    case 1:
        StrokerState["pattern"] = "TeasingPounding";
        break;
    case 2:
        StrokerState["pattern"] = "RoboStroke";
        break;
    case 3:
        StrokerState["pattern"] = "HalfnHalf";
        break;
    case 4:
        StrokerState["pattern"] = "Deeper";
        break;
    case 5:
        StrokerState["pattern"] = "StopNGo";
        break;
    case 6:
        StrokerState["pattern"] = "Insist";
        break;
    case 7:
        StrokerState["pattern"] = "JackHammer";
        break;
    case 8:
        StrokerState["pattern"] = "StrokeNibbler";
        break;
    default:
        break;
    }

    StrokerState["state"] = stringState[Stroker.getState()];
    StrokerState["code"] = Stroker.getState();
    
    String StrokerStateString;
    serializeJson(StrokerState, StrokerStateString);
    mg_ws_send(c, StrokerStateString.c_str(), StrokerStateString.length(), WEBSOCKET_OP_TEXT);
}


void WSMessage(struct mg_connection *c, int ev, void *ev_data) {
    struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;

    char msgtype = wm->flags & 0x0F;
    bool fin = wm->flags & 0x80;
    if (msgtype == WEBSOCKET_OP_TEXT && fin) {

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, wm->data.buf, wm->data.len);

        if (error) {
            Serial.print("deserializeJson() returned ");
            Serial.println(error.c_str());
        } else {
            ProcessCommand(c, doc);
        }
    }

    if (msgtype == WEBSOCKET_OP_PING && fin) {
        mg_ws_send(c, wm->data.buf, wm->data.len, WEBSOCKET_OP_PONG);
    }
}

void WSDisconnect(struct mg_connection *c, int ev, void *ev_data) {
    struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;
    Serial.println("Client WS Disconnected");
}


void ProcessCommand(struct mg_connection *c, JsonDocument doc) {
    //if Homed and Enabled set Params
    if (Stroker.getState() != UNDEFINED) {

        if (doc["type"] == "speed") {
            float speed = doc["value"];
            bool immediate = doc["immediate"];
            Stroker.setSpeed(constrain(speed, static_cast<float>(strokerUserLimits.speed.min), static_cast<float>(strokerUserLimits.speed.max)), immediate);
            Serial.printf("Setting Speed: %.2f\n", speed);
        }

        if (doc["type"] == "depth") {
            float depth = doc["value"];
            bool immediate = doc["immediate"];
            Stroker.setDepth(constrain(depth, static_cast<float>(strokerUserLimits.depth.min), static_cast<float>(strokerUserLimits.depth.max)), immediate);
            Serial.printf("Setting Depth: %.2f\n", depth);
        }
        
        if (doc["type"] == "stroke") {
            float stroke = doc["value"];
            bool immediate = doc["immediate"];
            Stroker.setStroke(constrain(stroke, static_cast<float>(strokerUserLimits.stroke.min), static_cast<float>(strokerUserLimits.stroke.max)), immediate);
            Serial.printf("Setting Stroke: %.2f\n", stroke);
        }

        if (doc["type"] == "sensation") {
            float sensation = doc["value"];
            bool immediate = doc["immediate"];
            Stroker.setSensation(constrain(sensation, static_cast<float>(strokerUserLimits.sensation.min), static_cast<float>(strokerUserLimits.sensation.max)), immediate);
            Serial.printf("Setting Sensation: %.2f\n", sensation);
        }

    }

    if (doc["type"] == "speedMin") {
        int value = doc["value"];
        strokerUserLimits.speed.min = value;
    }

    if (doc["type"] == "speedMax") {
        int value = doc["value"];
        strokerUserLimits.speed.max = value;
    }
    
    if (doc["type"] == "depthMin") {
        int value = doc["value"];
        strokerUserLimits.depth.min = value;
    }

    if (doc["type"] == "depthMax") {
        int value = doc["value"];
        strokerUserLimits.depth.max = value;
    }
    
    if (doc["type"] == "strokeMin") {
        int value = doc["value"];
        strokerUserLimits.stroke.min = value;
    }

    if (doc["type"] == "strokeMax") {
        int value = doc["value"];
        strokerUserLimits.stroke.max = value;
    }

    if (doc["type"] == "sensationMin") {
        int value = doc["value"];
        strokerUserLimits.sensation.min = value;
    }

    if (doc["type"] == "sensationMax") {
        int value = doc["value"];
        strokerUserLimits.sensation.max = value;
    }

    if (doc["type"] == "home") {
        if (doc["value"] == 1) {
            Stroker.enableAndHome(&endstop);
            Stroker.setDepth(0.0, false);
            Stroker.setStroke(0.0, false);
            Stroker.setSpeed(0.5, false);
            Serial.println("Setting Home");
        }
    }

    if (doc["type"] == "wifiSSID") {
        String value = doc["value"];
        settings.begin("BackdoorMachine", false); 
        settings.putString("ssid", value); 
        settings.end();
    }

    if (doc["type"] == "wifiPassword") {
        String value = doc["value"];
        settings.begin("BackdoorMachine", false); 
        settings.putString("password", value); 
        settings.end();
    }

    if (doc["type"] == "alive") {
        String Reply;
        Reply = doc["value"].as<String>();

        JsonDocument StrokerAlive;
        StrokerAlive["type"] = "alive";
        StrokerAlive["value"] = Reply;
        StrokerAlive["state"] = stringState[Stroker.getState()];
        StrokerAlive["code"] = Stroker.getState();

        String StrokerAliveString;
        serializeJson(StrokerAlive, StrokerAliveString);

        mg_ws_send(c, StrokerAliveString.c_str(), StrokerAliveString.length(), WEBSOCKET_OP_TEXT);
        //Serial.println("Responded to Alive Message");
    }

    if (doc["type"] == "run") {
        if (doc["value"] == 1) {
            bool error = Stroker.startPattern();
            if (error) {
                Serial.println("Start Stroking Error");
            }
            Serial.println("Starting Stroking");
        } else if (doc["value"] == 0) {
            Stroker.stopMotion();
            Serial.println("Stopping Stroking");
        } else {
            Stroker.disable();
            Serial.println("Disabling Motors");
        }
    }

    if (doc["type"] == "pattern") {
        Serial.printf("Setting Pattern to %s\n", doc["value"].as<String>());
        if (doc["value"] == "Depth") { Stroker.setupDepth(10.0F, false);};
        if (doc["value"] == "FancyDepth") { Stroker.setupDepth(10.0F, true);};
        if (doc["value"] == "SimpleStroke") { Stroker.setPattern(0, false);};
        if (doc["value"] == "TeasingPounding") { Stroker.setPattern(1, false);};
        if (doc["value"] == "RoboStroke") { Stroker.setPattern(2, false);};
        if (doc["value"] == "HalfnHalf") { Stroker.setPattern(3, false);};
        if (doc["value"] == "Deeper") { Stroker.setPattern(4, false);};
        if (doc["value"] == "StopNGo") { Stroker.setPattern(5, false);};
        if (doc["value"] == "Insist") { Stroker.setPattern(6, false);};
        if (doc["value"] == "JackHammer") { Stroker.setPattern(7, false);};
        if (doc["value"] == "StrokeNibbler") { Stroker.setPattern(8, false);};
        
    }
    //Serial.printf("Status: %s\n", verboseState[Stroker.getState()]);
}
