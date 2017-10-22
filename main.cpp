#include "mbed.h"

#include <string>

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

#include "BufferedSerial.h"

static DigitalOut led1(LED1);
static DigitalOut led2(LED2);
static DigitalOut led3(LED6);
static DigitalOut led4(LED5);

static DigitalIn btn(BUTTON1);

static Timer s_timer_1;

BufferedSerial pc_buffered_serial(SERIAL_TX, SERIAL_RX);

static Thread s_thread_serial_worker;
static EventQueue s_eq_serial_worker;

static Thread s_thread_command_handler_worker;
static EventQueue s_eq_command_handler_worker;

#define PROTOCOL_TIMEOUT_MS (1000)

void event_proc_blink(DigitalOut *pled)
{
    pled->write(!pled->read());
}

typedef enum _protocol_states_enum {
    WAITING_START,
    WAITING_END,

} ProtocolStates;

static ProtocolStates current_protocol_state;
static std::string current_protocol_content;
static int current_protocol_timeout_event_id;

void event_proc_command_handler(std::string *pcontent)
{

#if (DEBUG_CHANNEL_RTT)
    if (pcontent->size() != 0)
        SEGGER_RTT_printf(0, "[COMMAND_HANDLER - %d] Ricevuto Comando: '%s'\n", s_timer_1.read_ms(), pcontent->c_str());
#endif

    if (pcontent->compare("CIAO") == 0)
    {
        pc_buffered_serial.printf("!HOLA#");
    }

    delete pcontent;
}

void event_proc_protocol_timeout_handler()
{
    current_protocol_content.clear();
    current_protocol_state = WAITING_START;

#if (DEBUG_CHANNEL_RTT)
    SEGGER_RTT_printf(0, "[PROTOCOL_HANDLER - %d] TIMEOUT (%u ms), Stato Settato a 'WAITING_START'\n", s_timer_1.read_ms(), PROTOCOL_TIMEOUT_MS);
#endif
}

void event_proc_protocol_worker()
{
    led2 = true;

    while (pc_buffered_serial.readable())
    {
        char c = pc_buffered_serial.getc();

        if(c=='\r' || c=='\n') continue;

#if (DEBUG_CHANNEL_RTT)
        SEGGER_RTT_printf(0, "[PROTOCOL_HANDLER - %d] Ricevuto '%c'\n", s_timer_1.read_ms(), c);
#endif

        switch (current_protocol_state)
        {
        case WAITING_START:
            switch (c)
            {
            case '!':
                current_protocol_content.clear();
                current_protocol_state = WAITING_END;
                if (current_protocol_timeout_event_id != 0)
                    s_eq_serial_worker.cancel(current_protocol_timeout_event_id);
                current_protocol_timeout_event_id = s_eq_serial_worker.call_in(PROTOCOL_TIMEOUT_MS, event_proc_protocol_timeout_handler);

#if (DEBUG_CHANNEL_RTT)
                SEGGER_RTT_printf(0, "[PROTOCOL_HANDLER - %d] Stato Settato a 'WAITING_END'\n", s_timer_1.read_ms());
#endif
                break;
            }
            break;

        case WAITING_END:
            switch (c)
            {
            case '!':
                current_protocol_content.clear();
                current_protocol_state = WAITING_END;
                if (current_protocol_timeout_event_id != 0)
                    s_eq_serial_worker.cancel(current_protocol_timeout_event_id);
                current_protocol_timeout_event_id = s_eq_serial_worker.call_in(PROTOCOL_TIMEOUT_MS, event_proc_protocol_timeout_handler);

#if (DEBUG_CHANNEL_RTT)
                SEGGER_RTT_printf(0, "[PROTOCOL_HANDLER - %d] Stato Settato a 'WAITING_END'\n", s_timer_1.read_ms());
#endif
                break;

            case '#':
                s_eq_command_handler_worker.call(event_proc_command_handler, new std::string(current_protocol_content));
                current_protocol_content.clear();
                current_protocol_state = WAITING_START;
                if (current_protocol_timeout_event_id != 0)
                    s_eq_serial_worker.cancel(current_protocol_timeout_event_id);
                current_protocol_timeout_event_id = 0;

#if (DEBUG_CHANNEL_RTT)
                SEGGER_RTT_printf(0, "[PROTOCOL_HANDLER - %d] Stato Settato a 'WAITING_START'\n", s_timer_1.read_ms());
#endif

                break;

            default:
                current_protocol_content.push_back(c);
                break;
            }
            break;
        }
    }

    led2 = false;
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
    swo.printf("[SWO DEBUG CHANNEL] BufferedSerial Sample main()...\n");
#endif

#if (DEBUG_CHANNEL_RTT)
    SEGGER_RTT_SetTerminal(RTT_TID_INFO);
    SEGGER_RTT_printf(0, "%s[RTT DEBUG CHANNEL] BufferedSerial Sample main()...%s\n", RTT_CTRL_TEXT_BRIGHT_GREEN, RTT_CTRL_RESET);

    SEGGER_RTT_SetTerminal(RTT_TID_LOG);

    s_eq_rtt_input.call_every(1000, event_proc_rtt_input);
    s_thread_rtt_input.start(callback(&s_eq_rtt_input, &EventQueue::dispatch_forever));
#endif
}

int main()
{
    initTracingSystem();

    pc_buffered_serial.baud(921600);

    s_eq_serial_worker.call_every(100, event_proc_protocol_worker);
    s_thread_serial_worker.start(callback(&s_eq_serial_worker, &EventQueue::dispatch_forever));

    s_thread_command_handler_worker.start(callback(&s_eq_command_handler_worker, &EventQueue::dispatch_forever));

    s_timer_1.start();
}
