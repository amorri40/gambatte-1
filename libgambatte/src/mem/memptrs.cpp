/***************************************************************************
 *   Copyright (C) 2007-2010 by Sindre Aamås                               *
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
#include "memptrs.h"
#include <algorithm>
#include <cstring>
 #include <emscripten.h>

namespace gambatte
{

   MemPtrs::MemPtrs()
      : rmem_()
      , wmem_()
      , romdata_()
      , wramdata_()
      , vrambankptr_(0)
      , rsrambankptr_(0)
      , wsrambankptr_(0)
      ,memchunk_(0)
      , rambankdata_(0)
      , wramdataend_(0)
      , oamDmaSrc_(oam_dma_src_off)
   {
   }

   MemPtrs::~MemPtrs()
   {
      delete []memchunk_;
   }

   void MemPtrs::reset(const unsigned rombanks, const unsigned rambanks, const unsigned wrambanks)
   {
      delete []memchunk_;
      // 
      // # Initialise a big chunk of memory to store all the data
      // * 0x4000 is 16kb the size of the first rombank
      // * next it will add 16kb multiplied by the number of rombanks
      // * next it multiplies the rambanks by the size of a rambank which is 8kb
      // * next it multiplies the working ram banks by the size of the wram bank which is 4kb
      // * finally it adds 16kb for ??? (possibly disabled ram)
      // 
      memchunk_     = new unsigned char[
         0x4000 
         + rombanks * 0x4000ul + 0x4000
         + rambanks * 0x2000ul 
         + wrambanks * 0x1000ul 
         + 0x4000];

      // 
      // Now use the memory initialised above to seperate into the pointers
      // 
      romdata_[0]   = romdata();   
      rambankdata_  = romdata_[0] + rombanks * 0x4000ul + 0x4000;
      wramdata_[0]  = rambankdata_ + rambanks * 0x2000ul;
      wramdataend_ = wramdata_[0] + wrambanks * 0x1000ul;

      std::memset(rdisabledRamw(), 0xFF, 0x2000);

      oamDmaSrc_    = oam_dma_src_off;
      rmem_[0x3]    = rmem_[0x2] = rmem_[0x1] = rmem_[0x0] = romdata_[0];
      rmem_[0xC]    = wmem_[0xC] = wramdata_[0] - 0xC000;
      rmem_[0xE]    = wmem_[0xE] = wramdata_[0] - 0xE000;

      setRombank(1);
      setRambank(0, 0);
      setVrambank(0);
      setWrambank(1);
      EM_ASM_INT({
           window.resetPointers($0, $1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11);
         }, rombanks,rambanks, wrambanks, romdata_[0], romdata_[1], rambankdata_, wramdata_[0], wramdataend_,rdisabledRamw(), memchunk_, sizeof(memchunk_), 0x4000 
         + rombanks * 0x4000ul + 0x4000
         + rambanks * 0x2000ul 
         + wrambanks * 0x1000ul 
         + 0x4000 );
   }

   //    
   // rom_data_ array contains 2 elements
   //  * each element is a game boy memory bank
   //  * they are pointers to the currently set banks memory on the emscripten heap
   // 
   void MemPtrs::setRombank0(const unsigned bank)
   {
      EM_ASM_INT({
           window.setRombank0($0,$1,$2,$3);
         }, romdata(), bank, 0x4000ul,  romdata() + bank * 0x4000ul);
      romdata_[0] = romdata() + bank * 0x4000ul;
      rmem_[0x3] = rmem_[0x2] = rmem_[0x1] = rmem_[0x0] = romdata_[0];
      disconnectOamDmaAreas();
   }

   #define _16KB 0x4000
   #define UNSIGNED_16KB 0x4000ul

   // 
   // # Switch Rombank1
   // * Change the romdata_[1] pointer so it points to the bank specified by the bank parameter
   // * each rombank is 16kb (i.e 0x4000)
   // * so we need to multiply the origin romdata() offset by 16kb to get the start memory location of this new bank
   // ? But why do we need to subtrack 16kb after the multiplication?
   // 
   void MemPtrs::setRombank(const unsigned bank)
   {
      EM_ASM_INT({
           window.setRombank1($0,$1,$2,$3);
         }, romdata(), bank, (bank*UNSIGNED_16KB) - _16KB,  romdata() + bank * UNSIGNED_16KB - _16KB);
      romdata_[1] = romdata() + bank * UNSIGNED_16KB - _16KB;
      rmem_[0x7] = rmem_[0x6] = rmem_[0x5] = rmem_[0x4] = romdata_[1];
      disconnectOamDmaAreas();
   }

   void MemPtrs::setRambank(const unsigned flags, const unsigned rambank)
   {
      unsigned char *const srambankptr = (flags & RTC_EN)
         ? 0
         : (rambankdata() != rambankdataend()
               ? rambankdata_ + rambank * 0x2000ul - 0xA000 : wdisabledRam() - 0xA000);

      rsrambankptr_ = (flags & READ_EN) && srambankptr != wdisabledRam() - 0xA000 ? srambankptr : rdisabledRamw() - 0xA000;
      wsrambankptr_ = (flags & WRITE_EN) ? srambankptr : wdisabledRam() - 0xA000;
      rmem_[0xB] = rmem_[0xA] = rsrambankptr_;
      wmem_[0xB] = wmem_[0xA] = wsrambankptr_;
      disconnectOamDmaAreas();
   }

   void MemPtrs::setWrambank(const unsigned bank)
   {
      wramdata_[1] = wramdata_[0] + ((bank & 0x07) ? (bank & 0x07) : 1) * 0x1000;
      rmem_[0xD] = wmem_[0xD] = wramdata_[1] - 0xD000;
      disconnectOamDmaAreas();
   }

   void MemPtrs::setOamDmaSrc(const OamDmaSrc oamDmaSrc)
   {
      rmem_[0x3] = rmem_[0x2] = rmem_[0x1] = rmem_[0x0] = romdata_[0];
      rmem_[0x7] = rmem_[0x6] = rmem_[0x5] = rmem_[0x4] = romdata_[1];
      rmem_[0xB] = rmem_[0xA] = rsrambankptr_;
      wmem_[0xB] = wmem_[0xA] = wsrambankptr_;
      rmem_[0xC] = wmem_[0xC] = wramdata_[0] - 0xC000;
      rmem_[0xD] = wmem_[0xD] = wramdata_[1] - 0xD000;
      rmem_[0xE] = wmem_[0xE] = wramdata_[0] - 0xE000;

      oamDmaSrc_ = oamDmaSrc;
      disconnectOamDmaAreas();
   }

   void MemPtrs::disconnectOamDmaAreas()
   {
      if (isCgb(*this))
      {
         switch (oamDmaSrc_)
         {
            case oam_dma_src_rom:  // fall through
            case oam_dma_src_sram:
            case oam_dma_src_invalid:
               std::fill(rmem_, rmem_ + 8, static_cast<unsigned char *>(0));
               rmem_[0xB] = rmem_[0xA] = 0;
               wmem_[0xB] = wmem_[0xA] = 0;
               break;
            case oam_dma_src_vram:
               break;
            case oam_dma_src_wram:
               rmem_[0xE] = rmem_[0xD] = rmem_[0xC] = 0;
               wmem_[0xE] = wmem_[0xD] = wmem_[0xC] = 0;
               break;
            case oam_dma_src_off:
               break;
         }
      }
      else
      {
         switch (oamDmaSrc_)
         {
            case oam_dma_src_rom:  // fall through
            case oam_dma_src_sram:
            case oam_dma_src_wram:
            case oam_dma_src_invalid:
               std::fill(rmem_, rmem_ + 8, static_cast<unsigned char *>(0));
               rmem_[0xB] = rmem_[0xA] = 0;
               wmem_[0xB] = wmem_[0xA] = 0;
               rmem_[0xE] = rmem_[0xD] = rmem_[0xC] = 0;
               wmem_[0xE] = wmem_[0xD] = wmem_[0xC] = 0;
               break;
            case oam_dma_src_vram:
               break;
            case oam_dma_src_off:
               break;
         }
      }
   }

}
