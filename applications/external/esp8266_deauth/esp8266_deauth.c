#include <furry.h>
#include <furry_hal_console.h>
#include <furry_hal_gpio.h>
#include <furry_hal_power.h>
#include <furry_hal_uart.h>
#include <gui/canvas_i.h>
#include <gui/gui.h>
#include <input/input.h>
#include <dolphin/dolphin.h>
//#include <math.h>
//#include <notification/notification.h>
//#include <notification/notification_messages.h>
//#include <stdlib.h>

#include <u8g2.h>

#include "FlipperZeroWiFiDeauthModuleDefines.h"

#define DEAUTH_APP_DEBUG 0

#if DEAUTH_APP_DEBUG
#define APP_NAME_TAG "WiFi_Deauther"
#define DEAUTH_APP_LOG_I(format, ...) FURRY_LOG_I(APP_NAME_TAG, format, ##__VA_ARGS__)
#define DEAUTH_APP_LOG_D(format, ...) FURRY_LOG_D(APP_NAME_TAG, format, ##__VA_ARGS__)
#define DEAUTH_APP_LOG_E(format, ...) FURRY_LOG_E(APP_NAME_TAG, format, ##__VA_ARGS__)
#else
#define DEAUTH_APP_LOG_I(format, ...)
#define DEAUTH_APP_LOG_D(format, ...)
#define DEAUTH_APP_LOG_E(format, ...)
#endif // WIFI_APP_DEBUG

#define DISABLE_CONSOLE !DEAUTH_APP_DEBUG
#define ENABLE_MODULE_POWER 1
#define ENABLE_MODULE_DETECTION 1

typedef enum EEventType // app internally defined event types
{ EventTypeKey // flipper input.h type
} EEventType;

typedef struct SPluginEvent {
    EEventType m_type;
    InputEvent m_input;
} SPluginEvent;

typedef enum EAppContext {
    Undefined,
    WaitingForModule,
    Initializing,
    ModuleActive,
} EAppContext;

typedef enum EWorkerEventFlags {
    WorkerEventReserved = (1 << 0), // Reserved for StreamBuffer internal event
    WorkerEventStop = (1 << 1),
    WorkerEventRx = (1 << 2),
} EWorkerEventFlags;

typedef struct SGpioButtons {
    GpioPin const* pinButtonUp;
    GpioPin const* pinButtonDown;
    GpioPin const* pinButtonOK;
    GpioPin const* pinButtonBack;
} SGpioButtons;

typedef struct SWiFiDeauthApp {
    FurryMutex* mutex;
    Gui* m_gui;
    FurryThread* m_worker_thread;
    //NotificationApp* m_notification;
    FurryStreamBuffer* m_rx_stream;
    SGpioButtons m_GpioButtons;

    bool m_wifiDeauthModuleInitialized;
    bool m_wifiDeauthModuleAttached;

    EAppContext m_context;

    uint8_t m_backBuffer[128 * 8 * 8];
    //uint8_t m_renderBuffer[128 * 8 * 8];

    uint8_t* m_backBufferPtr;
    //uint8_t* m_m_renderBufferPtr;

    //uint8_t* m_originalBuffer;
    //uint8_t** m_originalBufferLocation;
    size_t m_canvasSize;

    bool m_needUpdateGUI;
} SWiFiDeauthApp;

/////// INIT STATE ///////
static void esp8266_deauth_app_init(SWiFiDeauthApp* const app) {
    app->m_context = Undefined;

    app->m_canvasSize = 128 * 8 * 8;
    memset(app->m_backBuffer, DEAUTH_APP_DEBUG ? 0xFF : 0x00, app->m_canvasSize);
    //memset(app->m_renderBuffer, DEAUTH_APP_DEBUG ? 0xFF : 0x00, app->m_canvasSize);

    //app->m_originalBuffer = NULL;
    //app->m_originalBufferLocation = NULL;

    //app->m_m_renderBufferPtr = app->m_renderBuffer;
    app->m_backBufferPtr = app->m_backBuffer;

    app->m_GpioButtons.pinButtonUp = &gpio_ext_pc3;
    app->m_GpioButtons.pinButtonDown = &gpio_ext_pb2;
    app->m_GpioButtons.pinButtonOK = &gpio_ext_pb3;
    app->m_GpioButtons.pinButtonBack = &gpio_ext_pa4;

    app->m_needUpdateGUI = false;

#if ENABLE_MODULE_POWER
    app->m_wifiDeauthModuleInitialized = false;
#else
    app->m_wifiDeauthModuleInitialized = true;
#endif // ENABLE_MODULE_POWER

#if ENABLE_MODULE_DETECTION
    app->m_wifiDeauthModuleAttached = false;
#else
    app->m_wifiDeauthModuleAttached = true;
#endif
}

static void esp8266_deauth_module_render_callback(Canvas* const canvas, void* ctx) {
    furry_assert(ctx);
    SWiFiDeauthApp* app = ctx;
    furry_mutex_acquire(app->mutex, FurryWaitForever);

    //if(app->m_needUpdateGUI)
    //{
    //    app->m_needUpdateGUI = false;

    //    //app->m_canvasSize = canvas_get_buffer_size(canvas);
    //    //app->m_originalBuffer = canvas_get_buffer(canvas);
    //    //app->m_originalBufferLocation = &u8g2_GetBufferPtr(&canvas->fb);
    //    //u8g2_GetBufferPtr(&canvas->fb) = app->m_m_renderBufferPtr;
    //}

    //uint8_t* exchangeBuffers = app->m_m_renderBufferPtr;
    //app->m_m_renderBufferPtr = app->m_backBufferPtr;
    //app->m_backBufferPtr = exchangeBuffers;

    //if(app->m_needUpdateGUI)
    //{
    //    //memcpy(app->m_renderBuffer, app->m_backBuffer, app->m_canvasSize);
    //    app->m_needUpdateGUI = false;
    //}

    switch(app->m_context) {
    case Undefined: {
        canvas_clear(canvas);
        canvas_set_font(canvas, FontPrimary);

        const char* strInitializing = "Something wrong";
        canvas_draw_str(
            canvas,
            (u8g2_GetDisplayWidth(&canvas->fb) / 2) -
                (canvas_string_width(canvas, strInitializing) / 2),
            (u8g2_GetDisplayHeight(&canvas->fb) /
             2) /* - (u8g2_GetMaxCharHeight(&canvas->fb) / 2)*/,
            strInitializing);
    } break;
    case WaitingForModule:
#if ENABLE_MODULE_DETECTION
        furry_assert(!app->m_wifiDeauthModuleAttached);
        if(!app->m_wifiDeauthModuleAttached) {
            canvas_clear(canvas);
            canvas_set_font(canvas, FontSecondary);

            const char* strInitializing = "Attach WiFi Deauther module";
            canvas_draw_str(
                canvas,
                (u8g2_GetDisplayWidth(&canvas->fb) / 2) -
                    (canvas_string_width(canvas, strInitializing) / 2),
                (u8g2_GetDisplayHeight(&canvas->fb) /
                 2) /* - (u8g2_GetMaxCharHeight(&canvas->fb) / 2)*/,
                strInitializing);
        }
#endif
        break;
    case Initializing:
#if ENABLE_MODULE_POWER
    {
        furry_assert(!app->m_wifiDeauthModuleInitialized);
        if(!app->m_wifiDeauthModuleInitialized) {
            canvas_set_font(canvas, FontPrimary);

            const char* strInitializing = "Initializing...";
            canvas_draw_str(
                canvas,
                (u8g2_GetDisplayWidth(&canvas->fb) / 2) -
                    (canvas_string_width(canvas, strInitializing) / 2),
                (u8g2_GetDisplayHeight(&canvas->fb) / 2) -
                    (u8g2_GetMaxCharHeight(&canvas->fb) / 2),
                strInitializing);
        }
    }
#endif // ENABLE_MODULE_POWER
    break;
    case ModuleActive: {
        uint8_t* buffer = canvas_get_buffer(canvas);
        app->m_canvasSize = canvas_get_buffer_size(canvas);
        memcpy(buffer, app->m_backBuffer, app->m_canvasSize);
    } break;
    default:
        break;
    }

    furry_mutex_release(app->mutex);
}

static void
    esp8266_deauth_module_input_callback(InputEvent* input_event, FurryMessageQueue* event_queue) {
    furry_assert(event_queue);

    SPluginEvent event = {.m_type = EventTypeKey, .m_input = *input_event};
    furry_message_queue_put(event_queue, &event, FurryWaitForever);
}

static void uart_on_irq_cb(UartIrqEvent ev, uint8_t data, void* context) {
    furry_assert(context);

    SWiFiDeauthApp* app = context;

    DEAUTH_APP_LOG_I("uart_echo_on_irq_cb");

    if(ev == UartIrqEventRXNE) {
        DEAUTH_APP_LOG_I("ev == UartIrqEventRXNE");
        furry_stream_buffer_send(app->m_rx_stream, &data, 1, 0);
        furry_thread_flags_set(furry_thread_get_id(app->m_worker_thread), WorkerEventRx);
    }
}

static int32_t uart_worker(void* context) {
    furry_assert(context);
    DEAUTH_APP_LOG_I("[UART] Worker thread init");

    SWiFiDeauthApp* app = context;
    furry_mutex_acquire(app->mutex, FurryWaitForever);
    if(app == NULL) {
        return 1;
    }

    FurryStreamBuffer* rx_stream = app->m_rx_stream;

    furry_mutex_release(app->mutex);

#if ENABLE_MODULE_POWER
    bool initialized = false;

    FurryString* receivedString;
    receivedString = furry_string_alloc();
#endif // ENABLE_MODULE_POWER

    while(true) {
        uint32_t events = furry_thread_flags_wait(
            WorkerEventStop | WorkerEventRx, FurryFlagWaitAny, FurryWaitForever);
        furry_check((events & FurryFlagError) == 0);

        if(events & WorkerEventStop) break;
        if(events & WorkerEventRx) {
            DEAUTH_APP_LOG_I("[UART] Received data");
            SWiFiDeauthApp* app = context;
            furry_mutex_acquire(app->mutex, FurryWaitForever);
            if(app == NULL) {
                return 1;
            }

            size_t dataReceivedLength = 0;
            int index = 0;
            do {
                const uint8_t dataBufferSize = 64;
                uint8_t dataBuffer[dataBufferSize];
                dataReceivedLength =
                    furry_stream_buffer_receive(rx_stream, dataBuffer, dataBufferSize, 25);
                if(dataReceivedLength > 0) {
#if ENABLE_MODULE_POWER
                    if(!initialized) {
                        if(!(dataReceivedLength > strlen(MODULE_CONTEXT_INITIALIZATION))) {
                            DEAUTH_APP_LOG_I("[UART] Found possible init candidate");
                            for(uint16_t i = 0; i < dataReceivedLength; i++) {
                                furry_string_push_back(receivedString, dataBuffer[i]);
                            }
                        }
                    } else
#endif // ENABLE_MODULE_POWER
                    {
                        DEAUTH_APP_LOG_I("[UART] Data copied to backbuffer");
                        memcpy(app->m_backBuffer + index, dataBuffer, dataReceivedLength);
                        index += dataReceivedLength;
                        app->m_needUpdateGUI = true;
                    }
                }

            } while(dataReceivedLength > 0);

#if ENABLE_MODULE_POWER
            if(!app->m_wifiDeauthModuleInitialized) {
                if(furry_string_cmp_str(receivedString, MODULE_CONTEXT_INITIALIZATION) == 0) {
                    DEAUTH_APP_LOG_I("[UART] Initialized");
                    initialized = true;
                    app->m_wifiDeauthModuleInitialized = true;
                    app->m_context = ModuleActive;
                    furry_string_free(receivedString);
                } else {
                    DEAUTH_APP_LOG_I("[UART] Not an initialization command");
                    furry_string_reset(receivedString);
                }
            }
#endif // ENABLE_MODULE_POWER

            furry_mutex_release(app->mutex);
        }
    }

    return 0;
}

int32_t esp8266_deauth_app(void* p) {
    UNUSED(p);

    DEAUTH_APP_LOG_I("Init");

    // FurryTimer* timer = furry_timer_alloc(blink_test_update, FurryTimerTypePeriodic, event_queue);
    // furry_timer_start(timer, furry_kernel_get_tick_frequency());

    FurryMessageQueue* event_queue = furry_message_queue_alloc(8, sizeof(SPluginEvent));

    SWiFiDeauthApp* app = malloc(sizeof(SWiFiDeauthApp));

    DOLPHIN_DEED(DolphinDeedPluginStart);
    esp8266_deauth_app_init(app);

    furry_hal_gpio_init_simple(app->m_GpioButtons.pinButtonUp, GpioModeOutputPushPull);
    furry_hal_gpio_init_simple(app->m_GpioButtons.pinButtonDown, GpioModeOutputPushPull);
    furry_hal_gpio_init_simple(app->m_GpioButtons.pinButtonOK, GpioModeOutputPushPull);
    furry_hal_gpio_init_simple(app->m_GpioButtons.pinButtonBack, GpioModeOutputPushPull);

    furry_hal_gpio_write(app->m_GpioButtons.pinButtonUp, true);
    furry_hal_gpio_write(app->m_GpioButtons.pinButtonDown, true);
    furry_hal_gpio_write(app->m_GpioButtons.pinButtonOK, true);
    furry_hal_gpio_write(
        app->m_GpioButtons.pinButtonBack, false); // GPIO15 - Boot fails if pulled HIGH

#if ENABLE_MODULE_DETECTION
    furry_hal_gpio_init(
        &gpio_ext_pc0,
        GpioModeInput,
        GpioPullUp,
        GpioSpeedLow); // Connect to the Flipper's ground just to be sure
    //furry_hal_gpio_add_int_callback(pinD0, input_isr_d0, this);
    app->m_context = WaitingForModule;
#else
#if ENABLE_MODULE_POWER
    app->m_context = Initializing;
    uint8_t attempts = 0;
    while(!furry_hal_power_is_otg_enabled() && attempts++ < 5) {
        furry_hal_power_enable_otg();
        furry_delay_ms(10);
    }
    furry_delay_ms(200);
#else
    app->m_context = ModuleActive;
#endif
#endif // ENABLE_MODULE_DETECTION

    app->mutex = furry_mutex_alloc(FurryMutexTypeNormal);
    if(!app->mutex) {
        DEAUTH_APP_LOG_E("cannot create mutex\r\n");
        free(app);
        return 255;
    }

    DEAUTH_APP_LOG_I("Mutex created");

    //app->m_notification = furry_record_open(RECORD_NOTIFICATION);

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, esp8266_deauth_module_render_callback, app);
    view_port_input_callback_set(view_port, esp8266_deauth_module_input_callback, event_queue);

    // Open GUI and register view_port
    Gui* gui = furry_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    //notification_message(app->notification, &sequence_set_only_blue_255);

    app->m_rx_stream = furry_stream_buffer_alloc(1 * 1024, 1);

    app->m_worker_thread = furry_thread_alloc();
    furry_thread_set_name(app->m_worker_thread, "WiFiDeauthModuleUARTWorker");
    furry_thread_set_stack_size(app->m_worker_thread, 1 * 1024);
    furry_thread_set_context(app->m_worker_thread, app);
    furry_thread_set_callback(app->m_worker_thread, uart_worker);
    furry_thread_start(app->m_worker_thread);
    DEAUTH_APP_LOG_I("UART thread allocated");

    // Enable uart listener
#if DISABLE_CONSOLE
    furry_hal_console_disable();
#endif
    furry_hal_uart_set_br(FurryHalUartIdUSART1, FLIPPERZERO_SERIAL_BAUD);
    furry_hal_uart_set_irq_cb(FurryHalUartIdUSART1, uart_on_irq_cb, app);
    DEAUTH_APP_LOG_I("UART Listener created");

    SPluginEvent event;
    for(bool processing = true; processing;) {
        FurryStatus event_status = furry_message_queue_get(event_queue, &event, 100);
        furry_mutex_acquire(app->mutex, FurryWaitForever);

#if ENABLE_MODULE_DETECTION
        if(!app->m_wifiDeauthModuleAttached) {
            if(furry_hal_gpio_read(&gpio_ext_pc0) == false) {
                DEAUTH_APP_LOG_I("Module Attached");
                app->m_wifiDeauthModuleAttached = true;
#if ENABLE_MODULE_POWER
                app->m_context = Initializing;
                uint8_t attempts2 = 0;
                while(!furry_hal_power_is_otg_enabled() && attempts2++ < 3) {
                    furry_hal_power_enable_otg();
                    furry_delay_ms(10);
                }
#else
                app->m_context = ModuleActive;
#endif
            }
        }
#endif // ENABLE_MODULE_DETECTION

        if(event_status == FurryStatusOk) {
            if(event.m_type == EventTypeKey) {
                if(app->m_wifiDeauthModuleInitialized) {
                    if(app->m_context == ModuleActive) {
                        switch(event.m_input.key) {
                        case InputKeyUp:
                            if(event.m_input.type == InputTypePress) {
                                DEAUTH_APP_LOG_I("Up Press");
                                furry_hal_gpio_write(app->m_GpioButtons.pinButtonUp, false);
                            } else if(event.m_input.type == InputTypeRelease) {
                                DEAUTH_APP_LOG_I("Up Release");
                                furry_hal_gpio_write(app->m_GpioButtons.pinButtonUp, true);
                            }
                            break;
                        case InputKeyDown:
                            if(event.m_input.type == InputTypePress) {
                                DEAUTH_APP_LOG_I("Down Press");
                                furry_hal_gpio_write(app->m_GpioButtons.pinButtonDown, false);
                            } else if(event.m_input.type == InputTypeRelease) {
                                DEAUTH_APP_LOG_I("Down Release");
                                furry_hal_gpio_write(app->m_GpioButtons.pinButtonDown, true);
                            }
                            break;
                        case InputKeyOk:
                            if(event.m_input.type == InputTypePress) {
                                DEAUTH_APP_LOG_I("OK Press");
                                furry_hal_gpio_write(app->m_GpioButtons.pinButtonOK, false);
                            } else if(event.m_input.type == InputTypeRelease) {
                                DEAUTH_APP_LOG_I("OK Release");
                                furry_hal_gpio_write(app->m_GpioButtons.pinButtonOK, true);
                            }
                            break;
                        case InputKeyBack:
                            if(event.m_input.type == InputTypePress) {
                                DEAUTH_APP_LOG_I("Back Press");
                                furry_hal_gpio_write(app->m_GpioButtons.pinButtonBack, false);
                            } else if(event.m_input.type == InputTypeRelease) {
                                DEAUTH_APP_LOG_I("Back Release");
                                furry_hal_gpio_write(app->m_GpioButtons.pinButtonBack, true);
                            } else if(event.m_input.type == InputTypeLong) {
                                DEAUTH_APP_LOG_I("Back Long");
                                processing = false;
                            }
                            break;
                        default:
                            break;
                        }
                    }
                } else {
                    if(event.m_input.key == InputKeyBack) {
                        if(event.m_input.type == InputTypeShort ||
                           event.m_input.type == InputTypeLong) {
                            processing = false;
                        }
                    }
                }
            }
        }

#if ENABLE_MODULE_DETECTION
        if(app->m_wifiDeauthModuleAttached && furry_hal_gpio_read(&gpio_ext_pc0) == true) {
            DEAUTH_APP_LOG_D("Module Disconnected - Exit");
            processing = false;
            app->m_wifiDeauthModuleAttached = false;
            app->m_wifiDeauthModuleInitialized = false;
        }
#endif

        view_port_update(view_port);
        furry_mutex_release(app->mutex);
    }

    DEAUTH_APP_LOG_I("Start exit app");

    furry_thread_flags_set(furry_thread_get_id(app->m_worker_thread), WorkerEventStop);
    furry_thread_join(app->m_worker_thread);
    furry_thread_free(app->m_worker_thread);

    DEAUTH_APP_LOG_I("Thread Deleted");

    // Reset GPIO pins to default state
    furry_hal_gpio_init(&gpio_ext_pc0, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furry_hal_gpio_init(&gpio_ext_pc3, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furry_hal_gpio_init(&gpio_ext_pb2, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furry_hal_gpio_init(&gpio_ext_pb3, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furry_hal_gpio_init(&gpio_ext_pa4, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

#if DISABLE_CONSOLE
    furry_hal_console_enable();
#endif

    //*app->m_originalBufferLocation = app->m_originalBuffer;

    view_port_enabled_set(view_port, false);

    gui_remove_view_port(gui, view_port);

    // Close gui record
    furry_record_close(RECORD_GUI);
    //furry_record_close(RECORD_NOTIFICATION);
    app->m_gui = NULL;

    view_port_free(view_port);

    furry_message_queue_free(event_queue);

    furry_stream_buffer_free(app->m_rx_stream);

    furry_mutex_free(app->mutex);

    // Free rest
    free(app);

    DEAUTH_APP_LOG_I("App freed");

#if ENABLE_MODULE_POWER
    if(furry_hal_power_is_otg_enabled()) {
        furry_hal_power_disable_otg();
    }
#endif

    return 0;
}
