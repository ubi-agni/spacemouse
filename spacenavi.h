#ifdef __cplusplus
extern "C" {
#endif

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

void* snavi_open (const char* pcName);
void  snavi_close(void* dev);
int   snavi_get_fd(void* dev);

int  snavi_set_led (void* dev, int led_state);
int  snavi_get_event (void* dev, snavi_event_t* event);

void snavi_set_threshold(void* v, unsigned int iDelta);
unsigned int snavi_get_threshold(void* v);

#ifdef __cplusplus
}
#endif
