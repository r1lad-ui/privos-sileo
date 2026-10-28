#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, PKCanvasMode) {
    PKCanvasModeCalibration = 0,
    PKCanvasModeTrace = 1,
    PKCanvasModeAdjust = 2,
    PKCanvasModeGuideAxis = 3,
    PKCanvasModeGuides = 4,
};

/// A local, manual outline editor. All points use document coordinates.
@interface PKCanvasView : UIView
@property(nonatomic, strong, nullable) UIImage *referenceImage;
@property(nonatomic) CGSize documentSize;
@property(nonatomic, strong) NSMutableArray<NSValue *> *points;
@property(nonatomic, strong) NSMutableArray<NSValue *> *calibrationPoints;
/// Presets: custom, card-long, card-short, coin-1, coin-2, coin-50.
@property(nonatomic, copy) NSString *referenceKind;
@property(nonatomic) BOOL referenceFlipped;
@property(nonatomic, strong) NSMutableArray<NSValue *> *guideAxisPoints;
@property(nonatomic, strong) NSMutableArray<NSValue *> *guidePoints;
@property(nonatomic) NSInteger selectedGuideIndex;
@property(nonatomic) CGFloat millimetersPerUnit;
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
