/*
    This is based on core_esp8266core_esp8266_app_entry_noextra4k.cpp.

*/
/*
 *  This is the original app_entry() not providing extra 4K heap, but allowing
 *  the use of WPS.
 *
 *  see comments in core_esp8266_main.cpp's app_entry()
 *
 */
#include <c_types.h>
#include "cont.h"
#include "coredecls.h"
#include <spi_flash.h>
#include <evlog/src/event_logger.h>
#include <evlog/src/flash_stats.h>

#if ENABLE_EVLOG_MAIN
void enable_evlog_at_link_time(void)
{
    /*
     * does nothing
     * allows overriding the core_esp8266_main.cpp's app_entry()
     * by this one below, at link time
     *
     */
}

/* the following code is linked only if a call to the above function is made somewhere */

extern "C" void call_user_start();

#if (ERASE_CONFIG_METHOD == 2)
extern int eboot_two_shots;
#endif

#ifdef EVLOG_NOEXTRA4K
/* this is the default NONOS-SDK user's heap location */
static cont_t g_cont __attribute__ ((aligned (16)));
#endif

extern cont_t* g_pcont;

extern "C" void app_entry_redefinable(void)
{
    evlog_preinit(1);
    EVLOG1("*** app_entry_redefinable()");
    EVLOG2("flashchip->chip_size, %d",  flashchip->chip_size);
#if ENABLE_FLASH_STATS
    preinit_flash_stats();
#endif

#ifdef EVLOG_NOEXTRA4K
    g_pcont = &g_cont;
#else
    /* Allocate continuation context on this SYS stack,
       and save pointer to it. */
    cont_t s_cont __attribute__((aligned(16)));
    g_pcont = &s_cont;
#endif

#if (ERASE_CONFIG_METHOD == 2)
    eboot_two_shots = 2;
#endif

    /* Call the entry point of the SDK code. */
    EVLOG1("*** call_user_start() - NONOS SDK");
    call_user_start();
}

#endif
