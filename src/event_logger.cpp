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
//
// A simple memory event logger log a const string and 32 bit data value.
//
#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "c_types.h"
#include "ets_sys.h"
#include "user_interface.h"
// #include <pgmspace.h>
#include <umm_malloc/umm_malloc_cfg.h>
#include <evlog/src/event_logger.h>

#ifdef ENABLE_EVLOG

extern "C" {
// Need when used from ISR etc context. Comment out otherwise.
#define IRAM_OPTION ICACHE_RAM_ATTR

// Start event log after OTA Data - this leaves 96 words
#ifdef EVLOG_WITH_DRAM
#define EVLOG_ADDR_QUALIFIER
#define EVLOG_ADDR ((EVLOG_ADDR_QUALIFIER uint32_t*)umm_static_reserve_addr)
#define EVLOG_ADDR_SZ (umm_static_reserve_size/sizeof(uint32_t) - 2U)

#else
#define EVLOG_RTC_MEMORY 1
#define EVLOG_ADDR_QUALIFIER volatile
#define EVLOG_ADDR ((EVLOG_ADDR_QUALIFIER uint32_t*)0x60001280U)
#define EVLOG_ADDR_SZ ((uint32_t)128 - 32U - 2U)  // USER_RTC - EBOOT - sizeof(num and state)
#endif

#ifndef MAX_EVENTS
#define MAX_EVENTS (EVLOG_ADDR_SZ/(sizeof(evlog_entry_t)/sizeof(uint32_t))) //(47)
#endif

typedef struct _EVENT_LOG {
    uint32_t num;
    uint32_t state;
    evlog_entry_t event[MAX_EVENTS];
} evlog_t;

#ifdef EVLOG_WITH_DRAM
static_assert((sizeof(evlog_t) <= umm_static_reserve_size), "MAX_EVENTS too large exceeds static reserve size.");
#else
static_assert((sizeof(evlog_t) + ((uint32_t)EVLOG_ADDR - 0x60001200U) <= 512U), "MAX_EVENTS too large. Total RTC Memory usage exceeds 512.");
#endif


static evlog_t EVLOG_ADDR_QUALIFIER * p_evlog __attribute__((section(".noinit")));;

inline void IRAM_OPTION evlog_clear_log(void) {
     for (size_t i=0; i<(sizeof(evlog_t)/sizeof(int32_t)); i++)
         EVLOG_ADDR[i]=0;
}

inline bool IRAM_OPTION is_inited(void) {
  // return ((evlog_t EVLOG_ADDR_QUALIFIER *)EVLOG_ADDR == p_evlog);
  return ((uint32_t)EVLOG_ADDR == (uint32_t)p_evlog);
}

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

void IRAM_OPTION evlog_init(bool force) {
    if (!force && is_inited())
        return;

    uint32_t dirty_value = (uint32_t)p_evlog;
    p_evlog = (evlog_t EVLOG_ADDR_QUALIFIER *)EVLOG_ADDR;

    // We are called early at boot time. When cookie is set don't zero RTC memory
    if ((p_evlog->state & EVENTLOG_COOKIE_MASK) == EVENTLOG_NOZERO_COOKIE) {
        EVLOG2("*** EventLog Resumed *** 0x%08X", dirty_value);
        return;
    }

    evlog_clear_log();
    // Auto enable on init, comment out as the need arises.
    evlog_set_state(1);
    EVLOG2("*** EventLog Started *** 0x%08", dirty_value);
}

// evlog_restart(EVENTLOG_NOZERO_COOKIE | 1);

void IRAM_OPTION evlog_restart(uint32_t state) {
    if (is_inited()) {
        evlog_clear_log();
        evlog_set_state(state);
        EVLOG1("*** EventLog Restarted ***");
    } else {
        evlog_init(false);
    }
}

bool IRAM_OPTION evlog_is_enable(void) {
    if (is_inited()) {
        uint32_t state = evlog_get_state();
        state &= EVENTLOG_ENABLE_MASK;
        return (0 == state) ? false : true;
    }

    return false;
}

#ifdef EVENTLOG_CIRCULAR
#if (EVLOG_ARG4 == 4)
uint32_t IRAM_OPTION evlog_event4(const char *fmt, uint32_t data0, uint32_t data1, uint32_t data2) {
  evlog_init(false);

  if (evlog_is_enable()) {
      uint32_t num = p_evlog->num;
      if (num >= MAX_EVENTS)
          num = 0;

      p_evlog->event[num].fmt = fmt;
      p_evlog->event[num].data[0] = data0;
      p_evlog->event[num].data[1] = data1;
      p_evlog->event[num].data[2] = data2;
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

#else
uint32_t IRAM_OPTION evlog_event2(const char *fmt, uint32_t data0) {
    evlog_init(false);

    if (evlog_is_enable()) {
        uint32_t num = p_evlog->num;
        if (num >= MAX_EVENTS)
            num = 0;

        p_evlog->event[num].fmt = fmt;
        p_evlog->event[num].data[0] = data0;
        p_evlog->num = ++num;

        return num;
    }

    return 0;
}
#endif

#else // Linear log and stop
#if (EVLOG_ARG4 == 4)
uint32_t IRAM_OPTION evlog_event4(const char *fmt, uint32_t data0, uint32_t data1, uint32_t data2) {
    evlog_init(false);

    if (evlog_is_enable()) {
        uint32_t num = p_evlog->num;
        if (num < MAX_EVENTS) {
            p_evlog->event[num].fmt = fmt;
            p_evlog->event[num].data[0] = data0;
            p_evlog->event[num].data[1] = data1;
            p_evlog->event[num].data[2] = data2;
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
            p_evlog->state &= ~EVENTLOG_ENABLE_MASK;
        }
    }

    return 0;
}

uint32_t IRAM_OPTION evlog_event2(const char *fmt, uint32_t data) {
  return evlog_event4(fmt, data, 0, 0);
}

#else
uint32_t IRAM_OPTION evlog_event2(const char *fmt, uint32_t data) {
    evlog_init(false);

    if (evlog_is_enable()) {
        uint32_t num = p_evlog->num;
        if (num < MAX_EVENTS) {
            p_evlog->event[num].fmt = fmt;
            p_evlog->event[num].data[0] = data;
            p_evlog->num = ++num;
            return num;
        } else {
            p_evlog->state &= ~EVENTLOG_ENABLE_MASK;
        }
    }

    return 0;
}
#endif
#endif

uint32_t evlog_get_count(void) {
    if (is_inited())
        return p_evlog->num;

    return 0U;
}

bool evlog_get_event(evlog_entry_t *entry, bool first) {
    static struct {
        uint32_t next;
        uint32_t start;
    } event = {0, 0};

    if (!is_inited())
        return false;

#ifdef EVENTLOG_CIRCULAR
    if (first) {
        event.start = event.next = evlog_get_count();
        event.next++;
    } else
    if (0 == event.next)
        return false;

    if (MAX_EVENTS <= event.next)
        event.next = 0;
#else
    if (first) {
        event.start = evlog_get_count();
        event.next = 0;
    } else
    if (0 == event.next)
        return false;

    if (MAX_EVENTS <= event.next)
        return false;
#endif

    if (entry)
        *entry = p_evlog->event[event.next];

    event.next++;

    if (event.next == event.start) {
        event.next = 0;  // stop we are done
        return false;
    }

    return true;
}

};

#include "Print.h"

extern "C" const char _irom0_pstr_start[];
extern "C" const char _irom0_pstr_end[];

inline __attribute__((__always_inline__))
bool isPstr(const void *pStr) {
  return (
    &_irom0_pstr_start[0] <= pStr &&
    &_irom0_pstr_end[0]    > pStr &&
    0 == ((uint32_t)pStr & 3U) );
}

#define EVLOG_TIMESTAMP_CLOCKCYCLES   (80000000U)
#define EVLOG_TIMESTAMP_MICROS        (1000000U)
#define EVLOG_TIMESTAMP_MILLIS        (1000U)

void evlog_print_report(Print& out) {
  out.println(F("Event Log Report"));

  uint32_t count = 0;
  for (bool more = true; (more) && (count<MAX_EVENTS); count++) {
    evlog_entry_t event;
    more = evlog_get_event(&event, (0 == count));
    out.printf("  ");
#if (EVLOG_ARG4 == 4)
    if (isPstr(event.fmt)) {

#if (EVLOG_TIMESTAMP == EVLOG_TIMESTAMP_CLOCKCYCLES)
        uint32_t fraction = event.ts;
        fraction /= clockCyclesPerMicrosecond();
        time_t gtime = (time_t)(fraction / 1000000U);
        fraction %= 1000000;
        const char *ts_fmt = PSTR("%s.%06u: ");
#elif (EVLOG_TIMESTAMP == EVLOG_TIMESTAMP_MICROS)
        uint32_t fraction = event.ts;
        time_t gtime = (time_t)(fraction / 1000000U);
        fraction %= 1000000;
        const char *ts_fmt = PSTR("%s.%06u: ");
#elif (EVLOG_TIMESTAMP == EVLOG_TIMESTAMP_MILLIS)
        uint32_t fraction = event.ts;
        time_t gtime = (time_t)(fraction / 1000U);
        fraction %= 1000U;
        const char *ts_fmt = PSTR("%s.%03u: ");
#endif
#if (EVLOG_TIMESTAMP == EVLOG_TIMESTAMP_CLOCKCYCLES) || (EVLOG_TIMESTAMP == EVLOG_TIMESTAMP_MICROS) || (EVLOG_TIMESTAMP == EVLOG_TIMESTAMP_MILLIS)
        struct tm *tv = gmtime(&gtime);
        char buf[10];
        strftime(buf, sizeof(buf), "%T", tv);
        out.printf_P(ts_fmt, buf, fraction);
#endif

      out.printf_P(event.fmt, event.data[0], event.data[1], event.data[2]);
    } else {
      out.printf_P("< ? >, 0x%8p, 0x%08X, 0x%08X, 0x%08X",
                   event.fmt, event.data[0], event.data[1], event.data[2]);
    }
#else
    if (isPstr(event.fmt)) {
      out.printf_P(event.fmt, event.data[0]);
    } else {
      out.printf_P("< ? >, 0x%08X, 0x%08X", event.fmt, event.data[0]);
    }
#endif
    out.println();
  }

  out.println(String(count) + F(" Log Events of possible ") + String(MAX_EVENTS) + F(".") );
  out.println(String("EVLOG_ADDR_SZ = ") + (EVLOG_ADDR_SZ));
}

void print_evlog(Print& out) {
  evlog_print_report(out);
}

#endif // DISABLE_EVENTLOG
