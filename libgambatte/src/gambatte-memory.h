//
//   Copyright (C) 2007 by sinamas <sinamas at users.sourceforge.net>
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License version 2 as
//   published by the Free Software Foundation.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License version 2 for more details.
//
//   You should have received a copy of the GNU General Public License
//   version 2 along with this program; if not, write to the
//   Free Software Foundation, Inc.,
//   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//

#ifndef MEMORY_H
#define MEMORY_H

#include "mem/cartridge.h"
#include "interrupter.h"
#include "sound.h"
#include "tima.h"
#include "video.h"

namespace gambatte {

class InputGetter;

class Memory {
public:
	explicit Memory(Interrupter const &interrupter);
	bool loaded() const { return cart_.loaded(); }
	void setStatePtrs(SaveState &state);
	unsigned long saveState(SaveState &state, unsigned long cc);
	void loadState(SaveState const &state);
#ifdef __LIBRETRO__
   void *savedata_ptr() { return cart_.savedata_ptr(); }
   unsigned savedata_size() { return cart_.savedata_size(); }
   void *rtcdata_ptr() { return cart_.rtcdata_ptr(); }
   unsigned rtcdata_size() { return cart_.rtcdata_size(); }
   void display_setColorCorrection(bool enable) { lcd_.setColorCorrection(enable); }
   video_pixel_t display_gbcToRgb32(const unsigned bgr15) { return lcd_.gbcToRgb32(bgr15); }
   void clearCheats() { cart_.clearCheats(); }
#else
   void loadSavedata() { cart_.loadSavedata(); }
   void saveSavedata() { cart_.saveSavedata(); }
#endif
	std::string const saveBasePath() const { return cart_.saveBasePath(); }

	unsigned long stop(unsigned long cycleCounter);
	bool isCgb() const { return lcd_.isCgb(); }
	bool ime() const { return intreq_.ime(); }
	bool halted() const { return intreq_.halted(); }
	unsigned long nextEventTime() const { return intreq_.minEventTime(); }
	bool isActive() const { return intreq_.eventTime(intevent_end) != disabled_time; }

	long cyclesSinceBlit(unsigned long cc) const {
		if (cc < intreq_.eventTime(intevent_blit))
			return -1;

		return (cc - intreq_.eventTime(intevent_blit)) >> isDoubleSpeed();
	}

	void halt() { intreq_.halt(); }
	void ei(unsigned long cycleCounter) { if (!ime()) { intreq_.ei(cycleCounter); } }
	void di() { intreq_.di(); }

	unsigned ff_read(unsigned p, unsigned long cc) {
		return p < 0x80 ? nontrivial_ff_read(p, cc) : ioamhram_[p + 0x100];
	}

	// 
	// Read memory
	// * Check if the pointer memory address is within the cart range
	//    * To do this we use a right shift to remove the last 12 bits
	//      * this then brings in 12 bits to the left which are the same value as the previous left most bit
	// * This checks if the value is very large (i.e greater than 65298) 
	//   * if the number is greater than 12 bits can hold then it will be a nontrivial_read
	// * cc is the CycleCounter and is only needed for non trivial reads as they take more cpu cycles
	// 
	unsigned read(unsigned p, unsigned long cc) {
		if (cart_.rmem(p >> 12))
		EM_ASM_INT({
           window.trivialReadMemory($0, $1, $2, $3);
         }, p, cart_.rmem(p >> 12), &(cart_.rmem(p >> 12)[p]), cart_.rmem(p >> 12)[p]);
		return cart_.rmem(p >> 12) ? cart_.rmem(p >> 12)[p] : nontrivial_read(p, cc);
	}

	void write(unsigned p, unsigned data, unsigned long cc) {
		if (cart_.wmem(p >> 12)) {
			cart_.wmem(p >> 12)[p] = data;
		} else
			nontrivial_write(p, data, cc);
	}

	void ff_write(unsigned p, unsigned data, unsigned long cc) {
		if (p - 0x80u < 0x7Fu) {
			ioamhram_[p + 0x100] = data;
		} else
			nontrivial_ff_write(p, data, cc);
	}

	unsigned long event(unsigned long cycleCounter);
	unsigned long resetCounters(unsigned long cycleCounter);
	void setSaveDir(std::string const &dir) { cart_.setSaveDir(dir); }
	void setInputGetter(InputGetter *getInput) { getInput_ = getInput; }
	void setEndtime(unsigned long cc, unsigned long inc);
	void setSoundBuffer(uint_least32_t *buf) { psg_.setBuffer(buf); }
	std::size_t fillSoundBuffer(unsigned long cc);

	void setVideoBuffer(video_pixel_t *videoBuf, std::ptrdiff_t pitch) {
		lcd_.setVideoBuffer(videoBuf, pitch);
	}

	void setDmgPaletteColor(int palNum, int colorNum, unsigned long rgb32) {
		lcd_.setDmgPaletteColor(palNum, colorNum, rgb32);
	}

	void setGameGenie(std::string const &codes) { cart_.setGameGenie(codes); }
	void setGameShark(std::string const &codes) { interrupter_.setGameShark(codes); }
	void updateInput();

   int loadROM(const void *romdata, unsigned romsize, const bool forceDmg, const bool multicartCompat);

private:
	Cartridge cart_;
	unsigned char ioamhram_[0x200];
	InputGetter *getInput_;
	unsigned long divLastUpdate_;
	unsigned long lastOamDmaUpdate_;
	InterruptRequester intreq_;
	Tima tima_;
	LCD lcd_;
	PSG psg_;
	Interrupter interrupter_;
	unsigned short dmaSource_;
	unsigned short dmaDestination_;
	unsigned char oamDmaPos_;
	unsigned char serialCnt_;
	bool blanklcd_;

	void decEventCycles(IntEventId eventId, unsigned long dec);
	void oamDmaInitSetup();
	void updateOamDma(unsigned long cycleCounter);
	void startOamDma(unsigned long cycleCounter);
	void endOamDma(unsigned long cycleCounter);
	unsigned char const * oamDmaSrcPtr() const;
	unsigned nontrivial_ff_read(unsigned p, unsigned long cycleCounter);
	unsigned nontrivial_read(unsigned p, unsigned long cycleCounter);
	void nontrivial_ff_write(unsigned p, unsigned data, unsigned long cycleCounter);
	void nontrivial_write(unsigned p, unsigned data, unsigned long cycleCounter);
	void updateSerial(unsigned long cc);
	void updateTimaIrq(unsigned long cc);
	void updateIrqs(unsigned long cc);
	bool isDoubleSpeed() const { return lcd_.isDoubleSpeed(); }
};

}

#endif
