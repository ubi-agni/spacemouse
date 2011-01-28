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

typedef struct snavi_dev {
   int fd;
   int idLED;
   int iLast;
   unsigned int iDelta; // threshold
	int offset[6];
} snavi_dev_t;

#define CAST(v) ((snavi_dev_t*)v)

static int _snavi_probe(const char* pcName, int flags) {
   char name[256]= "Unknown";
   int  fd = open (pcName, O_RDONLY | flags);
   if (fd < 0) return fd;

   if (ioctl (fd, EVIOCGNAME (sizeof (name)), name) < 0)
      goto error; // EVIOCGNAME ioctl failed
   
   if (strstr(name, "3Dconnexion") && strstr(name, "Space")) {
      close(fd);
      return open (pcName, O_RDWR | flags); // re-open RDWR
   }
   
error:
   close(fd);
   return -1;
}

void* snavi_open (const char* pcName, int flags) {
   u_int8_t led_bitmask[(LED_MAX + 7) / 8];
   snavi_dev_t* dev = (snavi_dev_t*) malloc(sizeof(snavi_dev_t));

   flags = flags & O_NONBLOCK;

   if (!dev) return NULL;
   dev->fd = -1;
   dev->idLED = -1;
   dev->iLast = 0;

   if (pcName) { // file explicitly requested
      dev->fd = _snavi_probe(pcName, flags);
   } else if ((dev->fd = _snavi_probe("/dev/input/spacemouse", flags)) < 0) {
      char file[256]; int i;
      for (i=0; i < 20; i++) {
         snprintf(file, 256, "/dev/input/event%d", i);
         if ((dev->fd = _snavi_probe(file, flags)) >= 0) break;
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

   dev->iDelta = 0;
	memset(dev->offset, 0, 6*sizeof(int));

	snavi_calibrate (dev);
   return dev;
}

void snavi_close(void* v) {
   snavi_dev_t* dev = CAST(v);
   if (!dev) return;
   snavi_set_led (dev, 0);
   close (dev->fd);
   free (dev);
}

int snavi_get_fd(void* v) {
   snavi_dev_t* dev = CAST(v);
   if (!dev) return -1;
   return dev->fd;
}

void snavi_set_threshold(void* v, unsigned int iDelta) {
   snavi_dev_t* dev = CAST(v);
   if (!dev) return;
   dev->iDelta = iDelta;
}
unsigned int snavi_get_threshold(void* v) {
   snavi_dev_t* dev = CAST(v);
   if (!dev) return 0;
   return dev->iDelta;
}

void snavi_calibrate(void* v) {
   snavi_dev_t* dev = CAST(v);
   if (!dev) return;

   fd_set         rfds;
   struct timeval tv;

   FD_ZERO(&rfds);
   FD_SET(dev->fd, &rfds);

	// wait 100ms at most
	tv.tv_sec = 0;
	tv.tv_usec = 100000;
		
	// reset threshold
	unsigned int theta = snavi_get_threshold(dev);
	snavi_set_threshold(dev, 0);
	
	// reset offset
	int offset[6];
	memset(offset, 0, 6*sizeof(int));
	memset(dev->offset, 0, 6*sizeof(int));

	// try to get data from device
	snavi_event_t e;
	unsigned short code = 0;
	memset(e.axes, 0, 6*sizeof(int));

	// loop until time is left
	while (select(dev->fd+1, &rfds, NULL, NULL, &tv) > 0) {
		snavi_get_event(dev, &e);
      if (e.type == MotionEvent) {
			if (e.code & TranslationMotion) memcpy(offset, e.axes, 3*sizeof(int));
			if (e.code & RotationMotion) memcpy(offset+3, e.axes+3, 3*sizeof(int));
			code |= e.code;
			// break from loop, if we got data for both motion types
			if ((code & TranslationMotion) && (code & RotationMotion)) break;
		}
	}
	// store new offset
	memcpy (dev->offset, offset, 6*sizeof(int));
	// restore threshold
	snavi_set_threshold(dev, theta);
}

int snavi_is_calibrated(void* v) {
   snavi_dev_t* dev = CAST(v);
   if (!dev) return 0;

	int i;
	for (i=0; i < 6; ++i) if (dev->offset[i]) return 0;
	return 1;
}

int snavi_set_led (void* v, int led_state) {
   snavi_dev_t* dev = CAST(v);
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

int  snavi_get_event (void* v, snavi_event_t* ev) {
   snavi_dev_t* dev = CAST(v);
   struct input_event event;
   ev->code = 0; ev->type = 0;
   if (!dev) return -1;

	unsigned short code=0;
   while (1) {
      if (read (dev->fd, &event, sizeof (struct input_event)) < 0) {
         if (errno == EAGAIN) return ev->type;
         perror ("read error");
         return -1;
      }

      switch (event.type) {
			case EV_REL:
			case EV_ABS:
			{
				unsigned short FIRST = event.type == EV_REL ? REL_X : ABS_X;
				unsigned short LAST  = event.type == EV_REL ? REL_RZ : ABS_RZ;
				if (event.code <= LAST) {
					unsigned int iAxes = event.code - FIRST;
					event.value -= dev->offset[iAxes];
					if (event.value >= 0) {
						if ((event.value -= dev->iDelta) < 0) event.value = 0;
					} else {
						if ((event.value += dev->iDelta) > 0) event.value = 0;
					}
					ev->axes[iAxes] = event.value;
					code |= (1 << iAxes);
					if (event.value) ev->code |= (1 << iAxes);
				}
				break;
			}
			case EV_KEY:
				ev->type = (event.value ? ButtonPressEvent : ButtonReleaseEvent);
				ev->code = event.code;
				ev->time = event.time;
				return ev->type;
				break;

			case EV_SYN:
				/* if multiple axes change simultaneously the linux
				 * input system sends multiple EV_REL events. EV_SYN
				 * then indicates that all changes have been reported.
				 */
				ev->time = event.time;
				if (ev->code) ev->type = MotionEvent;

				// linux reports translational and rotational axes intermittendly
				// here we keep track of the last transmitted type
				if (code & TranslationMotion) {
					dev->iLast = TranslationMotion;
				} else if (code & RotationMotion) {
					dev->iLast = RotationMotion;
				} else {
					if (dev->iLast == TranslationMotion) {
						memset (ev->axes+3, 0, 3*sizeof(int));
						dev->iLast = RotationMotion;
					} else if (dev->iLast == RotationMotion) {
						memset (ev->axes, 0, 3*sizeof(int));
						dev->iLast = TranslationMotion;
					}
				}
			  
				return ev->type;
				break;
			default:
				return 0;
				break;
      }
   }
}

#ifdef TEST
int main (int argc, char *argv[])
{
   int iPress=0;
   snavi_dev_t* dev = CAST(snavi_open (NULL, 0));
   snavi_event_t e;
   if (!dev) {
      perror ("could not open spacemouse device");
      exit(-1);
   }
   memset(e.axes, 0, 6*sizeof(int));

   // set threshold
   if (argc >= 2) snavi_set_threshold(dev, atoi(argv[1]));

   snavi_set_led (dev, 1);
   while (iPress < 3 && snavi_get_event(dev, &e) >= 0) {
      switch (e.type) {
			case ButtonPressEvent:
				iPress++;
			case ButtonReleaseEvent: 
				fprintf (stderr, "\nbutton: %d %s\n", e.code, 
							e.type==ButtonPressEvent ? "pressed" : "released");
				break;
			case MotionEvent:
				fprintf (stderr, "\rState: %c%c %c%c  %4d %4d %4d %4d %4d %4d",
							e.code & TranslationMotion ? 'T' : ' ',
							e.code & RotationMotion ? 'R' : ' ',
							dev->iLast & TranslationMotion ? 'T' : ' ',
							dev->iLast & RotationMotion ? 'R' : ' ',
							e.axes[0], e.axes[1], e.axes[2], e.axes[3], e.axes[4], e.axes[5]);
				if (e.code) iPress = 0;
				break;
      }
   }
   snavi_set_led (dev, 0);
   snavi_close (dev);
   exit (0);
}
#endif
