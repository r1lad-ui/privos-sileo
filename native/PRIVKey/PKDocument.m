#import "PKDocument.h"
#include <math.h>
#include <float.h>

static NSError *PKError(NSString *text) {
    return [NSError errorWithDomain:@"fr.privos.keylab" code:1 userInfo:@{NSLocalizedDescriptionKey:text}];
}
static BOOL PKNumber(id value, double low, double high) {
    return [value isKindOfClass:NSNumber.class] && isfinite([value doubleValue]) && [value doubleValue]>=low && [value doubleValue]<=high;
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
        _referenceMillimeters=20; _thicknessMillimeters=2;
    }
    return self;
}
- (double)millimetersPerUnit {
    if (self.calibrationPoints.count!=2 || !isfinite(self.referenceMillimeters) || self.referenceMillimeters<=0) return 0;
    CGPoint a=self.calibrationPoints[0].CGPointValue, b=self.calibrationPoints[1].CGPointValue;
    double distance=hypot(a.x-b.x,a.y-b.y);
    return isfinite(distance) && distance>=2 ? self.referenceMillimeters/distance : 0;
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
    NSMutableDictionary *json=[@{@"format":@"PRIVKey",@"version":@1,@"name":self.name?:@"Mon relevé",@"documentSize":@[@(self.documentSize.width),@(self.documentSize.height)],@"points":PKEncodePoints(self.points),@"calibration":PKEncodePoints(self.calibrationPoints),@"referenceMM":@(self.referenceMillimeters),@"thicknessMM":@(self.thicknessMillimeters),@"closed":@(self.closed),@"demo":@(self.demo),@"coordinateSpace":@"image-top-left",@"geometry":@"constant-thickness-silhouette"} mutableCopy];
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
    id json=[NSJSONSerialization JSONObjectWithData:data options:0 error:error];
    if (!json) return nil;
    if (![json isKindOfClass:NSDictionary.class] || ![json[@"format"] isEqual:@"PRIVKey"] || ![json[@"version"] isEqual:@1]) goto invalid;
    size=json[@"documentSize"];
    if (![size isKindOfClass:NSArray.class] || [size count]!=2 || !PKNumber(size[0],1,2400) || !PKNumber(size[1],1,2400)) goto invalid;
    if (![json[@"name"] isKindOfClass:NSString.class] || [json[@"name"] length]>120 || !PKNumber(json[@"referenceMM"],0.1,1000) || !PKNumber(json[@"thicknessMM"],0.1,20)) goto invalid;
    if (![json[@"closed"] isKindOfClass:NSNumber.class] || ![json[@"demo"] isKindOfClass:NSNumber.class]) goto invalid;
    {
        PKDocument *doc=[self new];doc.name=json[@"name"];doc.documentSize=CGSizeMake([size[0] doubleValue],[size[1] doubleValue]);
        doc.points=PKDecodePoints(json[@"points"],256,doc.documentSize);doc.calibrationPoints=PKDecodePoints(json[@"calibration"],2,doc.documentSize);
        if (!doc.points || !doc.calibrationPoints) goto invalid;
        doc.referenceMillimeters=[json[@"referenceMM"] doubleValue];doc.thicknessMillimeters=[json[@"thicknessMM"] doubleValue];doc.closed=[json[@"closed"] boolValue];doc.demo=[json[@"demo"] boolValue];
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
    d.referenceMillimeters=25;return d;
}
@end
