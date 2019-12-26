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
#ifndef EVENT_LOGGER_H
#define EVENT_LOGGER_H

#define ENABLE_EVLOG
// #define EVLOG_CIRCULAR

#ifdef ENABLE_EVLOG
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


#define EVLOG_NOZERO_COOKIE (0x5A5A0000U)
#define EVLOG_ENABLE_MASK (0x0FFU)
#define EVLOG_COOKIE_MASK (~EVLOG_ENABLE_MASK)
// #define EVLOG_INIT_MASK (0x0FFU<<8)

#define EVLOG_ARG4 4
#define EVLOG_ARGS_MAX ((size_t)EVLOG_ARG4 - 1U)

typedef struct _EVENT_LOG_ENTRY {
    const char *fmt;
    uint32_t data[EVLOG_ARGS_MAX];
#if (EVLOG_TIMESTAMP == EVLOG_TIMESTAMP_CLOCKCYCLES) || \
    (EVLOG_TIMESTAMP == EVLOG_TIMESTAMP_MICROS) || \
    (EVLOG_TIMESTAMP == EVLOG_TIMESTAMP_MILLIS)
    uint32_t ts;
#endif
} evlog_entry_t;

void evlog_init(bool force);
bool evlog_is_enable(void);
uint32_t evlog_set_state(uint32_t enable);
uint32_t evlog_get_state(void);
uint32_t evlog_get_count(void);
void evlog_restart(uint32_t state);

#if (EVLOG_ARG4 == 4)
uint32_t evlog_event4(const char *fmt, uint32_t data0, uint32_t data1, uint32_t data2);
uint32_t evlog_event2(const char *fmt, uint32_t data);
// inline __attribute__((__always_inline__))
// uint32_t evlog_event2(const char *fmt, uint32_t data) {
//   return evlog_event4(fmt, data, 0, 0);
// }

#else
uint32_t evlog_event2(const char *fmt, uint32_t data);

inline __attribute__((__always_inline__))
uint32_t evlog_event4(const char *fmt, uint32_t data0, uint32_t data1, uint32_t data2) {
  (void)data1;
  (void)data2;
  return evlog_event2(fmt, data0);
}
#endif

bool evlog_get_event(evlog_entry_t *entry, bool first);

#ifdef __cplusplus
};
#endif

#ifdef Print_h
void evlogPrintReport(Print& out);
#endif

#define EVLOG4_P(fmt, val0, val1, val2) evlog_event4((fmt), (uint32_t)(val0), (uint32_t)(val1), (uint32_t)(val2))
#define EVLOG4(fmt, val0, val1, val2)  EVLOG4_P(PSTR(fmt), (val0), (val1), (val2))

#define EVLOG3_P(fmt, val0, val1) EVLOG4_P((fmt), (val0), (val1), 0)
#define EVLOG3(fmt, val0, val1)  EVLOG3_P(PSTR(fmt), (val0), (val1))

#define EVLOG2_P(fmt, val0) EVLOG3_P((fmt), (val0), 0)
#define EVLOG2(fmt, val0) EVLOG2_P(PSTR(fmt), (val0))
//
// #define EVLOG2_P(fmt, val0) EVLOG4_P((fmt), (val0), 0, 0)
// #define EVLOG2(fmt, val0) EVLOG4_P(PSTR(fmt), (val0), 0, 0)

#define EVLOG1_P(fmt) EVLOG2_P(fmt, 0)
#define EVLOG1(fmt) EVLOG1_P(PSTR(fmt))

#else // ! ENABLE_EVLOG
#ifndef evlog_init
#define evlog_init(a) do{}while(false)
#endif
#ifndef evlog_restart
#define evlog_restart(state) do{}while(false)
#endif
#ifdef Print_h
#ifndef evlogPrintReport
#define evlogPrintReport(out) do{}while(false)
#endif
#endif
#ifndef EVLOG4
#define EVLOG4_P(fmt, val0, val1, val2) do{}while(false)
#define EVLOG4(fmt, val0, val1, val2) do{}while(false)
#endif
#ifndef EVLOG3
#define EVLOG3_P(fmt, val0, val1) do{}while(false)
#define EVLOG3(fmt, val0, val1) do{}while(false)
#endif
#ifndef EVLOG2
#define EVLOG2_P(fmt, val) do{}while(false)
#define EVLOG2(fmt, val) do{}while(false)
#endif
#ifndef EVLOG1
#define EVLOG1_P(fmt) do{}while(false)
#define EVLOG1(fmt) do{}while(false)
#endif
#endif  // ENABLE_EVLOG

// #ifndef EVLOG2
// #define EVLOG2_P(fmt, val) _EVLOG2_P(fmt,val)
// #define EVLOG2(fmt, val) _EVLOG2(fmt, val)
// #endif
// #ifndef EVLOG22
// #define EVLOG22_P(fmt, val) _EVLOG2_P(fmt,val)
// #define EVLOG22(fmt, val) _EVLOG2(fmt, val)
// #endif
// #ifndef EVLOG23
// #define EVLOG23_P(fmt, val) _EVLOG2_P(fmt,val)
// #define EVLOG23(fmt, val) _EVLOG2(fmt, val)
// #endif


#endif // EVENT_LOGGER_H
