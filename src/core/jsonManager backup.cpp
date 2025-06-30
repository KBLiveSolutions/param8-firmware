// #include "jsonManager.h"

// JsonManager json("/data.json");

// JsonManager::JsonManager(const char* path)
//     : _path(path) {}

// bool JsonManager::load() {
//     File file = LittleFS.open(_path, "r");
//     Serial.printf("Loading settings from %s\n", _path);
//     if (!file) {
//         Serial.println("Failed to open settings file");
//         return false;
//     }
//     Serial.println("Settings file opened successfully");
//     DeserializationError error = deserializeJson(_doc, file);
//     file.close();
//     return !error;
// }

// bool JsonManager::save() {
//     File file = LittleFS.open(_path, "w");
//     if (!file) return false;
//     bool ok = (serializeJsonPretty(_doc, file) > 0);
//     file.close();
//     return ok;
// }

// int JsonManager::getMode() {
//     return _doc["mode"] | 0;
// }

// void JsonManager::setMode(int mode) {
//     _doc["mode"] = mode;
// }

// JsonObject JsonManager::getPreset(uint8_t idx) {
//     return _doc[String(idx)].as<JsonObject>();
// }

// JsonArray JsonManager::getButtonShort(uint8_t preset, uint8_t btn) {
//     return _doc[String(preset)]["buttons_short"][String(btn)].as<JsonArray>();
// }

// void JsonManager::setButtonShort(uint8_t preset, uint8_t btn, int v0, int v1) {
//     JsonArray arr = _doc[String(preset)]["buttons_short"][String(btn)].to<JsonArray>();
//     arr[0] = v0;
//     arr[1] = v1;
// }

// JsonArray JsonManager::getButtonLong(uint8_t preset, uint8_t btn) {
//     return _doc[String(preset)]["buttons_long"][String(btn)].as<JsonArray>();
// }

// void JsonManager::setButtonLong(uint8_t preset, uint8_t btn, int v0, int v1) {
//     JsonArray arr = _doc[String(preset)]["buttons_long"][String(btn)].to<JsonArray>();
//     arr[0] = v0;
//     arr[1] = v1;
// }

// JsonArray JsonManager::getEncoder(uint8_t preset, uint8_t enc) {
//     return _doc[String(preset)]["encoders"][String(enc)].as<JsonArray>();
// }

// void JsonManager::getEncoderArray(uint8_t preset, uint8_t enc, int result[2]) {
//     JsonArray arr = _doc[String(preset)]["encoders"][String(enc)].as<JsonArray>();
//     result[0] = arr[0] | 0;  // Default to 0 if null
//     result[1] = arr[1] | 0;  // Default to 0 if null
// }

// ControlData JsonManager::getControlData(const char* control_type, uint8_t preset, uint8_t enc) {
//     JsonArray arr = _doc[String(preset)][control_type][String(enc)].as<JsonArray>();
//     ControlData data;
//     data.value0 = arr[0] | 0;  // Default to 0 if null
//     data.value1 = arr[1] | 0;  // Default to 0 if null
//     data.value2 = arr[2] | 0;  // Default to 0 if null
//     return data;
// }

// void JsonManager::setEncoder(uint8_t preset, uint8_t enc, int v0, int v1) {
//     JsonArray arr = _doc[String(preset)]["encoders"][String(enc)].to<JsonArray>();
//     arr[0] = v0;
//     arr[1] = v1;
// }

// void setupJsonManager() {
//     Serial.println("Initializing LittleFS...");
//     if (!LittleFS.begin()) {
//         Serial.println("Failed to mount LittleFS");
//         return;
//     }
//     Serial.println("LittleFS mounted successfully");
//     // File root = LittleFS.open("/");
//     // File file = root.openNextFile();
//     // while (file) {
//     //     Serial.println(file.name());
//     //     file = root.openNextFile();
//     // }
//     if (json.load()) {
//         int mode = json.getMode();
//         // ...
//     }
// }