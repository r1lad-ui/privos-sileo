#import <UIKit/UIKit.h>
#include "PKGeometry.h"

NS_ASSUME_NONNULL_BEGIN
@interface PKDocument : NSObject
@property(nonatomic,copy) NSString *name;
@property(nonatomic,strong,nullable) UIImage *image;
@property(nonatomic) CGSize documentSize;
@property(nonatomic,strong) NSMutableArray<NSValue *> *points;
@property(nonatomic,strong) NSMutableArray<NSValue *> *calibrationPoints;
@property(nonatomic,copy) NSString *referenceKind;
@property(nonatomic) BOOL referenceFlipped;
@property(nonatomic,strong) NSMutableArray<NSValue *> *guideAxisPoints;
@property(nonatomic,strong) NSMutableArray<NSValue *> *guidePoints;
@property(nonatomic) double referenceMillimeters;
@property(nonatomic) double thicknessMillimeters;
@property(nonatomic) BOOL closed;
@property(nonatomic) BOOL demo;
- (double)millimetersPerUnit;
- (BOOL)hasValidGuideAxis;
/// Millimetres from the axis origin; positive Y is perpendicular (-dy, dx).
/// Both coordinates are NaN when the scale, point or axis is invalid.
- (CGPoint)guideCoordinatesForPoint:(CGPoint)point;
- (BOOL)copyMillimeterPoints:(PKPoint *)destination error:(NSString * _Nullable * _Nullable)message;
- (nullable NSData *)serializedData:(NSError **)error;
+ (nullable instancetype)documentFromData:(NSData *)data error:(NSError **)error;
+ (instancetype)demoDocument;
+ (NSArray<NSString *> *)referenceKinds;
+ (double)millimetersForReferenceKind:(NSString *)kind;
+ (NSString *)titleForReferenceKind:(NSString *)kind;
@end
NS_ASSUME_NONNULL_END
