/* Host-only tests of the actual native app and SecurityCenter.inc.
   Runtime entry points are replaced at the boundary. No iOS authentication,
   settings, keychain, filesystem or Photos service is used by this test.
   This harness is for x86_64 Linux and verifies logic, not Apple's arm64 ABI. */
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *mock_objc_dispatch(void);
static void *objc_msgSend=(void*)mock_objc_dispatch;
static void *objc_msgSendSuper=(void*)mock_objc_dispatch;
/* Intercept only the extern declarations, keeping the production call sites
   bound to the runtime pointer above. The production source is not rewritten. */
#define objc_msgSend(...) unused_objc_msgSend_declaration
#define objc_msgSendSuper(...) unused_objc_msgSendSuper_declaration
#define dlopen mock_dlopen
#define dlsym mock_dlsym
#define main priv_application_entry
/* The original C app uses compact if/statement layout. Keep this legacy
   formatting warning scoped to the included source, not the harness. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#include "../native/PRIVControl.c"
#pragma GCC diagnostic pop
#undef main
#undef dlsym
#undef dlopen
#undef objc_msgSend
#undef objc_msgSendSuper

typedef struct Mock Mock;
struct Mock {
 const char *type;
 char text[2048];
 int refs,isClass,enabled,hidden,opaque,interactive;
 long number,autoresizing;
 Rect frame;
 Mock *parent,*childView,*domain,*children[48];
 int childCount,probes,lastPolicy;
 struct {char key[256];Mock *value;} entries[12];
 int entryCount;
};
static Mock objects[4096];
static int object_count,context_allocs,context_releases,policy_calls,type_reads;
static int la_class_available=1,la_symbol_available=1,la_selector_available=1;
static int biometry_selector_available=1,context_init_available=1;
static int policy_ok[3]={0,1,1};
static long policy_error_code[3]={0,0,0};
static const char *policy_error_domain[3]={0,0,0};
static long mock_biometry_type=1;
static int domain_reads,view_removals,bring_front_calls,settings_launches;
static int marker_calls,main_dispatches,allow_wallpaper_markers;
static id mock_la_domain,mock_preferences,pending_main_result;
static SEL pending_main_selector;
static const char *mock_system_version="17.5";

static Mock *O(id x){return (Mock*)x;}
static Mock *make(const char *type){
 assert(object_count<(int)(sizeof objects/sizeof objects[0]));
 Mock *o=&objects[object_count++];memset(o,0,sizeof *o);o->type=type;o->refs=1;o->enabled=1;return o;
}
static id string(const char *s){Mock *o=make("NSString");snprintf(o->text,sizeof o->text,"%s",s?s:"");return o;}
static void retain(id x){if(x){assert(O(x)->refs>0);O(x)->refs++;}}
static void release(id x){
 if(!x)return;
 Mock *o=O(x);assert(o->refs>0);o->refs--;
 if(o->refs==0){
  if(!strcmp(o->type,"LAContext"))context_releases++;
  for(int i=0;i<o->childCount;i++){o->children[i]->parent=0;release(o->children[i]);}
  o->childCount=0;
 }
}
static void attach(Mock *parent,Mock *child){
 assert(parent&&child&&child->refs>0&&parent->childCount<48);
 assert(!child->parent);parent->children[parent->childCount++]=child;child->parent=parent;retain(child);
}
static void detach(Mock *child){
 if(!child->parent)return;
 Mock *parent=child->parent;int i=0;
 for(;i<parent->childCount;i++)if(parent->children[i]==child)break;
 assert(i<parent->childCount);
 for(;i+1<parent->childCount;i++)parent->children[i]=parent->children[i+1];
 parent->childCount--;child->parent=0;view_removals++;release(child);
}
static id lookup(id x,const char *key){
 Mock *o=O(x);for(int i=0;i<o->entryCount;i++)if(!strcmp(o->entries[i].key,key))return o->entries[i].value;return 0;
}
static void put(id x,const char *key,id value){
 Mock *o=O(x);int i=0;for(;i<o->entryCount;i++)if(!strcmp(o->entries[i].key,key))break;
 assert(i<12);if(i==o->entryCount)o->entryCount++;
 snprintf(o->entries[i].key,sizeof o->entries[i].key,"%s",key);o->entries[i].value=O(value);
}
static id error_for(long policy){
 if(!policy_error_domain[policy])return 0;
 Mock *o=make("NSError");o->domain=O(string(policy_error_domain[policy]));o->number=policy_error_code[policy];return o;
}

Class objc_getClass(const char *name){
 if(!strcmp(name,"LAContext")&&!la_class_available)return 0;
 for(int i=0;i<object_count;i++)if(objects[i].isClass&&!strcmp(objects[i].type,name))return &objects[i];
 Mock *o=make(name);o->isClass=1;return o;
}
SEL sel_registerName(const char *name){return (SEL)name;}
Class objc_allocateClassPair(Class parent,const char *name,size_t extra){(void)parent;(void)extra;return objc_getClass(name);}
void objc_registerClassPair(Class c){(void)c;}
BOOL class_addMethod(Class c,SEL s,void *f,const char *types){(void)c;(void)s;(void)f;(void)types;return 1;}
int UIApplicationMain(int argc,char **argv,id principal,id delegate){(void)argc;(void)argv;(void)principal;(void)delegate;abort();}
int SecRandomCopyBytes(void *r,size_t n,void *out){(void)r;(void)n;(void)out;fprintf(stderr,"Unexpected crypto operation\n");abort();}
id UIImageJPEGRepresentation(id image,CGFloat q){(void)image;(void)q;fprintf(stderr,"Unexpected private-photo operation\n");abort();}
void *mock_dlopen(const char *path,int mode){
 (void)mode;
 if(strstr(path,"LocalAuthentication.framework"))return la_class_available?(void*)(uintptr_t)1:0;
 if(strstr(path,"UIKit.framework")||strstr(path,"Photos.framework"))return (void*)(uintptr_t)2;
 fprintf(stderr,"Unexpected dynamic library: %s\n",path);abort();
}
void *mock_dlsym(void *handle,const char *symbol){
 (void)handle;
 if(!strcmp(symbol,"LAErrorDomain"))return la_symbol_available?&mock_la_domain:0;
 /* Tests don't request real Photos writes or opening Settings URLs. */
 if(!strcmp(symbol,"UIApplicationOpenSettingsURLString"))return 0;
 fprintf(stderr,"Unexpected symbol: %s\n",symbol);abort();
}

__attribute__((used,noinline)) static id mock_msg_send(id receiver,SEL sel,...){
 if(!receiver)return 0;
 Mock *o=O(receiver);assert(o->refs>0);
 const char *s=(const char*)sel;va_list ap;va_start(ap,sel);id out=0;
 if(!strcmp(s,"respondsToSelector:")){
  const char *asked=(const char*)va_arg(ap,SEL);int yes=1;
  if(!strcmp(asked,"canEvaluatePolicy:error:"))yes=la_selector_available;
  if(!strcmp(asked,"biometryType"))yes=biometry_selector_available;
  out=(id)(uintptr_t)yes;
 }else if(!strcmp(s,"alloc")){
  out=make(o->type);if(!strcmp(o->type,"LAContext"))context_allocs++;
 }else if(!strcmp(s,"init")){
  if(!strcmp(o->type,"LAContext")&&!context_init_available){release(receiver);out=0;}else out=receiver;
 }else if(!strcmp(s,"retain")){retain(receiver);out=receiver;}
 else if(!strcmp(s,"release")||!strcmp(s,"drain")){release(receiver);}
 else if(!strcmp(s,"invalidate")){}
 else if(!strcmp(s,"canEvaluatePolicy:error:")){
  assert(!strcmp(o->type,"LAContext"));long p=va_arg(ap,long);assert(p==1||p==2);id *err=va_arg(ap,id*);
  assert(err);*err=error_for(p);o->probes++;o->lastPolicy=(int)p;policy_calls++;out=(id)(uintptr_t)policy_ok[p];
 }else if(!strcmp(s,"biometryType")){assert(o->probes>0);assert(o->lastPolicy==1);type_reads++;out=(id)(intptr_t)mock_biometry_type;}
 else if(!strcmp(s,"domain")){domain_reads++;out=o->domain;}
 else if(!strcmp(s,"code")||!strcmp(s,"integerValue")||!strcmp(s,"tag")){out=(id)(intptr_t)o->number;}
 else if(!strcmp(s,"isEqualToString:")){Mock *other=O(va_arg(ap,id));out=(id)(uintptr_t)(other&&!strcmp(o->text,other->text));}
 else if(!strcmp(s,"stringWithUTF8String:")){out=string(va_arg(ap,const char*));}
 else if(!strcmp(s,"UTF8String")){out=o->text;}
 else if(!strcmp(s,"length")){out=(id)(uintptr_t)strlen(o->text);}
 else if(!strcmp(s,"stringByAppendingString:")){
  Mock *other=O(va_arg(ap,id));char joined[2048];snprintf(joined,sizeof joined,"%.1000s%.1000s",o->text,other->text);out=string(joined);
 }else if(!strcmp(s,"currentDevice")){out=make("UIDevice");}
 else if(!strcmp(s,"mainScreen")){Mock *screen=make("UIScreen");screen->frame=R(0,0,375,667);out=screen;}
 else if(!strcmp(s,"systemVersion")){out=string(mock_system_version);}
 else if(!strcmp(s,"view")){out=o->childView;}
 else if(!strcmp(s,"initWithFrame:")){o->frame=va_arg(ap,Rect);out=receiver;}
 else if(!strcmp(s,"addSubview:")){attach(o,O(va_arg(ap,id)));}
 else if(!strcmp(s,"removeFromSuperview")){detach(o);}
 else if(!strcmp(s,"bringSubviewToFront:")){
  Mock *child=O(va_arg(ap,id));assert(child->parent==o);int i=0;for(;i<o->childCount;i++)if(o->children[i]==child)break;
  assert(i<o->childCount);for(;i+1<o->childCount;i++)o->children[i]=o->children[i+1];o->children[o->childCount-1]=child;bring_front_calls++;
 }else if(!strcmp(s,"setFrame:")){o->frame=va_arg(ap,Rect);}
 else if(!strcmp(s,"setAutoresizingMask:")){o->autoresizing=va_arg(ap,long);}
 else if(!strcmp(s,"setOpaque:")){o->opaque=va_arg(ap,int);}
 else if(!strcmp(s,"setUserInteractionEnabled:")){o->interactive=va_arg(ap,int);}
 else if(!strcmp(s,"setHidden:")){o->hidden=va_arg(ap,int);}
 else if(!strcmp(s,"setEnabled:")){o->enabled=va_arg(ap,int);}
 else if(!strcmp(s,"setText:")||!strcmp(s,"setTitle:forState:")){
  Mock *text=O(va_arg(ap,id));snprintf(o->text,sizeof o->text,"%s",text?text->text:"");
 }else if(!strcmp(s,"setTag:")){o->number=va_arg(ap,long);}
 else if(!strcmp(s,"layer")){out=make("CALayer");}
 else if(!strcmp(s,"titleLabel")){out=make("UILabel");}
 else if(!strcmp(s,"buttonWithType:")){out=make("UIButton");}
 else if(!strcmp(s,"systemFontOfSize:")||!strcmp(s,"boldSystemFontOfSize:")||!strcmp(s,"colorWithRed:green:blue:alpha:")){out=make("UIStyle");}
 else if(!strcmp(s,"setBackgroundColor:")||!strcmp(s,"setTextColor:")||!strcmp(s,"setFont:")||!strcmp(s,"setNumberOfLines:")||
         !strcmp(s,"setTextAlignment:")||!strcmp(s,"setTitleColor:forState:")||
         !strcmp(s,"setCornerRadius:")||!strcmp(s,"setMasksToBounds:")||!strcmp(s,"setAccessibilityViewIsModal:")||
         !strcmp(s,"setAccessibilityElementsHidden:")||!strcmp(s,"setIsAccessibilityElement:")||!strcmp(s,"setAccessibilityLabel:")||
         !strcmp(s,"setAlwaysBounceVertical:")||!strcmp(s,"setContentSize:")||!strcmp(s,"setAlpha:")||
         !strcmp(s,"addTarget:action:forControlEvents:")||!strcmp(s,"endEditing:")||!strcmp(s,"setEditable:")||!strcmp(s,"setImage:")){}
 else if(!strcmp(s,"defaultWorkspace")){out=make("LSApplicationWorkspace");}
 else if(!strcmp(s,"openApplicationWithBundleID:")){Mock *bundle=O(va_arg(ap,id));assert(!strcmp(bundle->text,"com.apple.Preferences"));settings_launches++;out=(id)(uintptr_t)1;}
 else if(!strcmp(s,"standardUserDefaults")){out=mock_preferences;}
 else if(!strcmp(s,"boolForKey:")){Mock *v=O(lookup(receiver,O(va_arg(ap,id))->text));out=(id)(uintptr_t)(v&&v->number);}
 else if(!strcmp(s,"setBool:forKey:")){assert(allow_wallpaper_markers);int b=va_arg(ap,int);Mock *key=O(va_arg(ap,id));Mock *v=make("NSNumber");v->number=b;put(receiver,key->text,v);marker_calls++;}
 else if(!strcmp(s,"numberWithLong:")){Mock *n=make("NSNumber");n->number=va_arg(ap,long);out=n;}
 else if(!strcmp(s,"objectForKey:")){out=lookup(receiver,O(va_arg(ap,id))->text);}
 else if(!strcmp(s,"setObject:forKey:")){id value=va_arg(ap,id);Mock *key=O(va_arg(ap,id));put(receiver,key->text,value);}
 else if(!strcmp(s,"performSelectorOnMainThread:withObject:waitUntilDone:")){
  assert(!pending_main_result);pending_main_selector=va_arg(ap,SEL);pending_main_result=va_arg(ap,id);assert(!va_arg(ap,int));retain(pending_main_result);main_dispatches++;
 }else {fprintf(stderr,"Unexpected selector: %s (%s)\n",s,o->type);abort();}
 va_end(ap);return out;
}

__attribute__((used,noinline)) static Rect mock_rect_msg_send(id receiver,SEL selector){
 assert(receiver&&O(receiver)->refs>0);assert(!strcmp((const char*)selector,"bounds"));return O(receiver)->frame;
}

/* CGRect returns use x86_64's hidden sret pointer, unlike scalar ObjC calls.
   An actual receiver belongs to our arena; a hidden return pointer does not.
   Route the unchanged production cast/call to the matching mock ABI. */
__attribute__((naked)) static void *mock_objc_dispatch(void){
#if defined(__x86_64__)
 __asm__ volatile(
  "testq %%rdi, %%rdi\n\t"
  "jz 1f\n\t"
  "leaq objects(%%rip), %%rax\n\t"
  "cmpq %%rax, %%rdi\n\t"
  "jb 2f\n\t"
  "leaq %c0(%%rax), %%rax\n\t"
  "cmpq %%rax, %%rdi\n\t"
  "jae 2f\n\t"
  "1: jmp mock_msg_send\n\t"
  "2: jmp mock_rect_msg_send\n\t"
  : : "i" (sizeof objects) : "rax");
#else
#error This host mock currently requires x86_64.
#endif
}

static void reset(void){
 memset(objects,0,sizeof objects);object_count=0;
 context_allocs=context_releases=policy_calls=type_reads=0;
 la_class_available=la_symbol_available=la_selector_available=1;
 biometry_selector_available=context_init_available=1;
 policy_ok[1]=policy_ok[2]=1;
 memset(policy_error_code,0,sizeof policy_error_code);
 memset(policy_error_domain,0,sizeof policy_error_domain);
 mock_biometry_type=1;
 domain_reads=view_removals=bring_front_calls=settings_launches=0;
 marker_calls=main_dispatches=allow_wallpaper_markers=0;pending_main_result=0;pending_main_selector=0;
 /* Deliberately not Apple's literal value: production must use the symbol. */
 mock_la_domain=string("AppleDomainProvidedBySymbol");
 mock_preferences=make("NSUserDefaults");mock_system_version="17.5";
 g_securityLocalAuthentication=0;g_securityLAErrorDomain=0;
 g_securityRoot=g_securityCodeLabel=g_securityBiometryLabel=g_securityOSLabel=g_securityCover=0;
 g_wpRoot=0;memset(g_wpSaveButtons,0,sizeof g_wpSaveButtons);
 memset(g_wpStatusLabels,0,sizeof g_wpStatusLabels);memset(g_wpNavigation,0,sizeof g_wpNavigation);
 g_wpBusy=0;g_wpPendingIndex=-1;g_wpPendingImage=0;
 g_wpPhotosLibrary=g_wpUIKitLibrary=0;g_wpSaveImage=0;
 g_vault=0;g_unlocked=0;g_picker=0;memset(g_pass,0,sizeof g_pass);memset(g_pwd,0,sizeof g_pwd);
 g_passwordField=g_noteField=g_status=g_generated=g_photoView=g_photoStatus=0;
 bg=make("UIColor");card=make("UIColor");soft=make("UIColor");green=make("UIColor");
 white=make("UIColor");muted=make("UIColor");red=make("UIColor");
 g_window=make("UIWindow");O(g_window)->frame=R(0,0,375,667);
 g_ctrl=make("PRIVViewController");O(g_ctrl)->childView=make("UIView");
 attach(O(g_window),O(g_ctrl)->childView);
 g_scroll=make("UIScrollView");attach(O(g_ctrl)->childView,O(g_scroll));
}
static void fail_policy(long policy,long code,const char *domain){
 policy_ok[policy]=0;policy_error_code[policy]=code;policy_error_domain[policy]=domain;
}
static void assert_contexts_clean(void){assert(context_allocs==context_releases);}
static void assert_zero(const char *p,size_t n){for(size_t i=0;i<n;i++)assert(p[i]==0);}
static void sensitive_vault(void){
 g_vault=make("NSMutableDictionary");g_unlocked=1;
 memset(g_pass,'p',sizeof g_pass);memset(g_pwd,'x',sizeof g_pwd);
 g_passwordField=make("UITextField");g_noteField=make("UITextView");
 g_status=make("UILabel");g_generated=make("UILabel");
 g_photoView=make("UIImageView");g_photoStatus=make("UILabel");
 strcpy(O(g_passwordField)->text,"secret");strcpy(O(g_noteField)->text,"private note");
 strcpy(O(g_generated)->text,"generated password");
}
static void assert_vault_locked(void){
 assert(!g_unlocked&&!g_vault);assert_zero(g_pass,sizeof g_pass);assert_zero(g_pwd,sizeof g_pwd);
 assert(!O(g_passwordField)->text[0]&&!O(g_noteField)->text[0]&&!O(g_generated)->text[0]);
}
static void pump_main_callback(void){
 assert(pending_main_result&&!strcmp((const char*)pending_main_selector,"wallpaperSaveFinished:"));
 id result=pending_main_result;pending_main_result=0;pending_main_selector=0;
 wpSaveFinished(g_ctrl,S("wallpaperSaveFinished:"),result);
 release(result);assert(O(result)->refs==0);
}
static int scenario_count;
#define CASE(label) do {reset();scenario_count++;printf("%02d %s\n",scenario_count,label);} while(0)

int main(void){
 PRIVSecuritySnapshot s;
 CASE("Configured code and Touch ID; fresh context and no persistence");
 s=securityReadSnapshot();assert(s.passcode==PRIVCodeConfigured&&s.biometry==PRIVTouchIDReady&&s.biometryType==1);
 assert(policy_calls==2&&type_reads==1&&context_allocs==1&&!marker_calls);assert_contexts_clean();

 CASE("Absent code requires matching domain and -5");
 fail_policy(2,-5,O(mock_la_domain)->text);s=securityReadSnapshot();assert(s.passcode==PRIVCodeAbsent);assert_contexts_clean();

 CASE("Unrelated domain cannot report an absent code");
 fail_policy(2,-5,"OtherDomain");s=securityReadSnapshot();assert(s.passcode==PRIVCodeUnknown);assert_contexts_clean();

 CASE("Missing errors and unexpected errors remain unknown");
 fail_policy(2,-5,0);s=securityReadSnapshot();assert(s.passcode==PRIVCodeUnknown);
 fail_policy(2,-8,O(mock_la_domain)->text);s=securityReadSnapshot();assert(s.passcode==PRIVCodeUnknown);assert_contexts_clean();

 CASE("Not enrolled, lockout and unavailable use exact LA error codes");
 const long errors[]={-7,-8,-6};
 const long states[]={PRIVBiometryNotEnrolled,PRIVBiometryLockout,PRIVBiometryUnavailable};
 for(int i=0;i<3;i++){fail_policy(1,errors[i],O(mock_la_domain)->text);s=securityReadSnapshot();assert(s.biometry==states[i]&&s.biometryType==1);}
 assert_contexts_clean();

 CASE("Foreign-domain biometric errors and unexpected codes are unknown");
 for(int i=0;i<3;i++){fail_policy(1,errors[i],"OtherDomain");s=securityReadSnapshot();assert(s.biometry==PRIVBiometryUnknown);}
 fail_policy(1,-5,O(mock_la_domain)->text);s=securityReadSnapshot();assert(s.biometry==PRIVBiometryUnknown);assert_contexts_clean();

 CASE("Biometric success on Face ID or unknown sensor is not called Touch ID");
 mock_biometry_type=2;s=securityReadSnapshot();assert(s.biometry==PRIVBiometryUnknown&&s.biometryType==2);
 mock_biometry_type=0;s=securityReadSnapshot();assert(s.biometry==PRIVBiometryUnknown);assert_contexts_clean();

 CASE("Missing LA class returns unknown without a context");
 la_class_available=0;s=securityReadSnapshot();assert(s.passcode==PRIVCodeUnknown&&s.biometry==PRIVBiometryUnknown&&!context_allocs);

 CASE("Missing LA method returns unknown and releases the context");
 la_selector_available=0;s=securityReadSnapshot();assert(s.passcode==PRIVCodeUnknown&&s.biometry==PRIVBiometryUnknown&&!policy_calls);assert_contexts_clean();

 CASE("Failed context initialization returns unknown");
 context_init_available=0;s=securityReadSnapshot();assert(s.passcode==PRIVCodeUnknown&&s.biometry==PRIVBiometryUnknown&&!policy_calls);assert_contexts_clean();

 CASE("Missing domain symbol never turns a negative result into a diagnosis");
 la_symbol_available=0;fail_policy(2,-5,O(mock_la_domain)->text);fail_policy(1,-7,O(mock_la_domain)->text);
 s=securityReadSnapshot();assert(s.passcode==PRIVCodeUnknown&&s.biometry==PRIVBiometryUnknown);assert_contexts_clean();

 CASE("Missing biometryType method cannot give a Touch ID positive");
 biometry_selector_available=0;s=securityReadSnapshot();assert(s.passcode==PRIVCodeConfigured&&s.biometry==PRIVBiometryUnknown&&!type_reads);assert_contexts_clean();

 CASE("New probe sees changes instead of caching a prior successful result");
 s=securityReadSnapshot();assert(s.passcode==PRIVCodeConfigured);
 fail_policy(2,-5,O(mock_la_domain)->text);fail_policy(1,-7,O(mock_la_domain)->text);
 s=securityReadSnapshot();assert(s.passcode==PRIVCodeAbsent&&s.biometry==PRIVBiometryNotEnrolled);
 assert(context_allocs==2&&!marker_calls);assert_contexts_clean();

 CASE("Opening the security page populates truthful labels and declared OS");
 securityOpen();assert(g_securityRoot&&O(g_scroll)->hidden);
 assert(!strcmp(O(g_securityCodeLabel)->text,"Code iOS configuré"));
 assert(!strcmp(O(g_securityBiometryLabel)->text,"Touch ID disponible pour PRIV"));
 assert(strstr(O(g_securityOSLabel)->text,"17.5"));assert(context_allocs==1&&!marker_calls);assert_contexts_clean();
 id root=g_securityRoot;int allocations=object_count;securityOpen();assert(g_securityRoot==root&&object_count==allocations);
 securityClose();assert(!g_securityRoot&&!g_securityCodeLabel&&!g_securityBiometryLabel&&!g_securityOSLabel&&!O(g_scroll)->hidden);
 assert(O(root)->refs==0);securityClose();

 CASE("Return from Settings refreshes the open security page with a new context");
 securityOpen();securityOpenSettings();assert(settings_launches==1);
 fail_policy(2,-5,O(mock_la_domain)->text);fail_policy(1,-8,O(mock_la_domain)->text);
 securityResign(0,0,0);assert(g_securityCover);securityBecameActive(0,0,0);
 assert(!g_securityCover&&context_allocs==2&&!marker_calls);
 assert(!strcmp(O(g_securityCodeLabel)->text,"Aucun code iOS détecté"));
 assert(strstr(O(g_securityBiometryLabel)->text,"Touch ID temporairement bloqué"));assert_contexts_clean();

 CASE("Closed security page is not probed when app becomes active");
 securityBecameActive(0,0,0);assert(!context_allocs&&!g_securityCover);

 CASE("Opaque window cover supports repeated show, resize and repeated hide");
 securityShowCover();id cover=g_securityCover;assert(cover&&O(cover)->parent==O(g_window));
 assert(O(cover)->opaque&&O(cover)->interactive&&O(cover)->autoresizing==18);
 assert(O(cover)->frame.size.width==375&&O(cover)->frame.size.height==667);
 assert(O(g_window)->children[O(g_window)->childCount-1]==O(cover));
 allocations=object_count;O(g_window)->frame=R(0,0,667,375);securityShowCover();
 assert(g_securityCover==cover&&object_count==allocations&&bring_front_calls==2);
 assert(O(cover)->frame.size.width==667&&O(cover)->frame.size.height==375);
 securityHideCover();assert(!g_securityCover&&O(cover)->refs==0);
 int removed=view_removals;securityHideCover();assert(view_removals==removed);

 CASE("Missing window makes cover calls harmless");
 g_window=0;securityShowCover();securityHideCover();assert(!g_securityCover);

 CASE("Background without prior resign still covers and locks the original vault");
 sensitive_vault();securityEnteredBackground(0,0,0);assert(g_securityCover);assert_vault_locked();
 securityBecameActive(0,0,0);assert(!g_securityCover);assert_vault_locked();

 CASE("Repeated resign/background keeps one cover and never unlocks on return");
 sensitive_vault();securityResign(0,0,0);cover=g_securityCover;assert(cover);assert_vault_locked();
 securityEnteredBackground(0,0,0);securityResign(0,0,0);assert(g_securityCover==cover);assert_vault_locked();
 securityBecameActive(0,0,0);securityBecameActive(0,0,0);assert(!g_securityCover);assert_vault_locked();

 CASE("Pending Photos export survives inactive/background/active and real callback");
 g_wpRoot=make("UIView");g_wpBusy=1;g_wpPendingIndex=0;g_wpPendingImage=make("UIImage");
 id image=g_wpPendingImage;id gallery=g_wpRoot;
 for(int i=0;i<2;i++){g_wpStatusLabels[i]=make("UILabel");g_wpSaveButtons[i]=make("UIButton");}
 for(int i=0;i<3;i++)g_wpNavigation[i]=make("UIButton");
 sensitive_vault();securityResign(0,0,0);securityEnteredBackground(0,0,0);securityBecameActive(0,0,0);
 assert(g_wpRoot==gallery&&g_wpBusy&&g_wpPendingIndex==0&&g_wpPendingImage==image&&O(image)->refs==1&&!marker_calls);
 assert(!g_securityCover&&!context_allocs);assert_vault_locked();
 wpSaveCompleted(g_ctrl,S("wallpaperImage:didFinishSavingWithError:contextInfo:"),image,0,(void*)&g_wallpapers[0]);
 assert(g_wpBusy&&!marker_calls&&main_dispatches==1);allow_wallpaper_markers=1;pump_main_callback();
 assert(!g_wpBusy&&g_wpPendingIndex==-1&&!g_wpPendingImage&&O(image)->refs==0&&marker_calls==1&&wpDidExport(0));
 assert(strstr(O(g_wpStatusLabels[0])->text,"Enregistré dans Photos"));

 printf("PASS: %d security diagnostics/lifecycle scenarios; no authentication or iOS-setting operation executed.\n",scenario_count);
 return 0;
}
