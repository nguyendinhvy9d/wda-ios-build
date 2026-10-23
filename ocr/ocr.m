// ocr.m — doc chu tren anh PNG bang Vision cua iOS (tieng Nhat + Anh), in ra JSON {t,x,y,w,h}.
// Toa do PHAN SO 0..1, goc TREN-TRAI (khop voi binary `touch`: tap fx fy = tam chu).
// Dung: ocr <duong_dan_anh.png>
#import <Foundation/Foundation.h>
#import <Vision/Vision.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>

int main(int argc, char **argv) {
    @autoreleasepool {
        // Doc bytes anh tu STDIN (tranh van de container/sandbox khi doc file tren may).
        // Neu co argv[1] la duong dan file thi doc file; mac dinh doc stdin.
        unsigned char *buf = NULL; size_t len = 0;
        if (argc >= 2 && strcmp(argv[1], "-") != 0) {
            FILE *f = fopen(argv[1], "rb");
            if (!f) { fprintf(stderr, "khong mo duoc file: %s\n", argv[1]); return 1; }
            fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
            buf = malloc((size_t)sz); len = fread(buf, 1, (size_t)sz, f); fclose(f);
        } else {
            size_t cap = 1 << 20; buf = malloc(cap);
            size_t n;
            while ((n = fread(buf + len, 1, cap - len, stdin)) > 0) {
                len += n;
                if (len == cap) { cap *= 2; buf = realloc(buf, cap); }
            }
        }
        if (!buf || len == 0) { fprintf(stderr, "khong co du lieu anh\n"); return 1; }
        CFDataRef cfdata = CFDataCreate(NULL, (const UInt8 *)buf, len);
        free(buf);
        CGImageSourceRef src = CGImageSourceCreateWithData(cfdata, NULL);
        if (cfdata) CFRelease(cfdata);
        if (!src) { fprintf(stderr, "khong tao duoc image source (bytes=%zu)\n", len); return 1; }
        CGImageRef img = CGImageSourceCreateImageAtIndex(src, 0, NULL);
        if (!img) { CFRelease(src); fprintf(stderr, "khong giai duoc anh\n"); return 1; }

        VNImageRequestHandler *handler = [[VNImageRequestHandler alloc] initWithCGImage:img options:@{}];
        VNRecognizeTextRequest *req = [[VNRecognizeTextRequest alloc] init];
        req.recognitionLevel = VNRequestTextRecognitionLevelAccurate;
        req.usesLanguageCorrection = NO;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        req.usesCPUOnly = YES;   // ep chay CPU: tranh loi "Error loading network" khi khong vao duoc Neural Engine
#pragma clang diagnostic pop
        if (@available(iOS 16.0, *)) { req.recognitionLanguages = @[@"ja-JP", @"en-US"]; }
        else { req.recognitionLanguages = @[@"en-US"]; }

        NSError *err = nil;
        [handler performRequests:@[req] error:&err];
        if (err) { fprintf(stderr, "loi OCR: %s\n", err.localizedDescription.UTF8String); }

        NSMutableArray *out = [NSMutableArray array];
        for (VNRecognizedTextObservation *obs in req.results) {
            VNRecognizedText *top = [[obs topCandidates:1] firstObject];
            if (!top) continue;
            CGRect bb = obs.boundingBox;  // normalized, goc duoi-trai
            double x = bb.origin.x;
            double y = 1.0 - (bb.origin.y + bb.size.height);  // doi ve goc tren-trai
            [out addObject:@{ @"t": top.string, @"x": @(x), @"y": @(y),
                              @"w": @(bb.size.width), @"h": @(bb.size.height),
                              @"c": @(top.confidence) }];
        }
        NSData *json = [NSJSONSerialization dataWithJSONObject:out options:0 error:nil];
        fwrite(json.bytes, 1, json.length, stdout);
        fflush(stdout);
        CGImageRelease(img); CFRelease(src);
    }
    return 0;
}
