#include <furry.h>
#include <furry_hal.h>
#include <gui/gui.h>
#include <input/input.h>

#include "i2csniffer.h"
#include "i2cscanner.h"
#include "i2csender.h"
#include "views/main_view.h"
#include "views/sniffer_view.h"
#include "views/scanner_view.h"
#include "views/sender_view.h"

// App datas
typedef struct {
    FurryMutex* mutex;
    ViewPort* view_port;
    i2cMainView* main_view;

    i2cScanner* scanner;
    i2cSniffer* sniffer;
    i2cSender* sender;
} i2cTools;
