#import "PKCanvasView.h"
#import <QuartzCore/QuartzCore.h>
#import <math.h>

static UIColor *PKTeal(void) {
    return [UIColor colorWithRed:0.35 green:0.91 blue:0.75 alpha:1.0];
}
static UIColor *PKAmber(void) {
    return [UIColor colorWithRed:1.0 green:0.77 blue:0.37 alpha:1.0];
}
static UIColor *PKBlue(void) {
    return [UIColor colorWithRed:0.37 green:0.72 blue:1.0 alpha:1.0];
}
static UIColor *PKRose(void) {
    return [UIColor colorWithRed:1.0 green:0.46 blue:0.56 alpha:1.0];
}
static CGFloat PKClamp(CGFloat value, CGFloat minimum, CGFloat maximum) {
    return fmax(minimum, fmin(maximum, value));
}

@interface PKCanvasView () <UIGestureRecognizerDelegate>
@end

@implementation PKCanvasView {
    CGFloat _zoom;
    CGPoint _pan;
    CGFloat _physicalPointsPerUnit;
    CGFloat _pinchStartZoom;
    CGPoint _pinchAnchor;
    CGPoint _panStart;
    CGPoint _dragStart;
    NSInteger _draggedIndex;
    PKCanvasMode _draggedMode;
    NSInteger _nextCalibrationPoint;
    NSInteger _nextGuideAxisPoint;
    BOOL _loupeVisible;
    CGPoint _loupePoint;
    UIPanGestureRecognizer *_pointPan;
    UIPanGestureRecognizer *_viewPan;
    UIPinchGestureRecognizer *_pinch;
}

- (instancetype)initWithFrame:(CGRect)frame {
    if ((self = [super initWithFrame:frame])) {
        _documentSize = CGSizeMake(1200, 700);
        _points = [NSMutableArray array];
        _calibrationPoints = [NSMutableArray array];
        _referenceKind = @"custom";
        _guideAxisPoints = [NSMutableArray array];
        _guidePoints = [NSMutableArray array];
        _selectedGuideIndex = -1;
        _selectedIndex = -1;
        _draggedIndex = -1;
        _zoom = 1.0;
        self.backgroundColor = [UIColor colorWithRed:0.055 green:0.075 blue:0.08 alpha:1];
        self.layer.cornerRadius = 16;
        self.clipsToBounds = YES;
        self.multipleTouchEnabled = YES;
        self.contentMode = UIViewContentModeRedraw;

        _pointPan = [[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(dragPoint:)];
        _pointPan.minimumNumberOfTouches = 1;
        _pointPan.maximumNumberOfTouches = 1;
        _pointPan.delegate = self;
        [self addGestureRecognizer:_pointPan];

        UITapGestureRecognizer *tap = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(tapped:)];
        tap.delegate = self;
        [tap requireGestureRecognizerToFail:_pointPan];
        [self addGestureRecognizer:tap];

        _viewPan = [[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(panViewport:)];
        _viewPan.minimumNumberOfTouches = 2;
        _viewPan.maximumNumberOfTouches = 2;
        _viewPan.delegate = self;
        [self addGestureRecognizer:_viewPan];

        _pinch = [[UIPinchGestureRecognizer alloc] initWithTarget:self action:@selector(pinchViewport:)];
        _pinch.delegate = self;
        [self addGestureRecognizer:_pinch];

        self.isAccessibilityElement = YES;
        self.accessibilityLabel = @"Zone de relevé du contour";
        self.accessibilityTraits = UIAccessibilityTraitAdjustable;
        self.accessibilityHint = @"Balayer vers le haut ou le bas pour changer le point sélectionné. Les actions permettent de le déplacer. Le pincement agrandit la vue ; deux doigts déplacent la vue.";
        self.accessibilityCustomActions = @[
            [[UIAccessibilityCustomAction alloc] initWithName:@"Ajouter un point au centre" target:self selector:@selector(accessibilityAddPoint)],
            [[UIAccessibilityCustomAction alloc] initWithName:@"Déplacer à gauche" target:self selector:@selector(accessibilityMoveLeft)],
            [[UIAccessibilityCustomAction alloc] initWithName:@"Déplacer à droite" target:self selector:@selector(accessibilityMoveRight)],
            [[UIAccessibilityCustomAction alloc] initWithName:@"Déplacer vers le haut" target:self selector:@selector(accessibilityMoveUp)],
            [[UIAccessibilityCustomAction alloc] initWithName:@"Déplacer vers le bas" target:self selector:@selector(accessibilityMoveDown)],
        ];
    }
    return self;
}

- (void)setReferenceImage:(UIImage *)referenceImage {
    _referenceImage = referenceImage;
    [self setNeedsDisplay];
}
- (void)setDocumentSize:(CGSize)documentSize {
    if (!isfinite(documentSize.width) || !isfinite(documentSize.height) ||
        documentSize.width <= 0 || documentSize.height <= 0) return;
    _documentSize = documentSize;
    [self setNeedsDisplay];
}
- (void)setPoints:(NSMutableArray<NSValue *> *)points {
    _points = points ?: [NSMutableArray array];
    if (_selectedIndex >= (NSInteger)_points.count) _selectedIndex = -1;
    [self setNeedsDisplay];
}
- (void)setCalibrationPoints:(NSMutableArray<NSValue *> *)calibrationPoints {
    _calibrationPoints = calibrationPoints ?: [NSMutableArray array];
    _nextCalibrationPoint = 0;
    [self setNeedsDisplay];
}
- (void)setReferenceKind:(NSString *)referenceKind {
    _referenceKind = [referenceKind copy] ?: @"custom";
    [self setNeedsDisplay];
}
- (void)setReferenceFlipped:(BOOL)referenceFlipped {
    _referenceFlipped = referenceFlipped;
    [self setNeedsDisplay];
}
- (void)setGuideAxisPoints:(NSMutableArray<NSValue *> *)guideAxisPoints {
    _guideAxisPoints = guideAxisPoints ?: [NSMutableArray array];
    _nextGuideAxisPoint = 0;
    [self setNeedsDisplay];
}
- (void)setGuidePoints:(NSMutableArray<NSValue *> *)guidePoints {
    _guidePoints = guidePoints ?: [NSMutableArray array];
    if (_selectedGuideIndex >= (NSInteger)_guidePoints.count) _selectedGuideIndex = -1;
    [self setNeedsDisplay];
}
- (void)setSelectedGuideIndex:(NSInteger)selectedGuideIndex {
    _selectedGuideIndex = selectedGuideIndex >= 0 && selectedGuideIndex < (NSInteger)_guidePoints.count ? selectedGuideIndex : -1;
    [self setNeedsDisplay];
}
- (void)setMillimetersPerUnit:(CGFloat)millimetersPerUnit {
    _millimetersPerUnit = isfinite(millimetersPerUnit) && millimetersPerUnit > 0 ? millimetersPerUnit : 0;
    [self setNeedsDisplay];
}
- (void)setClosed:(BOOL)closed {
    _closed = closed;
    [self setNeedsDisplay];
}
- (void)setMode:(PKCanvasMode)mode {
    if (_mode == mode) return;
    _mode = mode;
    _loupeVisible = NO;
    _draggedIndex = -1;
    [self setNeedsDisplay];
}
- (void)setSelectedIndex:(NSInteger)selectedIndex {
    _selectedIndex = selectedIndex >= 0 && selectedIndex < (NSInteger)_points.count ? selectedIndex : -1;
    [self setNeedsDisplay];
}
- (CGFloat)fitScale {
    CGFloat width = fmax(1.0, CGRectGetWidth(self.bounds) - 32.0);
    CGFloat height = fmax(1.0, CGRectGetHeight(self.bounds) - 54.0);
    return fmax(0.000001, fmin(width / _documentSize.width, height / _documentSize.height));
}
- (CGFloat)drawingScale {
    return _physicalPointsPerUnit > 0 ? _physicalPointsPerUnit : self.fitScale * _zoom;
}
- (CGPoint)viewCenter {
    return CGPointMake(CGRectGetMidX(self.bounds), CGRectGetMidY(self.bounds) - 10.0);
}
- (CGPoint)documentOrigin {
    CGPoint center = self.viewCenter;
    CGFloat scale = self.drawingScale;
    return CGPointMake(center.x + _pan.x - _documentSize.width * scale / 2,
                       center.y + _pan.y - _documentSize.height * scale / 2);
}
- (CGPoint)screenPoint:(CGPoint)point {
    CGPoint origin = self.documentOrigin;
    CGFloat scale = self.drawingScale;
    return CGPointMake(origin.x + point.x * scale, origin.y + point.y * scale);
}
- (CGPoint)documentPoint:(CGPoint)point {
    CGPoint origin = self.documentOrigin;
    CGFloat scale = self.drawingScale;
    return CGPointMake((point.x - origin.x) / scale, (point.y - origin.y) / scale);
}
- (CGPoint)clampedPoint:(CGPoint)point {
    return CGPointMake(PKClamp(point.x, 0, _documentSize.width),
                       PKClamp(point.y, 0, _documentSize.height));
}
- (BOOL)isPointInDocument:(CGPoint)point {
    return point.x >= 0 && point.y >= 0 && point.x <= _documentSize.width && point.y <= _documentSize.height;
}
- (void)resetViewport {
    _zoom = 1.0;
    _physicalPointsPerUnit = 0;
    _pan = CGPointZero;
    [self setNeedsDisplay];
}
- (void)setPhysicalScale:(CGFloat)pointsPerMillimeter unitsPerMillimeter:(CGFloat)unitsPerMillimeter {
    if (!isfinite(pointsPerMillimeter) || !isfinite(unitsPerMillimeter) ||
        pointsPerMillimeter <= 0 || unitsPerMillimeter <= 0) return;
    // Keep the exact requested scale, including across orientation changes.
    CGFloat ratio = pointsPerMillimeter / unitsPerMillimeter;
    if (!isfinite(ratio) || ratio <= 0) return;
    _physicalPointsPerUnit = ratio;
    _pan = CGPointZero;
    [self setNeedsDisplay];
}
- (void)clampPan {
    CGFloat scale = self.drawingScale;
    CGFloat limitX = fmax(0, (_documentSize.width * scale + self.bounds.size.width) / 2 - 40);
    CGFloat limitY = fmax(0, (_documentSize.height * scale + self.bounds.size.height) / 2 - 40);
    _pan.x = PKClamp(_pan.x, -limitX, limitX);
    _pan.y = PKClamp(_pan.y, -limitY, limitY);
}
- (void)changed {
    [self setNeedsDisplay];
    if (self.onChange) self.onChange();
}
- (NSInteger)nearestPoint:(CGPoint)screen inPoints:(NSArray<NSValue *> *)points radius:(CGFloat)radius {
    NSInteger closest = -1;
    CGFloat best = radius * radius;
    for (NSUInteger i = 0; i < points.count; i++) {
        CGPoint p = [self screenPoint:points[i].CGPointValue];
        CGFloat squared = (p.x - screen.x) * (p.x - screen.x) + (p.y - screen.y) * (p.y - screen.y);
        if (squared <= best) { best = squared; closest = (NSInteger)i; }
    }
    return closest;
}
- (NSMutableArray<NSValue *> *)pointsForMode:(PKCanvasMode)mode {
    if (mode == PKCanvasModeCalibration) return _calibrationPoints;
    if (mode == PKCanvasModeGuideAxis) return _guideAxisPoints;
    if (mode == PKCanvasModeGuides) return _guidePoints;
    return _points;
}
- (BOOL)guideOrigin:(CGPoint *)origin unit:(CGPoint *)unit {
    if (_guideAxisPoints.count != 2) return NO;
    CGPoint a = _guideAxisPoints[0].CGPointValue, b = _guideAxisPoints[1].CGPointValue;
    CGFloat length = hypot(b.x - a.x, b.y - a.y);
    if (!isfinite(length) || length < 2) return NO;
    if (origin) *origin = a;
    if (unit) *unit = CGPointMake((b.x - a.x) / length, (b.y - a.y) / length);
    return YES;
}
- (void)tapped:(UITapGestureRecognizer *)gesture {
    CGPoint screen = [gesture locationInView:self];
    CGPoint point = [self documentPoint:screen];
    if (![self isPointInDocument:point]) return;
    if (_mode == PKCanvasModeCalibration || _mode == PKCanvasModeGuideAxis) {
        NSMutableArray *handles = [self pointsForMode:_mode];
        NSInteger *next = _mode == PKCanvasModeCalibration ? &_nextCalibrationPoint : &_nextGuideAxisPoint;
        NSInteger nearest = [self nearestPoint:screen inPoints:handles radius:28];
        if (nearest >= 0) {
            *next = nearest;
            NSString *label = _mode == PKCanvasModeCalibration ? (nearest == 0 ? @"Référence A sélectionnée" : @"Référence B sélectionnée") : (nearest == 0 ? @"Origine O sélectionnée" : @"Direction X sélectionnée");
            UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification, label);
            [self setNeedsDisplay];
            return;
        }
        if (self.onGestureBegan) self.onGestureBegan();
        NSValue *value = [NSValue valueWithCGPoint:point];
        if (handles.count < 2) [handles addObject:value];
        else {
            handles[(NSUInteger)*next] = value;
            *next = 1 - *next;
        }
        [self changed];
        return;
    }
    if (_mode == PKCanvasModeGuides) {
        NSInteger nearest = [self nearestPoint:screen inPoints:_guidePoints radius:24];
        if (nearest >= 0) {
            self.selectedGuideIndex = nearest;
            if (self.onChange) self.onChange();
            return;
        }
        if (![self guideOrigin:NULL unit:NULL] || _guidePoints.count >= 64) return;
        if (self.onGestureBegan) self.onGestureBegan();
        [_guidePoints addObject:[NSValue valueWithCGPoint:point]];
        self.selectedGuideIndex = (NSInteger)_guidePoints.count - 1;
        [self changed];
        return;
    }
    NSInteger selected = [self nearestPoint:screen inPoints:_points radius:24];
    if (selected >= 0) {
        self.selectedIndex = selected;
        if (self.onChange) self.onChange();
        return;
    }
    if (_mode == PKCanvasModeTrace && !_closed && _points.count < 256) {
        if (self.onGestureBegan) self.onGestureBegan();
        [_points addObject:[NSValue valueWithCGPoint:point]];
        self.selectedIndex = (NSInteger)_points.count - 1;
        [self changed];
    }
}
- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer *)gestureRecognizer {
    if (gestureRecognizer == _pointPan) {
        NSArray *points = [self pointsForMode:_mode];
        return [self nearestPoint:[gestureRecognizer locationInView:self] inPoints:points radius:30] >= 0;
    }
    return YES;
}
- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
        shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)other {
    return (gestureRecognizer == _viewPan && other == _pinch) ||
           (gestureRecognizer == _pinch && other == _viewPan);
}
- (void)dragPoint:(UIPanGestureRecognizer *)gesture {
    if (gesture.state == UIGestureRecognizerStateBegan) {
        _draggedMode = _mode;
        NSMutableArray *points = [self pointsForMode:_draggedMode];
        // location includes the small movement that caused recognition; subtract it for hit testing.
        CGPoint translation = [gesture translationInView:self];
        CGPoint location = [gesture locationInView:self];
        CGPoint initial = CGPointMake(location.x - translation.x, location.y - translation.y);
        _draggedIndex = [self nearestPoint:initial inPoints:points radius:34];
        if (_draggedIndex < 0) return;
        _dragStart = [points[(NSUInteger)_draggedIndex] CGPointValue];
        if (_draggedMode == PKCanvasModeGuides) self.selectedGuideIndex = _draggedIndex;
        else if (_draggedMode == PKCanvasModeCalibration) _nextCalibrationPoint = _draggedIndex;
        else if (_draggedMode == PKCanvasModeGuideAxis) _nextGuideAxisPoint = _draggedIndex;
        else self.selectedIndex = _draggedIndex;
        if (self.onGestureBegan) self.onGestureBegan();
    }
    if ((gesture.state == UIGestureRecognizerStateBegan || gesture.state == UIGestureRecognizerStateChanged) && _draggedIndex >= 0) {
        NSMutableArray *points = [self pointsForMode:_draggedMode];
        if ((NSUInteger)_draggedIndex >= points.count) return;
        CGPoint delta = [gesture translationInView:self];
        CGFloat scale = self.drawingScale;
        CGPoint moved = [self clampedPoint:CGPointMake(_dragStart.x + delta.x / scale, _dragStart.y + delta.y / scale)];
        points[(NSUInteger)_draggedIndex] = [NSValue valueWithCGPoint:moved];
        _loupePoint = moved;
        _loupeVisible = YES;
        [self changed];
    }
    if (gesture.state == UIGestureRecognizerStateEnded || gesture.state == UIGestureRecognizerStateCancelled || gesture.state == UIGestureRecognizerStateFailed) {
        _draggedIndex = -1;
        _loupeVisible = NO;
        [self setNeedsDisplay];
    }
}
- (void)panViewport:(UIPanGestureRecognizer *)gesture {
    if (gesture.state == UIGestureRecognizerStateBegan) _panStart = _pan;
    if (gesture.state == UIGestureRecognizerStateBegan || gesture.state == UIGestureRecognizerStateChanged) {
        CGPoint translation = [gesture translationInView:self];
        if (_pinch.state == UIGestureRecognizerStateBegan || _pinch.state == UIGestureRecognizerStateChanged) {
            _panStart = CGPointMake(_pan.x - translation.x, _pan.y - translation.y);
            return;
        }
        _pan = CGPointMake(_panStart.x + translation.x, _panStart.y + translation.y);
        [self clampPan];
        [self setNeedsDisplay];
    }
}
- (void)pinchViewport:(UIPinchGestureRecognizer *)gesture {
    CGPoint location = [gesture locationInView:self];
    if (gesture.state == UIGestureRecognizerStateBegan) {
        _pinchAnchor = [self documentPoint:location];
        _pinchStartZoom = self.drawingScale / self.fitScale;
        _physicalPointsPerUnit = 0;
    }
    if (gesture.state == UIGestureRecognizerStateBegan || gesture.state == UIGestureRecognizerStateChanged) {
        _zoom = PKClamp(_pinchStartZoom * gesture.scale, fmin(1.0, _pinchStartZoom), fmax(12.0, _pinchStartZoom));
        CGFloat scale = self.drawingScale;
        CGPoint center = self.viewCenter;
        _pan = CGPointMake(location.x - center.x + (_documentSize.width / 2 - _pinchAnchor.x) * scale,
                           location.y - center.y + (_documentSize.height / 2 - _pinchAnchor.y) * scale);
        [self clampPan];
        CGPoint translation = [_viewPan translationInView:self];
        _panStart = CGPointMake(_pan.x - translation.x, _pan.y - translation.y);
        [self setNeedsDisplay];
    }
}

- (void)drawPoint:(CGPoint)point color:(UIColor *)color selected:(BOOL)selected label:(NSString *)label {
    CGFloat radius = selected ? 8 : 5;
    if (selected) {
        [[color colorWithAlphaComponent:0.17] setFill];
        [[UIBezierPath bezierPathWithOvalInRect:CGRectMake(point.x - 17, point.y - 17, 34, 34)] fill];
    }
    [color setFill];
    UIBezierPath *dot = [UIBezierPath bezierPathWithOvalInRect:CGRectMake(point.x - radius, point.y - radius, radius * 2, radius * 2)];
    [dot fill];
    [[UIColor colorWithWhite:0.05 alpha:1] setStroke];
    dot.lineWidth = 1.5;
    [dot stroke];
    if (label.length) {
        NSDictionary *attributes = @{ NSFontAttributeName:[UIFont monospacedDigitSystemFontOfSize:11 weight:UIFontWeightSemibold], NSForegroundColorAttributeName:color, NSStrokeColorAttributeName:[UIColor blackColor], NSStrokeWidthAttributeName:@(-2) };
        [label drawAtPoint:CGPointMake(point.x + 9, point.y - 20) withAttributes:attributes];
    }
}
- (void)drawBadge:(NSString *)label frame:(CGRect)box color:(UIColor *)color {
    if (box.size.width <= 12 || box.size.height <= 10) return;
    NSMutableParagraphStyle *style = [NSMutableParagraphStyle new];
    style.lineBreakMode = NSLineBreakByTruncatingTail;
    NSDictionary *attributes = @{NSFontAttributeName:[UIFont monospacedDigitSystemFontOfSize:10 weight:UIFontWeightSemibold], NSForegroundColorAttributeName:color, NSParagraphStyleAttributeName:style};
    [[UIColor colorWithWhite:0.035 alpha:0.88] setFill];
    [[UIBezierPath bezierPathWithRoundedRect:box cornerRadius:5] fill];
    CGRect textRect = CGRectMake(box.origin.x + 6, box.origin.y + (box.size.height - 12) / 2, box.size.width - 12, 12);
    [label drawInRect:textRect withAttributes:attributes];
}
- (void)drawReferenceBadge:(NSString *)label {
    NSString *zoom = _physicalPointsPerUnit > 0 ? @"Échelle écran" : [NSString stringWithFormat:@"Vue ×%.1f", (double)_zoom];
    CGFloat zoomWidth = [zoom sizeWithAttributes:@{NSFontAttributeName:[UIFont monospacedDigitSystemFontOfSize:11 weight:UIFontWeightMedium]}].width + 16;
    CGFloat available = fmax(0, self.bounds.size.width - 16 - zoomWidth - 8);
    CGFloat wanted = [label sizeWithAttributes:@{NSFontAttributeName:[UIFont monospacedDigitSystemFontOfSize:10 weight:UIFontWeightSemibold]}].width + 12;
    CGFloat width = fmin(wanted, available);
    [self drawBadge:label frame:CGRectMake(self.bounds.size.width - 8 - width, 8, width, 26) color:PKAmber()];
}
- (void)drawMeasurementBadge:(NSString *)label {
    CGFloat wanted = [label sizeWithAttributes:@{NSFontAttributeName:[UIFont monospacedDigitSystemFontOfSize:10 weight:UIFontWeightSemibold]}].width + 12;
    CGFloat width = fmin(wanted, fmax(0, self.bounds.size.width - 16));
    [self drawBadge:label frame:CGRectMake(8, fmax(38, self.bounds.size.height - 60), width, 24) color:PKRose()];
}
- (void)drawReferenceOverlay {
    if (_mode != PKCanvasModeCalibration || !_calibrationPoints.count) return;
    CGContextRef context = UIGraphicsGetCurrentContext();
    CGContextSaveGState(context);
    NSString *caption = nil;
    if (_calibrationPoints.count >= 2) {
        CGPoint a = [self screenPoint:_calibrationPoints[0].CGPointValue];
        CGPoint b = [self screenPoint:_calibrationPoints[1].CGPointValue];
        CGFloat length = hypot(b.x - a.x, b.y - a.y);
        UIBezierPath *shape = nil;
        if ([_referenceKind hasPrefix:@"card-"] && length > 0.01) {
            CGFloat ratio = [_referenceKind isEqualToString:@"card-short"] ? 85.60 / 53.98 : 53.98 / 85.60;
            CGFloat sign = _referenceFlipped ? -1 : 1;
            CGPoint offset = CGPointMake(-(b.y - a.y) * ratio * sign, (b.x - a.x) * ratio * sign);
            shape = [UIBezierPath bezierPath];
            [shape moveToPoint:a];[shape addLineToPoint:b];
            [shape addLineToPoint:CGPointMake(b.x + offset.x, b.y + offset.y)];
            [shape addLineToPoint:CGPointMake(a.x + offset.x, a.y + offset.y)];
            [shape closePath];
            caption = @"Carte · 85,60 × 53,98 mm";
        } else if ([_referenceKind hasPrefix:@"coin-"] && length > 0.01) {
            CGPoint center = CGPointMake((a.x + b.x) / 2, (a.y + b.y) / 2);
            shape = [UIBezierPath bezierPathWithOvalInRect:CGRectMake(center.x - length / 2, center.y - length / 2, length, length)];
            caption = [_referenceKind isEqualToString:@"coin-1"] ? @"1 € · Ø 23,25 mm" : ([_referenceKind isEqualToString:@"coin-2"] ? @"2 € · Ø 25,75 mm" : @"50 c · Ø 24,25 mm");
        }
        if (shape) {
            [[PKAmber() colorWithAlphaComponent:0.045] setFill];[shape fill];
            [PKAmber() setStroke];shape.lineWidth = 1.5;[shape stroke];
        }
        UIBezierPath *line = [UIBezierPath bezierPath];
        [line moveToPoint:a];[line addLineToPoint:b];
        CGFloat dash[] = {5, 4};[line setLineDash:dash count:2 phase:0];
        line.lineWidth = 1.5;[PKAmber() setStroke];[line stroke];
    }
    for (NSUInteger i = 0; i < MIN((NSUInteger)2, _calibrationPoints.count); i++) {
        [self drawPoint:[self screenPoint:_calibrationPoints[i].CGPointValue] color:PKAmber()
              selected:_mode == PKCanvasModeCalibration && (NSInteger)i == _nextCalibrationPoint label:i == 0 ? @"A" : @"B"];
    }
    CGContextRestoreGState(context);
    if (caption) [self drawReferenceBadge:caption];
}
- (void)drawGuides {
    if (_mode == PKCanvasModeCalibration || (!_guideAxisPoints.count && !_guidePoints.count)) return;
    BOOL active = _mode == PKCanvasModeGuideAxis || _mode == PKCanvasModeGuides;
    NSString *measurementCaption = nil;
    CGContextRef context = UIGraphicsGetCurrentContext();
    CGContextSaveGState(context);
    CGContextClipToRect(context, CGRectInset(self.bounds, 5, 5));
    CGContextSetAlpha(context, active ? 1.0 : 0.42);
    CGPoint origin, unit;
    if ([self guideOrigin:&origin unit:&unit]) {
        CGPoint screenOrigin = [self screenPoint:origin];
        CGPoint normal = CGPointMake(-unit.y, unit.x);
        CGFloat extent = hypot(_documentSize.width, _documentSize.height) * self.drawingScale * 2 + hypot(self.bounds.size.width, self.bounds.size.height);
        UIBezierPath *axis = [UIBezierPath bezierPath];
        [axis moveToPoint:CGPointMake(screenOrigin.x - unit.x * extent, screenOrigin.y - unit.y * extent)];
        [axis addLineToPoint:CGPointMake(screenOrigin.x + unit.x * extent, screenOrigin.y + unit.y * extent)];
        CGFloat dash[] = {7, 4};[axis setLineDash:dash count:2 phase:0];
        axis.lineWidth = 1.6;[PKBlue() setStroke];[axis stroke];
        for (NSUInteger i = 0; i < _guidePoints.count; i++) {
            CGPoint point = _guidePoints[i].CGPointValue;
            CGFloat x = (point.x - origin.x) * unit.x + (point.y - origin.y) * unit.y;
            CGFloat y = (point.x - origin.x) * normal.x + (point.y - origin.y) * normal.y;
            CGPoint foot = [self screenPoint:CGPointMake(origin.x + unit.x * x, origin.y + unit.y * x)];
            CGPoint screen = [self screenPoint:point];
            BOOL selected = (NSInteger)i == _selectedGuideIndex && _mode == PKCanvasModeGuides;
            UIColor *color = selected ? PKRose() : PKAmber();
            UIBezierPath *perpendicular = [UIBezierPath bezierPath];
            [perpendicular moveToPoint:CGPointMake(foot.x - normal.x * extent, foot.y - normal.y * extent)];
            [perpendicular addLineToPoint:CGPointMake(foot.x + normal.x * extent, foot.y + normal.y * extent)];
            CGFloat smallDash[] = {3, 5};[perpendicular setLineDash:smallDash count:2 phase:0];
            perpendicular.lineWidth = selected ? 1.4 : 0.8;
            [[color colorWithAlphaComponent:selected ? 0.85 : 0.50] setStroke];[perpendicular stroke];
            UIBezierPath *measurement = [UIBezierPath bezierPath];
            [measurement moveToPoint:foot];[measurement addLineToPoint:screen];
            for (NSValue *value in @[[NSValue valueWithCGPoint:foot], [NSValue valueWithCGPoint:screen]]) {
                CGPoint tick = value.CGPointValue;
                [measurement moveToPoint:CGPointMake(tick.x - unit.x * 9, tick.y - unit.y * 9)];
                [measurement addLineToPoint:CGPointMake(tick.x + unit.x * 9, tick.y + unit.y * 9)];
            }
            measurement.lineWidth = selected ? 2 : 1.25;[color setStroke];[measurement stroke];
            if (active) [self drawPoint:screen color:color selected:selected label:[NSString stringWithFormat:@"T%lu", (unsigned long)i + 1]];
            if (selected && _millimetersPerUnit > 0)
                measurementCaption = [NSString stringWithFormat:@"T%lu · X %.2f · Y %.2f mm", (unsigned long)i + 1, (double)(x * _millimetersPerUnit), (double)(y * _millimetersPerUnit)];
        }
    } else if (active) {
        for (NSUInteger i = 0; i < _guidePoints.count; i++)
            [self drawPoint:[self screenPoint:_guidePoints[i].CGPointValue] color:PKAmber() selected:NO label:[NSString stringWithFormat:@"T%lu", (unsigned long)i + 1]];
    }
    for (NSUInteger i = 0; active && i < MIN((NSUInteger)2, _guideAxisPoints.count); i++) {
        [self drawPoint:[self screenPoint:_guideAxisPoints[i].CGPointValue] color:PKBlue()
              selected:_mode == PKCanvasModeGuideAxis && (NSInteger)i == _nextGuideAxisPoint label:i == 0 ? @"O" : @"X"];
    }
    CGContextRestoreGState(context);
    if (measurementCaption) [self drawMeasurementBadge:measurementCaption];
}
- (void)drawLoupe {
    if (!_loupeVisible) return;
    CGFloat diameter = fmin(114, self.bounds.size.width * 0.34);
    CGPoint finger = [self screenPoint:_loupePoint];
    CGPoint center = CGPointMake(finger.x > CGRectGetMidX(self.bounds) ? 12 + diameter / 2 : self.bounds.size.width - 12 - diameter / 2, 48 + diameter / 2);
    CGRect lens = CGRectMake(center.x - diameter / 2, center.y - diameter / 2, diameter, diameter);
    CGContextRef context = UIGraphicsGetCurrentContext();
    CGContextSaveGState(context);
    UIBezierPath *circle = [UIBezierPath bezierPathWithOvalInRect:lens];
    [circle addClip];
    [[UIColor colorWithWhite:0.05 alpha:1] setFill];UIRectFill(lens);
    CGFloat scale = self.drawingScale * 2.5;
    if (_referenceImage && _referenceImage.size.width > 0 && _referenceImage.size.height > 0) {
        CGFloat imageScale = fmin(_documentSize.width / _referenceImage.size.width, _documentSize.height / _referenceImage.size.height);
        CGSize size = CGSizeMake(_referenceImage.size.width * imageScale, _referenceImage.size.height * imageScale);
        CGPoint imageOrigin = CGPointMake((_documentSize.width - size.width) / 2, (_documentSize.height - size.height) / 2);
        [_referenceImage drawInRect:CGRectMake(center.x + (imageOrigin.x - _loupePoint.x) * scale,
                                               center.y + (imageOrigin.y - _loupePoint.y) * scale, size.width * scale, size.height * scale)];
    }
    // Draw the nearby existing contour as a useful alignment cue. This helper
    // never calls drawRect: or renders the view recursively.
    if (_points.count) {
        UIBezierPath *outline = [UIBezierPath bezierPath];
        for (NSUInteger i = 0; i < _points.count; i++) {
            CGPoint p = _points[i].CGPointValue;
            CGPoint screen = CGPointMake(center.x + (p.x - _loupePoint.x) * scale, center.y + (p.y - _loupePoint.y) * scale);
            if (i == 0) [outline moveToPoint:screen];else [outline addLineToPoint:screen];
        }
        if (_closed && _points.count > 2) [outline closePath];
        [PKTeal() setStroke];outline.lineWidth = 1;[outline stroke];
    }
    UIBezierPath *crosshair = [UIBezierPath bezierPath];
    [crosshair moveToPoint:CGPointMake(center.x - 19, center.y)];[crosshair addLineToPoint:CGPointMake(center.x - 4, center.y)];
    [crosshair moveToPoint:CGPointMake(center.x + 4, center.y)];[crosshair addLineToPoint:CGPointMake(center.x + 19, center.y)];
    [crosshair moveToPoint:CGPointMake(center.x, center.y - 19)];[crosshair addLineToPoint:CGPointMake(center.x, center.y - 4)];
    [crosshair moveToPoint:CGPointMake(center.x, center.y + 4)];[crosshair addLineToPoint:CGPointMake(center.x, center.y + 19)];
    [UIColor.blackColor setStroke];crosshair.lineWidth = 3;[crosshair stroke];
    [UIColor.whiteColor setStroke];crosshair.lineWidth = 1;[crosshair stroke];
    CGContextRestoreGState(context);
    [PKBlue() setStroke];circle.lineWidth = 2;[circle stroke];
    NSString *label = @"Loupe ×2,5";
    NSDictionary *attributes = @{NSFontAttributeName:[UIFont systemFontOfSize:10 weight:UIFontWeightSemibold], NSForegroundColorAttributeName:UIColor.whiteColor, NSStrokeColorAttributeName:UIColor.blackColor, NSStrokeWidthAttributeName:@(-3)};
    CGSize size = [label sizeWithAttributes:attributes];
    [label drawAtPoint:CGPointMake(center.x - size.width / 2, CGRectGetMaxY(lens) - 17) withAttributes:attributes];
}
- (void)drawRect:(CGRect)rect {
    (void)rect;
    CGContextRef context = UIGraphicsGetCurrentContext();
    if (!context) return;
    [[UIColor colorWithRed:0.055 green:0.075 blue:0.08 alpha:1] setFill];
    UIRectFill(self.bounds);
    CGPoint origin = self.documentOrigin;
    CGFloat scale = self.drawingScale;
    CGRect documentRect = CGRectMake(origin.x, origin.y, _documentSize.width * scale, _documentSize.height * scale);
    CGContextSaveGState(context);
    CGContextClipToRect(context, documentRect);
    if (_referenceImage && _referenceImage.size.width > 0 && _referenceImage.size.height > 0) {
        CGFloat imageScale = fmin(documentRect.size.width / _referenceImage.size.width,
                                 documentRect.size.height / _referenceImage.size.height);
        CGSize imageSize = CGSizeMake(_referenceImage.size.width * imageScale, _referenceImage.size.height * imageScale);
        [_referenceImage drawInRect:CGRectMake(CGRectGetMidX(documentRect) - imageSize.width / 2,
                                               CGRectGetMidY(documentRect) - imageSize.height / 2,
                                               imageSize.width, imageSize.height)];
    } else {
        [[UIColor colorWithWhite:0.12 alpha:1] setFill];
        UIRectFill(documentRect);
        UIBezierPath *grid = [UIBezierPath bezierPath];
        CGFloat interval = 50 * scale;
        while (interval < 16) interval *= 2;
        while (interval > 64) interval /= 2;
        CGFloat firstX = fmax(origin.x, origin.x + floor((0 - origin.x) / interval) * interval);
        CGFloat firstY = fmax(origin.y, origin.y + floor((0 - origin.y) / interval) * interval);
        for (CGFloat x = firstX; x <= fmin(CGRectGetMaxX(documentRect), self.bounds.size.width); x += interval) {
            [grid moveToPoint:CGPointMake(x, fmax(0, origin.y))];
            [grid addLineToPoint:CGPointMake(x, fmin(self.bounds.size.height, CGRectGetMaxY(documentRect)))];
        }
        for (CGFloat y = firstY; y <= fmin(CGRectGetMaxY(documentRect), self.bounds.size.height); y += interval) {
            [grid moveToPoint:CGPointMake(fmax(0, origin.x), y)];
            [grid addLineToPoint:CGPointMake(fmin(self.bounds.size.width, CGRectGetMaxX(documentRect)), y)];
        }
        [[UIColor colorWithWhite:1 alpha:0.07] setStroke];
        grid.lineWidth = 0.5;
        [grid stroke];
    }
    CGContextRestoreGState(context);
    [[UIColor colorWithWhite:1 alpha:0.15] setStroke];
    UIBezierPath *border = [UIBezierPath bezierPathWithRect:documentRect];
    border.lineWidth = 1;
    [border stroke];

    if (_points.count && _mode != PKCanvasModeCalibration) {
        BOOL editingContour = _mode == PKCanvasModeTrace || _mode == PKCanvasModeAdjust;
        UIBezierPath *path = [UIBezierPath bezierPath];
        [path moveToPoint:[self screenPoint:_points[0].CGPointValue]];
        for (NSUInteger i = 1; i < _points.count; i++) [path addLineToPoint:[self screenPoint:_points[i].CGPointValue]];
        if (_closed && _points.count > 2) {
            [path closePath];
            [[PKTeal() colorWithAlphaComponent:editingContour ? (_referenceImage ? 0.10 : 0.15) : 0.035] setFill];
            [path fill];
        }
        [[PKTeal() colorWithAlphaComponent:editingContour ? 1.0 : 0.38] setStroke];
        path.lineWidth = editingContour ? 2 : 1;
        path.lineJoinStyle = kCGLineJoinRound;
        [path stroke];
        for (NSUInteger i = 0; editingContour && i < _points.count; i++) {
            BOOL selected = (NSInteger)i == _selectedIndex;
            NSString *label = _points.count <= 20 || selected ? [NSString stringWithFormat:@"%lu", (unsigned long)i + 1] : @"";
            [self drawPoint:[self screenPoint:_points[i].CGPointValue] color:PKTeal() selected:selected label:label];
        }
    }
    [self drawReferenceOverlay];
    [self drawGuides];

    NSString *scaleLabel = _physicalPointsPerUnit > 0 ? @"Échelle écran" : [NSString stringWithFormat:@"Vue ×%.1f", (double)_zoom];
    NSDictionary *scaleAttributes = @{NSFontAttributeName:[UIFont monospacedDigitSystemFontOfSize:11 weight:UIFontWeightMedium], NSForegroundColorAttributeName:_physicalPointsPerUnit > 0 ? PKAmber() : [UIColor colorWithWhite:0.82 alpha:1]};
    CGSize scaleSize = [scaleLabel sizeWithAttributes:scaleAttributes];
    CGRect scaleRect = CGRectMake(8, 8, scaleSize.width + 16, 26);
    [[UIColor colorWithRed:0.05 green:0.07 blue:0.08 alpha:0.92] setFill];
    [[UIBezierPath bezierPathWithRoundedRect:scaleRect cornerRadius:7] fill];
    [scaleLabel drawAtPoint:CGPointMake(16, 14) withAttributes:scaleAttributes];

    NSString *hint;
    if (_mode == PKCanvasModeCalibration) hint = [_referenceKind hasPrefix:@"coin-"] ? @"A et B : deux bords opposés de la pièce" : ([_referenceKind hasPrefix:@"card-"] ? @"A et B : extrémités du bord choisi" : @"Place A et B sur une distance connue");
    else if (_mode == PKCanvasModeGuideAxis) hint = @"Place O puis X le long de l’axe choisi";
    else if (_mode == PKCanvasModeGuides) hint = [self guideOrigin:NULL unit:NULL] ? @"Touche pour un trait · glisse son repère" : @"Place d’abord les deux points de l’axe";
    else if (_mode == PKCanvasModeTrace && !_closed) hint = @"Touche pour ajouter · glisse pour ajuster";
    else hint = @"Glisse un point · pince pour zoomer";
    CGRect hintRect = CGRectMake(8, self.bounds.size.height - 29, fmax(0, self.bounds.size.width - 16), 23);
    [[UIColor colorWithRed:0.05 green:0.07 blue:0.08 alpha:0.9] setFill];
    [[UIBezierPath bezierPathWithRoundedRect:hintRect cornerRadius:7] fill];
    NSMutableParagraphStyle *style = [[NSMutableParagraphStyle alloc] init];
    style.alignment = NSTextAlignmentCenter;
    [hint drawInRect:CGRectInset(hintRect, 4, 4) withAttributes:@{NSFontAttributeName:[UIFont systemFontOfSize:11 weight:UIFontWeightMedium], NSForegroundColorAttributeName:[UIColor colorWithWhite:0.82 alpha:1], NSParagraphStyleAttributeName:style}];
    [self drawLoupe];
}

- (NSString *)accessibilityValue {
    if (_mode == PKCanvasModeCalibration) return [NSString stringWithFormat:@"%lu repères de référence", (unsigned long)_calibrationPoints.count];
    if (_mode == PKCanvasModeGuideAxis) return [NSString stringWithFormat:@"%lu repères d’axe, %@ sélectionné", (unsigned long)_guideAxisPoints.count, _nextGuideAxisPoint == 0 ? @"O" : @"X"];
    if (_mode == PKCanvasModeGuides) return _selectedGuideIndex >= 0 ? [NSString stringWithFormat:@"Trait %ld sur %lu sélectionné", (long)_selectedGuideIndex + 1, (unsigned long)_guidePoints.count] : [NSString stringWithFormat:@"%lu traits", (unsigned long)_guidePoints.count];
    if (_selectedIndex >= 0 && _selectedIndex < (NSInteger)_points.count) return [NSString stringWithFormat:@"Point %ld sur %lu sélectionné", (long)_selectedIndex + 1, (unsigned long)_points.count];
    return [NSString stringWithFormat:@"%lu points, %@", (unsigned long)_points.count, _closed ? @"contour fermé" : @"contour ouvert"];
}
- (void)accessibilityIncrement {
    if (_mode == PKCanvasModeCalibration || _mode == PKCanvasModeGuideAxis) {
        NSInteger *next = _mode == PKCanvasModeCalibration ? &_nextCalibrationPoint : &_nextGuideAxisPoint;
        *next = 1 - *next;[self setNeedsDisplay];return;
    }
    if (_mode == PKCanvasModeGuides) {
        if (_guidePoints.count) {self.selectedGuideIndex = (_selectedGuideIndex + 1) % (NSInteger)_guidePoints.count;if (self.onChange) self.onChange();}
        return;
    }
    if (_points.count) {
        self.selectedIndex = (_selectedIndex + 1) % (NSInteger)_points.count;
        if (self.onChange) self.onChange();
    }
}
- (void)accessibilityDecrement {
    if (_mode == PKCanvasModeCalibration || _mode == PKCanvasModeGuideAxis) {
        [self accessibilityIncrement];return;
    }
    if (_mode == PKCanvasModeGuides) {
        if (_guidePoints.count) {self.selectedGuideIndex = (_selectedGuideIndex <= 0 ? (NSInteger)_guidePoints.count : _selectedGuideIndex) - 1;if (self.onChange) self.onChange();}
        return;
    }
    if (_points.count) {
        self.selectedIndex = (_selectedIndex <= 0 ? (NSInteger)_points.count : _selectedIndex) - 1;
        if (self.onChange) self.onChange();
    }
}
- (BOOL)accessibilityAddPoint {
    CGPoint point = [self clampedPoint:[self documentPoint:self.viewCenter]];
    if (_mode == PKCanvasModeCalibration || _mode == PKCanvasModeGuideAxis) {
        NSMutableArray *handles = [self pointsForMode:_mode];
        NSInteger *next = _mode == PKCanvasModeCalibration ? &_nextCalibrationPoint : &_nextGuideAxisPoint;
        if (self.onGestureBegan) self.onGestureBegan();
        if (handles.count < 2) [handles addObject:[NSValue valueWithCGPoint:point]];
        else handles[(NSUInteger)*next] = [NSValue valueWithCGPoint:point];
        *next = 1 - *next;
        [self changed];
        return YES;
    }
    if (_mode == PKCanvasModeGuides) {
        if (_guidePoints.count >= 64 || ![self guideOrigin:NULL unit:NULL]) return NO;
        if (self.onGestureBegan) self.onGestureBegan();
        [_guidePoints addObject:[NSValue valueWithCGPoint:point]];
        self.selectedGuideIndex = (NSInteger)_guidePoints.count - 1;
        [self changed];return YES;
    }
    if (_mode != PKCanvasModeTrace || _closed || _points.count >= 256) return NO;
    if (self.onGestureBegan) self.onGestureBegan();
    [_points addObject:[NSValue valueWithCGPoint:point]];
    self.selectedIndex = (NSInteger)_points.count - 1;
    [self changed];
    return YES;
}
- (BOOL)accessibilityMoveX:(CGFloat)x y:(CGFloat)y {
    NSMutableArray *points = [self pointsForMode:_mode];
    NSInteger index = _selectedIndex;
    if (_mode == PKCanvasModeCalibration) index = MIN(_nextCalibrationPoint, (NSInteger)points.count - 1);
    else if (_mode == PKCanvasModeGuideAxis) index = MIN(_nextGuideAxisPoint, (NSInteger)points.count - 1);
    else if (_mode == PKCanvasModeGuides) index = _selectedGuideIndex;
    if (index < 0 || index >= (NSInteger)points.count) return NO;
    if (self.onGestureBegan) self.onGestureBegan();
    CGPoint point = [points[(NSUInteger)index] CGPointValue];
    point.x += x / self.drawingScale;
    point.y += y / self.drawingScale;
    points[(NSUInteger)index] = [NSValue valueWithCGPoint:[self clampedPoint:point]];
    [self changed];
    return YES;
}
- (BOOL)accessibilityMoveLeft { return [self accessibilityMoveX:-1 y:0]; }
- (BOOL)accessibilityMoveRight { return [self accessibilityMoveX:1 y:0]; }
- (BOOL)accessibilityMoveUp { return [self accessibilityMoveX:0 y:-1]; }
- (BOOL)accessibilityMoveDown { return [self accessibilityMoveX:0 y:1]; }
@end
