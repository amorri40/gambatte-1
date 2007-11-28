/***************************************************************************
 *   Copyright (C) 2007 by Sindre Aam�s                                    *
 *   aamas@stud.ntnu.no                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License version 2 as     *
 *   published by the Free Software Foundation.                            *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License version 2 for more details.                *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   version 2 along with this program; if not, write to the               *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include "lyc_irq.h"

#include "ly_counter.h"

LycIrq::LycIrq(unsigned char &ifReg_in) :
	VideoEvent(1),
	ifReg(ifReg_in)
{
	setDoubleSpeed(false);
	setM2IrqEnabled(false);
	setLycReg(0);
	reset();
}

void LycIrq::doEvent() {
	if (!m2IrqEnabled || lycReg_ > 143 || lycReg_ == 0)
		ifReg |= 0x2;
	
	setTime(time() + frameTime);
}

void LycIrq::lycRegSchedule(const LyCounter &lyCounter, const unsigned long cycleCounter) {
	schedule(lyCounter, cycleCounter);
	
	if (lycReg_ > 0 && time() - cycleCounter > (4U >> lyCounter.isDoubleSpeed()) && time() - cycleCounter < 8)
		setTime(time() + frameTime);
}

void LycIrq::schedule(const LyCounter &lyCounter, const unsigned long cycleCounter) {
	//setTime(lyCounter.nextFrameCycle(lycReg_ ? lycReg_ * 456 - 1 : (153 * 456 + 7), cycleCounter));
	
	unsigned long next = lyCounter.time() + ((153u - lyCounter.ly()) * 456ul + (lycReg_ ? lycReg_ * 456ul : (153 * 456ul + 8)) << lyCounter.isDoubleSpeed()) - 1;
	
	if (next - cycleCounter > frameTime)
		next -= frameTime;
	
	setTime(next);
}
