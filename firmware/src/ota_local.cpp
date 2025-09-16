#include "global.h"

// === MDNS Configuration ===
void ota_mdns_init() {
    // Start the mDNS responder for using HOSTNAME_ESP.local
    if (!MDNS.begin(vp.hostname)) { 
        Serial.println("[mDNS] Error setting up mDNS responder!!");
    } else {
        Serial.println("[mDNS] DNS responder started!");
    }
}

// === OTA Configuration ===
void ota_init() {
    ArduinoOTA.setPort(OTA_PORT);
    ArduinoOTA.setHostname(vp.hostname);

    // No authentication by default
    #ifdef OTA_PASSWORD
        ArduinoOTA.setPassword(OTA_PASSWORD);

        // Password can be set with it's md5 value as well
        // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
        // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");
    #endif

    // Set up callbacks
    ArduinoOTA
        .onStart([]() {
            String type;
            if (ArduinoOTA.getCommand() == U_FLASH)
                type = "sketch";
            else // U_SPIFFS
                type = "filesystem";
            
            Serial.println("[OTA] Start updating " + type);
        })

        .onEnd([]() {
            Serial.println("\n[OTA] Update successful!");
        })
        
        .onProgress([](unsigned int progress, unsigned int total) {
            Serial.printf("[OTA] Progress: %u%%\r", (progress / (total / 100)));
        })

        .onError([](ota_error_t error) {
            Serial.printf("[OTA] Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR) Serial.println("[OTA] Auth Failed");
            else if (error == OTA_BEGIN_ERROR) Serial.println("[OTA] Begin Failed");
            else if (error == OTA_CONNECT_ERROR) Serial.println("[OTA] Connect Failed");
            else if (error == OTA_RECEIVE_ERROR) Serial.println("[OTA] Receive Failed");
            else if (error == OTA_END_ERROR) Serial.println("[OTA] End Failed");
            else Serial.println("[OTA] Unknown Error");
        });
    
    ArduinoOTA.begin();
    Serial.println(
        "[OTA] Initialized, Hostname: " 
        + String(ArduinoOTA.getHostname())
        + ", Port: " + String(OTA_PORT)
    );
}
