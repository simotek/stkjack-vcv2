#pragma once

#include "skjack.hh"
#include "dsp/resampler.hpp"
#include "dsp/ringbuffer.hpp"
#include "sr-latch.hh"

#define AUDIO_OUTPUTS 4
#define AUDIO_INPUTS 4
#define JACK_PORTS (AUDIO_OUTPUTS + AUDIO_INPUTS)

struct jack_audio_module_base: public Module {
   enum role_t {
      ROLE_DUPLEX,		// standard skjack module
      ROLE_OUTPUT,		// all ports are outputs
      ROLE_INPUT		// all ports are inputs
   };

   role_t role;
   sr_latch output_latch;

   int lastSampleRate = 0;
   int lastNumOutputs = -1;
   int lastNumInputs = -1;

   dsp::SampleRateConverter<AUDIO_INPUTS> inputSrc;
   dsp::SampleRateConverter<AUDIO_OUTPUTS> outputSrc;

   // in rack's sample rate
   dsp::DoubleRingBuffer<dsp::Frame<AUDIO_INPUTS>, 16> rack_input_buffer;
   dsp::DoubleRingBuffer<dsp::Frame<AUDIO_OUTPUTS>, 16> rack_output_buffer;
   dsp::DoubleRingBuffer<dsp::Frame<AUDIO_INPUTS>, (1<<15)> jack_input_buffer;
   dsp::DoubleRingBuffer<dsp::Frame<AUDIO_OUTPUTS>, (1<<15)> jack_output_buffer;

   std::mutex jmutex;
   jaq::port jport[JACK_PORTS];

   std::string port_names[8];

   void wipe_buffers();
   void globally_register();
   void globally_unregister();
   void assign_stupid_port_names();

   void report_backlogged();

   virtual json_t* toJson() override;
   virtual void fromJson(json_t* json) override;

   jack_audio_module_base(size_t params, size_t inputs,
			  size_t outputs, size_t lights);
   virtual ~jack_audio_module_base();
};

struct JackAudioModule: public jack_audio_module_base {
   enum ParamIds {
      NUM_PARAMS
   };
   enum InputIds {
      ENUMS(AUDIO_INPUT, AUDIO_INPUTS),
      NUM_INPUTS
   };
   enum OutputIds {
      ENUMS(AUDIO_OUTPUT, AUDIO_OUTPUTS),
      NUM_OUTPUTS
   };
   enum LightIds {
      NUM_LIGHTS
   };

   JackAudioModule();
   virtual ~JackAudioModule();

   void process(const ProcessArgs &args) override;
};

struct jack_audio_out8_module: public jack_audio_module_base {
   enum ParamIds { NUM_PARAMS }; // none
   enum InputIds {
      ENUMS(AUDIO_INPUT, JACK_PORTS),
      NUM_INPUTS
   };
   enum OutputIds { NUM_OUTPUTS }; // none
   enum LightIds { NUM_LIGHTS };   // none

   jack_audio_out8_module();
   virtual ~jack_audio_out8_module();

   void process(const ProcessArgs &args) override;
};

struct jack_audio_in8_module: public jack_audio_module_base {
   enum ParamIds { NUM_PARAMS }; // none
   enum InputIds { NUM_INPUTS }; // none
   enum OutputIds {
      ENUMS(AUDIO_OUTPUT, JACK_PORTS),
      NUM_OUTPUTS
   };
   enum LightIds { NUM_LIGHTS };   // none

   jack_audio_in8_module();
   virtual ~jack_audio_in8_module();

   void process(const ProcessArgs &args) override;
};
