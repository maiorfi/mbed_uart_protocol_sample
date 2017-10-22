#include "mbed.h"

#define DEBUG_CHANNEL_SWO 0
#define DEBUG_CHANNEL_RTT 1

#if (DEBUG_CHANNEL_SWO)
#include "SWO.h"
SWO_Channel swo("channel");
#endif

#if (DEBUG_CHANNEL_RTT)
#include "SEGGER_RTT.h"
#define RTT_TID_LOG 0
#define RTT_TID_INFO 1
static Thread s_thread_rtt_input;
static EventQueue s_eq_rtt_input;
#endif

static DigitalOut led1(LED1);

static DigitalIn btn(BUTTON1);

static Thread s_thread_blink;
static EventQueue s_eq_blink;

static Timer s_timer_1;

void event_proc_blink(DigitalOut *pled)
{
    pled->write(!pled->read());
}

#if (DEBUG_CHANNEL_RTT)
void event_proc_rtt_input()
{
    if (SEGGER_RTT_HasKey())
    {
        int c = SEGGER_RTT_GetKey();

        switch (c)
        {
        case 'r':
        case 'R':
            NVIC_SystemReset();
            break;
        }
    }
}
#endif

void initTracingSystem()
{
#if (DEBUG_CHANNEL_SWO)
    swo.printf("[SWO DEBUG CHANNEL] Lablet RTOS Demo #1 main()...\n");
#endif

#if (DEBUG_CHANNEL_RTT)
    SEGGER_RTT_SetTerminal(RTT_TID_INFO);
    SEGGER_RTT_printf(0, "%s[RTT DEBUG CHANNEL] Lablet RTOS Demo #1 main()...%s\n", RTT_CTRL_TEXT_BRIGHT_GREEN, RTT_CTRL_RESET);

    SEGGER_RTT_SetTerminal(RTT_TID_LOG);

    s_eq_rtt_input.call_every(1000, event_proc_rtt_input);
    s_thread_rtt_input.start(callback(&s_eq_rtt_input, &EventQueue::dispatch_forever));
#endif
}

int main()
{
    initTracingSystem();

    s_eq_blink.call_every(500, event_proc_blink, &led1);
    s_thread_blink.start(callback(&s_eq_blink, &EventQueue::dispatch_forever));

    s_timer_1.start();
}
