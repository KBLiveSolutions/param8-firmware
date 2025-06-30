#pragma once
#include <MIDI.h>
#include <Adafruit_TinyUSB.h>

extern Adafruit_USBD_MIDI usb_midi;
// extern MIDI_NAMESPACE::MidiInterface<Adafruit_USBD_MIDI> USBMIDI;

#define MIDI_MAX_PACKET_SIZE 4
#define MIDI_MAX_PACKET_SIZE 4
#define SYSEX_START_BYTE 0xF0
#define SYSEX_END_BYTE 0xF7

extern bool isSysExContinued;
extern size_t sysExBufferSize;
extern uint8_t sysExBuffer[MIDI_MAX_PACKET_SIZE * 10];

void setupMIDI();
void sendMidiMessage(uint8_t type, uint8_t number, uint8_t value, uint8_t channel);
void midiRead();
void handleMIDIDAWMessage(uint8_t *packet);
void clearSysExBuffer();
void handleSysExMessage(uint8_t *packet);
void handleSysExStart(uint8_t *packet) ;
void handleSysExContinuation(uint8_t *packet);
void onControlChange(uint8_t channel, uint8_t control, uint8_t value);
void onSysEx(const uint8_t* sysex, size_t len);
void decode_ascii_sysex(const uint8_t* data, size_t len, char* out, size_t out_len);