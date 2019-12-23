//
// A simple memory event logger log a const string and 32 bit data value.
//
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "c_types.h"
#include "ets_sys.h"
#include "user_interface.h"
// #include <pgmspace.h>
#include <umm_malloc/umm_malloc_cfg.h>
#include "event_logger.h"

#ifndef DISABLE_EVENTLOG

extern "C" {
#define IRAM_OPTION ICACHE_RAM_ATTR

// Start event log after OTA Data - this leaves 96 words
#ifndef RTC_LOG
#if 1
#define RTC_LOG ((volatile uint32_t*)umm_static_reserve_addr)
#define RTC_LOG_SZ (umm_static_reserve_size/sizeof(uint32_t) - 2U)
#else
#define RTC_LOG ((volatile uint32_t*)0x60001280U)
#define RTC_LOG_SZ ((uint32_t)128 - 32U - 2U)
#endif
#endif

typedef struct _EVENT_LOG_ENTRY {
    const char *str;
    uint32_t data;
} event_log_entry_t;

#ifndef MAX_LOG_EVENTS
#define MAX_LOG_EVENTS (RTC_LOG_SZ/(sizeof(event_log_entry_t)/sizeof(uint32_t))) //(47)
#endif

typedef struct _EVENT_LOG {
    uint32_t num;
    uint32_t state;
    event_log_entry_t event[MAX_LOG_EVENTS];
} event_log_t;

#if 1
static_assert((sizeof(event_log_t) <= umm_static_reserve_size), "MAX_LOG_EVENTS too large exceeds static reserve size.");
#else
static_assert((sizeof(event_log_t) + ((uint32_t)RTC_LOG - 0x60001200U) <= 512U), "MAX_LOG_EVENTS too large. Total RTC Memory usage exceeds 512.");
#endif


static event_log_t volatile * p_event_log __attribute__((section(".noinit")));;

inline void IRAM_OPTION eventlog_clear_log(void) {
     for (size_t i=0; i<(sizeof(event_log_t)/sizeof(int32_t)); i++)
         RTC_LOG[i]=0;
}

inline bool IRAM_OPTION is_inited(void) {
  return ((event_log_t volatile *)RTC_LOG == p_event_log);
}

uint32_t IRAM_OPTION eventlog_get_state(void) {
    if (is_inited())
        return p_event_log->state;

    return 0;
}

uint32_t IRAM_OPTION eventlog_set_state(uint32_t state) {
    uint32_t previous = eventlog_get_state();
    p_event_log->state = state;
    return previous;
}

void IRAM_OPTION eventlog_init(bool force) {
    if (!force && is_inited())
        return;

    uint32_t dirty_value = (uint32_t)p_event_log;
    p_event_log = (event_log_t volatile *)RTC_LOG;

    // We are called early at boot time. When cookie is set don't zero RTC memory
    if ((p_event_log->state & EVENTLOG_COOKIE_MASK) == EVENTLOG_NOZERO_COOKIE) {
        eventlog_log_event(PSTR("*** EventLog Resumed ***"), dirty_value);
        return;
    }

    eventlog_clear_log();
    // Auto enable on init, comment out as the need arises.
    eventlog_set_state(1);
    eventlog_log_event(PSTR("*** EventLog Started ***"), dirty_value);
}

// eventlog_restart(EVENTLOG_NOZERO_COOKIE | 1);

void IRAM_OPTION eventlog_restart(uint32_t state) {
    if (is_inited()) {
        eventlog_clear_log();
        eventlog_set_state(state);
        eventlog_log_event(PSTR("*** EventLog Restarted ***"), 0);
    } else {
        eventlog_init(false);
    }
}

bool IRAM_OPTION eventlog_is_enable(void) {
    if (is_inited()) {
        uint32_t state = eventlog_get_state();
        // if (!(EVENTLOG_INIT_MASK & state))
        //     return false;
        state &= EVENTLOG_ENABLE_MASK;
        return (0 == state) ? false : true;
    }

    return false;
}

#ifdef EVENTLOG_CIRCULAR
uint32_t IRAM_OPTION eventlog_log_event(const char *str, uint32_t data) {
    eventlog_init(false);
    if (eventlog_is_enable()) {
        uint32_t num = p_event_log->num;
        if (num >= MAX_LOG_EVENTS)
            num = 0;

        p_event_log->event[num].str = str;
        p_event_log->event[num].data = data;
        p_event_log->num = ++num;

        return num;
    }

    return 0;
}
#else // Linear log and stop
uint32_t IRAM_OPTION eventlog_log_event(const char *str, uint32_t data) {
    eventlog_init(false);

    if (eventlog_is_enable()) {
        uint32_t num = p_event_log->num;
        if (num < MAX_LOG_EVENTS) {
            p_event_log->event[num].str = str;
            p_event_log->event[num].data = data;
            p_event_log->num = ++num;
            return num;
        } else {
            p_event_log->state &= ~EVENTLOG_ENABLE_MASK;
        }
    }

    return 0;
}
#endif

uint32_t eventlog_get_count(void) {
    if (is_inited())
        return p_event_log->num;

    return 0U;
}

bool eventlog_get_event(const char **pStr, uint32_t *data, bool first) {
    static struct {
        uint32_t next;
        uint32_t start;
    } event = {0, 0};

    if (!is_inited())
        return false;

#ifdef EVENTLOG_CIRCULAR
    if (first) {
        event.start = event.next = eventlog_get_count();
        event.next++;
    } else
    if (0 == event.next)
        return false;

    if (MAX_LOG_EVENTS <= event.next)
        event.next = 0;
#else
    if (first) {
        event.start = eventlog_get_count();
        event.next = 0;
    } else
    if (0 == event.next)
        return false;

    if (MAX_LOG_EVENTS <= event.next)
        return false;
#endif

    if (pStr)
        *pStr = p_event_log->event[event.next].str;

    if (data)
        *data = p_event_log->event[event.next].data;

    event.next++;

    if (event.next == event.start) {
        event.next = 0;  // stop we are done
        return false;
    }

    return true;
}

};

#include "Print.h"

void print_eventlog(Print& out) {
  out.println(F("Event LOG From RTC Memory"));

  uint32_t count=0;
  for (bool more = true; (more) && (count<MAX_LOG_EVENTS); count++) {
    uint32_t data = 0;
    const char *pStr = NULL;
    more = eventlog_get_event(&pStr, &data, (0 == count));
    if (!pStr)
      pStr = PSTR("<null>");
    out.printf(" ");
    out.printf_P(pStr, data);
    out.println();
    // out.println( String(F(" ")) + String(FPSTR(pStr)) + F(" 0x0") + String(data, HEX) );
  }
  out.println(String(count) + F(" Log Events of possible ") + String(MAX_LOG_EVENTS) + F(".") );
}

#endif // DISABLE_EVENTLOG
