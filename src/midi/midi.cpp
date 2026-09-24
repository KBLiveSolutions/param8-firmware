#include <Arduino.h>
#include "midi.h"
#include "../core/controls.h"
#include "../core/actions.h"
#include "../core/version.h"
#include "../view/display.h"
#include "../core/jsonManager.h"
#include "../usb/serial_editor.h"

bool liveConnected = false;
bool isSysExContinued = false;
unsigned long liveConnectedTime = 0;
size_t sysExBufferSize = 0;
uint8_t sysExBuffer[SYSEX_BUFFER_SIZE];

void setupMIDI()
{
  USBDevice.setManufacturerDescriptor("KBD");
  USBDevice.setProductDescriptor("param8");
  usb_midi.begin();
  // usb_midi.setHandleNoteOn([](uint8_t channel, uint8_t note, uint8_t velocity) {
  //   Serial.printf("Note On: Channel %d, Note %d, Velocity %d\n", channel, note, velocity);
  // });
  // usb_midi.setHandleNoteOff([](uint8_t channel, uint8_t note, uint8_t velocity) {
  //   Serial.printf("Note Off: Channel %d, Note %d, Velocity %d\n", channel, note, velocity);
  // });
  // USBMIDI.setHandleControlChange([](uint8_t channel, uint8_t control, uint8_t value) {
  //   Serial.printf("Control Change: Channel %d, Control %d, Value %d\n", channel, control, value);
  // });
  // usb_midi.setHandleProgramChange([](uint8_t channel, uint8_t program) {
  //   Serial.printf("Program Change: Channel %d, Program %d\n", channel, program);
  // });
}

// void sendMidiMessage(uint8_t type, uint8_t number, uint8_t value, uint8_t channel) {
//       switch (type) {
//         case MIDI_CC:
//             usb_midi.send(0xB0 | channel, number, value);
//             break;
//         case MIDI_NOTE:
//             usb_midi.sendNoteOn(number, value, channel);
//             delay(10);
//             usb_midi.send(0x80 | channel, number, value);
//             break;
//         case MIDI_PC:
//             usb_midi.send(0xC0 | channel, number);
//             break;
//     }
// }

void sendMidiMessage(uint8_t type, uint8_t number, uint8_t value, uint8_t channel)
{
  lastInputTime = millis();
  if (screenSaverActive)
  {
    screenSaverActive = false;
    showDisplay(); // réaffiche l'UI normale
  }
  uint8_t packet[4] = {0x0A, 0, 0, 0};
  // Serial.print("Sending MIDI: ");
  // Serial.print(type);
  // Serial.print(", Number: ");
  // Serial.print(number);
  // Serial.print(", Value: ");
  // Serial.println(value);
  switch (type)
  {
  case MIDI_NOTE:
    packet[1] = (uint8_t)(0x90 | (channel & 0x0F));
    packet[2] = number;
    packet[3] = value;
    usb_midi.writePacket(packet);
    break;
  case MIDI_CC:
    packet[1] = (uint8_t)(0xB0 | (channel & 0x0F));
    packet[2] = number;
    packet[3] = value;
    usb_midi.writePacket(packet);
    break;
  case MIDI_PC:
    uint8_t pcPacket[3] = {0x0A, (uint8_t)(0xC0 | (channel & 0x0F)), number};
    usb_midi.write(pcPacket[1]);
    usb_midi.write(pcPacket[2]);
    break;
  }
}

void midiRead()
{
  int maxReads = 64;
  while (usb_midi.available() && --maxReads > 0)
  {
    uint8_t packet[MIDI_MAX_PACKET_SIZE];
    usb_midi.readPacket(packet);
    uint8_t cin = packet[0] & 0x0F;
    bool isSysExPacket = (cin >= 0x04 && cin <= 0x07);

    if (isSysExPacket)
    {
      if (cin == 0x04 && packet[1] == SYSEX_START_BYTE)
        handleSysExStart(packet);
      else if (isSysExContinued)
        handleSysExContinuation(packet);
      else
        clearSysExBuffer();
    }
    else
    {
      if (isSysExContinued)
        clearSysExBuffer();
      handleMIDIDAWMessage(packet);
    }
    lastInputTime = millis();
    if (screenSaverActive)
    {
      screenSaverActive = false;
      showDisplay();
    }
  }
}

void handleMIDIDAWMessage(uint8_t *packet)
{
  uint8_t channel = (packet[1] & 0x0F);
  switch (packet[1] & 0xF0)
  {
  case 0x80: // Note off
  {
    uint8_t note = packet[2];
    onMidiValueChange(channel, note, 0);
    break;
  }
  case 0x90: // Note on
  {
    uint8_t note = packet[2];
    uint8_t velocity = packet[3];
    onMidiValueChange(channel, note, velocity);
    break;
  }
  case 0xB0: // Control change
  {
    uint8_t control = packet[2];
    uint8_t value = packet[3];
    onMidiValueChange(channel, control, value);
    break;
  }
  case 0xC0: // Program change
  {
    uint8_t program = packet[2] & 0x7F;
    if (program < 8) {
      controls.setPreset(program);
      updateFaderTitles();
      updateFaderValues();
      sendPresetSysEx(program);
      showDisplay();
      deviceLabelDirty = true;
      bankLabelDirty = true;
      drawDeviceBankLabels();
    }
    break;
  }
  case 0xE0: // Pitch Bend
  {
    uint16_t pitchBendValue = (packet[3] << 7) | packet[2];
    // Serial.print("Pitch Bend - Value: ");
    // Serial.print(pitchBendValue);
    break;
  }
  default:
    // Serial.print("Unknown MIDI message type ");
    break;
  }
}

void clearSysExBuffer()
{
  memset(sysExBuffer, 0, sizeof(sysExBuffer));
  sysExBufferSize = 0;
  isSysExContinued = false;
}

void handleSysExMessage(uint8_t *packet)
{
  for (size_t i = 1; i < MIDI_MAX_PACKET_SIZE; i++)
  {
    if (sysExBufferSize >= SYSEX_BUFFER_SIZE) {
      clearSysExBuffer();
      return;
    }
    sysExBuffer[sysExBufferSize++] = packet[i];
    if (packet[i] == SYSEX_END_BYTE)
    {
      onSysEx(sysExBuffer, sysExBufferSize);
      clearSysExBuffer();
      return;
    }
  }
}

void handleSysExStart(uint8_t *packet)
{
  clearSysExBuffer();
  handleSysExMessage(packet);
  if (sysExBufferSize > 0)
    isSysExContinued = true;
}

void handleSysExContinuation(uint8_t *packet)
{
  handleSysExMessage(packet);
}

void onMidiValueChange(uint8_t channel, uint8_t control, uint8_t value)
{
  if (value == 0 && liveConnectedTime > 0 && millis() - liveConnectedTime < 500)
    return;
  controls.onMidiValueChange(channel, control, value);
}

void onSysEx(const uint8_t *sysex, size_t len)
{
  if (len < 5)
    return; // Maintenant on a besoin d'au moins 5 octets

  uint8_t constructor_byte = sysex[1];
  uint8_t status_byte = sysex[2];
  uint8_t param_number = sysex[3];
  uint8_t staticOverlay = sysex[4];

  // Les caractères commencent maintenant à sysex[5], chaque caractère = 2 octets
  const uint8_t *char_data = sysex + 5;
  size_t char_data_len = len > 6 ? len - 6 : 0; // -6 pour F0, constructeur, status, param, staticOverlay, F7

  char ascii_string[32] = {0}; // 10 caractères max + \0
  decode_ascii_sysex(char_data, char_data_len, ascii_string, sizeof(ascii_string));

  // Gestion des différents status_byte
  switch (status_byte)
  {
  case 0:
    if (param_number < 8)
      {
        if (strlen(ascii_string) > 0)
        {
          faders[param_number]->setParamName(ascii_string);
        }
        else
        {
          faders[param_number]->setParamName("");
        }
      }
    else
    {        if (strlen(ascii_string) > 0)
        {
          faders[param_number-8]->setButtonName(ascii_string);
        }
        else
        {
          // String vide, passe une string vide
          faders[param_number-8]->setButtonName("");
        }

    }

    break;

  case 1:
    faders[param_number]->updateTitle(ascii_string);
    break;

  case 2:
    strncpy(bankLabel, ascii_string, sizeof(bankLabel));
    bankLabel[sizeof(bankLabel)-1] = '\0';
    bankLabelDirty = true;
    break;

  case 3:
    strncpy(deviceLabel, ascii_string, sizeof(deviceLabel));
    deviceLabel[sizeof(deviceLabel)-1] = '\0';
    deviceLabelDirty = true;
    break;

  case 4:
    // Track name (preset 8 / mixer live-follow), shown between the
    // "Track -" and "Track +" buttons.
    strncpy(trackLabel, ascii_string, sizeof(trackLabel));
    trackLabel[sizeof(trackLabel)-1] = '\0';
    trackLabelDirty = true;
    break;

  case 5:
  {
    bool wasConnected = liveConnected;
    liveConnected = true;
    liveConnectedTime = millis();
    uint8_t preset = controls.getPreset();
    sendPresetSysEx(preset);
    if (!wasConnected) {
      if (preset == 7 || preset == 6) {
        updateFaderTitles();
      }
      showDisplay();
    }
    break;
  }
  case 7:
  {
    controls.getPresetControls(param_number);
    break;
  }
  case 13:
  {
    Serial.println("Configuration des encoders reçue");
    // Chaque contrôle utilise 3 octets: type, number, channel
    uint8_t preset = sysex[3];
    param_number = sysex[4];
    uint8_t type = sysex[5];
    uint8_t number = sysex[6];
    uint8_t channel = sysex[7];
    controls.setEncoder(preset, param_number, MIDI_CC, number, channel);
    if (preset == controls.getPreset()) {
      updateFaderTitles();
      showDisplay();
    }
    json.save();
    break;
  }
  case 12:
  {
    Serial.println("Configuration des boutons courts reçue (nouveau format)");
    // Structure : F0, constructeur, preset, status, type, control_number, channel, toggle, F7
    uint8_t preset = sysex[3];
    param_number = sysex[4];
    ControlMidiType _type = (sysex[5] == 0) ? MIDI_CC : MIDI_NOTE;
    uint8_t control = sysex[6];
    uint8_t channel = sysex[7];
    bool toggleMode = sysex[8] != 0;
    controls.setButtonShort(preset, param_number, _type, control, channel, toggleMode);
    JsonArray arr = json.getDoc()[String(preset)]["buttons_short"][String(param_number)].to<JsonArray>();
    arr[0] = static_cast<int>(_type);
    arr[1] = static_cast<int>(control);
    arr[2] = static_cast<int>(channel);
    json.setButtonToggleMode(preset, param_number, toggleMode ? 1 : 0);
    if (preset == controls.getPreset()) {
      updateFaderTitles();
      showDisplay();
    }
    json.save();
    break;
  }
  case 14:
  {
    uint8_t layout = sysex[3];
    if (layout <= 2) {
      faderLayout = static_cast<FaderLayout>(layout);
      json.setLayout(layout);
      json.save();
      uint8_t preset = controls.getPreset();
      if (preset < 6) {
        updateFaderTitles();
      }
      updateFaderValues();
      showDisplay();
      deviceLabelDirty = true;
      bankLabelDirty = true;
      drawDeviceBankLabels();
    }
    break;
  }
  case 15:
  {
    // Control name: F0 6F 0F <preset> <param> <isButton> <ascii chars...> F7
    uint8_t preset = sysex[3];
    uint8_t idx = sysex[4];
    uint8_t isButton = sysex[5];
    if (preset >= 6 || idx >= 8) break;

    char name[12] = {0};
    size_t nameLen = 0;
    for (size_t j = 6; j < len - 1 && nameLen < 11; j++) {
      name[nameLen++] = (char)sysex[j];
    }
    name[nameLen] = '\0';

    MidiControl& ctrl = isButton
      ? controls.getButtonShortAt(preset, idx)
      : controls.getEncoderAt(preset, idx);
    strncpy(ctrl.controlName, name, sizeof(ctrl.controlName) - 1);

    const char* section = isButton ? "button_names" : "encoder_names";
    json.getDoc()[String(preset)][section][String(idx)] = name;
    json.save();

    if (preset == controls.getPreset()) {
      updateFaderTitles();
      // setParamName() only updates the string; the fader area needs an
      // explicit redraw for the new name to show up right away.
      showDisplay();
    }
    break;
  }
  case 0x11:
  {
    uint8_t preset = sysex[3];
    if (preset >= 6) break;
    char name[20] = {0};
    size_t nameLen = 0;
    for (size_t j = 4; j < len - 1 && nameLen < 19; j++) {
      name[nameLen++] = (char)sysex[j];
    }
    name[nameLen] = '\0';
    Serial.printf("Preset name received: preset=%d name='%s'\n", preset, name);
    controls.setPresetName(preset, name);
    json.getDoc()[String(preset)]["preset_name"] = name;
    json.save();
    if (preset == controls.getPreset()) {
      updateFaderTitles();
      showDisplay();
    }
    break;
  }
  case 18:
  {
    // Set watcher flag: F0 6F 12 <preset> <idx> <on/off> F7
    uint8_t preset = sysex[3];
    uint8_t idx = sysex[4];
    uint8_t on = sysex[5];
    Serial.printf("SysEx18 watcher: preset=%d idx=%d on=%d\n", preset, idx, on);
    if (preset < 6 && idx < 8) {
      controls.getEncoderAt(preset, idx).hasWatcher = on != 0;
      Serial.printf("  -> hasWatcher[%d][%d] = %d\n", preset, idx, controls.getEncoderAt(preset, idx).hasWatcher);
    }
    break;
  }
  case 16:
  {
    // Screensaver timeout: F0 6F 10 <high7> <low7> F7
    uint16_t seconds = ((uint16_t)(sysex[3] & 0x7F) << 7) | (sysex[4] & 0x7F);
    screenSaverDelay = (unsigned long)seconds * 1000UL;
    json.getDoc()["screensaver"] = seconds;
    json.save();
    if (screenSaverDelay == 0 && screenSaverActive) {
      screenSaverActive = false;
      showDisplay();
    }
    break;
  }
  case 0x1B:
  {
    uint8_t val = sysex[3] & 0x7F;
    setBrightness(val);
    json.getDoc()["brightness"] = val;
    json.save();
    break;
  }
  case 0x13:
  {
    // Requête de version firmware depuis l'éditeur : F0 6F 13 .. F7
    uint8_t reply[7] = { 240, 111, 0x14, FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH, 247 };
    usb_midi.write(reply, 7);
    serialEditorSend(reply, 7);
    break;
  }
  default:
    break;
  }
}

void decode_ascii_sysex(const uint8_t *data, size_t len, char *out, size_t out_len)
{
  size_t out_idx = 0;
  for (size_t i = 0; i + 1 < len && out_idx + 1 < out_len; i += 2)
  {
    uint8_t type = data[i];
    uint8_t val = data[i + 1];
    char c = '_'; // Par défaut

    if (type == 0 && val <= 94)
    {
      c = (char)(val + 32); // ASCII standard
    }
    else if (type == 1 && val <= 127)
    {
      c = (char)(val + 128); // ASCII étendu
    }
    out[out_idx++] = c;
  }
  out[out_idx] = '\0';
}