#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, PKCanvasMode) {
    PKCanvasModeCalibration = 0,
    PKCanvasModeTrace = 1,
    PKCanvasModeAdjust = 2,
};

/// A local, manual outline editor. All points use document coordinates.
@interface PKCanvasView : UIView
@property(nonatomic, strong, nullable) UIImage *referenceImage;
@property(nonatomic) CGSize documentSize;
@property(nonatomic, strong) NSMutableArray<NSValue *> *points;
@property(nonatomic, strong) NSMutableArray<NSValue *> *calibrationPoints;
@property(nonatomic) BOOL closed;
@property(nonatomic) PKCanvasMode mode;
@property(nonatomic) NSInteger selectedIndex;
@property(nonatomic, copy, nullable) void (^onChange)(void);
@property(nonatomic, copy, nullable) void (^onGestureBegan)(void);
- (void)resetViewport;
- (void)setPhysicalScale:(CGFloat)pointsPerMillimeter
    unitsPerMillimeter:(CGFloat)unitsPerMillimeter;
@end

NS_ASSUME_NONNULL_END
