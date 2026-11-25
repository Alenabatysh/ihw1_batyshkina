#include "types.h"

// String representations of EVENT_TYPE enum values used in logging and display
const char *event_type_names[] = {
    "CUSTOMER_ARRIVE", "FINISH_SHOPPING",  "QUEUE_CHOICE",
    "QUEUE_SWITCH",    "LEAVE_NO_SERVICE", "CASHIER_WAKE",
    "START_CHECKOUT",  "FINISH_CHECKOUT",  "END_OF_DAY"};