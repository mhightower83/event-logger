# EvLog a Simple Event Logger for Debugging
* TODO: Explain core changes needed to use etc.
* TODO: Explain how to use

## An Event Logger for using with Arduino ESP8266 Core

A simple memory event logger. Logs a const string and 32 bit data value.

Well thats what I started with. This can now save up to 20 bytes per log entry. That is one printf format string created with `PSTR()`, 3 - 32 bit words, and a 32 bit timestamp. I origianly used User RTC Memory minus the 128
bytes the OTA/eboot task consumed.

It now use the highest address of DRAM available from the heap. Or more
accurately I take a block of memory, DRAM, away from the heap. This block is
no longer managed by anyone but my evlog. Nobody zero's it. I have been
successful with carrying data forward between boot cycles.

STATUS: It has been a while since I last used the RTC build option. So the
RTC build will not work. There is logic to support a Circular log; however,
only the linear log has been used so far. `EVLOG_CIRCULAR` is untested and
does not work. Linear logging option has been working well.

### Time stamp options
* `EVLOG_TIMESTAMP_CLOCKCYCLES` - lowest over head least intrusive to get. Safe
call in every context or just about. Good option for when paranoid.
* `EVLOG_TIMESTAMP_MICROS` - next in line. Handle up to an hour before wrapping.
No experiance at this time.
* `EVLOG_TIMESTAMP_MILLIS` - last in line. Can go for 49 days before wrapping.
More code is excuted to get time - unsure of general safety at this time.
May not be good for time critial logging. No experiance at this time.

## Flash Stats
An add on to use with Evlog.

Purpose here is to gain insight on what and how the ROM routines are used.
Two areas of interest
  1) Flash access
  2) Boot ROM Function calls

ROM functions entry points are provided through a liner load table using the
PROVIDE directive. This is in escence a weak link and can be replaced. We
can watch ROM function calls by offerring replacement functions that  then
passthrough to the origianl ROM function. Evlog is used to capture
interesting information. Some counters are kept on other Flash functions.

## Edits for umm_malloc_cfg.h
Find this block of code:
```cpp
#ifdef TEST_BUILD
extern char test_umm_heap[];
#endif
```
_new stuff will go here_
```cpp
#ifdef TEST_BUILD
/* Start addresses and the size of the heap */
#define UMM_MALLOC_CFG_HEAP_ADDR (test_umm_heap)
#define UMM_MALLOC_CFG_HEAP_SIZE 0x10000
#else
/* Start addresses and the size of the heap */
extern char _heap_start[];
#define UMM_MALLOC_CFG_HEAP_ADDR   ((uint32_t)&_heap_start[0])
#define UMM_MALLOC_CFG_HEAP_SIZE   ((size_t)(0x3fffc000 - UMM_MALLOC_CFG_HEAP_ADDR))
#endif

/* A couple of macros to make packing structures less compiler dependent */
```
Insert this block after the 1st `#endif`:
```cpp
/*
 * Reserve a block of space from the Heap DRAM.
 * Reserved space will not be zeroed by umm_init.
 * Reserved space is excluded from the heap allocation pool.
 *   ie. not reported by umm_info.
 * For all purposes the Heap Manager does not know it exist.
 * May be used for debugging issues across boot events and other applications.
 */
#define HEAP_STATIC_RESERVE_SIZE 1536
#ifndef HEAP_STATIC_RESERVE_SIZE
#define HEAP_STATIC_RESERVE_SIZE 0
#endif

#define HEAP_STATIC_RESERVE_ALIGN8_SIZE ((HEAP_STATIC_RESERVE_SIZE + 7U) & ~7U)

#if HEAP_STATIC_RESERVE_SIZE
constexpr void * umm_static_reserve_addr =
  (void *)((uint32_t)0x3fffc000 - (uint32_t)HEAP_STATIC_RESERVE_ALIGN8_SIZE);
#else
constexpr void * umm_static_reserve_addr = NULL;
#endif

constexpr size_t umm_static_reserve_size = (size_t)HEAP_STATIC_RESERVE_SIZE;

inline __attribute__((__always_inline__))
void *umm_get_static_reserve_addr(void) {
  return umm_static_reserve_addr;
}

inline __attribute__((__always_inline__))
size_t umm_get_static_reserve_size(void) {
  return umm_static_reserve_size;
}
```
Then edit this line:
```cpp
#define UMM_MALLOC_CFG_HEAP_SIZE   ((size_t)(0x3fffc000 - UMM_MALLOC_CFG_HEAP_ADDR))
```
To look like this:
```cpp
#define UMM_MALLOC_CFG_HEAP_SIZE   ((size_t)(0x3fffc000 - UMM_MALLOC_CFG_HEAP_ADDR - HEAP_STATIC_RESERVE_ALIGN8_SIZE))
```

To change the buffer size find and update this line for the amount of DRAM you want EvLog to use:
```cpp
#define HEAP_STATIC_RESERVE_SIZE 1536
```

