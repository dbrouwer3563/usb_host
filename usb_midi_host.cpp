/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2023 Deb Brouwer
 * Computer Science 444: Embedded/ Robotic Programming
 * Compile with g++ usb_midi_host.cpp -lrtmidi
 * Check output ports: aconnect -o
 */
#include <iostream>
#include <cstdlib>
#include <cmath>
#include <unistd.h>
#include <signal.h>
#include <cstring>
#include "rtmidi/RtMidi.h"

#define NOTE_ON 0x90
#define NOTE_OFF 0x80

enum playing_state { NOT_PLAYING, PLAYING };

int state = NOT_PLAYING;

RtMidiOut *midi_out;
RtMidiIn *midi_in = 0;
int port_out = 0;
int port_in = 0;

void play_note(int status, int channel, int midi_note, int velocity)
{
	std::vector<unsigned char> message;
	message.push_back(status | channel);
	message.push_back(midi_note);
	message.push_back(velocity);
	midi_out->sendMessage(&message);
}

int is_noise(int channel, int midi_note, int velocity)
{
	/*
	 * TODO: to control noise on the line, every midi "Note On" from arduino is sent with
	 * velocity 62 which acts as a magic number to distinguish notes from arduino verses
	 * noise on the line. Would be better to use MIDI system messages to control this.
	 */
	if (velocity != 62)
		return 1;
	/* Only play midi notes within the range of piano keys (21) 28 Hz to (108) 4 kHz*/
	if (midi_note < 20 || midi_note > 108)
		return 1;
	return 0;
}

void sigint_handler(int sig)
{
	std::vector<unsigned char> message;
	message.push_back(176); // A Control Change message
	message.push_back(120); // All Sound Off 0x78
	message.push_back(0);
	delete midi_out;
	delete midi_in;
	exit(1);
}

void get_message(double deltatime, std::vector< unsigned char > *message, void *userData)
{
	struct sigaction sa = {};
	sa.sa_handler = sigint_handler;
	sigaction(SIGINT, &sa, NULL);

	int status = (int) message->at(0);
	int channel = (int) message->at(0) & 0x0F;
	int byte1 = (int) message->at(1);
	int byte2 = (int) message->at(2);

	int midi_note = byte1;
	int velocity = byte2;

	if ((status & 0xF0) == NOTE_ON) {
		if (state == PLAYING)
			return;
		if (is_noise(channel, midi_note, velocity))
			return;
		std::cout << "NOTE_ON: " << midi_note << std::endl;
		state = PLAYING;
		play_note(status, channel, midi_note, velocity);
	}
	if ((status & 0xF0) == NOTE_OFF) {
		if (state == NOT_PLAYING)
			return;
		int midi_note = byte1;
		int velocity = byte2;
		state = NOT_PLAYING;
		velocity = 0; // Note off should always be 0
		std::cout << ", NOTE_OFF: " << midi_note << std::endl;
		play_note(status, channel, midi_note, velocity);
	}
}

void setup()
{
	try {
		midi_out = new RtMidiOut();
	}
	catch (RtMidiError &error) {
		error.printMessage();
		exit(EXIT_FAILURE);
	}

	unsigned int nPorts_out = midi_out->getPortCount();
	std::cout << "Available output ports: " << nPorts_out << std::endl;
	for (int i = 0; i < nPorts_out; i++)
		std::cout << "\t" << i << ": " << midi_out->getPortName() << std::endl;

	midi_out->openPort(port_out);
	std::cout << "Output MIDI to port: " << port_out << std::endl;

	try {
		midi_in = new RtMidiIn();
	}
	catch (RtMidiError &error) {
		error.printMessage();
		exit(EXIT_FAILURE);
	}

	unsigned int nPorts_in = midi_in->getPortCount();
	std::cout << "Available input ports: " << nPorts_in << std::endl;

	for (int i = 0; i < nPorts_in; i ++) {
		std::cout << "\t" << i << ": " << midi_in->getPortName(i) << std::endl;
	}

	midi_in->openPort(port_in);
	std::cout << "Input MIDI from port: " << port_in << std::endl;


	midi_in->setCallback(&get_message);

	/* Ignore SYSEX, Timing and Active Sensing messages. */
	midi_in->ignoreTypes(true, true, true);
	std::cout << "Reading MIDI from port" << port_in << std::endl;
}

int main(int argc, char *argv[])
{
	if (argc == 3) {
		port_in = atoi(argv[1]);
		port_out = atoi(argv[2]);
	} else {
		std::cout << "Usage: ./a.out [PORT IN] [PORT OUT]" << std::endl;
		exit(1);
	}

	setup();
	while(true)
		;
	delete midi_out;
	delete midi_in;
}
