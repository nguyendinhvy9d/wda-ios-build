// touch.c — bo bam su kien cham (HID digitizer) tren iOS da jailbreak, chay bang root.
// Dung API IOKit HID cong khai (IOHIDEventCreateDigitizerFingerEvent + IOHIDEventSystemClientDispatchEvent).
// Toa do nhan dang PHAN SO 0..1 (fx = px/rong, fy = py/cao) de doc lap phan giai.
// Lenh:
//   touch tap   fx fy
//   touch swipe fx1 fy1 fx2 fy2 steps stepMs
//   touch down  fx fy   |  touch move fx fy  |  touch up fx fy   (dieu khien tay)
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach_time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef double IOHIDFloat;
typedef uint32_t IOOptionBits;
typedef struct __IOHIDEvent *IOHIDEventRef;
typedef struct __IOHIDEventSystemClient *IOHIDEventSystemClientRef;

// --- hang so cong khai tu IOKit/hid/IOHIDEventTypes.h ---
#define kIOHIDDigitizerTransducerTypeHand   3
#define kIOHIDDigitizerEventRange      0x00000001
#define kIOHIDDigitizerEventTouch      0x00000002
#define kIOHIDDigitizerEventPosition   0x00000004
// field id: IsDisplayIntegrated  (IOHIDEventField: type 11 digitizer)
#define kIOHIDEventFieldDigitizerIsDisplayIntegrated 0xb000a

extern IOHIDEventSystemClientRef IOHIDEventSystemClientCreate(CFAllocatorRef allocator);
extern void IOHIDEventSystemClientDispatchEvent(IOHIDEventSystemClientRef client, IOHIDEventRef event);
extern IOHIDEventRef IOHIDEventCreateDigitizerEvent(CFAllocatorRef allocator, uint64_t timeStamp,
    uint32_t type, uint32_t index, uint32_t identity, uint32_t eventMask, uint32_t buttonMask,
    IOHIDFloat x, IOHIDFloat y, IOHIDFloat z, IOHIDFloat tipPressure, IOHIDFloat twist,
    boolean_t range, boolean_t touch, IOOptionBits options);
extern IOHIDEventRef IOHIDEventCreateDigitizerFingerEvent(CFAllocatorRef allocator, uint64_t timeStamp,
    uint32_t index, uint32_t identity, uint32_t eventMask, IOHIDFloat x, IOHIDFloat y, IOHIDFloat z,
    IOHIDFloat tipPressure, IOHIDFloat twist, boolean_t range, boolean_t touch, IOOptionBits options);
extern void IOHIDEventAppendEvent(IOHIDEventRef parent, IOHIDEventRef child, IOOptionBits options);
extern void IOHIDEventSetIntegerValue(IOHIDEventRef event, uint32_t field, int value);

static IOHIDEventSystemClientRef gClient;

static void postFinger(double fx, double fy, int touching) {
    uint32_t mask = kIOHIDDigitizerEventPosition;
    if (touching) mask |= kIOHIDDigitizerEventRange | kIOHIDDigitizerEventTouch;
    else          mask |= kIOHIDDigitizerEventRange | kIOHIDDigitizerEventTouch; // range/touch bao trang thai qua touch flag
    uint64_t ts = mach_absolute_time();
    IOHIDEventRef parent = IOHIDEventCreateDigitizerEvent(kCFAllocatorDefault, ts,
        kIOHIDDigitizerTransducerTypeHand, 0, 0, mask, 0, fx, fy, 0, 0, 0,
        touching ? true : false, touching ? true : false, 0);
    if (!parent) return;
    IOHIDEventSetIntegerValue(parent, kIOHIDEventFieldDigitizerIsDisplayIntegrated, 1);
    IOHIDEventRef child = IOHIDEventCreateDigitizerFingerEvent(kCFAllocatorDefault, ts,
        2, 2, mask, fx, fy, 0, 0, 0, touching ? true : false, touching ? true : false, 0);
    if (child) {
        IOHIDEventSetIntegerValue(child, kIOHIDEventFieldDigitizerIsDisplayIntegrated, 1);
        IOHIDEventAppendEvent(parent, child, 0);
        CFRelease(child);
    }
    IOHIDEventSystemClientDispatchEvent(gClient, parent);
    CFRelease(parent);
}

static void nsleep(long ms){ if(ms>0) usleep((useconds_t)ms*1000); }

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "dung: touch tap|swipe|down|move|up ...\n"); return 2; }
    gClient = IOHIDEventSystemClientCreate(kCFAllocatorDefault);
    if (!gClient) { fprintf(stderr, "khong tao duoc IOHIDEventSystemClient\n"); return 1; }

    const char *cmd = argv[1];
    if (!strcmp(cmd, "tap") && argc >= 4) {
        double fx = atof(argv[2]), fy = atof(argv[3]);
        postFinger(fx, fy, 1); nsleep(60); postFinger(fx, fy, 0);
    } else if (!strcmp(cmd, "swipe") && argc >= 8) {
        double x1=atof(argv[2]),y1=atof(argv[3]),x2=atof(argv[4]),y2=atof(argv[5]);
        int steps=atoi(argv[6]); long sms=atol(argv[7]); if(steps<2)steps=2;
        postFinger(x1,y1,1); nsleep(sms);
        for (int i=1;i<=steps;i++){ double t=(double)i/steps; postFinger(x1+(x2-x1)*t, y1+(y2-y1)*t, 1); nsleep(sms); }
        postFinger(x2,y2,0);
    } else if (!strcmp(cmd,"down") && argc>=4) { postFinger(atof(argv[2]),atof(argv[3]),1);
    } else if (!strcmp(cmd,"move") && argc>=4) { postFinger(atof(argv[2]),atof(argv[3]),1);
    } else if (!strcmp(cmd,"up")   && argc>=4) { postFinger(atof(argv[2]),atof(argv[3]),0);
    } else { fprintf(stderr,"tham so sai\n"); return 2; }
    usleep(20000);
    return 0;
}
