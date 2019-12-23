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
#ifndef EVENT_LOGGER_H
#define EVENT_LOGGER_H

#define ENABLE_EVENTLOG
#ifdef ENABLE_EVENTLOG

#ifdef __cplusplus
extern "C" {
#endif
//
// A simple memory event logger log a const string and 32 bit data value.
//

// // Start event log after OTA Data - this leaves 96 words
// #ifndef RTC_LOG
// #if 1
// #define RTC_LOG ((volatile uint32_t*)umm_static_reserve_addr)
// #define RTC_LOG_SZ (umm_static_reserve_size/sizeof(uint32_t) - 2U)
// #else
// #define RTC_LOG ((volatile uint32_t*)0x60001280U)
// #define RTC_LOG_SZ ((uint32_t)128 - 32U - 2U)
// #endif
// #endif
//
// typedef struct _EVENT_LOG_ENTRY {
//     const char *str;
//     uint32_t data;
// } event_log_entry_t;
//
// #ifndef MAX_LOG_EVENTS
// #define MAX_LOG_EVENTS (RTC_LOG_SZ/(sizeof(event_log_entry_t)/sizeof(uint32_t))) //(47)
// #endif
//
// typedef struct _EVENT_LOG {
//     uint32_t num;
//     uint32_t state;
//     event_log_entry_t event[MAX_LOG_EVENTS];
// } event_log_t;
//
// #define EVENTLOG_NOZERO_COOKIE (0x5A5A0000U)
// #define EVENTLOG_ENABLE_MASK (0x0FFU)
// #define EVENTLOG_INIT_MASK (0x0FFU<<8)
// #define EVENTLOG_COOKIE_MASK (~EVENTLOG_ENABLE_MASK)
// #if 1
// static_assert((sizeof(event_log_t) <= umm_static_reserve_size), "MAX_LOG_EVENTS too large exceeds static reserve size.");
// #else
// static_assert((sizeof(event_log_t) + ((uint32_t)RTC_LOG - 0x60001200U) <= 512U), "MAX_LOG_EVENTS too large. Total RTC Memory usage exceeds 512.");
// #endif


#define EVENTLOG_NOZERO_COOKIE (0x5A5A0000U)
#define EVENTLOG_ENABLE_MASK (0x0FFU)
#define EVENTLOG_INIT_MASK (0x0FFU<<8)
#define EVENTLOG_COOKIE_MASK (~EVENTLOG_ENABLE_MASK)

void eventlog_init(bool force);
bool eventlog_is_enable(void);
uint32_t eventlog_set_state(uint32_t enable);
uint32_t eventlog_get_state(void);
uint32_t eventlog_get_count(void);
uint32_t eventlog_log_event(const char *str, uint32_t data);
// const char *eventlog_get_event(uint32_t num, uint32_t *data);
bool eventlog_get_event(const char **pStr, uint32_t *data, bool first);
void eventlog_restart(uint32_t state);

#ifdef __cplusplus
};
#endif

#ifdef Print_h
void print_eventlog(Print& out);
#endif

#ifndef _EVENT_LOG
#define _EVENT_LOG_P(str, val) eventlog_log_event(str, (uint32_t)val)
#define _EVENT_LOG(str, val) EVENT_LOG_P(PSTR(str), val)
#endif
#ifndef EVENT_LOG
#define EVENT_LOG_P(str, val) _EVENT_LOG_P(str,val)
#define EVENT_LOG(str, val) _EVENT_LOG(str, val)
#endif
#ifndef EVENT_LOG2
#define EVENT_LOG2_P(str, val) _EVENT_LOG_P(str,val)
#define EVENT_LOG2(str, val) _EVENT_LOG(str, val)
#endif
#ifndef EVENT_LOG3
#define EVENT_LOG3_P(str, val) _EVENT_LOG_P(str,val)
#define EVENT_LOG3(str, val) _EVENT_LOG(str, val)
#endif

#else // ! ENABLE_EVENTLOG
#ifndef eventlog_init
#define eventlog_init(a) do{}while(false)
#endif
#ifndef eventlog_restart
#define eventlog_restart(state) do{}while(false)
#endif
#ifdef Print_h
#ifndef print_eventlog
#define print_eventlog(out)  do{}while(false)
#endif
#endif
#ifndef _EVENT_LOG
#define _EVENT_LOG_P(str, val) do{}while(false)
#define _EVENT_LOG(str, val) do{}while(false)
#endif
#ifndef EVENT_LOG
#define EVENT_LOG_P(str, val) _EVENT_LOG_P(str,val)
#define EVENT_LOG(str, val) _EVENT_LOG(str, val)
#endif
#ifndef EVENT_LOG2
#define EVENT_LOG2_P(str, val) _EVENT_LOG_P(str,val)
#define EVENT_LOG2(str, val) _EVENT_LOG(str, val)
#endif
#ifndef EVENT_LOG3
#define EVENT_LOG3_P(str, val) _EVENT_LOG_P(str,val)
#define EVENT_LOG3(str, val) _EVENT_LOG(str, val)
#endif
#endif  // ENABLE_EVENTLOG

#endif
