//Libs
#include <Arduino.h>

#include <WiFi.h>

#include <StrokeEngine.h>

#include <ArduinoJson.h>

//Local Headers
#include "main.h"
#include "config.h"

#include "mongoose.h"
#include "mongoose_config.h"  // Include the custom Mongoose config


#include "index_html.h"

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
  .enablePin = STEPPER_ENABLE,           // Pin of the enable signal
};

static machineGeometry strokingMachine = {
  .physicalTravel = PHYSICAL_TRAVEL,            // Real physical travel from one hard endstop to the other
  .keepoutBoundary = KEEPOUT_DISTANCE             // Safe distance the motion is constrained to avoiding crashes
};

// Configure Homing Procedure
static endstopProperties endstop = {
  .homeToBack = true,                 // Endstop sits at the rear of the machine
  .activeLow = false,                  // switch is wired active low
  .endstopPin = STEPPER_HOME,        // Pin number
  .pinMode = INPUT_PULLDOWN          // pinmode INPUT with external pull-up resistor
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

void setup() {
    // Start Serial For Debug
    Serial.begin(9600);

    delay(1000);

    Stroker.begin(&strokingMachine, &servoMotor);


    // Connect To Wifi
    WiFi.setHostname(hostname);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi..");
    }
    Serial.println("Connected to WiFi");
    
    mg_mgr_init(&mgr);

    // Create HTTP listener
    mg_http_listen(&mgr, "http://0.0.0.0:80", ev_handler, NULL);

}

void loop() {
    mg_mgr_poll(&mgr, 1000);
}

void ev_handler(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;
        if (mg_match(hm->uri, mg_str("/"), NULL)) {
            mg_http_reply(c, 200, "Content-Type: text/html\r\n", index_html);
        } else if (mg_match(hm->uri, mg_str("/ws"), NULL)) {
            mg_ws_upgrade(c, hm, NULL);
        }
         else {
            mg_http_reply(c, 404, "Content-Type: text/html\r\n", "<h1>Not Found</h1>");
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

void WSConnect(struct mg_connection *c, int ev, void *ev_data) {
    Serial.println("Client WS Connected");

    struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;

    JsonDocument StrokerState;

    StrokerState["type"] = "batch";
    StrokerState["speed"] = Stroker.getSpeed();
    StrokerState["depth"] = Stroker.getDepth();
    StrokerState["stroke"] = Stroker.getStroke();
    StrokerState["sensation"] = Stroker.getSensation();

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
            Stroker.setSpeed(speed, immediate);
            Serial.printf("Setting Speed: %.2f\n", speed);
        }

        if (doc["type"] == "depth") {
            float depth = doc["value"];
            bool immediate = doc["immediate"];
            Stroker.setDepth(depth, immediate);
            Serial.printf("Setting Depth: %.2f\n", depth);
        }
        
        if (doc["type"] == "stroke") {
            float stroke = doc["value"];
            bool immediate = doc["immediate"];
            Stroker.setStroke(stroke, immediate);
            Serial.printf("Setting Stroke: %.2f\n", stroke);
        }

        if (doc["type"] == "sensation") {
            float sensation = doc["value"];
            bool immediate = doc["immediate"];
            Stroker.setSensation(sensation, immediate);
            Serial.printf("Setting Sensation: %.2f\n", sensation);
        }
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