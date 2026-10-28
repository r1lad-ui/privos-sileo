/* PRIV Control 0.5.4 — UIKit app for iOS rootless jailbreak (laboratory build).
   No SpringBoard injection. Vault encrypted using Apple's CommonCrypto
   PBKDF2-HMAC-SHA256 + AES-256-CBC + encrypt-then-HMAC-SHA256.
   Compatible with files from THIS version only. NOT a hardened OS.
*/
#include <stddef.h>
typedef unsigned char uint8_t;
extern void *malloc(size_t); extern void free(void*); extern void *memcpy(void*,const void*,size_t);
extern void *memset(void*,int,size_t); extern size_t strlen(const char*);
extern int snprintf(char*,size_t,const char*,...);
extern void *dlopen(const char*,int); extern void *dlsym(void*,const char*);
typedef void *id; typedef void *Class; typedef void *SEL;
typedef unsigned char BOOL; typedef double CGFloat;
typedef struct{CGFloat x,y;} Point; typedef struct{CGFloat width,height;} Size;
typedef struct{Point origin;Size size;} Rect;
extern Class objc_getClass(const char*); extern Class objc_allocateClassPair(Class,const char*,size_t);
extern void objc_registerClassPair(Class); extern BOOL class_addMethod(Class,SEL,void*,const char*);
extern SEL sel_registerName(const char*); extern void *objc_msgSend(void);
extern void *objc_msgSendSuper(void); extern int UIApplicationMain(int,char**,id,id);
extern int SecRandomCopyBytes(void*,size_t,void*);
extern id UIImageJPEGRepresentation(id,CGFloat);
static SEL S(const char*n){return sel_registerName(n);}static Class C(const char*n){return objc_getClass(n);}
static id M0(id x,const char*n){return ((id(*)(id,SEL))objc_msgSend)(x,S(n));}
static id M1(id x,const char*n,id a){return ((id(*)(id,SEL,id))objc_msgSend)(x,S(n),a);}
static void V0(id x,const char*n){((void(*)(id,SEL))objc_msgSend)(x,S(n));}
static void V1(id x,const char*n,id a){((void(*)(id,SEL,id))objc_msgSend)(x,S(n),a);}
static void V2(id x,const char*n,id a,id b){((void(*)(id,SEL,id,id))objc_msgSend)(x,S(n),a,b);}
static void VI(id x,const char*n,long v){((void(*)(id,SEL,long))objc_msgSend)(x,S(n),v);}
static void VB(id x,const char*n,BOOL v){((void(*)(id,SEL,BOOL))objc_msgSend)(x,S(n),v);}
static void VR(id x,const char*n,Rect r){((void(*)(id,SEL,Rect))objc_msgSend)(x,S(n),r);}
static id MR(id x,const char*n,Rect r){return ((id(*)(id,SEL,Rect))objc_msgSend)(x,S(n),r);}
static id MD(id x,const char*n,double d){return ((id(*)(id,SEL,double))objc_msgSend)(x,S(n),d);}
static id MS(const char*utf){return ((id(*)(id,SEL,const char*))objc_msgSend)(C("NSString"),S("stringWithUTF8String:"),utf);}
static Rect R(double x,double y,double w,double h){Rect r={{x,y},{w,h}};return r;}
static id strAdd(id a,id b){return M1(a,"stringByAppendingString:",b);}
static const char* utf(id x){if(!x)return "";return ((const char*(*)(id,SEL))objc_msgSend)(x,S("UTF8String"));}
static unsigned long length(id x){if(!x)return 0;return ((unsigned long(*)(id,SEL))objc_msgSend)(x,S("length"));}
static const uint8_t* bytes(id x){return ((const uint8_t*(*)(id,SEL))objc_msgSend)(x,S("bytes"));}
static id data(const void*p,size_t sz){return ((id(*)(id,SEL,const void*,unsigned long))objc_msgSend)(C("NSData"),S("dataWithBytes:length:"),p,(unsigned long)sz);}
static id bg,card,soft,green,white,muted,red;
static id g_window,g_ctrl,g_scroll,g_passwordField,g_noteField,g_status,g_generated,g_photoView,g_photoStatus;
static id g_vault=0;static char g_pass[256];static BOOL g_unlocked=0;static char g_pwd[33];static BOOL g_picker=0;
static void wipe(void*p,size_t n){volatile unsigned char*b=p;while(n--)*b++=0;}
static id color(double r,double g,double b){return ((id(*)(id,SEL,double,double,double,double))objc_msgSend)(C("UIColor"),S("colorWithRed:green:blue:alpha:"),r,g,b,1.0);}
static void background(id v,id c){V1(v,"setBackgroundColor:",c);}
static void borderRadius(id v,double d){id layer=M0(v,"layer");((void(*)(id,SEL,double))objc_msgSend)(layer,S("setCornerRadius:"),d);VB(layer,"setMasksToBounds:",1);}
static void add(id p,id c){V1(p,"addSubview:",c);}
static id view(id cls,Rect f,id col){id x=MR(M0(cls,"alloc"),"initWithFrame:",f);if(col)background(x,col);return x;}
static id textLabel(id p,const char*t,Rect r,int font,BOOL heavy,id col){id x=view(C("UILabel"),r,0);V1(x,"setText:",MS(t));V1(x,"setTextColor:",col);V1(x,"setFont:",MD(C("UIFont"),heavy?"boldSystemFontOfSize:":"systemFontOfSize:",(double)font));VI(x,"setNumberOfLines:",0);add(p,x);return x;}
static void setLabel(id l,const char*s){V1(l,"setText:",MS(s));}
static id mkButton(id p,const char*t,int tag,Rect r,BOOL important){id b=((id(*)(id,SEL,long))objc_msgSend)(C("UIButton"),S("buttonWithType:"),0);VR(b,"setFrame:",r);background(b,important?green:soft);borderRadius(b,12);((void(*)(id,SEL,id,unsigned long))objc_msgSend)(b,S("setTitle:forState:"),MS(t),0);((void(*)(id,SEL,id,unsigned long))objc_msgSend)(b,S("setTitleColor:forState:"),important?bg:white,0);V1(M0(b,"titleLabel"),"setFont:",MD(C("UIFont"),"boldSystemFontOfSize:",13));VI(b,"setTag:",tag);((void(*)(id,SEL,id,SEL,unsigned long))objc_msgSend)(b,S("addTarget:action:forControlEvents:"),g_ctrl,S("press:"),64);add(p,b);return b;}
static id panel(id p,Rect f){id x=view(C("UIView"),f,card);borderRadius(x,17);add(p,x);return x;}
static void endKeyboard(void){if(g_ctrl)VB(M0(g_ctrl,"view"),"endEditing:",1);}
static void alert(const char*title,const char*message){id a=((id(*)(id,SEL,id,id,long))objc_msgSend)(C("UIAlertController"),S("alertControllerWithTitle:message:preferredStyle:"),MS(title),MS(message),1);id ok=((id(*)(id,SEL,id,long,void*))objc_msgSend)(C("UIAlertAction"),S("actionWithTitle:style:handler:"),MS("OK"),0,0);V1(a,"addAction:",ok);((void(*)(id,SEL,id,BOOL,void*))objc_msgSend)(g_ctrl,S("presentViewController:animated:completion:"),a,1,0);}
static id vaultFile(void){id path=strAdd(MS("/var/mobile/Library/PRIVControl"),MS("/vault.pv4"));return path;}
static BOOL vaultExists(void){return ((BOOL(*)(id,SEL,id))objc_msgSend)(M0(C("NSFileManager"),"defaultManager"),S("fileExistsAtPath:"),vaultFile());}
static BOOL cryptoReady=0;
typedef int(*pbkdfFn)(int,const char*,size_t,const uint8_t*,size_t,int,unsigned int,uint8_t*,size_t);
typedef int(*cryptFn)(int,int,int,const void*,size_t,const void*,const void*,size_t,void*,size_t,size_t*);
typedef void(*hmacFn)(int,const void*,size_t,const void*,size_t,void*);
static pbkdfFn fnPBKDF=0;static cryptFn fnCrypt=0;static hmacFn fnHMAC=0;
static BOOL initCrypto(void){if(cryptoReady)return 1;void*lib=dlopen("/usr/lib/system/libcommonCrypto.dylib",2);if(!lib)lib=dlopen("/usr/lib/libcommonCrypto.dylib",2);if(!lib)return 0;fnPBKDF=(pbkdfFn)dlsym(lib,"CCKeyDerivationPBKDF");fnCrypt=(cryptFn)dlsym(lib,"CCCrypt");fnHMAC=(hmacFn)dlsym(lib,"CCHmac");cryptoReady=fnPBKDF&&fnCrypt&&fnHMAC;return cryptoReady;}
static BOOL derive(const char*password,const uint8_t salt[16],uint8_t key[64]){if(!initCrypto())return 0;return fnPBKDF(2,password,strlen(password),salt,16,3,180000,key,64)==0;}
/* PRIV4.1 file layout: 8-byte magic, 16-byte salt, 16-byte IV, 32-byte HMAC,
   AES-256-CBC+PKCS7 ciphertext. HMAC-SHA256 authenticates magic|salt|IV|ciphertext.
   Always verify HMAC before decrypting. Never disclose wrong-key vs tampering. */
static BOOL encryptFile(id json,const char*password){
 if(!json||!password||!password[0]||length(json)>8*1024*1024)return 0;
 uint8_t salt[16],iv[16],keys[64],mac[32];size_t plen=length(json);
 if(SecRandomCopyBytes(0,16,salt)||SecRandomCopyBytes(0,16,iv))return 0;
 if(!derive(password,salt,keys))return 0;
 size_t cap=plen+32,outlen=0;uint8_t*ct=malloc(cap);if(!ct){wipe(keys,64);return 0;}
 int rc=fnCrypt(0,0,1,keys,32,iv,bytes(json),plen,ct,cap,&outlen);
 if(rc!=0||outlen==0){wipe(keys,64);wipe(ct,cap);free(ct);return 0;}
 size_t alen=40+outlen;uint8_t*auth=malloc(alen);if(!auth){wipe(keys,64);wipe(ct,cap);free(ct);return 0;}
 memcpy(auth,"PRIV4.1!",8);memcpy(auth+8,salt,16);memcpy(auth+24,iv,16);memcpy(auth+40,ct,outlen);
 fnHMAC(2,keys+32,32,auth,alen,mac);
 size_t filelen=72+outlen;uint8_t*file=malloc(filelen);
 BOOL ok=0;
 if(file){memcpy(file,auth,40);memcpy(file+40,mac,32);memcpy(file+72,ct,outlen);
   id manager=M0(C("NSFileManager"),"defaultManager");
   id folder=MS("/var/mobile/Library/PRIVControl");
   ((BOOL(*)(id,SEL,id,BOOL,id,id*))objc_msgSend)(manager,S("createDirectoryAtPath:withIntermediateDirectories:attributes:error:"),folder,1,0,0);
   id d=data(file,filelen);
   ok=((BOOL(*)(id,SEL,id,unsigned long,id*))objc_msgSend)(d,S("writeToFile:options:error:"),vaultFile(),1,0);
   if(ok){id at=M0(C("NSMutableDictionary"),"dictionary");V2(at,"setObject:forKey:",MS("NSFileProtectionComplete"),MS("NSFileProtectionKey"));
    ((BOOL(*)(id,SEL,id,id,id*))objc_msgSend)(manager,S("setAttributes:ofItemAtPath:error:"),at,vaultFile(),0);}
   wipe(file,filelen);free(file);
 }
 wipe(auth,alen);free(auth);wipe(ct,cap);free(ct);wipe(keys,64);wipe(mac,32);return ok;
}
static id decryptFile(const char*password){
 id d=((id(*)(id,SEL,id))objc_msgSend)(C("NSData"),S("dataWithContentsOfFile:"),vaultFile());
 if(!d)return 0;size_t n=length(d);if(n<88||n>9*1024*1024)return 0;
 const uint8_t*p=bytes(d);const char magic[]="PRIV4.1!";for(int i=0;i<8;i++)if(p[i]!=(uint8_t)magic[i])return 0;
 size_t clen=n-72;if(clen<16||(clen&15))return 0;
 uint8_t keys[64],mac[32];if(!derive(password,p+8,keys))return 0;
 size_t alen=40+clen;uint8_t*auth=malloc(alen);if(!auth){wipe(keys,64);return 0;}
 memcpy(auth,p,40);memcpy(auth+40,p+72,clen);fnHMAC(2,keys+32,32,auth,alen,mac);
 unsigned char diff=0;for(int i=0;i<32;i++)diff|=mac[i]^p[40+i];
 wipe(mac,32);wipe(auth,alen);free(auth);if(diff){wipe(keys,64);return 0;}
 uint8_t*plain=malloc(clen+1);if(!plain){wipe(keys,64);return 0;}
 size_t outlen=0;int rc=fnCrypt(1,0,1,keys,32,p+24,p+72,clen,plain,clen+1,&outlen);
 wipe(keys,64);id result=0;
 if(rc==0&&outlen>0&&outlen<=8*1024*1024){id body=data(plain,outlen);
  result=((id(*)(id,SEL,id,unsigned long,id*))objc_msgSend)(C("NSJSONSerialization"),S("JSONObjectWithData:options:error:"),body,1,0);
  if(result&& !((BOOL(*)(id,SEL,id))objc_msgSend)(result,S("isKindOfClass:"),C("NSDictionary")))result=0;
 }
 wipe(plain,clen+1);free(plain);return result;
}
static BOOL saveVault(void){if(!g_unlocked||!g_vault||!g_pass[0])return 0;
 id json=((id(*)(id,SEL,id,unsigned long,id*))objc_msgSend)(C("NSJSONSerialization"),S("dataWithJSONObject:options:error:"),g_vault,0,0);
 return encryptFile(json,g_pass);
}
static void refreshPhoto(void){id base=M1(g_vault,"objectForKey:",MS("photo"));if(!g_unlocked||!base||!length(base)){V1(g_photoView,"setImage:",0);setLabel(g_photoStatus,"Aucune photo dans le coffre.");return;}
 id pd=((id(*)(id,SEL,id,unsigned long))objc_msgSend)(M0(C("NSData"),"alloc"),S("initWithBase64EncodedString:options:"),base,0);
 id im=M1(C("UIImage"),"imageWithData:",pd);V1(g_photoView,"setImage:",im);setLabel(g_photoStatus,"Photo chargee depuis le coffre chiffre.");V0(pd,"release");}
static void updateLocked(void){
 if(g_status)setLabel(g_status,g_unlocked?"COFFRE OUVERT  /  session temporaire":"COFFRE VERROUILLE  /  mot de passe requis");
 if(g_noteField)VB(g_noteField,"setEditable:",g_unlocked);
 if(g_noteField&&!g_unlocked)V1(g_noteField,"setText:",MS(""));
 if(g_photoView&&!g_unlocked)V1(g_photoView,"setImage:",0);
 if(g_photoStatus&&!g_unlocked)setLabel(g_photoStatus,"Ouvre le coffre pour voir les photos.");
}
static void lockVault(void){if(g_vault){V0(g_vault,"release");g_vault=0;}g_unlocked=0;wipe(g_pass,sizeof(g_pass));if(g_passwordField)V1(g_passwordField,"setText:",MS(""));updateLocked();}
static BOOL inputPass(char out[256]){const char*p=utf(M0(g_passwordField,"text"));size_t n=strlen(p);if(n<10||n>200)return 0;memcpy(out,p,n+1);return 1;}
static void handleUnlock(void){char p[256];wipe(p,256);if(!inputPass(p)){alert("Mot de passe requis","Utilise une phrase de passe d'au moins 10 caracteres.");return;}
 if(!initCrypto()){wipe(p,256);alert("Chiffrement indisponible","Le moteur CommonCrypto n'est pas accessible. Aucune donnee n'a ete enregistree.");return;}
 id dic=0;if(vaultExists()){dic=decryptFile(p);if(!dic){wipe(p,256);alert("Ouverture impossible","Mot de passe incorrect, fichier endommage ou indisponible. Aucune donnee n'a ete modifiee.");return;}}
 if(g_vault)V0(g_vault,"release");g_vault=dic?M0(dic,"mutableCopy"):M0(M0(C("NSMutableDictionary"),"alloc"),"init");
 if(!dic){V2(g_vault,"setObject:forKey:",MS(""),MS("note"));}
 memcpy(g_pass,p,strlen(p)+1);wipe(p,256);g_unlocked=1;
 id note=M1(g_vault,"objectForKey:",MS("note"));V1(g_noteField,"setText:",note?note:MS(""));V1(g_passwordField,"setText:",MS(""));updateLocked();refreshPhoto();
 if(!dic){if(!saveVault()){lockVault();alert("Creation impossible","Le coffre n'a pas pu etre enregistre. Verifie le stockage de l'iPhone.");return;}alert("Coffre cree","Mot de passe requis a chaque ouverture. Conserve-le : il n'existe pas de recuperation automatique.");}
}
static void handleSave(void){if(!g_unlocked){alert("Coffre verrouille","Ouvre d'abord le coffre.");return;}
 id note=M0(g_noteField,"text");if(length(note)>50000){alert("Note trop longue","Maximum 50 000 caracteres.");return;}
 V2(g_vault,"setObject:forKey:",note?note:MS(""),MS("note"));
 if(saveVault())alert("Enregistre","La note est chiffree sur cet iPhone.");else alert("Erreur","La sauvegarde a echoue. Ne ferme pas l'application avant de copier ton texte ailleurs.");}
static void handleDeletePhoto(void){if(!g_unlocked){alert("Coffre verrouille","Ouvre d'abord le coffre.");return;}
 V1(g_vault,"removeObjectForKey:",MS("photo"));if(saveVault()){refreshPhoto();alert("Photo retiree","La photo a ete retiree du coffre. L'original dans Photos n'est pas supprime.");}else alert("Erreur","Impossible d'enregistrer la suppression.");}
static void handlePicker(void){if(!g_unlocked){alert("Coffre verrouille","Ouvre d'abord le coffre.");return;}
 if(!((BOOL(*)(id,SEL,long))objc_msgSend)(C("UIImagePickerController"),S("isSourceTypeAvailable:"),0)){alert("Indisponible","La phototheque n'est pas accessible.");return;}
 id pick=M0(M0(C("UIImagePickerController"),"alloc"),"init");VI(pick,"setSourceType:",0);V1(pick,"setDelegate:",g_ctrl);g_picker=1;((void(*)(id,SEL,id,BOOL,void*))objc_msgSend)(g_ctrl,S("presentViewController:animated:completion:"),pick,1,0);V0(pick,"release");}
static void pickerCancel(id self,SEL cmd,id picker){(void)cmd;(void)self;g_picker=0;((void(*)(id,SEL,BOOL,void*))objc_msgSend)(picker,S("dismissViewControllerAnimated:completion:"),1,0);}
static void pickerSelected(id self,SEL cmd,id picker,id info){(void)self;(void)cmd;g_picker=0;
 id img=M1(info,"objectForKey:",MS("UIImagePickerControllerOriginalImage"));
 if(!img)img=M1(info,"objectForKey:",MS("UIImagePickerControllerEditedImage"));
 BOOL saved=0,tooBig=0;
 if(g_unlocked&&img){id jpg=UIImageJPEGRepresentation(img,0.70);if(jpg){if(length(jpg)>5*1024*1024)tooBig=1;
 else{ id b64=((id(*)(id,SEL,unsigned long))objc_msgSend)(jpg,S("base64EncodedStringWithOptions:"),0);
 if(b64){id old=M1(g_vault,"objectForKey:",MS("photo"));if(old)V0(old,"retain");
 V2(g_vault,"setObject:forKey:",b64,MS("photo"));saved=saveVault();if(!saved){if(old)V2(g_vault,"setObject:forKey:",old,MS("photo"));else V1(g_vault,"removeObjectForKey:",MS("photo"));}if(old)V0(old,"release");}
 }}}
 ((void(*)(id,SEL,BOOL,void*))objc_msgSend)(picker,S("dismissViewControllerAnimated:completion:"),1,0);
 if(saved){refreshPhoto();alert("Photo chiffree","La photo selectionnee est enregistree dans le coffre. L'original reste dans Photos.");}
 else if(tooBig)alert("Image trop grande","Choisis une photo de moins de 5 Mo apres compression.");
 else alert("Import impossible","La photo n'a pas pu etre chiffree et enregistree.");
}
static void generate(void){const char chars[]="ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789-!@#$%*?";
 const unsigned int n=(unsigned int)sizeof(chars)-1;const unsigned int lim=256-256%n;
 uint8_t rnd;for(int i=0;i<28;i++){do{if(SecRandomCopyBytes(0,1,&rnd)){alert("Erreur","Source aleatoire indisponible.");return;}}while(rnd>=lim);g_pwd[i]=chars[rnd%n];}g_pwd[28]=0;
 V1(g_generated,"setText:",MS(g_pwd));}
static void copyGenerated(void){if(!g_pwd[0]){alert("Rien a copier","Genere d'abord un mot de passe.");return;}
 V1(M0(C("UIPasteboard"),"generalPasteboard"),"setString:",MS(g_pwd));alert("Copie","Mot de passe copie. Le presse-papiers iOS peut etre accessible a d'autres applications ; colle-le puis efface-le.");}
static void clearPaste(void){V1(M0(C("UIPasteboard"),"generalPasteboard"),"setString:",MS(""));alert("Efface","Le contenu texte du presse-papiers a ete remplace par une chaine vide.");}

static void scrollTo(double y){ if(!g_scroll)return; ((void(*)(id,SEL,Point,BOOL))objc_msgSend)(g_scroll,S("setContentOffset:animated:"),(Point){0,y},1); }
static BOOL launchBundle(const char *bundle){
 Class w=C("LSApplicationWorkspace"); if(!w)return 0;
 id work=M0(w,"defaultWorkspace");if(!work)return 0;
 SEL sel=S("openApplicationWithBundleID:");
 if(!((BOOL(*)(id,SEL,SEL))objc_msgSend)(work,S("respondsToSelector:"),sel))return 0;
 return ((BOOL(*)(id,SEL,id))objc_msgSend)(work,sel,MS(bundle));
}

#include "WallpaperGallery.inc"

static void press(id self,SEL cmd,id sender){(void)self;(void)cmd;long tag=((long(*)(id,SEL))objc_msgSend)(sender,S("tag"));endKeyboard();
 switch(tag){case 1:handleUnlock();break;case 2:lockVault();break;case 3:handleSave();break;case 4:handlePicker();break;case 5:handleDeletePhoto();break;
 case 6:generate();break;case 7:copyGenerated();break;case 8:clearPaste();break;
 case 9:alert("A propos","PRIV OS 0.5.4 est une application experimentale. Elle chiffre les donnees qu'elle enregistre, mais ne securise pas iOS jailbreaké et ne coupe pas camera/micro ni VPN.");break;
 case 10:if(g_unlocked){V1(g_noteField,"setText:",MS(""));handleSave();}else alert("Coffre verrouille","Ouvre le coffre.");break;
 case 11:scrollTo(650);break; /* notes */
 case 12:scrollTo(1005);break; /* photos */
 case 13:scrollTo(1315);break; /* generated */
 case 14:V1(M0(C("UIPasteboard"),"generalPasteboard"),"setString:",MS(""));alert("Presse-papiers","Le texte du presse-papiers a ete efface. Les autres types de contenu et historiques tiers ne sont pas garantis effaces.");break;
 case 15:if(!launchBundle("org.whispersystems.signal"))alert("Signal absent","Signal ne semble pas installe, ou iOS refuse son ouverture. Aucune installation automatique n'a ete effectuee.");break;
 case 16:if(!launchBundle("net.mullvad.MullvadVPN"))alert("VPN non configure","Le client Mullvad n'a pas pu etre ouvert. PRIV ne cree pas de connexion VPN et n'affiche pas de faux statut de protection.");break;
 case 17:if(!launchBundle("com.apple.Preferences"))alert("Reglages","Impossible d'ouvrir les reglages. Ouvre-les depuis l'ecran d'accueil.");break;
 case 18:wpOpenGallery();break;
 case 19:wpCloseGallery();break;
 case 20:wpSave(0);break;
 case 21:wpSave(1);break;
 case 22:wpOpenPhotos();break;
 case 23:wpOpenSettings();break;
 }}
static id txtInput(id p,Rect r,BOOL secret){id t=view(C("UITextField"),r,soft);borderRadius(t,11);V1(t,"setTextColor:",white);V1(t,"setFont:",MD(C("UIFont"),"systemFontOfSize:",14));VB(t,"setSecureTextEntry:",secret);VB(t,"setAutocorrectionType:",1);VB(t,"setSpellCheckingType:",1);VB(t,"setAutocapitalizationType:",0);VB(t,"setClearButtonMode:",1);VI(t,"setKeyboardAppearance:",1);id spacer=view(C("UIView"),R(0,0,11,1),0);V1(t,"setLeftView:",spacer);VI(t,"setLeftViewMode:",3);add(p,t);return t;}
static void buildHome(id self,SEL cmd){typedef struct{id receiver;Class super_class;} super_t;super_t sup={self,C("UIViewController")};((void(*)(super_t*,SEL))objc_msgSendSuper)(&sup,cmd);g_ctrl=self;
 id root=M0(self,"view");background(root,bg);Rect sb=((Rect(*)(id,SEL))objc_msgSend)(M0(C("UIScreen"),"mainScreen"),S("bounds"));double w=sb.size.width,h=sb.size.height;if(w<300)w=375;
 g_scroll=view(C("UIScrollView"),R(0,0,w,h),bg);VB(g_scroll,"setAlwaysBounceVertical:",1);VI(g_scroll,"setKeyboardDismissMode:",2);add(root,g_scroll);
 double total=1685;id content=view(C("UIView"),R(0,0,w,total),bg);add(g_scroll,content);((void(*)(id,SEL,Size))objc_msgSend)(g_scroll,S("setContentSize:"),(Size){w,total});
 textLabel(content,"PRIV",R(20,47,120,44),37,1,white);textLabel(content,"CONTROL",R(137,53,w-152,40),27,1,green);
 textLabel(content,"PRIV OS / v0.5.4 • LAB",R(20,105,w-200,20),11,1,muted);
 mkButton(content,"Fonds d'écran",18,R(w-165,99,145,32),1);
 id hero=panel(content,R(14,135,w-28,105));textLabel(hero,"COFFRE LOCAL CHIFFRE",R(16,11,w-58,21),15,1,green);
 textLabel(hero,"Mot de passe prive. Aucune synchronisation.\nSur cet iPhone, le jailbreak reste un risque.",R(16,39,w-60,54),12,0,white);
 textLabel(content,"RACCOURCIS  /  ACTIONS REELLES",R(20,251,w-40,22),14,1,white);
 id quick=panel(content,R(14,278,w-28,186));
 double cw=(w-62)/2;
 mkButton(quick,"Notes chiffrees",11,R(15,12,cw,45),1);
 mkButton(quick,"Photos privees",12,R(24+cw,12,cw-8,45),0);
 mkButton(quick,"Mots de passe",13,R(15,69,cw,45),0);
 mkButton(quick,"Effacer texte copie",14,R(24+cw,69,cw-8,45),0);
 mkButton(quick,"Ouvrir Signal",15,R(15,126,cw,45),0);
 mkButton(quick,"Ouvrir VPN",16,R(24+cw,126,cw-8,45),0);
 g_status=textLabel(content,"COFFRE VERROUILLE  /  mot de passe requis",R(20,473,w-35,21),12,1,green);
 id unlock=panel(content,R(14,501,w-28,141));textLabel(unlock,"Phrase de passe du coffre",R(15,11,w-63,22),13,1,white);
 g_passwordField=txtInput(unlock,R(15,40,w-58,42),1);V1(g_passwordField,"setPlaceholder:",MS("10 caracteres minimum"));
 mkButton(unlock,"Ouvrir / creer",1,R(15,90,(w-62)/2,40),1);
 mkButton(unlock,"Verrouiller",2,R(24+(w-62)/2,90,(w-62)/2-8,40),0);
 textLabel(content,"01  NOTES CHIFFREES",R(20,662,w-40,25),17,1,white);
 id notes=panel(content,R(14,695,w-28,285));textLabel(notes,"Ecris une note privee",R(15,11,w-59,22),13,1,white);
 g_noteField=view(C("UITextView"),R(15,38,w-58,169),soft);borderRadius(g_noteField,12);V1(g_noteField,"setTextColor:",white);V1(g_noteField,"setFont:",MD(C("UIFont"),"systemFontOfSize:",15));VI(g_noteField,"setKeyboardAppearance:",1);VB(g_noteField,"setEditable:",0);V1(g_noteField,"setText:",MS(""));add(notes,g_noteField);
 mkButton(notes,"Enregistrer",3,R(15,216,(w-62)/2,48),1);mkButton(notes,"Vider la note",10,R(24+(w-62)/2,216,(w-62)/2-8,48),0);
 textLabel(content,"02  PHOTO PRIVEE",R(20,995,w-40,25),17,1,white);
 id photos=panel(content,R(14,1027,w-28,270));g_photoView=view(C("UIImageView"),R(15,13,w-58,146),soft);borderRadius(g_photoView,11);VI(g_photoView,"setContentMode:",1);VB(g_photoView,"setClipsToBounds:",1);add(photos,g_photoView);
 g_photoStatus=textLabel(photos,"Ouvre le coffre pour voir les photos.",R(15,163,w-59,24),11,0,muted);
 mkButton(photos,"Importer",4,R(15,206,(w-62)/2,45),1);mkButton(photos,"Retirer",5,R(24+(w-62)/2,206,(w-62)/2-8,45),0);
 textLabel(content,"03  MOTS DE PASSE",R(20,1310,w-40,25),17,1,white);
 id passwords=panel(content,R(14,1342,w-28,173));g_generated=txtInput(passwords,R(15,15,w-58,48),0);VB(g_generated,"setEnabled:",0);V1(g_generated,"setPlaceholder:",MS("Generer un mot de passe de 28 caracteres"));
 mkButton(passwords,"Generer",6,R(15,73,(w-62)/2,43),1);mkButton(passwords,"Copier",7,R(24+(w-62)/2,73,(w-62)/2-8,43),0);
 mkButton(passwords,"Effacer le presse-papiers",8,R(15,124,w-58,36),0);
 id risk=panel(content,R(14,1531,w-28,112));
 textLabel(risk,"TRANSPARENCE",R(15,10,w-60,22),12,1,green);
 textLabel(risk,"Pas de VPN actif, pas de coupure camera/micro.\nLe coffre protege les fichiers de l'app seulement.",R(15,34,w-58,54),12,0,white);
 mkButton(risk,"Limites de cette version",9,R(15,84,(w-62)/2,25),0);mkButton(risk,"Reglages iOS",17,R(24+(w-62)/2,84,(w-62)/2-8,25),0);
 if(vaultExists())setLabel(g_status,"COFFRE VERROUILLE / existant");
}
static void resign(id self,SEL cmd,id app){(void)self;(void)cmd;(void)app;endKeyboard();lockVault();wipe(g_pwd,sizeof(g_pwd));if(g_generated)V1(g_generated,"setText:",MS(""));}
static BOOL finish(id self,SEL cmd,id app,id opts){(void)self;(void)cmd;(void)app;(void)opts;
 bg=color(.051,.078,.122);card=color(.087,.126,.184);soft=color(.133,.182,.247);green=color(.37,.93,.78);white=color(.951,.980,.990);muted=color(.64,.73,.80);red=color(.93,.39,.42);
 Rect r=((Rect(*)(id,SEL))objc_msgSend)(M0(C("UIScreen"),"mainScreen"),S("bounds"));g_window=MR(M0(C("UIWindow"),"alloc"),"initWithFrame:",r);id root=M0(M0(C("PRIVViewController"),"alloc"),"init");V1(g_window,"setRootViewController:",root);V0(g_window,"makeKeyAndVisible");return 1;}
int main(int argc,char**argv){id pool=M0(M0(C("NSAutoreleasePool"),"alloc"),"init");
 Class ctrl=objc_allocateClassPair(C("UIViewController"),"PRIVViewController",0);class_addMethod(ctrl,S("viewDidLoad"),(void*)buildHome,"v@:");class_addMethod(ctrl,S("press:"),(void*)press,"v@:@");class_addMethod(ctrl,S("imagePickerController:didFinishPickingMediaWithInfo:"),(void*)pickerSelected,"v@:@@");class_addMethod(ctrl,S("imagePickerControllerDidCancel:"),(void*)pickerCancel,"v@:@");class_addMethod(ctrl,S("wallpaperImage:didFinishSavingWithError:contextInfo:"),(void*)wpSaveCompleted,"v@:@@^v");class_addMethod(ctrl,S("wallpaperSaveFinished:"),(void*)wpSaveFinished,"v@:@");objc_registerClassPair(ctrl);
 Class del=objc_allocateClassPair(C("UIResponder"),"PRIVDelegate",0);class_addMethod(del,S("application:didFinishLaunchingWithOptions:"),(void*)finish,"c@:@@");class_addMethod(del,S("applicationWillResignActive:"),(void*)resign,"v@:@");objc_registerClassPair(del);
 int code=UIApplicationMain(argc,argv,0,MS("PRIVDelegate"));V0(pool,"drain");return code;}
