#include "core/jsonManager.h"

JsonManager json("/data.json");

JsonManager::JsonManager(const char* path)
    : _path(path) {}

bool JsonManager::load() {
    File file = LittleFS.open(_path, "r");
    Serial.printf("Loading settings from %s\n", _path);
    if (!file) {
        Serial.println("Failed to open settings file");
        return false;
    }
    Serial.println("Settings file opened successfully");
    DeserializationError error = deserializeJson(_doc, file);
    file.close();
    return !error;
}

bool JsonManager::save() {
    File file = LittleFS.open(_path, "w");
    if (!file) return false;
    bool ok = (serializeJsonPretty(_doc, file) > 0);
    file.close();
    return ok;
}

int JsonManager::getMode() {
    return _doc["mode"] | 0;
}

void JsonManager::setMode(int mode) {
    _doc["mode"] = mode;
}

JsonObject JsonManager::getPreset(uint8_t idx) {
    return _doc[String(idx)].as<JsonObject>();
}

JsonArray JsonManager::getButtonShort(uint8_t preset, uint8_t btn) {
    return _doc[String(preset)]["buttons_short"][String(btn)].as<JsonArray>();
}

void JsonManager::setButtonShort(uint8_t preset, uint8_t btn, int v0, int v1) {
    JsonArray arr = _doc[String(preset)]["buttons_short"][String(btn)].to<JsonArray>();
    arr[0] = v0;
    arr[1] = v1;
}

JsonArray JsonManager::getButtonLong(uint8_t preset, uint8_t btn) {
    return _doc[String(preset)]["buttons_long"][String(btn)].as<JsonArray>();
}

void JsonManager::setButtonLong(uint8_t preset, uint8_t btn, int v0, int v1) {
    JsonArray arr = _doc[String(preset)]["buttons_long"][String(btn)].to<JsonArray>();
    arr[0] = v0;
    arr[1] = v1;
}

JsonArray JsonManager::getEncoder(uint8_t preset, uint8_t enc) {
    return _doc[String(preset)]["encoders"][String(enc)].as<JsonArray>();
}

void JsonManager::getEncoderArray(uint8_t preset, uint8_t enc, int result[2]) {
    JsonArray arr = _doc[String(preset)]["encoders"][String(enc)].as<JsonArray>();
    result[0] = arr[0] | 0;  // Default to 0 if null
    result[1] = arr[1] | 0;  // Default to 0 if null
}

ControlData JsonManager::getControlData(const char* control_type, uint8_t preset, uint8_t enc) {
    JsonArray arr = _doc[String(preset)][control_type][String(enc)].as<JsonArray>();
    ControlData data;
    data.value0 = arr[0] | 0;  // Default to 0 if null
    data.value1 = arr[1] | 0;  // Default to 0 if null
    data.value2 = arr[2] | 0;  // Default to 0 if null
    return data;
}

void JsonManager::setEncoder(uint8_t preset, uint8_t enc, int v0, int v1) {
    JsonArray arr = _doc[String(preset)]["encoders"][String(enc)].to<JsonArray>();
    arr[0] = v0;
    arr[1] = v1;
}

void setupJsonManager() {
    Serial.println("Initializing LittleFS...");
    if (!LittleFS.begin()) {
        Serial.println("ERROR: Failed to mount LittleFS");
        return;
    }
    Serial.println("LittleFS mounted successfully");
    
    // Load JSON configuration
    Serial.println("Loading JSON configuration...");
    if (json.load()) {
        int mode = json.getMode();
        Serial.print("JSON loaded successfully, mode: ");
        Serial.println(mode);
        
        // Test: Vérifier quelques valeurs
        ControlData testData = json.getControlData("encoders", 0, 0);
        Serial.print("Test encoder[0][0]: type=");
        Serial.print(testData.value0);
        Serial.print(", number=");
        Serial.print(testData.value1);
        Serial.print(", channel=");
        Serial.println(testData.value2);
    } else {
        Serial.println("ERROR: Failed to load JSON configuration");
        Serial.println("Trying to create default data.json file...");
        
        // Créer un fichier de configuration par défaut si il n'existe pas
        File defaultFile = LittleFS.open("/data.json", "w");
        if (defaultFile) {
            defaultFile.print(R"({
  "mode": 2,
  "0": {
    "buttons_short": {
      "0": [0, 20, 0], "1": [0, 21, 0], "2": [0, 22, 0], "3": [0, 23, 0],
      "4": [0, 24, 0], "5": [0, 25, 0], "6": [0, 26, 0], "7": [0, 27, 0]
    },
    "buttons_long": {
      "0": [0, 30, 0], "1": [0, 31, 0], "2": [0, 32, 0], "3": [0, 33, 0],
      "4": [0, 34, 0], "5": [0, 35, 0], "6": [0, 36, 0], "7": [0, 37, 0]
    },
    "encoders": {
      "0": [0, 10, 0], "1": [0, 11, 0], "2": [0, 12, 0], "3": [0, 13, 0],
      "4": [0, 14, 0], "5": [0, 15, 0], "6": [0, 16, 0], "7": [0, 17, 0]
    }
  }
})");
            defaultFile.close();
            Serial.println("Default data.json created, retrying load...");
            
            // Retry loading
            if (json.load()) {
                Serial.println("JSON loaded successfully after creating default file");
            }
        }
    }
}