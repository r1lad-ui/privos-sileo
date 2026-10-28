/* PRIV Control 0.5.5 — passive device diagnostics and an iOS settings guide.
   LocalAuthentication is queried without initiating authentication.
   No passcode, fingerprint, security setting or authentication result is stored.
   Availability is refreshed on demand and whenever the app becomes active.

   API references:
   https://developer.apple.com/documentation/localauthentication/lacontext/canevaluatepolicy(_:error:)
   https://developer.apple.com/documentation/localauthentication/lacontext/biometrytype
   https://developer.apple.com/documentation/uikit/preparing-your-ui-to-run-in-the-background
*/

enum {PRIVCodeUnknown=0,PRIVCodeConfigured=1,PRIVCodeAbsent=2};
enum {PRIVBiometryUnknown=0,PRIVTouchIDReady=1,PRIVBiometryNotEnrolled=2,
      PRIVBiometryLockout=3,PRIVBiometryUnavailable=4};
typedef struct {long passcode,biometry,biometryType;} PRIVSecuritySnapshot;

static id g_securityRoot=0;
static id g_securityCodeLabel=0,g_securityBiometryLabel=0,g_securityOSLabel=0;
static id g_securityCover=0;
static void *g_securityLocalAuthentication=0;
static id g_securityLAErrorDomain=0;

static BOOL securityResponds(id object,const char *name){
 return object&&((BOOL(*)(id,SEL,SEL))objc_msgSend)(object,S("respondsToSelector:"),S(name));
}

/* Resolve Apple's domain constant rather than guessing its string value.
   If the symbol cannot be resolved, a negative result stays unknown. */
static void securityLoadLocalAuthentication(void){
 if(!g_securityLocalAuthentication)
  g_securityLocalAuthentication=dlopen("/System/Library/Frameworks/LocalAuthentication.framework/LocalAuthentication",2);
 if(g_securityLocalAuthentication&&!g_securityLAErrorDomain){
  id *symbol=(id*)dlsym(g_securityLocalAuthentication,"LAErrorDomain");
  if(symbol)g_securityLAErrorDomain=*symbol;
 }
}

static BOOL securityErrorMatches(id error,long expectedCode){
 if(!error||!g_securityLAErrorDomain)return 0;
 id domain=M0(error,"domain");
 if(!domain||!((BOOL(*)(id,SEL,id))objc_msgSend)(domain,S("isEqualToString:"),g_securityLAErrorDomain))return 0;
 return ((long(*)(id,SEL))objc_msgSend)(error,S("code"))==expectedCode;
}

static PRIVSecuritySnapshot securityReadSnapshot(void){
 PRIVSecuritySnapshot snapshot={PRIVCodeUnknown,PRIVBiometryUnknown,0};
 securityLoadLocalAuthentication();
 Class contextClass=C("LAContext");
 if(!contextClass)return snapshot;
 id context=M0(M0(contextClass,"alloc"),"init");
 if(!context)return snapshot;
 if(!securityResponds(context,"canEvaluatePolicy:error:")){V0(context,"release");return snapshot;}

 id codeError=0;
 BOOL codeAvailable=((BOOL(*)(id,SEL,long,id*))objc_msgSend)(context,S("canEvaluatePolicy:error:"),2,&codeError);
 if(codeAvailable)snapshot.passcode=PRIVCodeConfigured;
 else if(securityErrorMatches(codeError,-5))snapshot.passcode=PRIVCodeAbsent;

 id biometricError=0;
 BOOL biometricsAvailable=((BOOL(*)(id,SEL,long,id*))objc_msgSend)(context,S("canEvaluatePolicy:error:"),1,&biometricError);
 /* biometryType is meaningful only AFTER the biometric policy probe. */
 if(securityResponds(context,"biometryType"))
  snapshot.biometryType=((long(*)(id,SEL))objc_msgSend)(context,S("biometryType"));
 if(biometricsAvailable){
  if(snapshot.biometryType==1)snapshot.biometry=PRIVTouchIDReady;
 }else if(securityErrorMatches(biometricError,-7))snapshot.biometry=PRIVBiometryNotEnrolled;
 else if(securityErrorMatches(biometricError,-8))snapshot.biometry=PRIVBiometryLockout;
 else if(securityErrorMatches(biometricError,-6))snapshot.biometry=PRIVBiometryUnavailable;

 if(securityResponds(context,"invalidate"))V0(context,"invalidate");
 V0(context,"release");
 return snapshot;
}

static id securityLabel(id parent,const char *text,Rect frame,int font,BOOL heavy,id tint){
 id label=textLabel(parent,text,frame,font,heavy,tint);
 V0(label,"release");
 return label;
}

static id securityButton(id parent,const char *title,int tag,Rect frame,BOOL important){
 id button=mkButton(parent,title,tag,frame,important);
 V1(M0(button,"titleLabel"),"setFont:",MD(C("UIFont"),"boldSystemFontOfSize:",15));
 return button;
}

static void securityRefresh(void){
 if(!g_securityRoot)return;
 PRIVSecuritySnapshot snapshot=securityReadSnapshot();
 id systemVersion=M0(M0(C("UIDevice"),"currentDevice"),"systemVersion");
 char version[160];
 snprintf(version,sizeof(version),"Version déclarée par iOS : %s",length(systemVersion)?utf(systemVersion):"non disponible");
 setLabel(g_securityOSLabel,version);

 const char *codeText="État du code non vérifiable";
 id codeTint=muted;
 if(snapshot.passcode==PRIVCodeConfigured){codeText="Code iOS configuré";codeTint=green;}
 else if(snapshot.passcode==PRIVCodeAbsent){codeText="Aucun code iOS détecté";codeTint=red;}
 setLabel(g_securityCodeLabel,codeText);
 V1(g_securityCodeLabel,"setTextColor:",codeTint);

 const char *biometricText="État de la biométrie non vérifiable";
 id biometricTint=muted;
 if(snapshot.biometry==PRIVTouchIDReady){biometricText="Touch ID disponible pour PRIV";biometricTint=green;}
 else if(snapshot.biometry==PRIVBiometryNotEnrolled){
  biometricText=snapshot.biometryType==1?"Touch ID non configuré":"Biométrie non configurée";
 }else if(snapshot.biometry==PRIVBiometryLockout){
  biometricText=snapshot.biometryType==1?
   "Touch ID temporairement bloqué.\nDéverrouille l'iPhone avec son code.":
   "Biométrie temporairement bloquée.\nDéverrouille l'iPhone avec son code.";
 }else if(snapshot.biometry==PRIVBiometryUnavailable)biometricText="Biométrie indisponible pour PRIV";
 setLabel(g_securityBiometryLabel,biometricText);
 V1(g_securityBiometryLabel,"setTextColor:",biometricTint);
}

static void securityClose(void){
 if(!g_securityRoot)return;
 id previous=g_securityRoot;
 g_securityRoot=0;
 g_securityCodeLabel=0;g_securityBiometryLabel=0;g_securityOSLabel=0;
 V0(previous,"removeFromSuperview");
 V0(previous,"release");
 VB(g_scroll,"setHidden:",0);
}

static void securityOpenSettings(void){
 if(!launchBundle("com.apple.Preferences"))
  alert("Réglages iOS","Ouvre Réglages depuis l'écran d'accueil, puis suis les étapes de ce guide.");
}

static double securityGuideCard(id content,double width,double y,double height,const char *title,const char *body){
 id cardView=panel(content,R(14,y,width-28,height));
 securityLabel(cardView,title,R(16,14,width-60,29),18,1,white);
 securityLabel(cardView,body,R(16,52,width-60,height-66),15,0,white);
 V0(cardView,"release");
 return y+height+16;
}

static void securityOpen(void){
 if(g_securityRoot||g_wpRoot)return;
 endKeyboard();
 Rect screen=((Rect(*)(id,SEL))objc_msgSend)(M0(C("UIScreen"),"mainScreen"),S("bounds"));
 double w=screen.size.width,h=screen.size.height;
 if(w<300)w=375;
 if(h<400)h=667;
 g_securityRoot=view(C("UIView"),R(0,0,w,h),bg);
 VB(g_securityRoot,"setAccessibilityViewIsModal:",1);
 add(M0(g_ctrl,"view"),g_securityRoot);
 VB(g_scroll,"setHidden:",1);
 securityButton(g_securityRoot,"‹ Accueil",25,R(20,35,116,34),0);
 securityButton(g_securityRoot,"Actualiser",26,R(w-149,35,129,34),0);
 securityLabel(g_securityRoot,"Sécurité",R(20,80,w-40,39),30,1,white);
 securityLabel(g_securityRoot,"Vérifications et réglages recommandés.",R(20,123,w-40,23),14,0,muted);
 securityButton(g_securityRoot,"Ouvrir Réglages iOS",27,R(20,h-67,w-40,45),1);

 id scroll=view(C("UIScrollView"),R(0,154,w,h-238),bg);
 VB(scroll,"setAlwaysBounceVertical:",1);
 add(g_securityRoot,scroll);
 id content=view(C("UIView"),R(0,0,w,2000),bg);
 add(scroll,content);
 securityLabel(content,"Ces vérifications ne mesurent pas la force du code ni tous les réglages.",R(20,2,w-40,62),15,0,white);

 id diagnosis=panel(content,R(14,80,w-28,348));
 securityLabel(diagnosis,"CE QUE PRIV PEUT VÉRIFIER",R(16,13,w-60,24),16,1,white);
 g_securityOSLabel=securityLabel(diagnosis,"",R(16,43,w-60,44),15,0,white);
 securityLabel(diagnosis,"CODE DE L'IPHONE",R(16,96,w-60,19),13,1,muted);
 g_securityCodeLabel=securityLabel(diagnosis,"État du code non vérifiable",R(16,119,w-60,44),17,1,muted);
 securityLabel(diagnosis,"BIOMÉTRIE POUR PRIV",R(16,174,w-60,19),13,1,muted);
 g_securityBiometryLabel=securityLabel(diagnosis,"État de la biométrie non vérifiable",R(16,197,w-60,75),16,1,muted);
 securityLabel(diagnosis,"Disponibilité pour PRIV uniquement. Aucune authentification n'est lancée.",R(16,283,w-60,52),13,0,muted);
 V0(diagnosis,"release");

 securityLabel(content,"À régler dans iOS",R(20,448,w-40,29),21,1,white);
 securityLabel(content,"État inconnu pour PRIV : vérifie chacun de ces réglages dans Réglages.",R(20,481,w-40,62),14,0,muted);
 double y=558;
 y=securityGuideCard(content,w,y,236,"01  Code et Touch ID",
  "Réglages > Touch ID et code > Changer le code > Options de code.\n\nChoisis un code long ou alphanumérique. Garde uniquement tes empreintes. Si l'ancien code est connu, fais-le expirer quand iOS propose l'option.");
 y=securityGuideCard(content,w,y,176,"02  Verrouillage automatique",
  "Réglages > Luminosité et affichage > Verrouillage automatique.\n\nChoisis 30 secondes si cette option est proposée.");
 y=securityGuideCard(content,w,y,282,"03  Écran verrouillé",
  "Touch ID et code > Autoriser l'accès en mode verrouillé : limite le Centre de contrôle, Siri, les réponses et les widgets selon tes besoins.\n\nDésactive l'accès aux accessoires USB verrouillé si l'option est présente.\n\nNotifications > Afficher les aperçus > Si déverrouillé.");
 y=securityGuideCard(content,w,y,236,"04  En cas de vol",
  "Si l'option est présente : Touch ID et code > Protection en cas de vol de l'appareil. Choisis Toujours si proposé.\n\nCette protection nécessite notamment Localiser, l'identification à deux facteurs et Lieux importants.");
 y=securityGuideCard(content,w,y,254,"05  Prêter ton iPhone",
  "Réglages > Accessibilité > Accès guidé : choisis un code dédié.\n\nOuvre l'app à prêter, puis appuie trois fois sur le bouton principal pour démarrer Accès guidé. Il limite l'iPhone à cette app.\n\nPour les appels d'urgence, il faut quitter ce mode.");
 securityLabel(content,"Ces réglages se changent dans l'app Réglages. Le jailbreak conserve des risques même avec un code.",R(20,y+2,w-40,72),14,0,muted);
 double total=y+94;
 VR(content,"setFrame:",R(0,0,w,total));
 ((void(*)(id,SEL,Size))objc_msgSend)(scroll,S("setContentSize:"),(Size){w,total});
 V0(content,"release");
 V0(scroll,"release");
 securityRefresh();
}

/* Opaque, unanimated cover directly on the application window. It also covers
   presented app content, including a photo picker; this is not an iOS lock. */
static void securityShowCover(void){
 if(!g_window)return;
 Rect bounds=((Rect(*)(id,SEL))objc_msgSend)(g_window,S("bounds"));
 if(g_securityCover){
  VR(g_securityCover,"setFrame:",bounds);
  V1(g_window,"bringSubviewToFront:",g_securityCover);
  return;
 }
 g_securityCover=view(C("UIView"),bounds,bg);
 VB(g_securityCover,"setOpaque:",1);
 VB(g_securityCover,"setUserInteractionEnabled:",1);
 VB(g_securityCover,"setAccessibilityViewIsModal:",1);
 VI(g_securityCover,"setAutoresizingMask:",18);
 id title=securityLabel(g_securityCover,"PRIV",R(20,bounds.size.height/2-62,bounds.size.width-40,64),42,1,white);
 VI(title,"setTextAlignment:",1);
 id subtitle=securityLabel(g_securityCover,"Contenu masqué",R(20,bounds.size.height/2+9,bounds.size.width-40,32),18,0,muted);
 VI(subtitle,"setTextAlignment:",1);
 add(g_window,g_securityCover);
 V1(g_window,"bringSubviewToFront:",g_securityCover);
}

static void securityHideCover(void){
 if(!g_securityCover)return;
 id previous=g_securityCover;
 g_securityCover=0;
 V0(previous,"removeFromSuperview");
 V0(previous,"release");
}
