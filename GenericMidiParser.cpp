/*
 * See header file for details
 *
 *  This program is free software: you can redistribute it and/or modify\n
 *  it under the terms of the GNU General Public License as published by\n
 *  the Free Software Foundation, either version 3 of the License, or\n
 *  (at your option) any later version.\n
 * 
 *  This program is distributed in the hope that it will be useful,\n
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of\n
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n
 *  GNU General Public License for more details.\n
 * 
 *  You should have received a copy of the GNU General Public License\n
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.\n
 */
 
/* Includes */
#include "GenericMidiParser.hpp"
//#include <stdio.h>

GenericMidiParser::GenericMidiParser(uint8_t (*file_read_fnct)(void),
		uint8_t (*file_eof_fnct)(void), void (*us_delay_fnct)(uint32_t us),
		void (*assert_error_callback)(uint8_t errorCode)) :
		file_read_fnct(file_read_fnct), file_eof_fnct(file_eof_fnct), us_delay_fnct(
				us_delay_fnct), assert_error_callback(assert_error_callback), note_on_callback(
				0), note_off_callback(0), key_after_touch_callback(0), control_change_callback(
				0), patch_change_callback(0), channel_after_touch_callback(0), pitch_bend_callback(
				0), meta_callback(0), meta_onChannel_prefix(0), meta_onPort_prefix(
				0) {

}

uint32_t GenericMidiParser::readByte() {
	track.trackSize--;
	return file_read_fnct();
}

void GenericMidiParser::readBytes(uint8_t* buf, uint8_t len) {
	track.trackSize -= len;
	for (uint8_t i = 0; i < len; i++)
		buf[i] = file_read_fnct();
}

void GenericMidiParser::dropBytes(uint8_t len) {
	track.trackSize -= len;
	for (uint8_t i = 0; i < len; i++)
		file_read_fnct();
}

uint32_t GenericMidiParser::readVarLenValue() {
	uint32_t value = 0;
	uint8_t c;
	if ((value = readByte()) & 0x80) {
		value &= 0x7F;
		do {
			value = (value << 7) + ((c = readByte()) & 0x7F);
		} while (c & 0x80);
	}
	return value;
}

void GenericMidiParser::setNoteOnCallback(
		void (*note_on_callback)(uint8_t channel, uint8_t key,
				uint8_t velocity)) {
	this->note_on_callback = note_on_callback;
}

void GenericMidiParser::setNoteOffCallback(
		void (*note_off_callback)(uint8_t channel, uint8_t key,
				uint8_t velocity)) {
	this->note_off_callback = note_off_callback;
}

void GenericMidiParser::setKeyAfterTouchCallback(
		void (*key_after_touch_callback)(uint8_t channel, uint8_t key,
				uint8_t pressure)) {
	this->key_after_touch_callback = key_after_touch_callback;
}

void GenericMidiParser::setControlChangeCallback(
		void (*control_change_callback)(uint8_t channel, uint8_t controller,
				uint8_t data)) {
	this->control_change_callback = control_change_callback;
}

void GenericMidiParser::setPatchChangeCallback(
		void (*patch_change_callback)(uint8_t channel, uint8_t instrument)) {
	this->patch_change_callback = patch_change_callback;
}

void GenericMidiParser::setChannelAfterTouchCallback(
		void (*channel_after_touch_callback)(uint8_t channel,
				uint8_t pressure)) {
	this->channel_after_touch_callback = channel_after_touch_callback;
}

void GenericMidiParser::setPitchBendCallback(
		void (*pitch_bend_callback)(uint8_t channel, uint16_t bend)) {
	this->pitch_bend_callback = pitch_bend_callback;
}

void GenericMidiParser::setMetaCallback(
		void (*meta_callback)(uint8_t metaType, uint8_t dataLength)) {
	this->meta_callback = meta_callback;
}

void GenericMidiParser::setMetaOnChannelCallback(
		void (*meta_onChannel_prefix)(uint8_t channel)) {
	this->meta_onChannel_prefix = meta_onChannel_prefix;
}

void GenericMidiParser::setMetaOnPortCallback(
		void (*meta_onPort_prefix)(uint8_t channel)) {
	this->meta_onPort_prefix = meta_onPort_prefix;
}

uint8_t GenericMidiParser::processHeader() {
	//printf("DBG: Beginning header parsing ...\n");

	readBytes((uint8_t*) &header.MThd, 4);
	//printf("DBG: Header : %x %x %x %x\n", header.MThd[0], header.MThd[1],
	//		header.MThd[2], header.MThd[3]);

	header.headerSize = (readByte() << 24) | (readByte() << 16)
			| (readByte() << 8) | readByte();
	//printf("DBG: HeaderSize : %d\n", header.headerSize);

	header.formatType = (readByte() << 8) | readByte();
	//printf("DBG: Format type : %d\n", header.formatType);

	header.numberOfTracks = (readByte() << 8) | readByte();
	//printf("DBG: number of track : %d\n", header.numberOfTracks);

	header.timeDivision = (readByte() << 8) | readByte();
	//printf("DBG: time division : %d\n", header.timeDivision);

	if (header.MThd[0] != 0x4D || header.MThd[1] != 0x54
			|| header.MThd[2] != 0x68 || header.MThd[3] != 0x64) {
		//printf("DBG: Header check: ERROR\n");
		return BAD_FILE_HEADER;
	}
	//printf("DBG: Header check: PASS\n");

	if (header.headerSize != 0x06) {
		//printf("DBG: Header size check: ERROR\n");
		return BAD_FILE_HEADER;
	}
	//printf("DBG: Header size check: PASS\n");

#ifndef IGNORE_FILE_FORMAT
	if (header.formatType == MULTIPLE_TRACK_FILE
			|| header.formatType == MULTILPLE_SONG_FILE) {
		//printf("DBG: Fileformat check: ERROR\n");
		return NO_MULTIPLE_TRACKS_SUPPORT;
	}
#endif
	//printf("DBG: Fileformat check: PASS\n");

	if (header.timeDivision < 0) {
		//printf("DBG: Time division check: ERROR\n");
		return NO_SMPTE_SUPPORT; // TODO
	}
	//printf("DBG: Time division check: PASS\n");

	tempo = 500000; // Default tempo
	//printf("DBG: Tempo: 500000\n");

	if (file_eof_fnct()) {
		//printf("DBG: File struct check: ERROR\n");
		return BAD_FILE_STRUCT;
	}
	//printf("DBG: File struct check: PASS\n");

	//printf("DBG: Header parsing done !\n");
	return NO_ERROR;
}

uint8_t GenericMidiParser::processTrack() {
	//printf("DBG: Beginning parsing track ...\n");

	readBytes((uint8_t*) &track.MTrk, 4);
	//printf("DBG: Header : %x %x %x %x\n", track.MTrk[0], track.MTrk[1],
	//		track.MTrk[2], track.MTrk[3]);

	track.trackSize = (readByte() << 24) | (readByte() << 16)
			| (readByte() << 8) | readByte();
	//printf("DBG: TrackSize : %d\n", track.trackSize);

	if (track.MTrk[0] != 0x4D || track.MTrk[1] != 0x54 || track.MTrk[2] != 0x72
			|| track.MTrk[3] != 0x6B) {
		//printf("DBG: Header check: ERROR\n");
		return BAD_TRACK_HEADER;
	}
	//printf("DBG: Header check: PASS\n");

	//printf("DBG: Track parsing done !\n");
	return NO_ERROR;
}

uint8_t GenericMidiParser::processEvent() {
	uint32_t deltaTime = readVarLenValue();
	//printf("DBG: Delta time: %d\n", deltaTime);

	uint8_t cmd = readByte();
	uint8_t nybble = (cmd & 0xF0) >> 4;
	static uint8_t channel = 0;

	//printf("DBG: Command: %x\n", cmd);
	//printf("DBG: Nybble: %x\n", nybble);

	//printf("DBG: TimeDivision: %d\n", header.timeDivision);
	//printf("DBG: Tempo: %d\n", tempo);
	//printf("DBG: DeltaTime: %d\n", deltaTime);
	uint32_t waitTime = deltaTime * ((float) tempo / header.timeDivision);

#ifdef IGNORE_META_DELAY
	if(nybble == 0x0F) waitTime = 0;
#endif

	us_delay_fnct(waitTime);
	
	if (cmd < 0x80) { // Runnning status
		//printf("DBG: Event: Runnning status\n");
		//printf("DBG: Channel: %d\n", channel);

		uint8_t velocity = readByte();

		if (velocity > 0) {
			if (note_on_callback)
				note_on_callback(channel, cmd, velocity);
		} else {
			if (note_off_callback)
				note_off_callback(channel, cmd, velocity);
		}
	}

	channel = cmd & 0x0F;
	//printf("DBG: Channel: %d\n", channel);
	
	switch (nybble) {
	case 0x08: // note off
		//printf("DBG: Event: Note Off\n");
		if (note_off_callback)
			note_off_callback(channel, readByte(), readByte());
		else
			dropBytes(2);
		break;

	case 0x09: // note on
		//printf("DBG: Event: Note On\n");
		if (note_on_callback)
			note_on_callback(channel, readByte(), readByte());
		else
			dropBytes(2);
		break;

	case 0x0A: // key after-touch
		//printf("DBG: Event: Key after touch\n");
		if (key_after_touch_callback)
			key_after_touch_callback(channel, readByte(), readByte());
		else
			dropBytes(2);
		break;

	case 0x0B: // control change
		//printf("DBG: Event: Control change\n");
		if (control_change_callback)
			control_change_callback(channel, readByte(), readByte());
		else
			dropBytes(2);
		break;

	case 0x0C: // program change
		//printf("DBG: Event: Program change\n");
		if (patch_change_callback)
			patch_change_callback(channel, readByte());
		else
			readByte();
		break;

	case 0x0D: // channel after touch
		//printf("DBG: Event: Channel after touch\n");
		if (channel_after_touch_callback)
			channel_after_touch_callback(channel, readByte());
		else
			readByte();
		break;

	case 0x0E: // pitch wheel change
		//printf("DBG: Event: Pitch wheel change\n");
		if (pitch_bend_callback)
			pitch_bend_callback(channel, readByte());
		else
			readByte();
		break;
		
	case 0x0F: // meta
		//printf("DBG: Event: Meta\n");
		if ((errno = processMeta(cmd)))
			return errno;
		break;
	}

	return NO_ERROR;
}

uint8_t GenericMidiParser::processMeta(uint8_t cmd) {
	if (cmd == 0xFF) {
		//printf("DBG: Meta type: normal\n");

		uint8_t metaCmd = readByte();
		uint32_t dataLen = readVarLenValue();
		//printf("DBG: Meta Command: %x\n", metaCmd);
		//printf("DBG: Meta length: %d\n", dataLen);

		switch (metaCmd) {
		case 0x00: // Set track's sequence number
			//printf("DBG: Meta Event: set track number\n");
			if (dataLen != 0x02)
				return BAD_META_EVENT;
			track_number = (readByte() << 8) | readByte();
			break;

		case 0x01: // Text event- any text you want.
		case 0x02: // Same as text event, but used for copyright info.
		case 0x03: // Sequence or Track name
		case 0x04: // Track instrument name
		case 0x05: // Lyric
		case 0x06: // Marker
		case 0x07: // Cue point
			//printf("DBG: Meta Event: text or similar\n");
			if (dataLen == 0)
				return BAD_META_EVENT;
			if (meta_callback)
				meta_callback(metaCmd, dataLen);
			else
				dropBytes(dataLen);
			break;

		case 0x20: // Midi Channel Prefix
			//printf("DBG: Meta Event: Channel prefix\n");
			if (dataLen != 1)
				return BAD_META_EVENT;
			if (meta_onChannel_prefix)
				meta_onChannel_prefix(readByte());
			else
				readByte();
			break;

		case 0x21: // Midi Port Prefix
			//printf("DBG: Meta Event: Port prefix\n");
			if (dataLen != 1)
				return BAD_META_EVENT;
			if (meta_onPort_prefix)
				meta_onPort_prefix(readByte());
			else
				readByte();
			break;

		case 0x2F: // This event MUST come at the end of each track
			//printf("DBG: Meta Event: End of track\n");
			if (dataLen != 0x00)
				return BAD_META_EVENT;
			if (processTrack()) {
				track.trackSize = 0;
				//printf("DBG: End of all tracks\n");
			}
			break;

		case 0x51: // Set tempoSet tempo
			//printf("DBG: Meta Event: Set tempo\n");
			if (dataLen != 0x03)
				return BAD_META_EVENT;
			tempo = (readByte() << 16) | (readByte() << 8) | readByte();
			//printf("DBG: Tempo: %d\n", tempo);
			break;

		case 0x54: // SMTPE Offset TODO
			//printf("DBG: Meta Event: SMTPE offset\n");
			if (dataLen != 0x05)
				return BAD_META_EVENT;
			dropBytes(5);
			/*uint8_t hours = readByte();
			 uint8_t minutes = readByte();
			 uint8_t seconds = readByte();
			 uint8_t frames = readByte();
			 uint8_t fractional = readByte();*/
			// SMTPE not implemented
			break;

		case 0x58: // Time Signature TODO
			//printf("DBG: Meta Event: Time signature\n");
			if (dataLen != 0x04)
				return BAD_META_EVENT;
			dropBytes(4);
			/*uint8_t numerator = readByte();
			 uint8_t denominator = readByte();
			 uint8_t metronomeTick = readByte();
			 uint8_t note32NdNumber = readByte();*/
			// Time signature not implemented
			break;

		case 0x59: // Key signature TODO
			//printf("DBG: Meta Event: Key signature\n");
			if (dataLen != 0x02)
				return BAD_META_EVENT;
			dropBytes(2);
			/*uint8_t sharpsFlats = readByte();
			 uint8_t majorMinor = readByte();*/
			// Key signature not implemented
			break;

		case 0x7F: // Sequencer specific information
			//printf("DBG: Meta Event: Sequencer specific\n");
			if (dataLen == 0)
				return BAD_META_EVENT;
			if (meta_callback)
				meta_callback(META_SEQUENCER, dataLen);
			else
				dropBytes(dataLen);
			break;
		}

	} else {
		//printf("DBG: Meta type: sysex\n");

		switch (cmd) {
		case 0xF8: // Timing Clock Request
			//printf("DBG: Meta Event: Timing clock request\n");
			break;

		case 0xFA: // Start Sequence
			//printf("DBG: Meta Event: Start sequence\n");
			paused = false;
			break;

		case 0xFB: // Continue Stopped Sequence
			//printf("DBG: Meta Event: Resume sequence\n");
			paused = false;
			break;

		case 0xFC: // Stop Sequence
			//printf("DBG: Meta Event: Stop sequence\n");
			paused = true;
			break;

		case 0xF0: // sysex event
		case 0xF7: // sysex event
			//printf("DBG: Meta Event: Sysex message\n");
			uint32_t dataLen = readVarLenValue();
			//printf("DBG: Sysex length: %d\n", dataLen);

			if (dataLen == 0)
				return BAD_META_EVENT;
			if (meta_callback)
				meta_callback(META_SYSEX, dataLen);
			else
				dropBytes(dataLen);
			break;
		}
	}

	return NO_ERROR;
}

void GenericMidiParser::play() {
	errno = processHeader();
	if (errno) {
		assert_error_callback(errno);
		return;
	}

	errno = processTrack();
	if (errno) {
		assert_error_callback(errno);
		return;
	}

	//printf("DBG: Start playing ...\n");
	paused = false;
	while (track.trackSize > 0 && !file_eof_fnct()) {

		while (paused)
			;

		errno = processEvent();
		if (errno) {
			assert_error_callback(errno);
			return;
		}
	}
}

void GenericMidiParser::pause() {
	paused = true;
}

void GenericMidiParser::resume() {
	paused = false;
}

void GenericMidiParser::stop() {
	track.trackSize = 0;
}

uint8_t GenericMidiParser::getErrno() const {
	return errno;
}

uint32_t GenericMidiParser::getTempo() const {
	return tempo;
}

void GenericMidiParser::setTempo(uint32_t tempo) {
	this->tempo = tempo;
}
