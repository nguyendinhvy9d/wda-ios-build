// touch.c v2 — bo bam su kien cham (HID digitizer) tren iOS jailbreak, chay bang root.
// Dung API IOKit HID cong khai. Diem mau chot: su kien gia PHAI mang senderID cua cam bien that,
// nen co che "learn": lang nghe mot cu cham that -> lay senderID -> luu file -> moi lan bam dung lai.
// Toa do PHAN SO 0..1 (fx=px/rong, fy=py/cao). Lenh:
//   touch learn                      (giu, cho nguoi cham 1 cai vao man -> luu senderID)
//   touch tap   fx fy
//   touch swipe fx1 fy1 fx2 fy2 steps stepMs
//   touch down|move|up fx fy
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
typedef void (*IOHIDEventSystemClientEventCallback)(void *target, void *refcon, void *sender, IOHIDEventRef event);

#define kIOHIDEventTypeDigitizer 11
#define FINGER_INDEX 2
#define TSID_FILE "/var/mobile/.tsid"

extern IOHIDEventSystemClientRef IOHIDEventSystemClientCreate(CFAllocatorRef allocator);
extern void IOHIDEventSystemClientDispatchEvent(IOHIDEventSystemClientRef client, IOHIDEventRef event);
extern void IOHIDEventSystemClientScheduleWithRunLoop(IOHIDEventSystemClientRef client, CFRunLoopRef runLoop, CFStringRef runLoopMode);
extern void IOHIDEventSystemClientRegisterEventCallback(IOHIDEventSystemClientRef client, IOHIDEventSystemClientEventCallback callback, void *target, void *refcon);
extern uint32_t IOHIDEventGetType(IOHIDEventRef event);
extern uint64_t IOHIDEventGetSenderID(IOHIDEventRef event);
extern void IOHIDEventSetSenderID(IOHIDEventRef event, uint64_t senderID);
extern void IOHIDEventSetIntegerValue(IOHIDEventRef event, uint32_t field, int value);
extern void IOHIDEventSetFloatValue(IOHIDEventRef event, uint32_t field, IOHIDFloat value);
extern void IOHIDEventAppendEvent(IOHIDEventRef parent, IOHIDEventRef child, IOOptionBits options);
extern IOHIDEventRef IOHIDEventCreateDigitizerEvent(CFAllocatorRef allocator, uint64_t timeStamp,
    uint32_t type, uint32_t index, uint32_t identity, uint32_t eventMask, uint32_t buttonMask,
    IOHIDFloat x, IOHIDFloat y, IOHIDFloat z, IOHIDFloat tipPressure, IOHIDFloat twist,
    boolean_t range, boolean_t touch, IOOptionBits options);
extern IOHIDEventRef IOHIDEventCreateDigitizerFingerEvent(CFAllocatorRef allocator, uint64_t timeStamp,
    uint32_t index, uint32_t identity, uint32_t eventMask, IOHIDFloat x, IOHIDFloat y, IOHIDFloat z,
    IOHIDFloat tipPressure, IOHIDFloat twist, boolean_t range, boolean_t touch, IOOptionBits options);

static IOHIDEventSystemClientRef gClient;
static uint64_t gSender = 0;

// ---- learn senderID tu cu cham that ----
static uint64_t gLearned = 0;
static void learnCB(void *t, void *r, void *s, IOHIDEventRef e) {
    if (!gLearned && IOHIDEventGetType(e) == kIOHIDEventTypeDigitizer) {
        gLearned = IOHIDEventGetSenderID(e);
    }
}
static int doLearn(void) {
    IOHIDEventSystemClientRef c = IOHIDEventSystemClientCreate(kCFAllocatorDefault);
    if (!c) { fprintf(stderr, "khong tao duoc client\n"); return 1; }
    IOHIDEventSystemClientScheduleWithRunLoop(c, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOHIDEventSystemClientRegisterEventCallback(c, learnCB, NULL, NULL);
    fprintf(stderr, "dang cho mot cu cham that len man hinh (toi da 60s)...\n");
    for (int i = 0; i < 60 && !gLearned; i++)
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, true);
    if (!gLearned) { fprintf(stderr, "khong bat duoc cu cham nao\n"); return 2; }
    FILE *f = fopen(TSID_FILE, "w");
    if (f) { fprintf(f, "%llu", (unsigned long long)gLearned); fclose(f); }
    printf("%llu\n", (unsigned long long)gLearned);
    return 0;
}
static uint64_t loadSender(void) {
    FILE *f = fopen(TSID_FILE, "r"); if (!f) return 0;
    unsigned long long v = 0; if (fscanf(f, "%llu", &v) != 1) v = 0; fclose(f); return v;
}

// ---- bam mot su kien finger ----
// type: 0=down, 1=move, 2=up
static void sendFinger(int type, double fx, double fy) {
    uint64_t ts = mach_absolute_time();
    IOHIDEventRef parent = IOHIDEventCreateDigitizerEvent(kCFAllocatorDefault, ts, 3, 99, 1, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0);
    if (!parent) return;
    IOHIDEventSetIntegerValue(parent, 0xb0019, 1);
    IOHIDEventSetIntegerValue(parent, 0x4, 1);
    uint32_t mask; boolean_t range, touch;
    if (type == 0) { mask = 3; range = 1; touch = 1; }        // down
    else if (type == 1) { mask = 4; range = 1; touch = 1; }   // move
    else { mask = 2; range = 0; touch = 0; }                  // up
    IOHIDEventRef child = IOHIDEventCreateDigitizerFingerEvent(kCFAllocatorDefault, ts,
        FINGER_INDEX, 3, mask, fx, fy, 0, 0, 0, range, touch, 0);
    if (child) {
        IOHIDEventSetFloatValue(child, 0xb0014, 0.04);
        IOHIDEventSetFloatValue(child, 0xb0015, 0.04);
        IOHIDEventAppendEvent(parent, child, 0);
        CFRelease(child);
    }
    IOHIDEventSetIntegerValue(parent, 0xb0007, 0x23);
    IOHIDEventSetIntegerValue(parent, 0xb0008, 0x1);
    IOHIDEventSetIntegerValue(parent, 0xb0009, 0x1);
    IOHIDEventSetSenderID(parent, gSender);
    IOHIDEventSystemClientDispatchEvent(gClient, parent);
    CFRelease(parent);
}
static void ms(long m){ if(m>0) usleep((useconds_t)m*1000); }

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "dung: touch learn|tap|swipe|down|move|up ...\n"); return 2; }
    if (!strcmp(argv[1], "learn")) return doLearn();

    gSender = loadSender();
    if (!gSender) { fprintf(stderr, "chua co senderID — chay 'touch learn' truoc\n"); return 3; }
    gClient = IOHIDEventSystemClientCreate(kCFAllocatorDefault);
    if (!gClient) { fprintf(stderr, "khong tao duoc client\n"); return 1; }

    const char *cmd = argv[1];
    if (!strcmp(cmd, "tap") && argc >= 4) {
        double fx = atof(argv[2]), fy = atof(argv[3]);
        sendFinger(0, fx, fy); ms(70); sendFinger(2, fx, fy);
    } else if (!strcmp(cmd, "swipe") && argc >= 8) {
        double x1=atof(argv[2]),y1=atof(argv[3]),x2=atof(argv[4]),y2=atof(argv[5]);
        int steps=atoi(argv[6]); long sms=atol(argv[7]); if(steps<2)steps=2;
        sendFinger(0, x1, y1); ms(sms);
        for (int i=1;i<=steps;i++){ double t=(double)i/steps; sendFinger(1, x1+(x2-x1)*t, y1+(y2-y1)*t); ms(sms); }
        sendFinger(2, x2, y2);
    } else if (!strcmp(cmd,"down") && argc>=4) sendFinger(0, atof(argv[2]), atof(argv[3]));
    else if (!strcmp(cmd,"move") && argc>=4) sendFinger(1, atof(argv[2]), atof(argv[3]));
    else if (!strcmp(cmd,"up")   && argc>=4) sendFinger(2, atof(argv[2]), atof(argv[3]));
    else { fprintf(stderr, "tham so sai\n"); return 2; }
    usleep(20000);
    return 0;
}
