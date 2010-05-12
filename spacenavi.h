#ifndef SPACENAVI_2343242342342_H
#define SPACENAVI_2343242342342_H 

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/time.h>
#include <time.h>

enum snavi_event_types {
   MotionEvent = 1,
   ButtonPressEvent, 
   ButtonReleaseEvent
};
#define TranslationMotion (7)
#define RotationMotion (7 << 3)

#define KEYPRESS 0

typedef struct snavi_event {
   struct timeval time;
   unsigned short type;
   unsigned short code;
   int axes[6];
} snavi_event_t;

/* open the given device, supported flags: O_NONBLOCK */
void* snavi_open (const char* pcName, int flags);
void  snavi_close(void* dev);
int   snavi_get_fd(void* dev);

/* set status of internal LED */
int  snavi_set_led (void* dev, int led_state);
/* retrieve an event */
int  snavi_get_event (void* dev, snavi_event_t* event);

/* set deadzone threshold */
void snavi_set_threshold(void* v, unsigned int iDelta);
unsigned int snavi_get_threshold(void* v);

/* estimate a software calibration in case, the hardware calibration fails */
void snavi_calibrate(void* v);

/* return true, if calibration offset is zero,
	i.e. the device's internal calibration is ok */
int snavi_is_calibrated(void* v);

#ifdef __cplusplus
}
#endif

#endif

