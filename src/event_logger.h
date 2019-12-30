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
 // A simple memory event logger. Logs a const string and 32 bit data value.
 //
// #ifdef EVLOG_ENABLE

// #define EVLOG_ENABLE
#if !defined(EVENT_LOGGER_H) && defined(EVLOG_ENABLE)
#define EVENT_LOGGER_H

// #define EVLOG_CIRCULAR
#define EVLOG_WITH_DRAM 1

/*
    Timestamp options

    `EVLOG_TIMESTAMP_CLOCKCYCLES` - lowest overhead least intrusive to get. Safe
    call in every context or just about. A good option for when paranoid.

    `EVLOG_TIMESTAMP_MICROS` - next in line. Handle up to an hour before
    wrapping. No experience at this time.

    `EVLOG_TIMESTAMP_MILLIS` - last in line. It can go for 49 days before
    wrapping. More code is executed to get time - unsure of general safety at
    this time. It may not be good for time-critical logging. No experience at
    this time.

*/
#define EVLOG_TIMESTAMP_CLOCKCYCLES   (80000000U) // Wraps at 53.687091 secs. w/80 Mhz CPU clock
#define EVLOG_TIMESTAMP_MICROS        (1000000U)  // Wraps at 1:11:34.967295
#define EVLOG_TIMESTAMP_MILLIS        (1000U)     // Wraps at 49D 17:02:47.295

// Selected Timestamp option from above
#define EVLOG_TIMESTAMP     EVLOG_TIMESTAMP_CLOCKCYCLES
// #undef EVLOG_TIMESTAMP

#ifdef __cplusplus
extern "C" {
#endif


#define EVLOG_NOZERO_COOKIE (0x5A4E0000U)
#define EVLOG_ENABLE_MASK (0x0FFU)
#define EVLOG_COOKIE_MASK (~EVLOG_ENABLE_MASK)
// #define EVLOG_INIT_MASK (0x0FFU<<8)

// EVLOG_TOTAL_ARGS can range from 2 to 5
#define EVLOG_TOTAL_ARGS 5
#define EVLOG_DATA_MAX ((size_t)EVLOG_TOTAL_ARGS - 1U)

typedef struct _EVENT_LOG_ENTRY {
    const char *fmt;
    uint32_t data[EVLOG_DATA_MAX];
#if (EVLOG_TIMESTAMP == EVLOG_TIMESTAMP_CLOCKCYCLES) || \
    (EVLOG_TIMESTAMP == EVLOG_TIMESTAMP_MICROS) || \
    (EVLOG_TIMESTAMP == EVLOG_TIMESTAMP_MILLIS)
    uint32_t ts;
#endif
} evlog_entry_t;

uint32_t evlog_init(void);
void evlog_preinit(uint32_t new_state);
void evlog_restart(uint32_t state);
bool evlog_is_enable(void);
bool evlog_is_inited(void);
void evlog_clear(void);
uint32_t evlog_set_state(uint32_t enable);
uint32_t evlog_get_state(void);
uint32_t evlog_get_count(void);
void evlog_restart(uint32_t state);

inline __attribute__((__always_inline__))
uint32_t evlog_stop(void) {
  return evlog_set_state(evlog_get_state() & ~EVLOG_ENABLE_MASK);
}

inline __attribute__((__always_inline__))
uint32_t evlog_start(void) {
  return evlog_set_state(evlog_get_state() | 1);
}


#if   (EVLOG_TOTAL_ARGS == 5)
uint32_t evlog_event5(const char *fmt, uint32_t data0, uint32_t data1, uint32_t data2, uint32_t data3);
#elif (EVLOG_TOTAL_ARGS == 4)
uint32_t evlog_event4(const char *fmt, uint32_t data0, uint32_t data1, uint32_t data2);
#elif (EVLOG_TOTAL_ARGS == 3)
uint32_t evlog_event3(const char *fmt, uint32_t data0, uint32_t data1);
#elif (EVLOG_TOTAL_ARGS == 2)
uint32_t evlog_event2(const char *fmt, uint32_t data0, uint32_t data1);
#elif (EVLOG_TOTAL_ARGS == 1)
uint32_t evlog_event1(const char *fmt, uint32_t data);
#else
#endif

#if (EVLOG_TOTAL_ARGS > 4)
inline __attribute__((__always_inline__))
uint32_t evlog_event4(const char *fmt, uint32_t data0, uint32_t data1, uint32_t data2) {
  return evlog_event5(fmt, data0, data1, data2, 0);
}
#define EVLOG5_P(fmt, val0, val1, val2, val3) evlog_event5((fmt), (uint32_t)(val0), (uint32_t)(val1), (uint32_t)(val2), (uint32_t)(val3))
#endif

#if (EVLOG_TOTAL_ARGS > 3)
inline __attribute__((__always_inline__))
uint32_t evlog_event3(const char *fmt, uint32_t data0, uint32_t data1) {
  return evlog_event4(fmt, data0, data1, 0);
}
#define EVLOG4_P(fmt, val0, val1, val2) evlog_event4((fmt), (uint32_t)(val0), (uint32_t)(val1), (uint32_t)(val2))
#endif

#if (EVLOG_TOTAL_ARGS > 2)
inline __attribute__((__always_inline__))
uint32_t evlog_event2(const char *fmt, uint32_t data0) {
  return evlog_event3(fmt, data0, 0);
}
#define EVLOG3_P(fmt, val0, val1) evlog_event3((fmt), (val0), (val1))
#endif

#if (EVLOG_TOTAL_ARGS > 1)
inline __attribute__((__always_inline__))
uint32_t evlog_event1(const char *fmt) {
  return evlog_event2(fmt, 0);
}
#define EVLOG2_P(fmt, val0) evlog_event2((fmt), (val0))
#endif

#define EVLOG1_P(fmt) evlog_event1(fmt)

#if (EVLOG_TOTAL_ARGS <= 1)
#define EVLOG2_P(fmt, d0) EVLOG1_P((fmt))
#endif

#if (EVLOG_TOTAL_ARGS <= 2)
#define EVLOG3_P(fmt, d0, d1) EVLOG2_P((fmt), (uint32_t)(d0))
#endif

#if (EVLOG_TOTAL_ARGS <= 3)
#define EVLOG4_P(fmt, d0, d1, d2) EVLOG3_P((fmt), (uint32_t)(d0), (uint32_t)(d1))
#endif

#if (EVLOG_TOTAL_ARGS <= 4)
#define EVLOG5_P(fmt, d0, d1, d2, d3) EVLOG4_P((fmt), (uint32_t)(d0), (uint32_t)(d1), (uint32_t)(d2))
#endif

bool evlog_get_event(evlog_entry_t *entry, bool first);

#ifdef __cplusplus
};
#endif

#ifdef Print_h
// void evlogPrintReport(Print& out);
void evlogPrintReport(Print& out, bool bLocalTime = false);
#endif

#define EVLOG5(fmt, val0, val1, val2, val3)  EVLOG5_P(PSTR(fmt), (val0), (val1), (val2), (val3))
#define EVLOG4(fmt, val0, val1, val2)  EVLOG4_P(PSTR(fmt), (val0), (val1), (val2))
#define EVLOG3(fmt, val0, val1)  EVLOG3_P(PSTR(fmt), (val0), (val1))
#define EVLOG2(fmt, val0) EVLOG2_P(PSTR(fmt), (val0))
#define EVLOG1(fmt) EVLOG1_P(PSTR(fmt))

#else // ! EVLOG_ENABLE
#ifndef evlog_init
#define evlog_init(a) do{}while(false)
#endif
#ifndef evlog_restart
#define evlog_restart(state) do{}while(false)
#endif
#ifdef Print_h
#ifndef evlogPrintReport
// #define evlogPrintReport(out) do{}while(false)
inline __attribute__((__always_inline__))
void evlogPrintReport(Print& out, bool bLocalTime = false) {
  (void)out;
  (void)bLocalTime;
}
#endif
#endif
#ifndef EVLOG5
#define EVLOG5_P(fmt, val0, val1, val2, val3) do{ (void)fmt; (void)val0; (void)val1; (void)val2; (void)val3; }while(false)
#define EVLOG5 EVLOG5_P
#endif
#ifndef EVLOG4
#define EVLOG4_P(fmt, val0, val1, val2) do{ (void)fmt; (void)val0; (void)val1; (void)val2; }while(false)
#define EVLOG4 EVLOG4_P
#endif
#ifndef EVLOG3
#define EVLOG3_P(fmt, val0, val1) do{ (void)fmt; (void)val0; (void)val1; }while(false)
#define EVLOG3 EVLOG3_P
#endif
#ifndef EVLOG2
#define EVLOG2_P(fmt, val0) do{ (void)fmt; (void)val0; }while(false)
#define EVLOG2 EVLOG2_P
#endif
#ifndef EVLOG1
#define EVLOG1_P(fmt) do{ (void)fmt; }while(false)
#define EVLOG1 EVLOG1_P
#endif


#endif  // !defined(EVENT_LOGGER_H) && defined(EVLOG_ENABLE)
