#import "PKDocument.h"
#include <math.h>
#include <float.h>

static NSError *PKError(NSString *text) {
    return [NSError errorWithDomain:@"fr.privos.keylab" code:1 userInfo:@{NSLocalizedDescriptionKey:text}];
}
static BOOL PKBoolean(id value) {
    return [value isKindOfClass:NSNumber.class] && CFGetTypeID((__bridge CFTypeRef)value)==CFBooleanGetTypeID();
}
static BOOL PKNumber(id value, double low, double high) {
    return [value isKindOfClass:NSNumber.class] && !PKBoolean(value) && isfinite([value doubleValue]) && [value doubleValue]>=low && [value doubleValue]<=high;
}
static NSArray *PKEncodePoints(NSArray<NSValue *> *points) {
    NSMutableArray *result=[NSMutableArray arrayWithCapacity:points.count];
    for (NSValue *value in points) {CGPoint p=value.CGPointValue; [result addObject:@[@(p.x),@(p.y)]];}
    return result;
}
static NSMutableArray<NSValue *> *PKDecodePoints(id raw, NSUInteger limit, CGSize size) {
    if (![raw isKindOfClass:NSArray.class] || [raw count]>limit) return nil;
    NSMutableArray *result=[NSMutableArray array];
    for (id row in raw) {
        if (![row isKindOfClass:NSArray.class] || [row count]!=2 || !PKNumber(row[0],0,size.width) || !PKNumber(row[1],0,size.height)) return nil;
        [result addObject:[NSValue valueWithCGPoint:CGPointMake([row[0] doubleValue],[row[1] doubleValue])]];
    }
    return result;
}
@implementation PKDocument
- (instancetype)init {
    if ((self=[super init])) {
        _name=@"Mon relevé"; _documentSize=CGSizeMake(1200,700);
        _points=[NSMutableArray array]; _calibrationPoints=[NSMutableArray array];
        _referenceKind=@"custom";_referenceFlipped=NO;
        _guideAxisPoints=[NSMutableArray array];_guidePoints=[NSMutableArray array];
        _referenceMillimeters=20; _thicknessMillimeters=2;
    }
    return self;
}
+ (NSArray<NSString *> *)referenceKinds {
    return @[@"custom",@"card-long",@"card-short",@"coin-1",@"coin-2",@"coin-50"];
}
+ (double)millimetersForReferenceKind:(NSString *)kind {
    if([kind isEqualToString:@"card-long"])return 85.60;
    if([kind isEqualToString:@"card-short"])return 53.98;
    if([kind isEqualToString:@"coin-1"])return 23.25;
    if([kind isEqualToString:@"coin-2"])return 25.75;
    if([kind isEqualToString:@"coin-50"])return 24.25;
    return 0;
}
+ (NSString *)titleForReferenceKind:(NSString *)kind {
    if([kind isEqualToString:@"card-long"])return @"Carte · grand côté";
    if([kind isEqualToString:@"card-short"])return @"Carte · petit côté";
    if([kind isEqualToString:@"coin-1"])return @"Pièce de 1 €";
    if([kind isEqualToString:@"coin-2"])return @"Pièce de 2 €";
    if([kind isEqualToString:@"coin-50"])return @"Pièce de 50 centimes";
    return @"Mesure personnalisée";
}
- (void)setReferenceKind:(NSString *)referenceKind {
    _referenceKind=[referenceKind copy]?:@"custom";
    double preset=[PKDocument millimetersForReferenceKind:_referenceKind];
    if(preset>0)_referenceMillimeters=preset;
}
- (void)setReferenceMillimeters:(double)referenceMillimeters {
    double preset=[PKDocument millimetersForReferenceKind:self.referenceKind];
    _referenceMillimeters=preset>0?preset:referenceMillimeters;
}
- (double)millimetersPerUnit {
    if(![[PKDocument referenceKinds] containsObject:self.referenceKind])return 0;
    if (self.calibrationPoints.count!=2 || !isfinite(self.referenceMillimeters) || self.referenceMillimeters<=0) return 0;
    CGPoint a=self.calibrationPoints[0].CGPointValue, b=self.calibrationPoints[1].CGPointValue;
    double distance=hypot(a.x-b.x,a.y-b.y);
    return isfinite(distance) && distance>=2 ? self.referenceMillimeters/distance : 0;
}
- (BOOL)hasValidGuideAxis {
    if(self.guideAxisPoints.count!=2)return NO;
    CGPoint a=self.guideAxisPoints[0].CGPointValue,b=self.guideAxisPoints[1].CGPointValue;
    double distance=hypot(b.x-a.x,b.y-a.y);
    return isfinite(a.x)&&isfinite(a.y)&&isfinite(b.x)&&isfinite(b.y)&&isfinite(distance)&&distance>=2;
}
- (CGPoint)guideCoordinatesForPoint:(CGPoint)point {
    double scale=self.millimetersPerUnit;
    if(!self.hasValidGuideAxis||!(scale>0)||!isfinite(point.x)||!isfinite(point.y))return CGPointMake(NAN,NAN);
    CGPoint a=self.guideAxisPoints[0].CGPointValue,b=self.guideAxisPoints[1].CGPointValue;
    double length=hypot(b.x-a.x,b.y-a.y),ux=(b.x-a.x)/length,uy=(b.y-a.y)/length;
    double dx=point.x-a.x,dy=point.y-a.y;
    return CGPointMake((dx*ux+dy*uy)*scale,(-dx*uy+dy*ux)*scale);
}
- (BOOL)copyMillimeterPoints:(PKPoint *)destination error:(NSString **)message {
    if (!self.closed || self.points.count<3 || self.points.count>256) {if(message)*message=@"Ferme un contour de 3 à 256 points.";return NO;}
    double scale=self.millimetersPerUnit;
    if (!(scale>0)) {if(message)*message=@"Place les deux repères A et B et indique leur distance réelle.";return NO;}
    double minX=DBL_MAX,maxY=-DBL_MAX;
    for (NSValue *v in self.points) {CGPoint p=v.CGPointValue; minX=fmin(minX,p.x);maxY=fmax(maxY,p.y);}
    for (NSUInteger i=0;i<self.points.count;i++) {
        CGPoint p=self.points[i].CGPointValue;
        destination[i]=(PKPoint){(p.x-minX)*scale,(maxY-p.y)*scale};
    }
    PKStatus status=pk_validate_polygon(destination,self.points.count);
    if (status!=PK_OK) {if(message)*message=[NSString stringWithUTF8String:pk_status_message(status)]; return NO;}
    return YES;
}
- (NSData *)serializedData:(NSError **)error {
    double preset=[PKDocument millimetersForReferenceKind:self.referenceKind];
    if(![[PKDocument referenceKinds] containsObject:self.referenceKind]||
       ![self.name isKindOfClass:NSString.class]||self.name.length>120||
       !PKNumber(@(self.documentSize.width),1,2400)||!PKNumber(@(self.documentSize.height),1,2400)||
       !PKNumber(@(self.referenceMillimeters),0.1,1000)||!PKNumber(@(self.thicknessMillimeters),0.1,20)||
       (preset>0&&fabs(self.referenceMillimeters-preset)>0.000001)){
        if(error)*error=PKError(@"Le nom, les dimensions ou la référence du relevé sont invalides.");return nil;
    }
    NSArray *points=PKEncodePoints(self.points),*calibration=PKEncodePoints(self.calibrationPoints);
    NSArray *axis=PKEncodePoints(self.guideAxisPoints),*guides=PKEncodePoints(self.guidePoints);
    if(!PKDecodePoints(points,256,self.documentSize)||!PKDecodePoints(calibration,2,self.documentSize)||
       !PKDecodePoints(axis,2,self.documentSize)||!PKDecodePoints(guides,64,self.documentSize)){
        if(error)*error=PKError(@"Des points ou des traits dépassent les limites du relevé.");return nil;
    }
    // Schema 2 adds reference presets and manual guides; all point coordinates
    // remain in the original image space so old contours are not transformed.
    NSMutableDictionary *json=[@{@"format":@"PRIVKey",@"version":@2,@"name":self.name,@"documentSize":@[@(self.documentSize.width),@(self.documentSize.height)],@"points":points,@"calibration":calibration,@"referenceKind":self.referenceKind,@"referenceFlipped":@(self.referenceFlipped),@"guideAxis":axis,@"guides":guides,@"referenceMM":@(self.referenceMillimeters),@"thicknessMM":@(self.thicknessMillimeters),@"closed":@(self.closed),@"demo":@(self.demo),@"coordinateSpace":@"image-top-left",@"geometry":@"constant-thickness-silhouette"} mutableCopy];
    if (self.image) {
        NSData *imageData=UIImagePNGRepresentation(self.image);
        if (!imageData || imageData.length>12*1024*1024) {if(error)*error=PKError(@"Image trop grande pour ce relevé. Choisis une photo plus petite.");return nil;}
        json[@"imagePNG"]=[imageData base64EncodedStringWithOptions:0];
    }
    return [NSJSONSerialization dataWithJSONObject:json options:NSJSONWritingPrettyPrinted error:error];
}
+ (instancetype)documentFromData:(NSData *)data error:(NSError **)error {
    if (data.length>20*1024*1024) {if(error)*error=PKError(@"Ce fichier dépasse la limite de 20 Mo.");return nil;}
    id size=nil;
    NSInteger version=0;
    id json=[NSJSONSerialization JSONObjectWithData:data options:0 error:error];
    if (!json) return nil;
    if (![json isKindOfClass:NSDictionary.class] || ![json[@"format"] isEqual:@"PRIVKey"] || !PKNumber(json[@"version"],1,2)) goto invalid;
    version=[json[@"version"] integerValue];
    if([json[@"version"] doubleValue]!=(double)version)goto invalid;
    size=json[@"documentSize"];
    if (![size isKindOfClass:NSArray.class] || [size count]!=2 || !PKNumber(size[0],1,2400) || !PKNumber(size[1],1,2400)) goto invalid;
    if (![json[@"name"] isKindOfClass:NSString.class] || [json[@"name"] length]>120 || !PKNumber(json[@"referenceMM"],0.1,1000) || !PKNumber(json[@"thicknessMM"],0.1,20)) goto invalid;
    if (!PKBoolean(json[@"closed"]) || !PKBoolean(json[@"demo"])) goto invalid;
    {
        PKDocument *doc=[self new];doc.name=json[@"name"];doc.documentSize=CGSizeMake([size[0] doubleValue],[size[1] doubleValue]);
        doc.points=PKDecodePoints(json[@"points"],256,doc.documentSize);doc.calibrationPoints=PKDecodePoints(json[@"calibration"],2,doc.documentSize);
        if (!doc.points || !doc.calibrationPoints) goto invalid;
        doc.referenceMillimeters=[json[@"referenceMM"] doubleValue];doc.thicknessMillimeters=[json[@"thicknessMM"] doubleValue];doc.closed=[json[@"closed"] boolValue];doc.demo=[json[@"demo"] boolValue];
        // Version 1 files have a custom two-point reference and no guide data.
        // Version 2 requires all added fields rather than silently losing them.
        if(version==2){
            id kind=json[@"referenceKind"];
            if(![kind isKindOfClass:NSString.class]||![[self referenceKinds] containsObject:kind]||!PKBoolean(json[@"referenceFlipped"]))goto invalid;
            double preset=[self millimetersForReferenceKind:kind];
            if(preset>0&&fabs(doc.referenceMillimeters-preset)>0.000001)goto invalid;
            doc.referenceKind=kind;doc.referenceFlipped=[json[@"referenceFlipped"] boolValue];
            doc.guideAxisPoints=PKDecodePoints(json[@"guideAxis"],2,doc.documentSize);
            doc.guidePoints=PKDecodePoints(json[@"guides"],64,doc.documentSize);
            if(!doc.guideAxisPoints||!doc.guidePoints)goto invalid;
        }
        id rawImage=json[@"imagePNG"];
        if (rawImage) {
            if (![rawImage isKindOfClass:NSString.class] || [rawImage length]>17*1024*1024) goto invalid;
            NSData *imageData=[[NSData alloc] initWithBase64EncodedString:rawImage options:0];
            UIImage *image=[UIImage imageWithData:imageData scale:1];
            if (!image || image.imageOrientation!=UIImageOrientationUp || image.size.width!=doc.documentSize.width || image.size.height!=doc.documentSize.height) goto invalid;
            doc.image=image;
        }
        return doc;
    }
invalid:
    if(error)*error=PKError(@"Fichier PRIV Key invalide : format, image ou coordonnées incohérents.");return nil;
}
+ (instancetype)demoDocument {
    PKDocument *d=[self new];d.name=@"Exemple fictif";d.demo=YES;d.closed=YES;
    CGPoint points[]={{110,170},{290,170},{290,260},{800,260},{850,310},{850,350},{720,350},{720,330},{660,330},{660,350},{570,350},{570,320},{510,320},{510,350},{290,350},{290,440},{110,440}};
    for(NSUInteger i=0;i<sizeof(points)/sizeof(points[0]);i++) [d.points addObject:[NSValue valueWithCGPoint:points[i]]];
    [d.calibrationPoints addObject:[NSValue valueWithCGPoint:CGPointMake(100,580)]];
    [d.calibrationPoints addObject:[NSValue valueWithCGPoint:CGPointMake(600,580)]];
    d.referenceMillimeters=25;
    [d.guideAxisPoints addObject:[NSValue valueWithCGPoint:CGPointMake(290,420)]];
    [d.guideAxisPoints addObject:[NSValue valueWithCGPoint:CGPointMake(850,420)]];
    for(NSValue *point in @[[NSValue valueWithCGPoint:CGPointMake(510,320)],[NSValue valueWithCGPoint:CGPointMake(660,330)],[NSValue valueWithCGPoint:CGPointMake(720,350)]])[d.guidePoints addObject:point];
    return d;
}
@end
