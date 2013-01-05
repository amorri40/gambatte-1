#include "libretro.h"
#include "resamplerinfo.h"

#include <gambatte.h>

#include <assert.h>
#include <stdio.h>

static retro_video_refresh_t video_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
static retro_audio_sample_t audio_cb;
static retro_environment_t environ_cb;
static gambatte::GB gb;

namespace input
{
   struct map { unsigned snes; unsigned gb; };
   static const map btn_map[] = {
      { RETRO_DEVICE_ID_JOYPAD_A, gambatte::InputGetter::A },
      { RETRO_DEVICE_ID_JOYPAD_B, gambatte::InputGetter::B },
      { RETRO_DEVICE_ID_JOYPAD_SELECT, gambatte::InputGetter::SELECT },
      { RETRO_DEVICE_ID_JOYPAD_START, gambatte::InputGetter::START },
      { RETRO_DEVICE_ID_JOYPAD_RIGHT, gambatte::InputGetter::RIGHT },
      { RETRO_DEVICE_ID_JOYPAD_LEFT, gambatte::InputGetter::LEFT },
      { RETRO_DEVICE_ID_JOYPAD_UP, gambatte::InputGetter::UP },
      { RETRO_DEVICE_ID_JOYPAD_DOWN, gambatte::InputGetter::DOWN },
   };
}

class SNESInput : public gambatte::InputGetter
{
   public:
      unsigned operator()()
      {
         input_poll_cb();
         unsigned res = 0;
         for (unsigned i = 0; i < sizeof(input::btn_map) / sizeof(input::map); i++)
            res |= input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, input::btn_map[i].snes) ? input::btn_map[i].gb : 0;
         return res;
      }
} static gb_input;

static Resampler *resampler;

void retro_get_system_info(struct retro_system_info *info)
{
   info->library_name = "gambatte";
   info->library_version = "v0.5.0";
   info->need_fullpath = false;
   info->block_extract = false;
   info->valid_extensions = "gb|gbc|dmg|zip|GB|GBC|DMG|ZIP";
}

static struct retro_system_timing g_timing;

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   retro_game_geometry geom = { 160, 144, 160, 144 };
   info->geometry = geom;
   info->timing   = g_timing;
}

void retro_init()
{
   // Using uint_least32_t in an audio interface expecting you to cast to short*? :( Weird stuff.
   assert(sizeof(gambatte::uint_least32_t) == sizeof(uint32_t));
   gb.setInputGetter(&gb_input);

   double fps = 4194304.0 / 70224.0;
   double sample_rate = fps * 35112;

   resampler = ResamplerInfo::get(ResamplerInfo::num() - 2).create(sample_rate, 32000.0, 2 * 2064);

   if (environ_cb)
   {
      g_timing.fps = fps;

      unsigned long mul, div;
      resampler->exactRatio(mul, div);

      g_timing.sample_rate = sample_rate * mul / div;
   }
}

void retro_deinit()
{
   delete resampler;
}

void retro_set_environment(retro_environment_t cb) { environ_cb = cb; }
void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { audio_cb = cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t) {}
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

void retro_set_controller_port_device(unsigned, unsigned) {}

void retro_reset() { gb.reset(); }

static size_t serialize_size = 0;
size_t retro_serialize_size()
{
   return gb.stateSize();
}

bool retro_serialize(void *data, size_t size)
{
   if (serialize_size == 0)
      serialize_size = retro_serialize_size();

   if (size != serialize_size)
      return false;

   gb.saveState(data);
   return true;
}

bool retro_unserialize(const void *data, size_t size)
{
   if (serialize_size == 0)
      serialize_size = retro_serialize_size();

   if (size != serialize_size)
      return false;

   gb.loadState(data);
   return true;
}

void retro_cheat_reset() {}
void retro_cheat_set(unsigned, bool, const char *) {}

bool retro_load_game(const struct retro_game_info *info)
{
   bool can_dupe = false;
   environ_cb(RETRO_ENVIRONMENT_GET_CAN_DUPE, &can_dupe);
   if (!can_dupe)
   {
      fprintf(stderr, "[Gambatte]: Cannot dupe frames!\n");
      return false;
   }

   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      fprintf(stderr, "[Gambatte]: XRGB8888 is not supported.\n");
      return false;
   }

   bool load_result = gb.load(info->data, info->size);
   if(load_result==false) return true;
   // else
   //std:string custom_palette_path = TODO: $system_directory/palettes/$input_rom_basename.pal;
   if(fileExists(custom_palette_path))
   {
      FILE palette_file = fopen(custom_palette_path, "r");
      
      unsigned rgb32 = 0;
      for (unsigned palnum = 0; palnum < 3; ++palnum)
         for (unsigned colornum = 0; colornum < 4; ++colornum) {
            // read+parse next line
            // rgb32=...;
            gb.setDmgPaletteColor(palnum, colornum, rgb32);
         }
   }
   
   return(false);
}

static bool fileExists(const std::string &filename) {
   if (std::FILE *const file = std::fopen(filename.c_str(), "rb")) {
		std::fclose(file);
		return true;
	}
	
	return false;
}

bool retro_load_game_special(unsigned, const struct retro_game_info*, size_t) { return false; }

void retro_unload_game() {}

unsigned retro_get_region() { return RETRO_REGION_NTSC; }

void *retro_get_memory_data(unsigned id)
{
   if (id == RETRO_MEMORY_SAVE_RAM)
      return gb.savedata_ptr();
   if (id == RETRO_MEMORY_RTC)
      return gb.rtcdata_ptr();

   return 0;
}

size_t retro_get_memory_size(unsigned id)
{
   if (id == RETRO_MEMORY_SAVE_RAM)
      return gb.savedata_size();
   if (id == RETRO_MEMORY_RTC)
      return gb.rtcdata_size();

   return 0;
}

static void output_audio(const int16_t *samples, unsigned frames)
{
   if (!frames)
      return;

   static int16_t output[2 * 2064];
   std::size_t len = resampler->resample(output, samples, frames);

   // Gambatte pushes so few samples here that we let frontend handle buffering.
   len <<= 1;
   for (std::size_t i = 0; i < len; i += 2)
      audio_cb(output[i], output[i + 1]);
}

void retro_run()
{
   static uint64_t samples_count = 0;
   static uint64_t frames_count = 0;

   input_poll_cb();

   uint64_t expected_frames = samples_count / 35112;
   if (frames_count < expected_frames) // Detect frame dupes.
   {
      video_cb(0, 160, 144, 512);
      frames_count++;
      return;
   }

   union
   {
      gambatte::uint_least32_t u32[2064 + 2064];
      int16_t i16[2 * (2064 + 2064)];
   } static sound_buf;
   unsigned samples = 2064;

   static gambatte::uint_least32_t video_buf[256 * 144];
   gambatte::uint_least32_t param2 = 256;
   while (gb.runFor(video_buf, param2, sound_buf.u32, samples) == -1)
   {
      output_audio(sound_buf.i16, samples);
      samples_count += samples;
      samples = 2064;
   }

   samples_count += samples;
   output_audio(sound_buf.i16, samples);

   video_cb(video_buf, 160, 144, 1024);

   frames_count++;
}

unsigned retro_api_version() { return RETRO_API_VERSION; }

