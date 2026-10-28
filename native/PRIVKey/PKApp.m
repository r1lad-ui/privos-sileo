#import <UIKit/UIKit.h>
#import <PhotosUI/PhotosUI.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <SceneKit/SceneKit.h>
#import "PKDocument.h"
#import "PKCanvasView.h"
#include <math.h>
#include <float.h>

static UIColor *PKBackground(void){return [UIColor colorWithRed:0.043 green:0.071 blue:0.078 alpha:1];}
static UIColor *PKCard(void){return [UIColor colorWithRed:0.082 green:0.118 blue:0.129 alpha:1];}
static UIColor *PKAccent(void){return [UIColor colorWithRed:0.39 green:0.91 blue:0.73 alpha:1];}
static UIColor *PKMuted(void){return [UIColor colorWithRed:0.64 green:0.72 blue:0.74 alpha:1];}
static UILabel *PKLabel(NSString *text,CGFloat size,BOOL bold){
    UILabel *l=[UILabel new];l.text=text;l.numberOfLines=0;l.textColor=UIColor.whiteColor;
    l.font=[UIFont systemFontOfSize:size weight:bold?UIFontWeightSemibold:UIFontWeightRegular];
    l.adjustsFontForContentSizeCategory=YES;return l;
}
static UIStackView *PKStack(NSArray<UIView *> *views,BOOL horizontal,CGFloat spacing){
    UIStackView *s=[[UIStackView alloc]initWithArrangedSubviews:views];s.axis=horizontal?UILayoutConstraintAxisHorizontal:UILayoutConstraintAxisVertical;s.spacing=spacing;return s;
}
static UIButton *PKButton(NSString *title,NSString *icon,BOOL prominent,id target,SEL action){
    UIButtonConfiguration *cfg=prominent?[UIButtonConfiguration filledButtonConfiguration]:[UIButtonConfiguration tintedButtonConfiguration];
    cfg.title=title;cfg.image=icon?[UIImage systemImageNamed:icon]:nil;cfg.imagePadding=7;
    cfg.baseBackgroundColor=prominent?PKAccent():PKCard();cfg.baseForegroundColor=prominent?PKBackground():UIColor.whiteColor;
    cfg.cornerStyle=UIButtonConfigurationCornerStyleMedium;
    cfg.contentInsets=NSDirectionalEdgeInsetsMake(11,12,11,12);
    UIButton *b=[UIButton buttonWithConfiguration:cfg primaryAction:nil];
    [b addTarget:target action:action forControlEvents:UIControlEventTouchUpInside];
    [b.heightAnchor constraintGreaterThanOrEqualToConstant:44].active=YES;return b;
}
static UITextField *PKField(NSString *placeholder){
    UITextField *f=[UITextField new];f.placeholder=placeholder;f.keyboardType=UIKeyboardTypeDecimalPad;
    f.borderStyle=UITextBorderStyleRoundedRect;f.backgroundColor=PKCard();f.textColor=UIColor.whiteColor;
    f.font=[UIFont monospacedDigitSystemFontOfSize:15 weight:UIFontWeightMedium];[f.heightAnchor constraintEqualToConstant:44].active=YES;
    return f;
}
static BOOL PKReadNumber(NSString *text,double min,double max,double *out){
    NSString *normalized=[[text stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet] stringByReplacingOccurrencesOfString:@"," withString:@"."];
    NSScanner *scanner=[NSScanner scannerWithString:normalized];scanner.locale=[NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];double value=0;
    if(![scanner scanDouble:&value]||!scanner.isAtEnd||!isfinite(value)||value<min||value>max)return NO;
    *out=value;return YES;
}
static NSURL *PKDocumentsDirectory(void){
    NSURL *base=[[NSFileManager defaultManager] URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask].firstObject;
    return [base URLByAppendingPathComponent:@"PRIVKey/Releves" isDirectory:YES];
}
static BOOL PKProtectedDirectory(NSURL *url,NSError **error){
    if(![[NSFileManager defaultManager] createDirectoryAtURL:url withIntermediateDirectories:YES attributes:@{NSFileProtectionKey:NSFileProtectionComplete} error:error])return NO;
    [url setResourceValue:@YES forKey:NSURLIsExcludedFromBackupKey error:nil];return YES;
}
static void PKMessage(UIViewController *vc,NSString *title,NSString *text){
    UIAlertController *a=[UIAlertController alertControllerWithTitle:title message:text preferredStyle:UIAlertControllerStyleAlert];
    [a addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];[vc presentViewController:a animated:YES completion:nil];
}
static void PKPopover(UIAlertController *alert,UIViewController *vc){
    if(alert.popoverPresentationController){alert.popoverPresentationController.sourceView=vc.view;alert.popoverPresentationController.sourceRect=CGRectMake(CGRectGetMidX(vc.view.bounds),80,1,1);}
}

@interface PKFilesController : UITableViewController
@property(nonatomic,copy) void (^openFile)(NSURL *);
@property(nonatomic,strong) NSArray<NSURL *> *files;
@end
@implementation PKFilesController
- (void)viewDidLoad{
    [super viewDidLoad];self.title=@"Relevés enregistrés";self.view.backgroundColor=PKBackground();self.tableView.backgroundColor=PKBackground();
    self.navigationItem.rightBarButtonItem=[[UIBarButtonItem alloc]initWithBarButtonSystemItem:UIBarButtonSystemItemDone target:self action:@selector(done)];
    [self reloadFiles];
}
- (void)done{[self dismissViewControllerAnimated:YES completion:nil];}
- (void)reloadFiles{
    NSArray *all=[[NSFileManager defaultManager]contentsOfDirectoryAtURL:PKDocumentsDirectory() includingPropertiesForKeys:@[NSURLContentModificationDateKey] options:NSDirectoryEnumerationSkipsHiddenFiles error:nil]?:@[];
    self.files=[[all filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(NSURL *url,NSDictionary *bindings){return [url.lastPathComponent hasSuffix:@".privkey.json"];}]] sortedArrayUsingComparator:^NSComparisonResult(NSURL *a,NSURL *b){return [b.lastPathComponent compare:a.lastPathComponent];}];
    if(!self.files.count){UILabel *empty=PKLabel(@"Aucun relevé enregistré.\nUtilise Enregistrer dans l’atelier.",16,NO);empty.textAlignment=NSTextAlignmentCenter;empty.textColor=PKMuted();self.tableView.backgroundView=empty;}else self.tableView.backgroundView=nil;
    [self.tableView reloadData];
}
- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section{return self.files.count;}
- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath{
    UITableViewCell *cell=[[UITableViewCell alloc]initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:nil];cell.backgroundColor=PKCard();cell.textLabel.textColor=UIColor.whiteColor;cell.detailTextLabel.textColor=PKMuted();
    NSURL *url=self.files[indexPath.row];cell.textLabel.text=[url.lastPathComponent stringByReplacingOccurrencesOfString:@".privkey.json" withString:@""];cell.textLabel.numberOfLines=2;
    cell.detailTextLabel.text=@"Stocké sur cet iPhone · glisser pour supprimer";cell.accessoryType=UITableViewCellAccessoryDisclosureIndicator;return cell;
}
- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath{
    NSURL *url=self.files[indexPath.row];void(^open)(NSURL *)=self.openFile;[self dismissViewControllerAnimated:YES completion:^{if(open)open(url);}];
}
- (void)tableView:(UITableView *)tableView commitEditingStyle:(UITableViewCellEditingStyle)style forRowAtIndexPath:(NSIndexPath *)indexPath{
    if(style!=UITableViewCellEditingStyleDelete)return;NSError *e=nil;
    if(![[NSFileManager defaultManager]removeItemAtURL:self.files[indexPath.row] error:&e]){PKMessage(self,@"Suppression impossible",e.localizedDescription);return;}[self reloadFiles];
}
@end

@interface PKPreviewController : UIViewController
@property(nonatomic,strong) PKDocument *document;
@end
@implementation PKPreviewController
- (void)viewDidLoad{
    [super viewDidLoad];self.title=@"Aperçu de l’extrusion";self.view.backgroundColor=PKBackground();
    self.navigationItem.rightBarButtonItem=[[UIBarButtonItem alloc]initWithBarButtonSystemItem:UIBarButtonSystemItemDone target:self action:@selector(done)];
    PKPoint points[256];NSString *error=nil;if(![self.document copyMillimeterPoints:points error:&error]){PKMessage(self,@"Contour à corriger",error);return;}
    UIBezierPath *path=[UIBezierPath bezierPath];[path moveToPoint:CGPointMake(points[0].x,points[0].y)];
    for(NSUInteger i=1;i<self.document.points.count;i++)[path addLineToPoint:CGPointMake(points[i].x,points[i].y)];[path closePath];
    SCNShape *shape=[SCNShape shapeWithPath:path extrusionDepth:self.document.thicknessMillimeters];
    SCNMaterial *material=[SCNMaterial material];material.diffuse.contents=PKAccent();material.metalness.contents=@0.45;material.roughness.contents=@0.35;shape.materials=@[material];
    SCNScene *scene=[SCNScene scene];SCNNode *object=[SCNNode nodeWithGeometry:shape];
    PKBounds b;pk_bounds(points,self.document.points.count,&b);object.position=SCNVector3Make(-(b.max_x+b.min_x)/2,-(b.max_y+b.min_y)/2,0);[scene.rootNode addChildNode:object];
    SCNNode *camera=[SCNNode node];camera.camera=[SCNCamera camera];camera.camera.usesOrthographicProjection=YES;camera.camera.orthographicScale=fmax(b.max_x-b.min_x,b.max_y-b.min_y)*0.75;
    camera.position=SCNVector3Make(0,0,150);camera.camera.zNear=0.01;camera.camera.zFar=2000;[scene.rootNode addChildNode:camera];
    SCNView *preview=[SCNView new];preview.scene=scene;preview.pointOfView=camera;preview.autoenablesDefaultLighting=YES;preview.allowsCameraControl=YES;preview.backgroundColor=PKBackground();
    preview.accessibilityLabel=@"Aperçu 3D de la silhouette. Faire glisser pour tourner.";preview.translatesAutoresizingMaskIntoConstraints=NO;[self.view addSubview:preview];
    UILabel *note=PKLabel(@"Silhouette à épaisseur constante.\nLes rainures et profils latéraux ne sont pas reconstruits.\nGlisse pour tourner, pince pour zoomer.",14,NO);note.textColor=PKMuted();note.translatesAutoresizingMaskIntoConstraints=NO;[self.view addSubview:note];
    [NSLayoutConstraint activateConstraints:@[[preview.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],[preview.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],[preview.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],[preview.bottomAnchor constraintEqualToAnchor:note.topAnchor constant:-12],[note.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor constant:20],[note.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor constant:-20],[note.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor constant:-20]]];
}
- (void)done{[self dismissViewControllerAnimated:YES completion:nil];}
@end

@interface PKScreenCalibrationController : UIViewController
@property(nonatomic,strong) UIView *line;
@property(nonatomic,strong) NSLayoutConstraint *lineWidth;
@property(nonatomic,strong) UISlider *slider;
@property(nonatomic,copy) void(^completed)(CGFloat);
@end
@implementation PKScreenCalibrationController
- (void)viewDidLoad{
    [super viewDidLoad];self.title=@"Étalonner l’écran";self.view.backgroundColor=PKBackground();
    UILabel *heading=PKLabel(@"Une règle, 40 mm.",27,YES);
    UILabel *help=PKLabel(@"Pose une règle sur l’écran et ajuste le trait jusqu’à ce qu’il mesure exactement 40 mm. La valeur dépend de l’appareil et du zoom d’affichage.",16,NO);help.textColor=PKMuted();
    self.line=[UIView new];self.line.backgroundColor=PKAccent();self.line.translatesAutoresizingMaskIntoConstraints=NO;self.lineWidth=[self.line.widthAnchor constraintEqualToConstant:255];self.lineWidth.active=YES;[self.line.heightAnchor constraintEqualToConstant:4].active=YES;
    UIView *lineContainer=[UIView new];[lineContainer addSubview:self.line];[lineContainer.heightAnchor constraintEqualToConstant:90].active=YES;
    [NSLayoutConstraint activateConstraints:@[[self.line.centerXAnchor constraintEqualToAnchor:lineContainer.centerXAnchor],[self.line.centerYAnchor constraintEqualToAnchor:lineContainer.centerYAnchor]]];
    self.slider=[UISlider new];self.slider.minimumValue=120;self.slider.maximumValue=285;self.slider.value=255;self.slider.tintColor=PKAccent();[self.slider addTarget:self action:@selector(change) forControlEvents:UIControlEventValueChanged];
    CGFloat saved=[[NSUserDefaults standardUserDefaults]doubleForKey:@"screenPointsPerMM"];if(saved>=3&&saved<=7.125)self.slider.value=saved*40;[self change];
    UIButton *save=PKButton(@"Le trait mesure 40 mm",nil,YES,self,@selector(save));
    UIScrollView *scroll=[UIScrollView new];scroll.translatesAutoresizingMaskIntoConstraints=NO;[self.view addSubview:scroll];
    [NSLayoutConstraint activateConstraints:@[[scroll.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],[scroll.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor],[scroll.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],[scroll.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor]]];
    UIStackView *stack=PKStack(@[heading,help,lineContainer,self.slider,save],NO,22);stack.translatesAutoresizingMaskIntoConstraints=NO;[scroll addSubview:stack];
    [NSLayoutConstraint activateConstraints:@[[stack.topAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.topAnchor constant:28],[stack.bottomAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.bottomAnchor constant:-28],[stack.leadingAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.leadingAnchor constant:24],[stack.trailingAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.trailingAnchor constant:-24],[stack.widthAnchor constraintEqualToAnchor:scroll.frameLayoutGuide.widthAnchor constant:-48]]];
    self.navigationItem.leftBarButtonItem=[[UIBarButtonItem alloc]initWithBarButtonSystemItem:UIBarButtonSystemItemCancel target:self action:@selector(cancel)];
}
- (void)change{self.lineWidth.constant=self.slider.value;}
- (void)cancel{[self dismissViewControllerAnimated:YES completion:nil];}
- (void)save{CGFloat scale=self.slider.value/40;[[NSUserDefaults standardUserDefaults]setDouble:scale forKey:@"screenPointsPerMM"];void(^done)(CGFloat)=self.completed;[self dismissViewControllerAnimated:YES completion:^{if(done)done(scale);}];}
@end

@interface PKRootController : UIViewController<PHPickerViewControllerDelegate,UIImagePickerControllerDelegate,UINavigationControllerDelegate,UIDocumentPickerDelegate,UITextFieldDelegate>
@property(nonatomic,strong) PKDocument *doc;
@property(nonatomic,strong) PKCanvasView *canvas;
@property(nonatomic,strong) UILabel *summary,*hint,*selection,*dimensions,*privacy;
@property(nonatomic,strong) UITextField *reference,*thickness,*xField,*yField;
@property(nonatomic,strong) UISegmentedControl *modes;
@property(nonatomic,strong) UIButton *closeButton,*undoButton,*redoButton;
@property(nonatomic,strong) NSMutableArray<NSDictionary *> *undoStates,*redoStates;
@property(nonatomic) BOOL dirty;
@property(nonatomic,strong) NSMutableArray<NSURL *> *temporaryExports;
@end
@implementation PKRootController
- (void)viewDidLoad{
    [super viewDidLoad];self.title=@"PRIV Key";self.view.backgroundColor=PKBackground();self.view.tintColor=PKAccent();
    self.doc=[PKDocument demoDocument];self.undoStates=[NSMutableArray array];self.redoStates=[NSMutableArray array];self.temporaryExports=[NSMutableArray array];
    self.navigationItem.leftBarButtonItem=[[UIBarButtonItem alloc]initWithImage:[UIImage systemImageNamed:@"folder"] style:UIBarButtonItemStylePlain target:self action:@selector(files)];
    self.navigationItem.rightBarButtonItem=[[UIBarButtonItem alloc]initWithImage:[UIImage systemImageNamed:@"questionmark.circle"] style:UIBarButtonItemStylePlain target:self action:@selector(help)];
    UIScrollView *scroll=[UIScrollView new];scroll.translatesAutoresizingMaskIntoConstraints=NO;scroll.keyboardDismissMode=UIScrollViewKeyboardDismissModeOnDrag;[self.view addSubview:scroll];
    [NSLayoutConstraint activateConstraints:@[[scroll.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],[scroll.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor],[scroll.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],[scroll.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor]]];
    UILabel *eyebrow=PKLabel(@"RELEVÉ MANUEL · 0.1.0",11,YES);eyebrow.textColor=PKAccent();
    UILabel *headline=PKLabel(@"Du contour au modèle.",26,YES);
    self.summary=PKLabel(@"",14,NO);self.summary.textColor=PKMuted();
    UIButton *source=PKButton(@"Photo / nouveau",@"plus.viewfinder",NO,self,@selector(sourceMenu));
    UIButton *save=PKButton(@"Enregistrer",@"square.and.arrow.down",NO,self,@selector(saveDocument));
    UIStackView *sourceRow=PKStack(@[source,save],YES,8);sourceRow.distribution=UIStackViewDistributionFillEqually;
    self.modes=[[UISegmentedControl alloc]initWithItems:@[@"1. Échelle",@"2. Contour",@"3. Ajuster"]];self.modes.selectedSegmentIndex=2;self.modes.selectedSegmentTintColor=PKAccent();[self.modes setTitleTextAttributes:@{NSForegroundColorAttributeName:UIColor.whiteColor} forState:UIControlStateNormal];[self.modes setTitleTextAttributes:@{NSForegroundColorAttributeName:PKBackground()} forState:UIControlStateSelected];[self.modes addTarget:self action:@selector(modeChanged) forControlEvents:UIControlEventValueChanged];[self.modes.heightAnchor constraintEqualToConstant:36].active=YES;
    self.hint=PKLabel(@"",13,NO);self.hint.textColor=PKMuted();
    self.canvas=[PKCanvasView new];self.canvas.layer.cornerRadius=16;self.canvas.clipsToBounds=YES;[self.canvas.heightAnchor constraintEqualToConstant:310].active=YES;
    __weak typeof(self) weakSelf=self;
    self.canvas.onGestureBegan=^{[weakSelf snapshot];};
    self.canvas.onChange=^{[weakSelf canvasChanged];};
    self.undoButton=PKButton(@"Annuler",@"arrow.uturn.backward",NO,self,@selector(undo));
    self.redoButton=PKButton(@"Refaire",@"arrow.uturn.forward",NO,self,@selector(redo));
    UIButton *fit=PKButton(@"Cadrer",@"arrow.up.left.and.arrow.down.right",NO,self,@selector(fit));
    UIStackView *history=PKStack(@[self.undoButton,self.redoButton,fit],YES,6);history.distribution=UIStackViewDistributionFillEqually;
    self.reference=PKField(@"Distance A–B");self.reference.accessibilityLabel=@"Distance réelle A–B en millimètres";
    self.thickness=PKField(@"Épaisseur");self.thickness.accessibilityLabel=@"Épaisseur mesurée en millimètres";
    UIButton *apply=PKButton(@"Appliquer",nil,NO,self,@selector(applyMeasurements));
    UIStackView *referenceColumn=PKStack(@[PKLabel(@"A–B réels (mm)",12,YES),self.reference],NO,6);
    UIStackView *thicknessColumn=PKStack(@[PKLabel(@"Épaisseur (mm)",12,YES),self.thickness],NO,6);
    UIStackView *measures=PKStack(@[referenceColumn,thicknessColumn],YES,10);measures.distribution=UIStackViewDistributionFillEqually;
    self.selection=PKLabel(@"",13,YES);self.xField=PKField(@"X");self.yField=PKField(@"Y");self.xField.accessibilityLabel=@"Abscisse du point depuis le bord gauche, en millimètres";self.yField.accessibilityLabel=@"Ordonnée du point depuis le bord haut, en millimètres";
    UIButton *move=PKButton(@"Déplacer",nil,NO,self,@selector(movePoint));UIStackView *coordinates=PKStack(@[self.xField,self.yField,move],YES,8);coordinates.distribution=UIStackViewDistributionFillEqually;
    self.closeButton=PKButton(@"Fermer le contour",@"checkmark.seal",NO,self,@selector(toggleClosed));
    UIButton *delete=PKButton(@"Retirer le point",@"minus.circle",NO,self,@selector(deletePoint));UIStackView *edit=PKStack(@[self.closeButton,delete],YES,8);edit.distribution=UIStackViewDistributionFillEqually;
    self.dimensions=PKLabel(@"",15,YES);self.dimensions.font=[UIFont monospacedDigitSystemFontOfSize:15 weight:UIFontWeightMedium];
    UIButton *preview=PKButton(@"Voir en 3D",@"cube.transparent",NO,self,@selector(preview));UIButton *realSize=PKButton(@"Échelle écran",@"ruler",NO,self,@selector(physicalScale));UIStackView *previewRow=PKStack(@[preview,realSize],YES,8);previewRow.distribution=UIStackViewDistributionFillEqually;
    UIButton *export=PKButton(@"Exporter le relevé",@"square.and.arrow.up",YES,self,@selector(exportMenu));
    UILabel *limit=PKLabel(@"Le modèle restitue le contour et une épaisseur constante. Les rainures, profils latéraux et la précision de fabrication restent à vérifier.",13,NO);limit.textColor=PKMuted();
    self.privacy=PKLabel(@"Traitement sur l’iPhone. Aucun envoi réseau.\nEnregistre le relevé pour le conserver après fermeture.",12,NO);self.privacy.textColor=PKMuted();
    UIStackView *stack=PKStack(@[eyebrow,headline,self.summary,sourceRow,self.modes,self.hint,self.canvas,history,measures,apply,self.selection,coordinates,edit,self.dimensions,previewRow,export,limit,self.privacy],NO,13);
    stack.translatesAutoresizingMaskIntoConstraints=NO;[scroll addSubview:stack];
    [NSLayoutConstraint activateConstraints:@[[stack.topAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.topAnchor constant:18],[stack.bottomAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.bottomAnchor constant:-24],[stack.leadingAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.leadingAnchor constant:16],[stack.trailingAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.trailingAnchor constant:-16],[stack.widthAnchor constraintEqualToAnchor:scroll.frameLayoutGuide.widthAnchor constant:-32]]];
    UIToolbar *keyboard=[UIToolbar new];[keyboard sizeToFit];keyboard.items=@[[[UIBarButtonItem alloc]initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace target:nil action:nil],[[UIBarButtonItem alloc]initWithTitle:@"Terminé" style:UIBarButtonItemStyleDone target:self action:@selector(endEditing)]];
    for(UITextField *f in @[self.reference,self.thickness,self.xField,self.yField]){f.inputAccessoryView=keyboard;f.delegate=self;}
    [self bindDocument];[self cleanupOldExports];
}
- (void)endEditing{[self.view endEditing:YES];}
- (void)bindDocument{
    self.canvas.referenceImage=self.doc.image;self.canvas.documentSize=self.doc.documentSize;self.canvas.points=self.doc.points;self.canvas.calibrationPoints=self.doc.calibrationPoints;self.canvas.closed=self.doc.closed;self.canvas.selectedIndex=-1;self.canvas.mode=(PKCanvasMode)self.modes.selectedSegmentIndex;
    self.reference.text=[NSString stringWithFormat:@"%.12g",self.doc.referenceMillimeters];self.thickness.text=[NSString stringWithFormat:@"%.12g",self.doc.thicknessMillimeters];[self.canvas resetViewport];[self refresh];
}
- (void)refresh{
    self.summary.text=[NSString stringWithFormat:@"%@ · %lu points%@",self.doc.name,(unsigned long)self.doc.points.count,self.dirty?@" · modifié":@""];
    NSString *hints[]={
        @"Place A et B sur deux repères d’une distance connue, dans le même plan que la clé. Une règle graduée dans la photo convient.",
        @"Touche le contour pour ajouter un point. Place les points dans l’ordre, puis ferme le contour. Deux doigts pour déplacer, pince pour zoomer.",
        @"Touche un point, puis glisse-le ou saisis ses coordonnées. Deux doigts pour déplacer la vue, pince pour zoomer."
    };
    self.hint.text=hints[self.modes.selectedSegmentIndex];
    self.undoButton.enabled=self.undoStates.count>0;self.redoButton.enabled=self.redoStates.count>0;
    UIButtonConfiguration *cfg=[self.closeButton.configuration copy];cfg.title=self.doc.closed?@"Rouvrir":@"Fermer";self.closeButton.configuration=cfg;
    NSInteger index=self.canvas.selectedIndex;double scale=self.doc.millimetersPerUnit;BOOL selected=index>=0 && index<(NSInteger)self.doc.points.count && self.modes.selectedSegmentIndex!=0;
    self.selection.text=selected?[NSString stringWithFormat:@"Point %ld · X / Y depuis le bord de l’image (mm)",(long)index+1]:@"Sélectionne un point du contour pour le déplacer.";
    self.xField.enabled=self.yField.enabled=selected && scale>0;
    if(selected&&scale>0){CGPoint p=self.doc.points[index].CGPointValue;if(!self.xField.isFirstResponder)self.xField.text=[NSString stringWithFormat:@"%.3f",p.x*scale];if(!self.yField.isFirstResponder)self.yField.text=[NSString stringWithFormat:@"%.3f",p.y*scale];}
    else {self.xField.text=@"";self.yField.text=@"";}
    PKPoint points[256];NSString *error=nil;
    if([self.doc copyMillimeterPoints:points error:&error]){PKBounds b;pk_bounds(points,self.doc.points.count,&b);self.dimensions.text=[NSString stringWithFormat:@"%.2f × %.2f × %.2f mm\nContour valide · %.2f mm²",b.max_x-b.min_x,b.max_y-b.min_y,self.doc.thicknessMillimeters,fabs(pk_area(points,self.doc.points.count))];self.dimensions.textColor=PKAccent();}
    else {self.dimensions.text=error;self.dimensions.textColor=PKMuted();}
    [self.canvas setNeedsDisplay];
}
- (NSDictionary *)state{
    return @{@"points":[self.doc.points copy],@"calibration":[self.doc.calibrationPoints copy],@"reference":@(self.doc.referenceMillimeters),@"thickness":@(self.doc.thicknessMillimeters),@"closed":@(self.doc.closed)};
}
- (void)snapshot{[self.undoStates addObject:[self state]];if(self.undoStates.count>60)[self.undoStates removeObjectAtIndex:0];[self.redoStates removeAllObjects];self.dirty=YES;}
- (void)restore:(NSDictionary *)state{
    self.doc.points=[state[@"points"] mutableCopy];self.doc.calibrationPoints=[state[@"calibration"] mutableCopy];self.doc.referenceMillimeters=[state[@"reference"]doubleValue];self.doc.thicknessMillimeters=[state[@"thickness"]doubleValue];self.doc.closed=[state[@"closed"]boolValue];self.dirty=YES;
    self.canvas.points=self.doc.points;self.canvas.calibrationPoints=self.doc.calibrationPoints;self.canvas.closed=self.doc.closed;self.canvas.selectedIndex=-1;self.reference.text=[NSString stringWithFormat:@"%.12g",self.doc.referenceMillimeters];self.thickness.text=[NSString stringWithFormat:@"%.12g",self.doc.thicknessMillimeters];[self.canvas resetViewport];[self refresh];
}
- (void)undo{if(!self.undoStates.count)return;[self.redoStates addObject:[self state]];NSDictionary *s=self.undoStates.lastObject;[self.undoStates removeLastObject];[self restore:s];}
- (void)redo{if(!self.redoStates.count)return;[self.undoStates addObject:[self state]];NSDictionary *s=self.redoStates.lastObject;[self.redoStates removeLastObject];[self restore:s];}
- (void)canvasChanged{self.doc.closed=self.canvas.closed;[self refresh];}
- (void)modeChanged{[self endEditing];self.canvas.mode=(PKCanvasMode)self.modes.selectedSegmentIndex;self.canvas.selectedIndex=-1;if(self.modes.selectedSegmentIndex==0)[self.canvas resetViewport];[self refresh];}
- (void)fit{[self.canvas resetViewport];}
- (void)applyMeasurements{
    double reference=0,thickness=0;
    if(!PKReadNumber(self.reference.text,0.1,1000,&reference)||!PKReadNumber(self.thickness.text,0.1,20,&thickness)){PKMessage(self,@"Mesure invalide",@"Distance A–B : de 0,1 à 1 000 mm. Épaisseur : de 0,1 à 20 mm. Utilise tes mesures réelles.");return;}
    [self snapshot];self.doc.referenceMillimeters=reference;self.doc.thicknessMillimeters=thickness;self.dirty=YES;[self endEditing];[self.canvas resetViewport];[self refresh];
}
- (void)movePoint{
    NSInteger i=self.canvas.selectedIndex;double scale=self.doc.millimetersPerUnit,x=0,y=0;if(i<0||i>=(NSInteger)self.doc.points.count||!(scale>0)){PKMessage(self,@"Point requis",@"Sélectionne un point du contour après avoir étalonné l’image.");return;}
    if(!PKReadNumber(self.xField.text,0,self.doc.documentSize.width*scale,&x)||!PKReadNumber(self.yField.text,0,self.doc.documentSize.height*scale,&y)){PKMessage(self,@"Coordonnées invalides",@"Les coordonnées doivent rester dans l’image.");return;}
    [self snapshot];self.doc.points[i]=[NSValue valueWithCGPoint:CGPointMake(x/scale,y/scale)];self.dirty=YES;[self endEditing];[self refresh];
}
- (void)deletePoint{
    NSInteger i=self.canvas.selectedIndex;if(self.modes.selectedSegmentIndex==0){PKMessage(self,@"Mode contour",@"Passe sur Contour ou Ajuster pour retirer un point.");return;}
    if(i<0||i>=(NSInteger)self.doc.points.count)return;[self snapshot];[self.doc.points removeObjectAtIndex:i];if(self.doc.points.count<3)self.doc.closed=self.canvas.closed=NO;self.canvas.selectedIndex=-1;self.dirty=YES;[self refresh];
}
- (void)toggleClosed{
    if(!self.doc.closed && self.doc.points.count<3){PKMessage(self,@"Contour incomplet",@"Ajoute au moins trois points.");return;}
    [self snapshot];self.doc.closed=!self.doc.closed;self.canvas.closed=self.doc.closed;self.dirty=YES;[self refresh];
}
- (void)replaceDocument:(PKDocument *)doc{
    self.doc=doc;self.dirty=NO;[self.undoStates removeAllObjects];[self.redoStates removeAllObjects];self.modes.selectedSegmentIndex=doc.calibrationPoints.count==2?2:0;[self bindDocument];
}
- (void)confirmReplacement:(void(^)(void))action{
    if(!self.dirty){action();return;}
    UIAlertController *a=[UIAlertController alertControllerWithTitle:@"Remplacer ce relevé ?" message:@"Les modifications non enregistrées seront perdues." preferredStyle:UIAlertControllerStyleAlert];
    [a addAction:[UIAlertAction actionWithTitle:@"Annuler" style:UIAlertActionStyleCancel handler:nil]];
    [a addAction:[UIAlertAction actionWithTitle:@"Remplacer" style:UIAlertActionStyleDestructive handler:^(UIAlertAction *x){action();}]];[self presentViewController:a animated:YES completion:nil];
}
- (void)sourceMenu{
    [self endEditing];UIAlertController *menu=[UIAlertController alertControllerWithTitle:@"Commencer un relevé" message:@"Une photo de face avec une règle dans le même plan permet un relevé manuel. La perspective n’est pas corrigée automatiquement." preferredStyle:UIAlertControllerStyleActionSheet];
    [menu addAction:[UIAlertAction actionWithTitle:@"Choisir une photo" style:UIAlertActionStyleDefault handler:^(UIAlertAction *a){[self confirmReplacement:^{[self pickPhoto];}];}]];
    if([UIImagePickerController isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera])[menu addAction:[UIAlertAction actionWithTitle:@"Prendre une photo" style:UIAlertActionStyleDefault handler:^(UIAlertAction *a){[self confirmReplacement:^{UIImagePickerController *picker=[UIImagePickerController new];picker.delegate=self;picker.sourceType=UIImagePickerControllerSourceTypeCamera;[self presentViewController:picker animated:YES completion:nil];}];}]];
    [menu addAction:[UIAlertAction actionWithTitle:@"Plan vierge" style:UIAlertActionStyleDefault handler:^(UIAlertAction *a){[self confirmReplacement:^{[self replaceDocument:[PKDocument new]];}];}]];
    [menu addAction:[UIAlertAction actionWithTitle:@"Recharger l’exemple fictif" style:UIAlertActionStyleDefault handler:^(UIAlertAction *a){[self confirmReplacement:^{[self replaceDocument:[PKDocument demoDocument]];}];}]];
    [menu addAction:[UIAlertAction actionWithTitle:@"Annuler" style:UIAlertActionStyleCancel handler:nil]];PKPopover(menu,self);[self presentViewController:menu animated:YES completion:nil];
}
- (void)pickPhoto{
    PHPickerConfiguration *config=[[PHPickerConfiguration alloc]init];config.filter=PHPickerFilter.imagesFilter;config.selectionLimit=1;
    PHPickerViewController *picker=[[PHPickerViewController alloc]initWithConfiguration:config];picker.delegate=self;[self presentViewController:picker animated:YES completion:nil];
}
- (void)picker:(PHPickerViewController *)picker didFinishPicking:(NSArray<PHPickerResult *> *)results{
    NSItemProvider *provider=results.firstObject.itemProvider;if(!provider){[picker dismissViewControllerAnimated:YES completion:nil];return;}
    if(![provider canLoadObjectOfClass:UIImage.class]){[picker dismissViewControllerAnimated:YES completion:^{PKMessage(self,@"Image non prise en charge",@"Choisis une image JPEG, PNG ou HEIC.");}];return;}
    picker.view.userInteractionEnabled=NO;
    __weak typeof(self) weakSelf=self;
    [provider loadObjectOfClass:UIImage.class completionHandler:^(id<NSItemProviderReading> item,NSError *error){dispatch_async(dispatch_get_main_queue(),^{[picker dismissViewControllerAnimated:YES completion:^{if([item isKindOfClass:UIImage.class])[weakSelf useImage:(UIImage *)item];else PKMessage(weakSelf,@"Import impossible",error.localizedDescription?:@"La photo n’a pas pu être lue.");}];});}];
}
- (void)imagePickerController:(UIImagePickerController *)picker didFinishPickingMediaWithInfo:(NSDictionary<UIImagePickerControllerInfoKey,id> *)info{
    UIImage *image=info[UIImagePickerControllerOriginalImage];[picker dismissViewControllerAnimated:YES completion:^{if(image)[self useImage:image];}];
}
- (void)imagePickerControllerDidCancel:(UIImagePickerController *)picker{[picker dismissViewControllerAnimated:YES completion:nil];}
- (void)useImage:(UIImage *)image{
    double factor=fmin(1,2400/fmax(image.size.width,image.size.height));CGSize size=CGSizeMake(fmax(1,round(image.size.width*factor)),fmax(1,round(image.size.height*factor)));
    UIGraphicsImageRendererFormat *format=[UIGraphicsImageRendererFormat defaultFormat];format.scale=1;format.opaque=YES;
    UIGraphicsImageRenderer *renderer=[[UIGraphicsImageRenderer alloc]initWithSize:size format:format];
    UIImage *normalized=[renderer imageWithActions:^(UIGraphicsImageRendererContext *context){[UIColor.whiteColor setFill];UIRectFill(CGRectMake(0,0,size.width,size.height));[image drawInRect:CGRectMake(0,0,size.width,size.height)];}];
    PKDocument *doc=[PKDocument new];doc.name=@"Relevé photo";doc.image=normalized;doc.documentSize=size;[self replaceDocument:doc];self.dirty=YES;[self refresh];
}
- (void)saveDocument{
    [self endEditing];if(![self checkPendingMeasurements])return;UIAlertController *a=[UIAlertController alertControllerWithTitle:@"Enregistrer sur l’iPhone" message:@"Le relevé et sa photo seront conservés dans PRIV Key jusqu’à leur suppression." preferredStyle:UIAlertControllerStyleAlert];
    [a addTextFieldWithConfigurationHandler:^(UITextField *f){f.text=self.doc.name;f.placeholder=@"Nom du relevé";f.autocorrectionType=UITextAutocorrectionTypeNo;}];
    [a addAction:[UIAlertAction actionWithTitle:@"Annuler" style:UIAlertActionStyleCancel handler:nil]];
    [a addAction:[UIAlertAction actionWithTitle:@"Enregistrer" style:UIAlertActionStyleDefault handler:^(UIAlertAction *x){
        NSString *name=[a.textFields.firstObject.text stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];if(!name.length)name=@"Mon relevé";if(name.length>80)name=[name substringToIndex:80];self.doc.name=name;
        NSError *error=nil;NSData *data=[self.doc serializedData:&error];if(!data||!PKProtectedDirectory(PKDocumentsDirectory(),&error)){PKMessage(self,@"Enregistrement impossible",error.localizedDescription);return;}
        NSCharacterSet *allowed=[NSCharacterSet characterSetWithCharactersInString:@"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ "];
        NSString *safe=[[name componentsSeparatedByCharactersInSet:allowed.invertedSet] componentsJoinedByString:@"_"];
        NSString *file=[NSString stringWithFormat:@"%@_%@.privkey.json",safe,[[NSUUID UUID].UUIDString substringToIndex:8]];
        NSURL *url=[PKDocumentsDirectory()URLByAppendingPathComponent:file];
        if(![data writeToURL:url options:NSDataWritingAtomic|NSDataWritingFileProtectionComplete error:&error]){PKMessage(self,@"Enregistrement impossible",error.localizedDescription);return;}
        self.dirty=NO;[self refresh];PKMessage(self,@"Relevé enregistré",@"Tu peux le retrouver avec le bouton dossier, en haut à gauche.");
    }]];[self presentViewController:a animated:YES completion:nil];
}
- (void)files{
    [self endEditing];UIAlertController *menu=[UIAlertController alertControllerWithTitle:@"Relevés" message:nil preferredStyle:UIAlertControllerStyleActionSheet];
    [menu addAction:[UIAlertAction actionWithTitle:@"Relevés sur cet iPhone" style:UIAlertActionStyleDefault handler:^(UIAlertAction *a){[self confirmReplacement:^{PKFilesController *vc=[PKFilesController new];__weak typeof(self) w=self;vc.openFile=^(NSURL *url){[w openURL:url];};[self presentViewController:[[UINavigationController alloc]initWithRootViewController:vc] animated:YES completion:nil];}];}]];
    [menu addAction:[UIAlertAction actionWithTitle:@"Importer depuis Fichiers" style:UIAlertActionStyleDefault handler:^(UIAlertAction *a){[self confirmReplacement:^{UIDocumentPickerViewController *picker=[[UIDocumentPickerViewController alloc]initForOpeningContentTypes:@[UTTypeJSON] asCopy:YES];picker.delegate=self;picker.allowsMultipleSelection=NO;[self presentViewController:picker animated:YES completion:nil];}];}]];
    [menu addAction:[UIAlertAction actionWithTitle:@"Annuler" style:UIAlertActionStyleCancel handler:nil]];PKPopover(menu,self);[self presentViewController:menu animated:YES completion:nil];
}
- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls{if(urls.firstObject)[self openURL:urls.firstObject];}
- (void)openURL:(NSURL *)url{
    BOOL scoped=[url startAccessingSecurityScopedResource];NSNumber *size=nil;[url getResourceValue:&size forKey:NSURLFileSizeKey error:nil];
    if(size.unsignedLongLongValue>20*1024*1024){if(scoped)[url stopAccessingSecurityScopedResource];PKMessage(self,@"Fichier trop volumineux",@"La limite d’import est de 20 Mo.");return;}
    NSError *error=nil;NSData *data=[NSData dataWithContentsOfURL:url options:NSDataReadingMappedIfSafe error:&error];
    PKDocument *doc=data?[PKDocument documentFromData:data error:&error]:nil;if(scoped)[url stopAccessingSecurityScopedResource];
    if(!doc){PKMessage(self,@"Import impossible",error.localizedDescription?:@"Fichier invalide.");return;}[self replaceDocument:doc];
}
- (BOOL)checkPendingMeasurements{
    double r,t;if(!PKReadNumber(self.reference.text,0.1,1000,&r)||!PKReadNumber(self.thickness.text,0.1,20,&t)){PKMessage(self,@"Mesures à vérifier",@"Corrige et applique les mesures avant de continuer.");return NO;}
    if(fabs(r-self.doc.referenceMillimeters)>1e-8 || fabs(t-self.doc.thicknessMillimeters)>1e-8){PKMessage(self,@"Mesures non appliquées",@"Appuie sur Appliquer pour utiliser les valeurs saisies.");return NO;}return YES;
}
- (BOOL)validGeometry{
    if(![self checkPendingMeasurements])return NO;PKPoint points[256];NSString *error=nil;if(![self.doc copyMillimeterPoints:points error:&error]){PKMessage(self,@"Contour à corriger",error);return NO;}return YES;
}
- (void)preview{[self endEditing];if(![self validGeometry])return;PKPreviewController *vc=[PKPreviewController new];vc.document=self.doc;[self presentViewController:[[UINavigationController alloc]initWithRootViewController:vc] animated:YES completion:nil];}
- (void)physicalScale{
    [self endEditing];if(![self checkPendingMeasurements])return;if(!(self.doc.millimetersPerUnit>0)){PKMessage(self,@"Échelle de l’image requise",@"Place A et B et applique leur distance réelle avant d’afficher le contour à taille réelle.");return;}
    self.modes.selectedSegmentIndex=2;self.canvas.mode=PKCanvasModeAdjust;self.canvas.selectedIndex=-1;[self refresh];
    UIAlertController *menu=[UIAlertController alertControllerWithTitle:@"Contour à taille réelle" message:@"L’écran doit être étalonné avec une règle. Un zoom ultérieur quitte l’échelle réelle." preferredStyle:UIAlertControllerStyleActionSheet];
    CGFloat saved=[[NSUserDefaults standardUserDefaults]doubleForKey:@"screenPointsPerMM"];
    if(saved>=3&&saved<=7.125)[menu addAction:[UIAlertAction actionWithTitle:@"Appliquer l’étalonnage enregistré" style:UIAlertActionStyleDefault handler:^(UIAlertAction *a){[self.canvas setPhysicalScale:saved unitsPerMillimeter:1/self.doc.millimetersPerUnit];}]];
    [menu addAction:[UIAlertAction actionWithTitle:@"Étalonner avec une règle" style:UIAlertActionStyleDefault handler:^(UIAlertAction *a){PKScreenCalibrationController *vc=[PKScreenCalibrationController new];__weak typeof(self) w=self;vc.completed=^(CGFloat p){[w.canvas setPhysicalScale:p unitsPerMillimeter:1/w.doc.millimetersPerUnit];};[self presentViewController:[[UINavigationController alloc]initWithRootViewController:vc] animated:YES completion:nil];}]];
    [menu addAction:[UIAlertAction actionWithTitle:@"Annuler" style:UIAlertActionStyleCancel handler:nil]];PKPopover(menu,self);[self presentViewController:menu animated:YES completion:nil];
}
- (void)exportMenu{
    [self endEditing];UIAlertController *menu=[UIAlertController alertControllerWithTitle:@"Exporter" message:@"STL : modèle en millimètres, à épaisseur constante. Les exports partagés ne sont plus gérés par PRIV Key." preferredStyle:UIAlertControllerStyleActionSheet];
    for(NSString *type in @[@"STL",@"SVG",@"JSON"]){NSString *title=[@{@"STL":@"Modèle 3D · STL",@"SVG":@"Contour à l’échelle · SVG",@"JSON":@"Relevé modifiable avec photo · JSON"} objectForKey:type];
        [menu addAction:[UIAlertAction actionWithTitle:title style:UIAlertActionStyleDefault handler:^(UIAlertAction *a){[self exportType:type];}]];
    }
    [menu addAction:[UIAlertAction actionWithTitle:@"Annuler" style:UIAlertActionStyleCancel handler:nil]];PKPopover(menu,self);[self presentViewController:menu animated:YES completion:nil];
}
- (void)exportType:(NSString *)type{
    if(![self checkPendingMeasurements])return;
    if(![type isEqual:@"JSON"] && ![self validGeometry])return;
    NSString *folder=[@"PRIVKey-Export-" stringByAppendingString:[NSUUID UUID].UUIDString];
    NSURL *dir=[[NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES]URLByAppendingPathComponent:folder isDirectory:YES];NSError *error=nil;
    if(!PKProtectedDirectory(dir,&error)){PKMessage(self,@"Export impossible",error.localizedDescription);return;}
    NSURL *file=[dir URLByAppendingPathComponent:[@"releve." stringByAppendingString:[type isEqual:@"JSON"]?@"privkey.json":type.lowercaseString]];
    BOOL success=NO;
    if([type isEqual:@"JSON"]){NSData *data=[self.doc serializedData:&error];success=data && [data writeToURL:file options:NSDataWritingAtomic|NSDataWritingFileProtectionComplete error:&error];}
    else{
        PKPoint points[256];[self.doc copyMillimeterPoints:points error:nil];
        if([type isEqual:@"SVG"]){PKBounds b;pk_bounds(points,self.doc.points.count,&b);for(NSUInteger i=0;i<self.doc.points.count;i++)points[i].y=b.max_y+b.min_y-points[i].y;}
        FILE *stream=fopen(file.fileSystemRepresentation,"wb");
        if(stream){PKStatus result=[type isEqual:@"STL"]?pk_write_stl(stream,points,self.doc.points.count,self.doc.thicknessMillimeters):pk_write_svg(stream,points,self.doc.points.count);int closed=fclose(stream);success=result==PK_OK && closed==0;if(!success)error=[NSError errorWithDomain:@"fr.privos.keylab" code:2 userInfo:@{NSLocalizedDescriptionKey:[NSString stringWithUTF8String:pk_status_message(result)]}];}
        if(success)[[NSFileManager defaultManager]setAttributes:@{NSFileProtectionKey:NSFileProtectionComplete} ofItemAtPath:file.path error:nil];
    }
    if(!success){[[NSFileManager defaultManager]removeItemAtURL:dir error:nil];PKMessage(self,@"Export impossible",error.localizedDescription?:@"Le fichier n’a pas pu être écrit.");return;}
    [self.temporaryExports addObject:dir];
    UIActivityViewController *share=[[UIActivityViewController alloc]initWithActivityItems:@[file] applicationActivities:nil];
    if(share.popoverPresentationController){share.popoverPresentationController.sourceView=self.view;share.popoverPresentationController.sourceRect=CGRectMake(CGRectGetMidX(self.view.bounds),CGRectGetMidY(self.view.bounds),1,1);}
    __weak typeof(self) w=self;share.completionWithItemsHandler=^(UIActivityType activity,BOOL completed,NSArray *items,NSError *err){[[NSFileManager defaultManager]removeItemAtURL:dir error:nil];[w.temporaryExports removeObject:dir];};
    [self presentViewController:share animated:YES completion:nil];
}
- (void)cleanupOldExports{
    NSURL *tmp=[NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES];NSArray *urls=[[NSFileManager defaultManager]contentsOfDirectoryAtURL:tmp includingPropertiesForKeys:@[NSURLContentModificationDateKey] options:NSDirectoryEnumerationSkipsHiddenFiles error:nil];
    for(NSURL *url in urls){if(![url.lastPathComponent hasPrefix:@"PRIVKey-Export-"])continue;NSDate *date=nil;[url getResourceValue:&date forKey:NSURLContentModificationDateKey error:nil];if(date && [date timeIntervalSinceNow]<-86400)[[NSFileManager defaultManager]removeItemAtURL:url error:nil];}
}
- (void)help{
    PKMessage(self,@"PRIV Key · premier test",@"1. Commence avec l’exemple fictif pour tester l’aperçu et les exports.\n\n2. Pour un relevé, prends une photo de face, avec une règle dans le même plan. Place A et B puis indique leur distance réelle.\n\n3. Trace les points dans l’ordre. Ferme le contour et corrige les croisements signalés.\n\n4. Mesure l’épaisseur, applique les valeurs, puis vérifie la 3D.\n\n5. Exporte le STL, le SVG ou le relevé JSON.\n\nLa perspective n’est pas corrigée. La silhouette ne décrit pas les rainures latérales. Une copie fonctionnelle exige des mesures et une validation mécanique complémentaires.\n\nL’app fonctionne sans réseau. L’enregistrement conserve la photo localement ; les exports partagés dépendent de l’application destinataire.");
}
@end

@interface PKAppDelegate : UIResponder<UIApplicationDelegate>
@property(nonatomic,strong) UIWindow *window;
@property(nonatomic,strong) UIView *privacyCover;
@end
@implementation PKAppDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)options{
    self.window=[[UIWindow alloc]initWithFrame:UIScreen.mainScreen.bounds];self.window.overrideUserInterfaceStyle=UIUserInterfaceStyleDark;
    UINavigationController *nav=[[UINavigationController alloc]initWithRootViewController:[PKRootController new]];
    UINavigationBarAppearance *appearance=[UINavigationBarAppearance new];[appearance configureWithOpaqueBackground];appearance.backgroundColor=PKBackground();appearance.titleTextAttributes=@{NSForegroundColorAttributeName:UIColor.whiteColor};nav.navigationBar.standardAppearance=appearance;nav.navigationBar.scrollEdgeAppearance=appearance;nav.navigationBar.tintColor=PKAccent();
    self.window.rootViewController=nav;[self.window makeKeyAndVisible];return YES;
}
- (void)applicationWillResignActive:(UIApplication *)application{
    if(!self.privacyCover){self.privacyCover=[[UIView alloc]initWithFrame:self.window.bounds];self.privacyCover.autoresizingMask=UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;self.privacyCover.backgroundColor=PKBackground();UILabel *l=PKLabel(@"PRIV Key",28,YES);l.textAlignment=NSTextAlignmentCenter;l.frame=self.privacyCover.bounds;l.autoresizingMask=UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;[self.privacyCover addSubview:l];}
    [self.window addSubview:self.privacyCover];
}
- (void)applicationDidBecomeActive:(UIApplication *)application{[self.privacyCover removeFromSuperview];}
@end
