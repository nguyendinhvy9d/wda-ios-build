// ocr.m — doc chu tren anh PNG bang Vision cua iOS (tieng Nhat + Anh), in ra JSON {t,x,y,w,h}.
// Toa do PHAN SO 0..1, goc TREN-TRAI (khop voi binary `touch`: tap fx fy = tam chu).
// Dung: ocr <duong_dan_anh.png>
#import <Foundation/Foundation.h>
#import <Vision/Vision.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>

int main(int argc, char **argv) {
    @autoreleasepool {
        if (argc < 2) { fprintf(stderr, "dung: ocr <anh.png>\n"); return 2; }
        NSString *path = [NSString stringWithUTF8String:argv[1]];
        NSURL *url = [NSURL fileURLWithPath:path];
        CGImageSourceRef src = CGImageSourceCreateWithURL((__bridge CFURLRef)url, NULL);
        if (!src) { fprintf(stderr, "khong doc duoc anh\n"); return 1; }
        CGImageRef img = CGImageSourceCreateImageAtIndex(src, 0, NULL);
        if (!img) { CFRelease(src); fprintf(stderr, "anh loi\n"); return 1; }

        VNImageRequestHandler *handler = [[VNImageRequestHandler alloc] initWithCGImage:img options:@{}];
        VNRecognizeTextRequest *req = [[VNRecognizeTextRequest alloc] init];
        req.recognitionLevel = VNRequestTextRecognitionLevelAccurate;
        req.usesLanguageCorrection = NO;
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
