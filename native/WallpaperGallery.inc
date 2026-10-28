/* PRIV Control 0.5.4 — bundled wallpaper gallery.
   Included after the existing UIKit helpers and launchBundle().
   This module never reads Photos, private vault content or the DCIM directory.
   It exports only the two bundled assets after an explicit button press.
*/

typedef struct {
 const char *title;
 const char *filename;
 const char *description;
 const char *exportKey;
} PRIVWallpaper;

static const PRIVWallpaper g_wallpapers[2] = {
 {"PRIV Nuit", "PRIV-Wallpaper-Night.png",
  "Bleu nuit et lumière turquoise.\n\nPour l'écran verrouillé ou l'accueil.",
  "PRIVWallpaperExport.Night.1e5aebaa5f5c82827bf2a7d17432bdb43fd87b66175ec5919ea75d6637777568"},
 {"PRIV Classique", "PRIV-Wallpaper-Classic.png",
  "Le symbole PRIV sur un fond sombre.\n\nLe style d'origine du thème.",
  "PRIVWallpaperExport.Classic.3b69cfb3dcf5dfe551ac3bac83a6b804160684c26b6f9a90999d7b16520fc6e6"}
};

static id g_wpRoot=0;
static id g_wpSaveButtons[2]={0,0};
static id g_wpStatusLabels[2]={0,0};
static id g_wpNavigation[3]={0,0,0};
static BOOL g_wpBusy=0;
static long g_wpPendingIndex=-1;
static id g_wpPendingImage=0;
static void *g_wpPhotosLibrary=0;
static void *g_wpUIKitLibrary=0;

typedef void (*PRIVSaveWallpaperFn)(id,id,SEL,void*);
static PRIVSaveWallpaperFn g_wpSaveImage=0;

static BOOL wpValidIndex(long index){return index>=0&&index<2;}

/* PHAccessLevelAddOnly = 1. Querying authorization does not prompt the user. */
static long wpReadAuthorization(void){
 Class library=C("PHPhotoLibrary");
 if(!library){
  if(!g_wpPhotosLibrary)g_wpPhotosLibrary=dlopen("/System/Library/Frameworks/Photos.framework/Photos",2);
  library=C("PHPhotoLibrary");
 }
 SEL query=S("authorizationStatusForAccessLevel:");
 if(!library||!((BOOL(*)(id,SEL,SEL))objc_msgSend)(library,S("respondsToSelector:"),query))return -1;
 return ((long(*)(id,SEL,long))objc_msgSend)(library,query,1);
}

static BOOL wpWriterReady(void){
 if(g_wpSaveImage)return 1;
 if(!g_wpUIKitLibrary)g_wpUIKitLibrary=dlopen("/System/Library/Frameworks/UIKit.framework/UIKit",2);
 if(g_wpUIKitLibrary)g_wpSaveImage=(PRIVSaveWallpaperFn)dlsym(g_wpUIKitLibrary,"UIImageWriteToSavedPhotosAlbum");
 return g_wpSaveImage!=0;
}

static id wpImagePath(long index){
 if(!wpValidIndex(index))return 0;
 id folder=M1(M0(M0(C("NSBundle"),"mainBundle"),"bundlePath"),"stringByAppendingPathComponent:",MS("Wallpapers"));
 return M1(folder,"stringByAppendingPathComponent:",MS(g_wallpapers[index].filename));
}

static BOOL wpDidExport(long index){
 if(!wpValidIndex(index))return 0;
 return ((BOOL(*)(id,SEL,id))objc_msgSend)(M0(C("NSUserDefaults"),"standardUserDefaults"),S("boolForKey:"),MS(g_wallpapers[index].exportKey));
}

static void wpMarkExported(long index){
 if(!wpValidIndex(index))return;
 ((void(*)(id,SEL,BOOL,id))objc_msgSend)(M0(C("NSUserDefaults"),"standardUserDefaults"),S("setBool:forKey:"),1,MS(g_wallpapers[index].exportKey));
}

static void wpSetButtonTitle(id button,const char *title){
 if(button)((void(*)(id,SEL,id,unsigned long))objc_msgSend)(button,S("setTitle:forState:"),MS(title),0);
}

static void wpEnable(id button,BOOL enabled){
 if(!button)return;
 VB(button,"setEnabled:",enabled);
 ((void(*)(id,SEL,double))objc_msgSend)(button,S("setAlpha:"),enabled?1.0:0.55);
}

static void wpSetStatus(long index,const char *message,id tint){
 if(!g_wpRoot||!wpValidIndex(index)||!g_wpStatusLabels[index])return;
 setLabel(g_wpStatusLabels[index],message);
 V1(g_wpStatusLabels[index],"setTextColor:",tint);
}

static void wpRefreshControls(void){
 if(!g_wpRoot)return;
 for(long i=0;i<2;i++){
  const char *title=wpDidExport(i)?"Enregistrer à nouveau":"Enregistrer dans Photos";
  if(g_wpBusy)title=i==g_wpPendingIndex?"Enregistrement…":"Patiente…";
  wpSetButtonTitle(g_wpSaveButtons[i],title);
  wpEnable(g_wpSaveButtons[i],!g_wpBusy);
 }
 for(int i=0;i<3;i++)wpEnable(g_wpNavigation[i],!g_wpBusy);
}

/* This selector is the only completion path that changes export markers or UI. */
static void wpSaveFinished(id self,SEL cmd,id result){
 (void)self;(void)cmd;
 long index=((long(*)(id,SEL))objc_msgSend)(M1(result,"objectForKey:",MS("index")),S("integerValue"));
 if(!g_wpBusy||!wpValidIndex(index)||index!=g_wpPendingIndex){V0(result,"release");return;}
 id errorMessage=M1(result,"objectForKey:",MS("error"));
 g_wpBusy=0;
 g_wpPendingIndex=-1;
 if(g_wpPendingImage){V0(g_wpPendingImage,"release");g_wpPendingImage=0;}
 if(!errorMessage){
  wpMarkExported(index);
  wpSetStatus(index,"Enregistré dans Photos. Ouvre Photos pour l'appliquer.",green);
 }else{
  long authorization=wpReadAuthorization();
  if(authorization==1)wpSetStatus(index,"Les restrictions iOS empêchent l'ajout à Photos.",red);
  else if(authorization==2)wpSetStatus(index,"Accès refusé. Autorise l'ajout dans Réglages, puis réessaie.",red);
  else wpSetStatus(index,"L'enregistrement a échoué. Réessaie ou vérifie l'espace disponible.",red);
 }
 wpRefreshControls();
 V0(result,"release");
}

/* UIKit calls this selector when the write has completed, with an NSError on failure.
   The retained result survives the dispatch; wpSaveFinished releases our ownership. */
static void wpSaveCompleted(id self,SEL cmd,id image,id error,void *context){
 (void)cmd;(void)image;
 long index=-1;
 for(long i=0;i<2;i++)if(context==(void*)&g_wallpapers[i])index=i;
 if(!wpValidIndex(index))return;
 id pool=M0(M0(C("NSAutoreleasePool"),"alloc"),"init");
 id result=M0(M0(C("NSMutableDictionary"),"alloc"),"init");
 id number=((id(*)(id,SEL,long))objc_msgSend)(C("NSNumber"),S("numberWithLong:"),index);
 V2(result,"setObject:forKey:",number,MS("index"));
 if(error){
  id description=M0(error,"localizedDescription");
  V2(result,"setObject:forKey:",description?description:MS("Enregistrement impossible."),MS("error"));
 }
 ((void(*)(id,SEL,SEL,id,BOOL))objc_msgSend)(self,S("performSelectorOnMainThread:withObject:waitUntilDone:"),S("wallpaperSaveFinished:"),result,0);
 V0(pool,"drain");
}

static void wpSave(long index){
 if(g_wpBusy||!wpValidIndex(index)||!g_wpRoot)return;
 long authorization=wpReadAuthorization();
 if(authorization==1){wpSetStatus(index,"Les restrictions iOS empêchent l'ajout à Photos.",red);return;}
 if(authorization==2){wpSetStatus(index,"Accès refusé. Autorise l'ajout dans Réglages, puis réessaie.",red);return;}
 if(authorization!=0&&authorization!=3&&authorization!=4){wpSetStatus(index,"Photos n'est pas disponible sur cet appareil.",red);return;}
 if(!wpWriterReady()){wpSetStatus(index,"L'enregistrement dans Photos n'est pas disponible.",red);return;}
 id image=M1(C("UIImage"),"imageWithContentsOfFile:",wpImagePath(index));
 if(!image){wpSetStatus(index,"Le fond d'écran manque dans cette installation.",red);return;}
 g_wpBusy=1;
 g_wpPendingIndex=index;
 g_wpPendingImage=M0(image,"retain");
 wpRefreshControls();
 wpSetStatus(index,"Enregistrement dans Photos…",muted);
 /* With status 0, UIKit asks for add-only permission here, after this tap. */
 g_wpSaveImage(image,g_ctrl,S("wallpaperImage:didFinishSavingWithError:contextInfo:"),(void*)&g_wallpapers[index]);
}

/* New labels relinquish their alloc ownership after the parent view retains them. */
static id wpLabel(id parent,const char *text,Rect frame,int size,BOOL heavy,id tint){
 id label=textLabel(parent,text,frame,size,heavy,tint);
 V0(label,"release");
 return label;
}

static id wpButton(id parent,const char *title,int tag,Rect frame,BOOL important){
 id button=mkButton(parent,title,tag,frame,important);
 V1(M0(button,"titleLabel"),"setFont:",MD(C("UIFont"),"boldSystemFontOfSize:",15));
 return button;
}

static void wpCloseGallery(void){
 if(g_wpBusy||!g_wpRoot)return;
 id oldRoot=g_wpRoot;
 g_wpRoot=0;
 for(int i=0;i<2;i++){g_wpSaveButtons[i]=0;g_wpStatusLabels[i]=0;}
 for(int i=0;i<3;i++)g_wpNavigation[i]=0;
 V0(oldRoot,"removeFromSuperview");
 V0(oldRoot,"release");
 VB(g_scroll,"setHidden:",0);
}

static void wpOpenPhotos(void){
 if(g_wpBusy)return;
 if(!launchBundle("com.apple.mobileslideshow"))alert("Photos","Ouvre Photos depuis l'écran d'accueil, puis choisis le fond enregistré.");
}

static void wpOpenSettings(void){
 if(g_wpBusy)return;
 /* UIApplicationOpenSettingsURLString is the public app-settings destination. */
 if(!g_wpUIKitLibrary)g_wpUIKitLibrary=dlopen("/System/Library/Frameworks/UIKit.framework/UIKit",2);
 id *settingsURL=g_wpUIKitLibrary?(id*)dlsym(g_wpUIKitLibrary,"UIApplicationOpenSettingsURLString"):0;
 id application=M0(C("UIApplication"),"sharedApplication");
 SEL open=S("openURL:options:completionHandler:");
 if(settingsURL&&*settingsURL&&((BOOL(*)(id,SEL,SEL))objc_msgSend)(application,S("respondsToSelector:"),open)){
  id url=M1(C("NSURL"),"URLWithString:",*settingsURL);
  if(url){((void(*)(id,SEL,id,id,void*))objc_msgSend)(application,open,url,M0(C("NSDictionary"),"dictionary"),0);return;}
 }
 if(!launchBundle("com.apple.Preferences"))alert("Réglages","Ouvre Réglages depuis l'écran d'accueil, puis PRIV Control > Photos.");
}

static void wpOpenGallery(void){
 if(g_wpRoot)return;
 endKeyboard();
 Rect screen=((Rect(*)(id,SEL))objc_msgSend)(M0(C("UIScreen"),"mainScreen"),S("bounds"));
 double w=screen.size.width,h=screen.size.height;
 if(w<300)w=375;
 if(h<400)h=667;
 g_wpRoot=view(C("UIView"),R(0,0,w,h),bg);
 VB(g_wpRoot,"setAccessibilityViewIsModal:",1);
 add(M0(g_ctrl,"view"),g_wpRoot);
 VB(g_scroll,"setHidden:",1);
 g_wpNavigation[0]=wpButton(g_wpRoot,"‹ Accueil",19,R(20,35,116,34),0);
 wpLabel(g_wpRoot,"Fonds d'écran",R(20,80,w-40,39),30,1,white);
 wpLabel(g_wpRoot,"Fonds locaux, prêts pour ton iPhone.",R(20,123,w-40,23),14,0,muted);
 id scroll=view(C("UIScrollView"),R(0,154,w,h-154),bg);
 VB(scroll,"setAlwaysBounceVertical:",1);
 add(g_wpRoot,scroll);
 const double total=1040;
 id content=view(C("UIView"),R(0,0,w,total),bg);
 add(scroll,content);
 ((void(*)(id,SEL,Size))objc_msgSend)(scroll,S("setContentSize:"),(Size){w,total});
 wpLabel(content,"Enregistre un fond dans Photos, puis choisis-le comme fond d'écran.",R(20,2,w-40,60),15,0,white);
 for(long i=0;i<2;i++){
  id cardView=panel(content,R(14,80+i*400,w-28,382));
  wpLabel(cardView,g_wallpapers[i].title,R(16,12,w-60,29),21,1,white);
  id image=M1(C("UIImage"),"imageWithContentsOfFile:",wpImagePath(i));
  id preview=view(C("UIImageView"),R(16,53,120,213),soft);
  VI(preview,"setContentMode:",1);
  borderRadius(preview,10);
  V1(preview,"setImage:",image);
  VB(preview,"setIsAccessibilityElement:",1);
  V1(preview,"setAccessibilityLabel:",MS(g_wallpapers[i].title));
  add(cardView,preview);
  V0(preview,"release");
  wpLabel(cardView,g_wallpapers[i].description,R(152,62,w-198,155),15,0,white);
  wpLabel(cardView,"Format iPhone SE",R(152,230,w-198,29),12,1,muted);
  g_wpStatusLabels[i]=wpLabel(cardView,"",R(16,273,w-60,44),13,0,green);
  g_wpSaveButtons[i]=wpButton(cardView,"Enregistrer dans Photos",20+(int)i,R(16,327,w-60,43),1);
  if(!image)wpSetStatus(i,"Aperçu indisponible : le fichier du fond est absent.",red);
  else if(wpDidExport(i))wpSetStatus(i,"Déjà exporté. Tu peux enregistrer une nouvelle copie.",green);
  else wpSetStatus(i,"Prêt à enregistrer dans Photos.",muted);
  V0(cardView,"release");
 }
 wpLabel(content,"Dans Photos : ouvre l'image, touche Partager, puis Utiliser en fond d'écran.",R(20,884,w-40,64),15,0,white);
 double buttonWidth=(w-48)/2;
 g_wpNavigation[1]=wpButton(content,"Ouvrir Photos",22,R(18,962,buttonWidth,45),1);
 g_wpNavigation[2]=wpButton(content,"Réglages iOS",23,R(30+buttonWidth,962,buttonWidth,45),0);
 V0(content,"release");
 V0(scroll,"release");
 wpRefreshControls();
}
