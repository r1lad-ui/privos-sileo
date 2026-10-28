// Simulator-only smoke coverage. This translation unit must never enter the
// production package; the separate simulator builder supplies it explicitly.
#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import <TargetConditionals.h>
#import "PKDocument.h"
#import "PKCanvasView.h"
#include <math.h>

#if !TARGET_OS_SIMULATOR
#error PKReferenceSmoke must only be compiled into the separate simulator build
#endif

@interface PKRootController : UIViewController
- (void)bindDocument;
- (void)refresh;
- (void)modeChanged;
- (void)chooseReferenceKind:(NSString *)kind;
- (void)moveGuide;
- (void)undo;
- (void)redo;
@end

static id PKSmokeObserver;

static void PKSmokeRequire(BOOL condition, NSString *message) {
    if (!condition) {
        @throw [NSException exceptionWithName:@"PKReferenceSmokeFailure"
                                      reason:message userInfo:nil];
    }
}

static void PKSmokeNear(double actual, double expected, NSString *message) {
    PKSmokeRequire(isfinite(actual) && fabs(actual - expected) < 0.0000001,
                   [NSString stringWithFormat:@"%@ (actual %.12g, expected %.12g)",
                    message, actual, expected]);
}

static NSValue *PKSmokePoint(CGFloat x, CGFloat y) {
    return [NSValue valueWithCGPoint:CGPointMake(x, y)];
}

static NSMutableDictionary *PKSmokeJSON(PKDocument *document) {
    NSError *error = nil;
    NSData *data = [document serializedData:&error];
    PKSmokeRequire(data != nil, [@"Document serialization failed: " stringByAppendingString:
                   (error.localizedDescription ?: @"unknown error")]);
    id json = [NSJSONSerialization JSONObjectWithData:data
                                            options:NSJSONReadingMutableContainers
                                              error:&error];
    PKSmokeRequire([json isKindOfClass:NSMutableDictionary.class],
                   @"The serialized document was not a dictionary");
    return json;
}

static PKDocument *PKSmokeReadJSON(NSDictionary *json) {
    NSData *data = [NSJSONSerialization dataWithJSONObject:json options:0 error:NULL];
    PKSmokeRequire(data != nil, @"The JSON test fixture could not be encoded");
    return [PKDocument documentFromData:data error:NULL];
}

static void PKSmokeModelChecks(NSMutableArray<NSString *> *checks) {
    NSDictionary<NSString *, NSNumber *> *expected = @{
        @"custom":@0, @"card-long":@85.60, @"card-short":@53.98,
        @"coin-1":@23.25, @"coin-2":@25.75, @"coin-50":@24.25
    };
    PKSmokeRequire([PKDocument referenceKinds].count == expected.count,
                   @"Unexpected number of reference presets");
    for (NSString *kind in [PKDocument referenceKinds]) {
        PKSmokeRequire(expected[kind] != nil, @"Unknown preset in the public list");
        double preset = expected[kind].doubleValue;
        PKSmokeNear([PKDocument millimetersForReferenceKind:kind], preset,
                    [@"Preset dimension: " stringByAppendingString:kind]);
        PKDocument *document = [PKDocument demoDocument];
        document.referenceKind = kind;
        document.referenceMillimeters = 17.5;
        document.referenceFlipped = YES;
        double length = preset > 0 ? preset : 17.5;
        PKSmokeNear(document.referenceMillimeters, length, @"Preset setter invariant");
        NSMutableDictionary *json = PKSmokeJSON(document);
        PKSmokeRequire([json[@"version"] isEqual:@2], @"Schema 2 was not written");
        PKDocument *copy = PKSmokeReadJSON(json);
        PKSmokeRequire(copy != nil && [copy.referenceKind isEqualToString:kind] &&
                       copy.referenceFlipped, @"Reference preset roundtrip failed");
        PKSmokeNear(copy.referenceMillimeters, length, @"Reference length roundtrip");
        PKSmokeRequire([copy.points isEqualToArray:document.points] &&
                       [copy.calibrationPoints isEqualToArray:document.calibrationPoints] &&
                       [copy.guideAxisPoints isEqualToArray:document.guideAxisPoints] &&
                       [copy.guidePoints isEqualToArray:document.guidePoints],
                       @"Point or guide arrays changed in the JSON roundtrip");
        [checks addObject:[@"Preset and JSON roundtrip: " stringByAppendingString:kind]];
    }

    PKDocument *original = [PKDocument demoDocument];
    NSMutableDictionary *baseline = PKSmokeJSON(original);
    NSMutableDictionary *legacy = [baseline mutableCopy];
    legacy[@"version"] = @1;
    [legacy removeObjectsForKeys:@[@"referenceKind", @"referenceFlipped", @"guideAxis", @"guides"]];
    PKDocument *migrated = PKSmokeReadJSON(legacy);
    PKSmokeRequire(migrated != nil && [migrated.referenceKind isEqualToString:@"custom"] &&
                   !migrated.referenceFlipped && migrated.guideAxisPoints.count == 0 &&
                   migrated.guidePoints.count == 0, @"Schema 1 defaults were not preserved");
    PKSmokeRequire([migrated.points isEqualToArray:original.points] &&
                   [migrated.calibrationPoints isEqualToArray:original.calibrationPoints],
                   @"Schema 1 migration changed existing geometry");
    PKSmokeNear(migrated.referenceMillimeters, 25, @"Schema 1 reference length");
    [checks addObject:@"Schema 1 migration preserves geometry and uses custom reference"];

    NSMutableDictionary *invalid = [baseline mutableCopy];
    invalid[@"referenceKind"] = @"card-long";
    invalid[@"referenceMM"] = @85.61;
    PKSmokeRequire(PKSmokeReadJSON(invalid) == nil, @"Mismatched preset length was accepted");
    [checks addObject:@"Mismatched reference preset is rejected"];

    invalid = [baseline mutableCopy];
    invalid[@"version"] = @3;
    PKSmokeRequire(PKSmokeReadJSON(invalid) == nil, @"Unknown JSON version was accepted");
    [checks addObject:@"Unknown JSON version is rejected"];

    invalid = [baseline mutableCopy];
    invalid[@"referenceKind"] = @"unknown-reference";
    PKSmokeRequire(PKSmokeReadJSON(invalid) == nil, @"Unknown reference kind was accepted");
    [checks addObject:@"Unknown reference kind is rejected"];

    NSMutableArray *manyGuides = [NSMutableArray array];
    for (NSUInteger index = 0; index < 65; index++) [manyGuides addObject:@[@100, @100]];
    invalid = [baseline mutableCopy];
    invalid[@"guides"] = manyGuides;
    PKSmokeRequire(PKSmokeReadJSON(invalid) == nil, @"65 guides exceeded the limit without rejection");
    invalid[@"guides"] = [manyGuides subarrayWithRange:NSMakeRange(0, 64)];
    PKSmokeRequire(PKSmokeReadJSON(invalid).guidePoints.count == 64,
                   @"The valid boundary of 64 guides was rejected");
    [checks addObject:@"Guide count accepts 64 and rejects 65"];

    invalid = [baseline mutableCopy];
    invalid[@"guides"] = @[@[@1201, @100]];
    PKSmokeRequire(PKSmokeReadJSON(invalid) == nil, @"Out-of-bounds guide was accepted");
    invalid = [baseline mutableCopy];
    invalid[@"guideAxis"] = @[@[@-1, @100], @[@100, @200]];
    PKSmokeRequire(PKSmokeReadJSON(invalid) == nil, @"Out-of-bounds guide axis was accepted");
    [checks addObject:@"Out-of-bounds guides and axis are rejected"];

    PKDocument *rotated = [PKDocument demoDocument];
    rotated.guideAxisPoints = [@[PKSmokePoint(100,100), PKSmokePoint(100,200)] mutableCopy];
    CGPoint coordinate = [rotated guideCoordinatesForPoint:CGPointMake(70,130)];
    PKSmokeRequire(rotated.hasValidGuideAxis, @"The rotated axis was not valid");
    PKSmokeNear(coordinate.x, 30 * rotated.millimetersPerUnit, @"Rotated axis X projection");
    PKSmokeNear(coordinate.y, 30 * rotated.millimetersPerUnit, @"Rotated axis signed Y projection");
    [checks addObject:@"Rotated guide axis projects signed millimetre coordinates"];
    rotated.guideAxisPoints[1] = PKSmokePoint(100,100);
    coordinate = [rotated guideCoordinatesForPoint:CGPointMake(70,130)];
    PKSmokeRequire(!rotated.hasValidGuideAxis && isnan(coordinate.x) && isnan(coordinate.y),
                   @"Degenerate axis did not return invalid coordinates");
    [checks addObject:@"Degenerate guide axis returns NaN coordinates"];
}

static UIImage *PKSmokePhoto(BOOL coin) {
    UIGraphicsImageRendererFormat *format = [UIGraphicsImageRendererFormat defaultFormat];
    format.scale = 1;
    format.opaque = YES;
    UIGraphicsImageRenderer *renderer = [[UIGraphicsImageRenderer alloc]
        initWithSize:CGSizeMake(1200,800) format:format];
    return [renderer imageWithActions:^(UIGraphicsImageRendererContext *context) {
        (void)context;
        [[UIColor colorWithRed:.19 green:.22 blue:.26 alpha:1] setFill];
        UIRectFill(CGRectMake(0,0,1200,800));
        if (!coin) {
            [[UIColor colorWithRed:.80 green:.86 blue:.91 alpha:1] setFill];
            [[UIBezierPath bezierPathWithRoundedRect:CGRectMake(100,120,856,539.8)
                                       cornerRadius:32] fill];
            [@"CARTE DE TEST · ID-1" drawAtPoint:CGPointMake(150,175)
                withAttributes:@{NSFontAttributeName:[UIFont boldSystemFontOfSize:24],
                                 NSForegroundColorAttributeName:UIColor.darkGrayColor}];
        } else {
            [[UIColor colorWithRed:.84 green:.76 blue:.46 alpha:1] setFill];
            [[UIBezierPath bezierPathWithOvalInRect:CGRectMake(91.25,521.25,257.5,257.5)] fill];
            [[UIColor colorWithWhite:.78 alpha:1] setFill];
            [[UIBezierPath bezierPathWithOvalInRect:CGRectMake(120,550,200,200)] fill];
            [@"2 €" drawAtPoint:CGPointMake(172,615)
                withAttributes:@{NSFontAttributeName:[UIFont boldSystemFontOfSize:48],
                                 NSForegroundColorAttributeName:UIColor.darkGrayColor}];
        }
        // Deliberately generic calibration coupon; it is not a physical key.
        UIBezierPath *shape = [UIBezierPath bezierPath];
        [shape moveToPoint:CGPointMake(250,500)];
        for (NSValue *value in @[PKSmokePoint(250,430), PKSmokePoint(400,350),
                                PKSmokePoint(450,400), PKSmokePoint(650,370),
                                PKSmokePoint(760,420), PKSmokePoint(850,330),
                                PKSmokePoint(930,430), PKSmokePoint(930,500)])
            [shape addLineToPoint:value.CGPointValue];
        [shape closePath];
        [[UIColor colorWithRed:.24 green:.29 blue:.34 alpha:1] setFill];
        [shape fill];
        [[UIColor colorWithWhite:.62 alpha:1] setStroke];
        shape.lineWidth = 5;
        [shape stroke];
        [@"EXEMPLE SYNTHÉTIQUE" drawAtPoint:CGPointMake(470,720)
            withAttributes:@{NSFontAttributeName:[UIFont boldSystemFontOfSize:22],
                             NSForegroundColorAttributeName:UIColor.whiteColor}];
    }];
}

static PKDocument *PKSmokeUIFixture(void) {
    PKDocument *document = [PKDocument new];
    document.name = @"Test photo · référence et traits";
    document.demo = YES;
    document.documentSize = CGSizeMake(1200,800);
    document.image = PKSmokePhoto(NO);
    document.referenceKind = @"card-long";
    document.calibrationPoints = [@[PKSmokePoint(100,120), PKSmokePoint(956,120)] mutableCopy];
    document.guideAxisPoints = [@[PKSmokePoint(250,500), PKSmokePoint(1000,500)] mutableCopy];
    document.guidePoints = [@[PKSmokePoint(400,350), PKSmokePoint(650,370),
                             PKSmokePoint(850,330)] mutableCopy];
    document.points = [@[PKSmokePoint(250,500), PKSmokePoint(250,430),
                        PKSmokePoint(400,350), PKSmokePoint(450,400),
                        PKSmokePoint(650,370)] mutableCopy];
    document.closed = NO;
    return document;
}

static void PKSmokeCapture(UIWindow *window, PKCanvasView *canvas, NSURL *directory,
                           NSString *name, NSMutableArray<NSString *> *images) {
    [window layoutIfNeeded];
    PKSmokeRequire(canvas.bounds.size.width > 200 && canvas.bounds.size.height >= 300,
                   @"The native canvas did not receive its expected layout size");
    UIScrollView *scroll = nil;
    for (UIView *parent = canvas.superview; parent; parent = parent.superview) {
        if ([parent isKindOfClass:UIScrollView.class]) {scroll = (UIScrollView *)parent; break;}
    }
    PKSmokeRequire(scroll != nil, @"The canvas is not inside its native scroll view");
    CGRect canvasRect = [canvas convertRect:canvas.bounds toView:scroll];
    CGFloat top = canvasRect.origin.y - 48;
    CGFloat maximum = MAX(-scroll.adjustedContentInset.top,
        scroll.contentSize.height - scroll.bounds.size.height + scroll.adjustedContentInset.bottom);
    [scroll setContentOffset:CGPointMake(0, MAX(-scroll.adjustedContentInset.top, MIN(top, maximum)))
                    animated:NO];
    [window layoutIfNeeded];
    [canvas setNeedsDisplay];
    UIGraphicsImageRendererFormat *format = [UIGraphicsImageRendererFormat defaultFormat];
    format.scale = 2;
    format.opaque = YES;
    UIGraphicsImageRenderer *renderer = [[UIGraphicsImageRenderer alloc]
        initWithSize:window.bounds.size format:format];
    UIImage *image = [renderer imageWithActions:^(UIGraphicsImageRendererContext *context) {
        (void)context;
        [window drawViewHierarchyInRect:window.bounds afterScreenUpdates:YES];
    }];
    NSString *filename = [name stringByAppendingString:@".png"];
    PKSmokeRequire([UIImagePNGRepresentation(image) writeToURL:
                    [directory URLByAppendingPathComponent:filename] atomically:YES],
                   [@"Could not write screenshot: " stringByAppendingString:filename]);
    [images addObject:filename];

    renderer = [[UIGraphicsImageRenderer alloc] initWithSize:canvas.bounds.size format:format];
    image = [renderer imageWithActions:^(UIGraphicsImageRendererContext *context) {
        (void)context;
        [canvas drawViewHierarchyInRect:canvas.bounds afterScreenUpdates:YES];
    }];
    filename = [name stringByAppendingString:@"-canvas.png"];
    PKSmokeRequire([UIImagePNGRepresentation(image) writeToURL:
                    [directory URLByAppendingPathComponent:filename] atomically:YES],
                   [@"Could not write canvas screenshot: " stringByAppendingString:filename]);
    [images addObject:filename];
}

static void PKSmokeRun(void) {
    NSMutableArray<NSString *> *checks = [NSMutableArray array];
    NSMutableArray<NSString *> *images = [NSMutableArray array];
    NSURL *documents = [[NSFileManager defaultManager] URLsForDirectory:NSDocumentDirectory
                                                             inDomains:NSUserDomainMask].firstObject;
    NSURL *directory = [documents URLByAppendingPathComponent:@"keylab-smoke" isDirectory:YES];
    NSMutableDictionary *report = [@{@"passed":@NO, @"checks":checks, @"screenshots":images,
                                     @"scope":@"Simulator model and native UIKit smoke test"} mutableCopy];
    @try {
        NSError *directoryError = nil;
        BOOL created = [[NSFileManager defaultManager] createDirectoryAtURL:directory
            withIntermediateDirectories:YES attributes:nil error:&directoryError];
        PKSmokeRequire(created, directoryError.localizedDescription ?: @"Cannot create smoke report directory");
        PKSmokeModelChecks(checks);

        UIWindow *window = UIApplication.sharedApplication.delegate.window;
        PKSmokeRequire(window != nil && !window.hidden, @"The native application window is unavailable");
        UIViewController *controller = window.rootViewController;
        if ([controller isKindOfClass:UINavigationController.class])
            controller = ((UINavigationController *)controller).viewControllers.firstObject;
        PKSmokeRequire([controller isKindOfClass:PKRootController.class],
                       @"The native root controller was not found");
        PKRootController *root = (PKRootController *)controller;
        [root loadViewIfNeeded];
        PKDocument *fixture = PKSmokeUIFixture();
        [root setValue:fixture forKey:@"doc"];
        [[root valueForKey:@"undoStates"] removeAllObjects];
        [[root valueForKey:@"redoStates"] removeAllObjects];
        [root bindDocument];
        PKCanvasView *canvas = [root valueForKey:@"canvas"];
        UISegmentedControl *modes = [root valueForKey:@"modes"];
        UISegmentedControl *guideModes = [root valueForKey:@"guideModes"];
        report[@"windowPoints"] = @[@(window.bounds.size.width), @(window.bounds.size.height)];
        PKSmokeNear(window.bounds.size.width, 375, @"iPhone SE logical window width");
        PKSmokeNear(window.bounds.size.height, 667, @"iPhone SE logical window height");
        [checks addObject:@"Native iPhone SE window is 375 × 667 points in portrait"];

        modes.selectedSegmentIndex = 0;
        [root modeChanged];
        PKSmokeRequire(canvas.mode == PKCanvasModeCalibration &&
                       [canvas.referenceKind isEqualToString:@"card-long"],
                       @"Card reference mode did not bind to the canvas");
        PKSmokeNear(canvas.millimetersPerUnit, .1, @"Card reference scale on the canvas");
        PKSmokeCapture(window, canvas, directory, @"card-reference", images);
        [checks addObject:@"Native card reference controls and overlay render"];

        [root chooseReferenceKind:@"coin-1"];
        PKSmokeRequire([fixture.referenceKind isEqualToString:@"coin-1"] &&
                       fixture.calibrationPoints.count == 0, @"Changing reference did not reset A–B");
        PKSmokeNear(fixture.referenceMillimeters, 23.25, @"Coin reference selected in controller");
        [root undo];
        PKSmokeRequire([fixture.referenceKind isEqualToString:@"card-long"] &&
                       fixture.calibrationPoints.count == 2, @"Undo failed to restore the card calibration");
        PKSmokeNear(fixture.referenceMillimeters, 85.60, @"Undo restores matching preset dimension");
        [root redo];
        PKSmokeRequire([fixture.referenceKind isEqualToString:@"coin-1"] &&
                       fixture.calibrationPoints.count == 0, @"Redo failed to restore the coin reference");
        [root undo];
        [checks addObject:@"Controller reference changes, undo and redo preserve preset invariants"];

        modes.selectedSegmentIndex = 1;
        guideModes.selectedSegmentIndex = 1;
        [root modeChanged];
        canvas.selectedGuideIndex = 0;
        [root refresh];
        UITextField *guideX = [root valueForKey:@"guideXField"];
        UITextField *guideY = [root valueForKey:@"guideYField"];
        PKSmokeRequire(guideX.enabled && guideY.enabled && canvas.mode == PKCanvasModeGuides,
                       @"Selected guide coordinate fields are not enabled");
        guideX.text = @"20";
        guideY.text = @"-10";
        [root moveGuide];
        CGPoint moved = [fixture guideCoordinatesForPoint:fixture.guidePoints[0].CGPointValue];
        PKSmokeNear(moved.x, 20, @"Controller guide X movement");
        PKSmokeNear(moved.y, -10, @"Controller signed guide Y movement");
        PKSmokeRequire(root.presentedViewController == nil, @"Moving a valid guide unexpectedly raised an alert");
        PKSmokeCapture(window, canvas, directory, @"manual-guides", images);
        [checks addObject:@"Native guide editor accepts signed millimetre coordinates and renders"];

        NSError *imageError = nil;
        NSData *imageJSON = [fixture serializedData:&imageError];
        PKDocument *imageCopy = imageJSON ? [PKDocument documentFromData:imageJSON error:&imageError] : nil;
        PKSmokeRequire(imageCopy.image != nil && CGSizeEqualToSize(imageCopy.image.size, fixture.documentSize),
                       imageError.localizedDescription ?: @"Photo and guide document roundtrip failed");
        [checks addObject:@"Synthetic photo, reference and guides survive JSON roundtrip"];

        [root chooseReferenceKind:@"coin-2"];
        fixture.image = PKSmokePhoto(YES);
        fixture.calibrationPoints = [@[PKSmokePoint(91.25,650), PKSmokePoint(348.75,650)] mutableCopy];
        [root bindDocument];
        modes.selectedSegmentIndex = 0;
        [root modeChanged];
        PKSmokeRequire([canvas.referenceKind isEqualToString:@"coin-2"], @"Coin overlay was not bound");
        PKSmokeNear(canvas.millimetersPerUnit, .1, @"Coin diameter scale on the canvas");
        PKSmokeCapture(window, canvas, directory, @"coin-reference", images);
        [checks addObject:@"Native coin reference controls and overlay render"];
        report[@"passed"] = @YES;
    } @catch (NSException *exception) {
        report[@"failure"] = exception.reason ?: exception.name;
        report[@"exception"] = exception.name;
    }
    report[@"checkCount"] = @(checks.count);
    NSError *writeError = nil;
    NSData *data = [NSJSONSerialization dataWithJSONObject:report
                                                 options:NSJSONWritingPrettyPrinted
                                                   error:&writeError];
    BOOL written = data && [data writeToURL:[directory URLByAppendingPathComponent:@"report.json"]
                                  options:NSDataWritingAtomic error:&writeError];
    NSLog(@"PRIV Key reference smoke: %@ (%lu checks). Report %@",
          [report[@"passed"] boolValue] ? @"PASSED" : @"FAILED",
          (unsigned long)checks.count, written ? @"saved" : writeError.localizedDescription);
}

__attribute__((constructor)) static void PKInstallReferenceSmoke(void) {
    @autoreleasepool {
        if (![[NSProcessInfo processInfo].arguments containsObject:@"--keylab-reference-smoke"]) return;
        PKSmokeObserver = [[NSNotificationCenter defaultCenter]
            addObserverForName:UIApplicationDidBecomeActiveNotification object:nil
            queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification *notification) {
                (void)notification;
                static BOOL started = NO;
                if (started) return;
                started = YES;
                [[NSNotificationCenter defaultCenter] removeObserver:PKSmokeObserver];
                PKSmokeObserver = nil;
                dispatch_async(dispatch_get_main_queue(), ^{ PKSmokeRun(); });
            }];
    }
}
