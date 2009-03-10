#ifdef __cplusplus
extern "C" {
#endif

enum snavi_event_types {
   MotionEvent,
   ButtonPressEvent, 
   ButtonReleaseEvent
};
#define TranslationMotion (7)
#define RotationMotion (7 << 3)

typedef struct snavi_dev {
   int fd;
   int idLED;
   int iLast;
} snavi_dev_t;

#define KEYPRESS 0

typedef struct snavi_event {
   struct timeval time;
   unsigned short type;
   unsigned short code;
   int axes[3];
} snavi_event_t;

snavi_dev_t* snavi_open (const char* pcName);
void snavi_close(snavi_dev_t* dev);

int  snavi_set_led (snavi_dev_t* dev, int led_state);
int  snavi_get_event (snavi_dev_t* dev, snavi_event_t* event);

#ifdef __cplusplus
}
#endif
