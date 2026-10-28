#import "PKCanvasView.h"
#import <QuartzCore/QuartzCore.h>
#import <math.h>

static UIColor *PKTeal(void) {
    return [UIColor colorWithRed:0.35 green:0.91 blue:0.75 alpha:1.0];
}
static UIColor *PKAmber(void) {
    return [UIColor colorWithRed:1.0 green:0.77 blue:0.37 alpha:1.0];
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
    BOOL _draggedCalibration;
    NSInteger _nextCalibrationPoint;
    UIPanGestureRecognizer *_pointPan;
    UIPanGestureRecognizer *_viewPan;
    UIPinchGestureRecognizer *_pinch;
}

- (instancetype)initWithFrame:(CGRect)frame {
    if ((self = [super initWithFrame:frame])) {
        _documentSize = CGSizeMake(1200, 700);
        _points = [NSMutableArray array];
        _calibrationPoints = [NSMutableArray array];
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
- (void)setClosed:(BOOL)closed {
    _closed = closed;
    [self setNeedsDisplay];
}
- (void)setMode:(PKCanvasMode)mode {
    _mode = mode;
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
- (void)tapped:(UITapGestureRecognizer *)gesture {
    CGPoint screen = [gesture locationInView:self];
    CGPoint point = [self documentPoint:screen];
    if (![self isPointInDocument:point]) return;
    if (_mode == PKCanvasModeCalibration) {
        NSInteger nearest = [self nearestPoint:screen inPoints:_calibrationPoints radius:28];
        if (nearest >= 0) {
            _nextCalibrationPoint = nearest;
            UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                            nearest == 0 ? @"Référence A sélectionnée" : @"Référence B sélectionnée");
            return;
        }
        if (self.onGestureBegan) self.onGestureBegan();
        NSValue *value = [NSValue valueWithCGPoint:point];
        if (_calibrationPoints.count < 2) [_calibrationPoints addObject:value];
        else {
            _calibrationPoints[(NSUInteger)_nextCalibrationPoint] = value;
            _nextCalibrationPoint = 1 - _nextCalibrationPoint;
        }
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
        NSArray *points = _mode == PKCanvasModeCalibration ? _calibrationPoints : _points;
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
        _draggedCalibration = _mode == PKCanvasModeCalibration;
        NSMutableArray *points = _draggedCalibration ? _calibrationPoints : _points;
        // location includes the small movement that caused recognition; subtract it for hit testing.
        CGPoint translation = [gesture translationInView:self];
        CGPoint location = [gesture locationInView:self];
        CGPoint initial = CGPointMake(location.x - translation.x, location.y - translation.y);
        _draggedIndex = [self nearestPoint:initial inPoints:points radius:34];
        if (_draggedIndex < 0) return;
        _dragStart = [points[(NSUInteger)_draggedIndex] CGPointValue];
        if (!_draggedCalibration) self.selectedIndex = _draggedIndex;
        if (self.onGestureBegan) self.onGestureBegan();
    }
    if ((gesture.state == UIGestureRecognizerStateBegan || gesture.state == UIGestureRecognizerStateChanged) && _draggedIndex >= 0) {
        NSMutableArray *points = _draggedCalibration ? _calibrationPoints : _points;
        if ((NSUInteger)_draggedIndex >= points.count) return;
        CGPoint delta = [gesture translationInView:self];
        CGFloat scale = self.drawingScale;
        CGPoint moved = [self clampedPoint:CGPointMake(_dragStart.x + delta.x / scale, _dragStart.y + delta.y / scale)];
        points[(NSUInteger)_draggedIndex] = [NSValue valueWithCGPoint:moved];
        [self changed];
    }
    if (gesture.state == UIGestureRecognizerStateEnded || gesture.state == UIGestureRecognizerStateCancelled || gesture.state == UIGestureRecognizerStateFailed) {
        _draggedIndex = -1;
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

    if (_points.count) {
        UIBezierPath *path = [UIBezierPath bezierPath];
        [path moveToPoint:[self screenPoint:_points[0].CGPointValue]];
        for (NSUInteger i = 1; i < _points.count; i++) [path addLineToPoint:[self screenPoint:_points[i].CGPointValue]];
        if (_closed && _points.count > 2) {
            [path closePath];
            [[PKTeal() colorWithAlphaComponent:_referenceImage ? 0.10 : 0.15] setFill];
            [path fill];
        }
        [PKTeal() setStroke];
        path.lineWidth = 2;
        path.lineJoinStyle = kCGLineJoinRound;
        [path stroke];
        for (NSUInteger i = 0; i < _points.count; i++) {
            BOOL selected = (NSInteger)i == _selectedIndex;
            NSString *label = _points.count <= 20 || selected ? [NSString stringWithFormat:@"%lu", (unsigned long)i + 1] : @"";
            [self drawPoint:[self screenPoint:_points[i].CGPointValue] color:PKTeal() selected:selected label:label];
        }
    }
    if (_calibrationPoints.count) {
        if (_calibrationPoints.count >= 2) {
            UIBezierPath *line = [UIBezierPath bezierPath];
            [line moveToPoint:[self screenPoint:_calibrationPoints[0].CGPointValue]];
            [line addLineToPoint:[self screenPoint:_calibrationPoints[1].CGPointValue]];
            CGFloat dash[] = {5, 4};
            [line setLineDash:dash count:2 phase:0];
            line.lineWidth = 1.5;
            [PKAmber() setStroke];
            [line stroke];
        }
        for (NSUInteger i = 0; i < MIN((NSUInteger)2, _calibrationPoints.count); i++) {
            [self drawPoint:[self screenPoint:_calibrationPoints[i].CGPointValue] color:PKAmber()
                  selected:_mode == PKCanvasModeCalibration label:i == 0 ? @"A" : @"B"];
        }
    }

    NSString *scaleLabel = _physicalPointsPerUnit > 0 ? @"Échelle écran" : [NSString stringWithFormat:@"Vue ×%.1f", (double)_zoom];
    NSDictionary *scaleAttributes = @{NSFontAttributeName:[UIFont monospacedDigitSystemFontOfSize:11 weight:UIFontWeightMedium], NSForegroundColorAttributeName:_physicalPointsPerUnit > 0 ? PKAmber() : [UIColor colorWithWhite:0.82 alpha:1]};
    CGSize scaleSize = [scaleLabel sizeWithAttributes:scaleAttributes];
    CGRect scaleRect = CGRectMake(8, 8, scaleSize.width + 16, 26);
    [[UIColor colorWithRed:0.05 green:0.07 blue:0.08 alpha:0.92] setFill];
    [[UIBezierPath bezierPathWithRoundedRect:scaleRect cornerRadius:7] fill];
    [scaleLabel drawAtPoint:CGPointMake(16, 14) withAttributes:scaleAttributes];

    NSString *hint;
    if (_mode == PKCanvasModeCalibration) hint = @"Place A et B sur une distance connue";
    else if (_mode == PKCanvasModeTrace && !_closed) hint = @"Touche pour ajouter · glisse pour ajuster";
    else hint = @"Glisse un point · pince pour zoomer";
    CGRect hintRect = CGRectMake(8, self.bounds.size.height - 29, fmax(0, self.bounds.size.width - 16), 23);
    [[UIColor colorWithRed:0.05 green:0.07 blue:0.08 alpha:0.9] setFill];
    [[UIBezierPath bezierPathWithRoundedRect:hintRect cornerRadius:7] fill];
    NSMutableParagraphStyle *style = [[NSMutableParagraphStyle alloc] init];
    style.alignment = NSTextAlignmentCenter;
    [hint drawInRect:CGRectInset(hintRect, 4, 4) withAttributes:@{NSFontAttributeName:[UIFont systemFontOfSize:11 weight:UIFontWeightMedium], NSForegroundColorAttributeName:[UIColor colorWithWhite:0.82 alpha:1], NSParagraphStyleAttributeName:style}];
}

- (NSString *)accessibilityValue {
    if (_mode == PKCanvasModeCalibration) return [NSString stringWithFormat:@"%lu repères de référence", (unsigned long)_calibrationPoints.count];
    if (_selectedIndex >= 0 && _selectedIndex < (NSInteger)_points.count) return [NSString stringWithFormat:@"Point %ld sur %lu sélectionné", (long)_selectedIndex + 1, (unsigned long)_points.count];
    return [NSString stringWithFormat:@"%lu points, %@", (unsigned long)_points.count, _closed ? @"contour fermé" : @"contour ouvert"];
}
- (void)accessibilityIncrement {
    if (_points.count) {
        self.selectedIndex = (_selectedIndex + 1) % (NSInteger)_points.count;
        if (self.onChange) self.onChange();
    }
}
- (void)accessibilityDecrement {
    if (_points.count) {
        self.selectedIndex = (_selectedIndex <= 0 ? (NSInteger)_points.count : _selectedIndex) - 1;
        if (self.onChange) self.onChange();
    }
}
- (BOOL)accessibilityAddPoint {
    CGPoint point = [self clampedPoint:[self documentPoint:self.viewCenter]];
    if (_mode == PKCanvasModeCalibration) {
        if (self.onGestureBegan) self.onGestureBegan();
        if (_calibrationPoints.count < 2) [_calibrationPoints addObject:[NSValue valueWithCGPoint:point]];
        else _calibrationPoints[(NSUInteger)_nextCalibrationPoint] = [NSValue valueWithCGPoint:point];
        _nextCalibrationPoint = 1 - _nextCalibrationPoint;
        [self changed];
        return YES;
    }
    if (_mode != PKCanvasModeTrace || _closed || _points.count >= 256) return NO;
    if (self.onGestureBegan) self.onGestureBegan();
    [_points addObject:[NSValue valueWithCGPoint:point]];
    self.selectedIndex = (NSInteger)_points.count - 1;
    [self changed];
    return YES;
}
- (BOOL)accessibilityMoveX:(CGFloat)x y:(CGFloat)y {
    NSMutableArray *points = _mode == PKCanvasModeCalibration ? _calibrationPoints : _points;
    NSInteger index = _mode == PKCanvasModeCalibration ? MIN(_nextCalibrationPoint, (NSInteger)points.count - 1) : _selectedIndex;
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
