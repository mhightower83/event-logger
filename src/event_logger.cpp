/*
 *   Copyright 2019 M Hightower
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */
/*
    A simple memory event logger. Logs a const string and 32-bit data value.

    Well, that's what I started with. This can now save up to 20 bytes per log
    entry. That is one printf format string created with PSTR(), 3 - 32-bit
    words, and a 32-bit timestamp. I originally used User RTC Memory minus the
    128 bytes the OTA/eboot task consumed.

    It now uses the highest address of DRAM available from the heap. Or more
    accurately I take a block of memory, DRAM, away from the heap. This block
    is no longer managed by anyone but EvLog. Nobody zero's it. I have been
    successful in carrying data forward between boot cycles.

    STATUS: It has been a while since I last used the RTC build option. So the
    RTC build will not work. There is logic to support circular logging or
    linear logging. EVLOG_CIRCULAR has not been tested a lot. The linear
    logging option has been working well. Linear logging is the default when
    #define EVLOG_CIRCULAR is omitted.

*/
#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "c_types.h"
#include "ets_sys.h"
#include "user_interface.h"

#include <umm_malloc/umm_malloc_cfg.h>
#include <evlog/src/event_logger.h>

#ifdef EVENT_LOGGER_H //EVLOG_ENABLE

extern "C" {
// Need when used from ISR etc context. Comment out otherwise.
#define IRAM_OPTION ICACHE_RAM_ATTR

// Start event log after OTA Data - this leaves 96 words
#ifdef EVLOG_WITH_DRAM
#define EVLOG_ADDR_QUALIFIER
#define EVLOG_ADDR ((EVLOG_ADDR_QUALIFIER uint32_t *)umm_static_reserve_addr)
#define EVLOG_ADDR_SZ (umm_static_reserve_size/sizeof(uint32_t) - 5U)

#else
#define EVLOG_RTC_MEMORY 1
#define EVLOG_ADDR_QUALIFIER volatile
#define EVLOG_ADDR ((EVLOG_ADDR_QUALIFIER uint32_t*)0x60001280U)
#define EVLOG_ADDR_SZ ((uint32_t)128 - 32U - 5U)  // USER_RTC - EBOOT - sizeof(num and state) - sizeof(bool?)
#endif


#ifndef MAX_EVENTS
#define MAX_EVENTS (EVLOG_ADDR_SZ/(sizeof(evlog_entry_t)/sizeof(uint32_t))) //(47)
#endif

typedef struct _EVLOG_STRUCT evlog_t;

struct _EVLOG_STRUCT {
    uintptr_t cookie; // Must be 1st and
    uint32_t num;     // must be 2nd. If changed, clear_log must be updated!
    uint32_t state;
    evlog_entry_t event[MAX_EVENTS];
    bool wrapped;
};

#ifdef EVLOG_WITH_DRAM
static_assert((sizeof(evlog_t) <= umm_static_reserve_size), "MAX_EVENTS too large exceeds static reserve size.");
#else
static_assert((sizeof(evlog_t) + ((uint32_t)EVLOG_ADDR - 0x60001200U) <= 512U), "MAX_EVENTS too large. Total RTC Memory usage exceeds 512.");
#endif

// Solution inspired by https://stackoverflow.com/a/1254012
#define _STR_CAT(w, x) w ## x
#define MK_NAME(y, z) _STR_CAT(y, z)

constexpr uint32_t EVLOG_ADDR_QUALIFIER *pu32_evlog_addr = EVLOG_ADDR;
constexpr evlog_t EVLOG_ADDR_QUALIFIER * p_evlog = (evlog_t EVLOG_ADDR_QUALIFIER *)EVLOG_ADDR;
constexpr uintptr_t k_cookie = (((uintptr_t)p_evlog) << 1 | 1);

inline __attribute__((__always_inline__))
void IRAM_OPTION clear_log(void) {
#if 1
    // cookie must be 1st element in structure and num 2nd.
    ets_memset(&p_evlog->num, 0, sizeof(evlog_t) - sizeof(p_evlog->cookie));
#else
    uintptr_t cookie = p_evlog->cookie;
    for (size_t i=0; i<(sizeof(evlog_t)/sizeof(int32_t)); i++)
        pu32_evlog_addr[i]=0;

    p_evlog->cookie = cookie;
#endif
}

void IRAM_OPTION evlog_clear(void) {
    clear_log();
}

inline __attribute__((__always_inline__))
bool IRAM_OPTION is_inited(void) {
    return (k_cookie == p_evlog->cookie);
}

bool IRAM_OPTION evlog_is_inited(void) {
    return is_inited();
}

/*
*/
uint32_t IRAM_OPTION evlog_get_state(void) {
    if (is_inited())
        return p_evlog->state;

    return 0;
}

uint32_t IRAM_OPTION evlog_set_state(uint32_t state) {
    uint32_t previous = evlog_get_state();
    p_evlog->state = state;
    return previous;
}


/*
  Our block of memory lives outside the normal "C" runtime initialization stuff.
  We need to detect when its bad and zero it.

  `p_evlog->cookie` is our flag that memory has been initialized by us. If
  it is invalid, either we were just powered on, in deep power save (PD pin was
  held low), or we just came out of deep sleep. Either way we need to initialize
  the log buffer.

  TODO: tracking and logging previous value of p_evlog is unecessary once pass early development phase.
*/
uint32_t IRAM_OPTION evlog_init(void) {
    uint32_t dirty_value = (uint32_t)p_evlog;
    if (!is_inited()) {
        clear_log();
        // A unique value to indicate log buffer was initialized
        p_evlog->cookie = k_cookie;
        // Make things just work. For now always enable an inited log.
        // evlog_preinit() can change it from there.
        evlog_set_state(1U);
    }
    return dirty_value;
}

/*
  Ideally this would be called before the NONSDK is called.
  Asside from initial setup it marks the start of a new boot.
  Good places to call from app_entry_redefinable() or preinit()

  `new_state` is ignored if `EVLOG_NOZERO_COOKIE` is found. In this case EvLog
  will continue operation with pre-existing state.
*/
void IRAM_OPTION evlog_preinit(uint32_t new_state) {
    uint32_t dirty_value = evlog_init();
    // If we are called early at boot time. When cookie is set don't zero memory
    if ((p_evlog->state & EVLOG_COOKIE_MASK) == EVLOG_NOZERO_COOKIE) {
        if (MAX_EVENTS < p_evlog->num)
            p_evlog->num = MAX_EVENTS; // Make it a valid value number, log full.
            // Should never occur; however, this will allow for recovery of
            // log in some broken situations.
        EVLOG4(">>> EvLog Resumed <<< state(0x%08X), cookie(0x%08X), p_evlog(0x%08X))", p_evlog->state, p_evlog->cookie, dirty_value);
        return;
    }
    clear_log();
    evlog_set_state(new_state);
    EVLOG4(">>> EvLog Inited <<< state(0x%08X), cookie(0x%08X), p_evlog(0x%08X)", p_evlog->state, p_evlog->cookie, dirty_value);
}

/*
  Use something like this when you want to log activity between boot events.
  Place the line where needed to capture things you want to see before the
  reboot command.

      `evlog_restart(EVLOG_NOZERO_COOKIE | 1);`
*/

void IRAM_OPTION evlog_restart(uint32_t state) {
    uint32_t dirty_value = evlog_init();
    clear_log();
    evlog_set_state(state);
    EVLOG4(">>> EvLog Restarted <<< state(0x%08X), cookie(0x%08X), p_evlog(0x%08X)", state, p_evlog->cookie, dirty_value);
    return;
}

bool IRAM_OPTION evlog_is_enable(void) {
    if (is_inited()) {
        uint32_t state = evlog_get_state();
        state &= EVLOG_ENABLE_MASK;
        return (0 == state) ? false : true;
    }

    return false;
}


#ifdef EVLOG_CIRCULAR

// uint32_t IRAM_OPTION evlog_event5(const char *fmt, uint32_t data0
uint32_t IRAM_OPTION MK_NAME(evlog_event, EVLOG_TOTAL_ARGS)(const char *fmt
#if (EVLOG_TOTAL_ARGS > 1)
      , uint32_t data0
#endif
#if (EVLOG_TOTAL_ARGS > 2)
      , uint32_t data1
#endif
#if (EVLOG_TOTAL_ARGS > 3)
      , uint32_t data2
#endif
#if (EVLOG_TOTAL_ARGS > 4)
      , uint32_t data3
#endif

) {
  evlog_init();

  if (evlog_is_enable()) {
      uint32_t num = p_evlog->num;
      if (num >= MAX_EVENTS) {
          num = 0;
          p_evlog->wrapped = true;
      }

      p_evlog->event[num].fmt = fmt;
#if (EVLOG_TOTAL_ARGS > 1)
      p_evlog->event[num].data[0] = data0;
#endif
#if (EVLOG_TOTAL_ARGS > 2)
      p_evlog->event[num].data[1] = data1;
#endif
#if (EVLOG_TOTAL_ARGS > 3)
      p_evlog->event[num].data[2] = data2;
#endif
#if (EVLOG_TOTAL_ARGS > 4)
      p_evlog->event[num].data[3] = data3;
#endif
#if (EVLOG_TIMESTAMP == EVLOG_TIMESTAMP_CLOCKCYCLES)
      p_evlog->event[num].ts = esp_get_cycle_count();
#elif (EVLOG_TIMESTAMP == EVLOG_TIMESTAMP_MICROS)
      p_evlog->event[num].ts = micros();
#elif (EVLOG_TIMESTAMP == EVLOG_TIMESTAMP_MILLIS)
      p_evlog->event[num].ts = millis();
#endif
      p_evlog->num = ++num;

      return num;
    }

    return 0;
}


#else // Linear log and stop
// Should look something like this when done
//uint32_t IRAM_OPTION evlog_event5(const char *fmt, uint32_t data0, uint32_t data1, uint32_t data2, uint32_t data3)

uint32_t IRAM_OPTION MK_NAME(evlog_event, EVLOG_TOTAL_ARGS)(const char *fmt
// uint32_t IRAM_OPTION evlog_event5(const char *fmt, uint32_t data0
#if (EVLOG_TOTAL_ARGS > 1)
      , uint32_t data0
#endif
#if (EVLOG_TOTAL_ARGS > 2)
      , uint32_t data1
#endif
#if (EVLOG_TOTAL_ARGS > 3)
      , uint32_t data2
#endif
#if (EVLOG_TOTAL_ARGS > 4)
      , uint32_t data3
#endif

) {
    evlog_init();

    if (evlog_is_enable()) {
        uint32_t num = p_evlog->num;
        if (num < MAX_EVENTS) {
            p_evlog->event[num].fmt = fmt;
#if (EVLOG_TOTAL_ARGS > 1)
            p_evlog->event[num].data[0] = data0;
#endif
#if (EVLOG_TOTAL_ARGS > 2)
            p_evlog->event[num].data[1] = data1;
#endif
#if (EVLOG_TOTAL_ARGS > 3)
            p_evlog->event[num].data[2] = data2;
#endif
#if (EVLOG_TOTAL_ARGS > 4)
            p_evlog->event[num].data[3] = data3;
#endif
#if (EVLOG_TIMESTAMP == EVLOG_TIMESTAMP_CLOCKCYCLES)
            p_evlog->event[num].ts = esp_get_cycle_count();
#elif (EVLOG_TIMESTAMP == EVLOG_TIMESTAMP_MICROS)
            p_evlog->event[num].ts = micros();
#elif (EVLOG_TIMESTAMP == EVLOG_TIMESTAMP_MILLIS)
            p_evlog->event[num].ts = millis();
#endif
            p_evlog->num = ++num;
            return num;
        } else {
            p_evlog->state &= ~EVLOG_ENABLE_MASK;
            p_evlog->wrapped = true;
        }
    }

    return 0;
}
#endif

uint32_t evlog_get_count(void) {
    if (is_inited()) {
        if (p_evlog->wrapped)
            return MAX_EVENTS;

        return p_evlog->num;
    }

    return 0U;
}

uint32_t evlog_get_start_index(void) {
#ifdef EVLOG_CIRCULAR
    if (is_inited() && p_evlog->wrapped)
            return p_evlog->num;
#endif
    return 0U;
}

bool evlog_get_event(evlog_entry_t *entry, bool first) {
    static struct {
        uint32_t next;
        uint32_t stop;
    } event = {0, 0};

    if (!is_inited())
        return false;

    if (first) {
        event.stop = p_evlog->num;
        event.next = evlog_get_start_index();
    } else
    if (0 == event.next)
        return false;

#ifdef EVLOG_CIRCULAR
    if (MAX_EVENTS <= event.next)
        event.next = 0;
#else
    if (MAX_EVENTS <= event.next || 0 == event.stop)
        return false;
#endif

    if (entry)
        *entry = p_evlog->event[event.next];

    event.next++;

    if (event.next == event.stop) {
        event.next = 0;  // stop we are done
        return false;
    }

    return true;
}

};

#include "Print.h"
#if 0
extern "C" const char _irom0_pstr_start[];
extern "C" const char _irom0_pstr_end[];
constexpr const char *pstr_area_start = &_irom0_pstr_start[0];
constexpr const char *pstr_area_end = &_irom0_pstr_end[0];

#else
// An alternative for when the above are not defined.
extern "C" const char _irom0_text_start[];
extern "C" const char _irom0_text_end[];
constexpr const char *pstr_area_start = &_irom0_text_start[0];
constexpr const char *pstr_area_end = &_irom0_text_end[0];
#endif
/*
  Validate PSTR fmt pointers -  This is mainly needed to catch misaligned/bad
  pointers from a previous/different boot image.
*/
bool isPstrFmt(const char *pStr) {
  if (pstr_area_start > pStr)
    return false;

  if (pstr_area_end <= pStr)
    return false;

  if (0 != ((uint32_t)pStr & 3U))
    return false;

  if (pstr_area_start == pStr)
    return true;

  // Check that, what whould be a previous string, is 0 terminated.
  union {
    uint32_t word;
    char byte[4];
  } previous;
  previous.word = ((uint32_t *)pStr)[-1];
  // Don't let compiler optomize away the word access from flash
  asm volatile("":::"memory");
  if (previous.byte[3] == 0)
    return true;

  return false;
}

#define EVLOG_TIMESTAMP_CLOCKCYCLES   (80000000U)
#define EVLOG_TIMESTAMP_MICROS        (1000000U)
#define EVLOG_TIMESTAMP_MILLIS        (1000U)

void evlogPrintReport(Print& out, bool bLocalTime) {
  out.println(F("EvLog Report"));

  uint32_t count = 0;
  for (bool more = true; (more) && (count<MAX_EVENTS); count++) {
    evlog_entry_t event;
    more = evlog_get_event(&event, (0 == count));
    if (0 == count && !more)
        break;

    out.printf("  ");

    (void)bLocalTime;
#if (EVLOG_TIMESTAMP == EVLOG_TIMESTAMP_CLOCKCYCLES)
    uint32_t fraction = event.ts;
    fraction /= clockCyclesPerMicrosecond();
    time_t gtime = (time_t)(fraction / 1000000U);
    fraction %= 1000000;
    const char *ts_fmt = PSTR("%s.%06u: ");
    struct tm *tv = gmtime(&gtime);
    char buf[4];
    // if (bLocalTime) not a real option  with a 57 sec resolution
    if (strftime(buf, sizeof(buf), "%S", tv) > 0) {
        out.printf_P(ts_fmt, buf, fraction);
    } else {
        out.print(F("--->>> "));
    }
#elif (EVLOG_TIMESTAMP == EVLOG_TIMESTAMP_MICROS)
    uint32_t fraction = event.ts;
    time_t gtime = (time_t)(fraction / 1000000U);
    fraction %= 1000000;
    const char *ts_fmt = PSTR("%s.%06u: ");
    // TODO: Factor this out of the loop, create an adjustment value
    // Leave to a later date, this needs a lot more thought.
    // if (bLocalTime) {
    //   time_t real_gtime;
    //   time(&real_gtime);
    //   time_t up_time = (time_t)(micros() / 1000000U);
    //   gtime += real_gtime - up_time; // Adjust
    // }
    // if (strftime(buf, sizeof(buf), "%T", localtime(&gtime)) > 0) {
#elif (EVLOG_TIMESTAMP == EVLOG_TIMESTAMP_MILLIS)
    uint32_t fraction = event.ts;
    time_t gtime = (time_t)(fraction / 1000U);
    fraction %= 1000U;
    const char *ts_fmt = PSTR("%s.%03u: ");
#endif
#if (EVLOG_TIMESTAMP == EVLOG_TIMESTAMP_MICROS) || \
    (EVLOG_TIMESTAMP == EVLOG_TIMESTAMP_MILLIS)
    // struct tm *tv = gmtime(&gtime); //localtime(&gtime)
    char buf[10];
    if (strftime(buf, sizeof(buf), "%T", gmtime(&gtime)) > 0) {
        out.printf_P(ts_fmt, buf, fraction);
    } else {
        out.print(F("--->>> "));
    }
#endif

    if (isPstrFmt(event.fmt)) {
        // TODO: Still getting into trouble with badly formated printf format
        // string. Need more intense evaluation of the format string. Should
        // validate that it only contains %d, %u, %p, %x and nutralize
        // additional % values that exceed the supported argument count.
        // Do not want printf to follow pointer values like with a %s.
        out.printf_P(event.fmt
#if (EVLOG_TOTAL_ARGS > 1)
            , event.data[0]
#endif
#if (EVLOG_TOTAL_ARGS > 2)
            , event.data[1]
#endif
#if (EVLOG_TOTAL_ARGS > 3)
            , event.data[2]
#endif
#if (EVLOG_TOTAL_ARGS > 4)
            , event.data[3]
#endif
        );
    } else {
        out.printf_P(PSTR("< ? >, 0x%08X"), (uint32_t)event.fmt);
        for (size_t i=0; i<EVLOG_DATA_MAX ; i++)
            out.printf(PSTR(", 0x%08X"), event.data[i]);
    }
    out.println();
  }

  out.println(String(count) + F(" Logged Events of a possible ") + String(MAX_EVENTS) + '.');
  out.println(String("EVLOG_ADDR_SZ = ") + (EVLOG_ADDR_SZ));
}

#endif // DISABLE_EVLOG
