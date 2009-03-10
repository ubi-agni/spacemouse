/*
 * spacenavi.c - a proof-of-concept hack to access the
 * 3dconnexion space navigator
 *
 * compile with  gcc -Wall -o spacenavi spacenavi.c
 *
 * run with ./spacenavi [/path/to/eventdevice]
 *
 * Written by Simon Budig, placed in the public domain.
 * it helps to read http://www.frogmouth.net/hid-doco/linux-hid.html .
 *
 * For the LED to work a patch to the linux kernel is needed:
 *   http://www.home.unix-ag.org/simon/files/spacenavigator-hid.patch
 *
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <linux/types.h>
#include <linux/input.h>

#include "spacenavi.h"

#define test_bit(bit, array)  (array [bit / 8] & (1 << (bit % 8)))


static int _snavi_probe(const char* pcName) {
   char name[256]= "Unknown";
   int  fd = open (pcName, O_RDONLY);
   if (fd < 0) return fd;

   if (ioctl (fd, EVIOCGNAME (sizeof (name)), name) < 0)
      goto error; // EVIOCGNAME ioctl failed
   
   if (strstr(name, "3Dconnexion") && strstr(name, "Space")) {
      close(fd);
      return open (pcName, O_RDWR); // re-open RDWR
   }
   
error:
   close(fd);
   return -1;
}

snavi_dev_t* snavi_open (const char* pcName) {
   u_int8_t led_bitmask[(LED_MAX + 7) / 8];
   snavi_dev_t* dev = (snavi_dev_t*) malloc(sizeof(snavi_dev_t));

   if (!dev) return NULL;
   dev->fd = -1;
   dev->idLED = -1;
   dev->iLast = 0;

   if (pcName) { // file explicitly requested
      dev->fd = _snavi_probe(pcName);
   } else if ((dev->fd = _snavi_probe("/dev/input/spacemouse")) < 0) {
      char file[256]; int i;
      for (i=0; i < 20; i++) {
         snprintf(file, 256, "/dev/input/event%d", i);
         if ((dev->fd = _snavi_probe(file)) >= 0) break;
      }
      if (dev->fd >= 0) fprintf (stderr, "found spacemouse at: %s\n", file);
   }
   if (dev->fd < 0) {free (dev); return NULL;}

   // check for LEDs
   memset (led_bitmask, 0, sizeof (led_bitmask));
   if (ioctl (dev->fd, EVIOCGBIT (EV_LED, sizeof (led_bitmask)), led_bitmask) >= 0) {
      int i;
      for (i=0; i < LED_MAX; i++) {
         if (test_bit (i, led_bitmask)) {
            dev->idLED = i;
            break;
         }
      }
   }

   return dev;
}

void snavi_close(snavi_dev_t* dev) {
   if (!dev) return;
   snavi_set_led (dev, 1);
   close (dev->fd);
   free (dev);
}

int snavi_set_led (snavi_dev_t* dev, int led_state) {
   struct input_event event;
   int ret;
   
   if (!dev || dev->idLED < 0) return 0;

   event.type  = EV_LED;
   event.code  = dev->idLED;
   event.value = led_state;

   ret = write (dev->fd, &event, sizeof (struct input_event));
   if (ret < 0) perror ("setting led state failed");
   return ret < sizeof (struct input_event);
}

int  snavi_get_event (snavi_dev_t* dev, snavi_event_t* snavi_event) {
   struct input_event event;
   snavi_event->code = 0;
   snavi_event->axes[0] = snavi_event->axes[1] = snavi_event->axes[2] = 0;

   while (1) {
      if (read (dev->fd, &event, sizeof (struct input_event)) < 0) {
         perror ("read error");
         return -1;
      }

      switch (event.type) {
        case EV_REL:
           if (event.code <= REL_Z) {
              snavi_event->axes[event.code - REL_X] = event.value;
              snavi_event->code |= (1 << (event.code - REL_X));
           } else if (event.code <= REL_RZ) {
              snavi_event->axes[event.code - REL_RX] = event.value;
              snavi_event->code |= (1 << (event.code - REL_X));
           }
           break;

        case EV_KEY:
           snavi_event->type = (event.value ? ButtonPressEvent : ButtonReleaseEvent);
           snavi_event->code = event.code;
           snavi_event->time = event.time;
           return snavi_event->type;
           break;

        case EV_SYN:
           /* if multiple axes change simultaneously the linux
            * input system sends multiple EV_REL events. EV_SYN
            * then indicates that all changes have been reported.
            */
           snavi_event->time = event.time;
           snavi_event->type = MotionEvent;
           return snavi_event->type;
           break;
      }
   }
}

#ifdef TEST
int main (int argc, char *argv[])
{
   int axes[6];
   snavi_dev_t*  dev = snavi_open (NULL);
   snavi_event_t e;
   if (!dev) {
      perror ("could not open spacemouse device");
      exit(-1);
   }
   memset(axes, 0, 6*sizeof(int));

   snavi_set_led (dev, 1);
   while (snavi_get_event(dev, &e) >= 0) {
      switch (e.type) {
        case ButtonPressEvent:
        case ButtonReleaseEvent: 
           fprintf (stderr, "button: %d %s\n", e.code, 
                    e.type==ButtonPressEvent ? "pressed" : "released");
           break;
        case MotionEvent:
           if (e.code & TranslationMotion) {
              axes[0] = e.axes[0];
              axes[1] = e.axes[1];
              axes[2] = e.axes[2];
              dev->iLast = TranslationMotion;
           } else if (e.code & RotationMotion) {
              axes[3] = e.axes[0];
              axes[4] = e.axes[1];
              axes[5] = e.axes[2];
              dev->iLast = RotationMotion;
           } else {
              if (dev->iLast == TranslationMotion) {
                 memset (axes+3, 0, 3*sizeof(int));
                 dev->iLast = RotationMotion;
              } else if (dev->iLast == RotationMotion) {
                 memset (axes, 0, 3*sizeof(int));
                 dev->iLast = TranslationMotion;
              }
           }
           fprintf (stderr, "\nState: %c%c %c%c  %4d %4d %4d %4d %4d %4d",
                    e.code & TranslationMotion ? 'T' : ' ',
                    e.code & RotationMotion ? 'R' : ' ',
                    dev->iLast & TranslationMotion ? 'T' : ' ',
                    dev->iLast & RotationMotion ? 'R' : ' ',
                    axes[0], axes[1], axes[2], axes[3], axes[4], axes[5]);
           break;
      }
   }

   snavi_close (dev);
   exit (0);
}
#endif
