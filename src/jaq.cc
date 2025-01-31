#include "skjack.hh"

#ifndef ARCH_WIN
#include <dlfcn.h>
#else
#include <windows.h>
#define dlopen(x, y) reinterpret_cast<void*>(LoadLibraryA(x))
#define dlsym(x, y) GetProcAddress(reinterpret_cast<HMODULE>(x), y)
#define dlclose(x) FreeLibrary(reinterpret_cast<HMODULE>(x))
#endif

namespace jaq {

   void* client::lib = 0;

   jack_client_t* (*client::x_jack_client_open)(const char*, unsigned long, jack_status_t*);
   jack_nframes_t (*client::x_jack_get_buffer_size)(jack_client_t*);
   jack_nframes_t (*client::x_jack_get_sample_rate)(jack_client_t*);
   int (*client::x_jack_set_buffer_size_callback)(jack_client_t*, JackBufferSizeCallback, void*);
   int (*client::x_jack_set_sample_rate_callback)(jack_client_t*, JackSampleRateCallback, void*);
   int (*client::x_jack_set_process_callback)(jack_client_t*, JackProcessCallback, void*);
   int (*client::x_jack_port_rename)(jack_client_t*, jack_port_t*, const char*);
   int (*client::x_jack_port_unregister)(jack_client_t*, jack_port_t*);
   jack_port_t* (*client::x_jack_port_register)(jack_client_t*, const char*, const char*, unsigned long, unsigned long);
   void* (*client::x_jack_port_get_buffer)(jack_port_t*, jack_nframes_t);
   int (*client::x_jack_activate)(jack_client_t*);
   jack_port_t* (*client::x_jack_port_by_name)(jack_client_t*, const char *);
   char* (*client::x_jack_get_client_name)(jack_client_t *);

   bool port::alive() const {
      return (mom && mom->alive() && handle);
   }

   bool port::register_audio(client& mom, const char* name, unsigned long flags) {
      if (!mom.alive()) return false;
      this->mom = &mom;
      m_flags = flags;

      m_output = (flags & JackPortIsOutput) > 0;

      static const size_t buffer_size = 256;
      char port_name[buffer_size];
      snprintf
	 (reinterpret_cast<char*>(&port_name),
	  buffer_size,
	  "%s:%s-%s",
	  client::x_jack_get_client_name(mom.handle),
	  name,			    // desired port name
	  m_output ? "out" : "in"); // idiomatic suffix

      auto x = client::x_jack_port_by_name(mom.handle, port_name);

      if (x == NULL) {
	 snprintf
	    (reinterpret_cast<char*>(&port_name),
	     buffer_size,
	     "%s-%s",
	     name,		       // desired port name
	     m_output ? "out" : "in"); // idiomatic suffix
       DEBUG("Registering : %s", name);
	 handle = client::x_jack_port_register(
	    mom.handle,
	    name,
	    JACK_DEFAULT_AUDIO_TYPE,
	    flags,
	    0);
      } else return false;

      if (handle) {
        return true;
      }
      else {
	 this->mom = 0;
	 return false;
      }
   }

   bool port::is_output() const {
      return m_output;
   }

   int port::rename(const std::string& new_name) {
      if (!alive()) return -99;

      static const size_t buffer_size = 256;
      char port_name[buffer_size];
      snprintf
	 (reinterpret_cast<char*>(&port_name),
	  buffer_size,
	  "%s:%s-%s",
	  client::x_jack_get_client_name(mom->handle),
	  new_name.c_str(),	    // current port name
	  m_output ? "out" : "in"); // idiomatic suffix
      auto x = client::x_jack_port_by_name(mom->handle, port_name);
      if (x == NULL) {
   	  snprintf
   	    (reinterpret_cast<char*>(&port_name),
   	     buffer_size,
   	     "%s-%s",
   	     new_name.c_str(),	    // desired port name
   	     m_output ? "out" : "in"); // idiomatic suffix
        //DEBUG("Rename %s", new_name.c_str());
        // Rename isn't working for reasons I don't understand.
	     //int ret = client::x_jack_port_rename(mom->handle, handle, new_name.c_str());
        //return ret;
        client::x_jack_port_unregister(mom->handle, handle);
        handle = client::x_jack_port_register(
           mom->handle,
           port_name,
           JACK_DEFAULT_AUDIO_TYPE,
           m_flags,
           0);
        if (handle == NULL) {
         return -98;
        } else {
         return 0;
        }
      }
      else 
	 return -97; // Make this obvious
   }

   void port::unregister() {
      if (alive()) client::x_jack_port_unregister(mom->handle, handle);
   }

   int client::on_jack_buffer_size(jack_nframes_t nframes, void* dptr) {
      auto self = reinterpret_cast<client*>(dptr);
      self->buffersize = nframes;
      return 0;
   }

   int client::on_jack_sample_rate(jack_nframes_t nframes, void* dptr) {
      auto self = reinterpret_cast<client*>(dptr);
      self->samplerate = nframes;
      return 0;
   }

#if ARCH_LIN
#define PARTY_HAT_SUFFIX ".so.0"
#elif ARCH_MAC
#define PARTY_HAT_SUFFIX ".dylib"
#elif ARCH_WIN
#define PARTY_HAT_SUFFIX "64.dll"
#else
#error "Your platform is not yet supported :("
#endif

   bool client::link() {
      lib = dlopen("libjack" PARTY_HAT_SUFFIX, RTLD_LAZY);

#ifndef ARCH_WIN
      if (!lib) {
	 WARN("libjack" PARTY_HAT_SUFFIX " is not in linker path!");
	 lib = dlopen("/usr/lib/libjack" PARTY_HAT_SUFFIX, RTLD_LAZY);
	 if (!lib) {
	    WARN("/usr/lib/libjack" PARTY_HAT_SUFFIX " was not found either");
	    lib = dlopen("/usr/lib/libjack" PARTY_HAT_SUFFIX, RTLD_LAZY);
#endif
	    if (!lib) {
#ifndef ARCH_WIN
	       WARN("/usr/local/lib/libjack" PARTY_HAT_SUFFIX " was not found either");
#endif
	       WARN("I can't find any JACKs.");
	       return false;
	    }
#ifndef ARCH_WIN
	 }
      }
#endif

      INFO("We linked to JACK :^)");

#define knab(y, z) x_##y = reinterpret_cast<z>(dlsym(lib, #y)); if (!x_##y) { WARN("Could not find " #y " in your JACK."); goto goddamnit; }

      knab(jack_client_open, jack_client_t* (*)(const char*, unsigned long, jack_status_t*));
      knab(jack_get_buffer_size, jack_nframes_t (*)(jack_client_t*));
      knab(jack_get_sample_rate, jack_nframes_t (*)(jack_client_t*));
      knab(jack_set_buffer_size_callback, int (*)(jack_client_t*, JackBufferSizeCallback, void*));
      knab(jack_set_sample_rate_callback, int (*)(jack_client_t*, JackSampleRateCallback, void*));
      knab(jack_set_process_callback, int (*)(jack_client_t*, JackProcessCallback, void*));
      knab(jack_port_rename, int (*)(jack_client_t*, jack_port_t*, const char*));
      knab(jack_port_unregister, int (*)(jack_client_t*, jack_port_t*));
      knab(jack_port_register, jack_port_t* (*)(jack_client_t*, const char*, const char*, unsigned long, unsigned long));
      knab(jack_port_get_buffer, void* (*)(jack_port_t*, jack_nframes_t));
      knab(jack_activate, int (*)(jack_client_t*));
      knab(jack_port_by_name, jack_port_t* (*)(jack_client_t*, const char *));
      knab(jack_get_client_name, char* (*)(jack_client_t *));

#undef knab

      return true;

     goddamnit:
      dlclose(lib);
      lib = 0;
      return false;
   }

   bool client::open() {
      if (handle) return true;

      jack_status_t jstatus;
      handle = x_jack_client_open("VCV Rack", JackNoStartServer, &jstatus);
      if (!handle) return false;

      buffersize_max = x_jack_get_buffer_size(handle);
      buffersize = x_jack_get_buffer_size(handle);
      samplerate = x_jack_get_sample_rate(handle);

      x_jack_set_buffer_size_callback(handle, &on_jack_buffer_size, this);
      x_jack_set_sample_rate_callback(handle, &on_jack_sample_rate, this);

      return true;
   }

   bool client::close() {
      if (lib) {
#if 0
	 jack_client_open = 0;
	 jack_get_buffer_size = 0;
	 jack_get_sample_rate = 0;
	 jack_set_buffer_size_callback = 0;
	 jack_set_sample_rate_callback = 0;
	 jack_set_process_callback = 0;
	 jack_port_rename = 0;
	 jack_port_unregister = 0;
	 jack_port_register = 0;
	 jack_port_get_buffer = 0;
	 jack_activate = 0;
#endif
	 dlclose(lib);
	 lib = 0;
      }
      return true;
   }

} /* namespace jaq */

#undef PARTY_HAT_SUFFIX
