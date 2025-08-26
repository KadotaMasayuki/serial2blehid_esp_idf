#ifndef MY_DEBUG_H
#define MY_DEBUG_H 1

#include "esp_log.h"


#ifdef MYDEBUG
// DEBUG用printfマクロ。DEBUGPRINT()を使う。
#define DEBUGPRINT(...) DEBUGPRINT_INNER(__VA_ARGS__, "")
#define DEBUGPRINT_INNER(fmt, ...)                  \
  do{                                               \
    ESP_LOGI("MYDEBUG", "['%s', %s(), L%d] "fmt"%s",    \
        __FILE__, __func__, __LINE__, __VA_ARGS__); \
  }while(0)
#else
#define DEBUGPRINT(...) (void)0
#endif  // ifdef MYDEBUG


#endif

