/* Host-only behavioral test of the unmodified WallpaperGallery.inc.
   This mocks Foundation/UIKit boundaries; it does not validate iOS TCC,
   rendering, Objective-C ABI, or the real Photos writer. */
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

typedef struct Mock Mock;
typedef Mock *id;
typedef Mock *Class;
typedef const char *SEL;
typedef unsigned char BOOL;
typedef double CGFloat;
typedef struct { CGFloat x,y; } Point;
typedef struct { CGFloat width,height; } Size;
typedef struct { Point origin; Size size; } Rect;
struct Mock {
 const char *kind;
 char text[2048];
 long number;
 int refs, enabled, hidden;
 struct { char key[256]; id value; } entries[12];
 int nentries;
};
static Mock objects[2048];
static int object_count;
static id preferences, controller, pending_result;
static SEL pending_selector;
static int authorization=3, photokit_available=1, selector_available=1;
static int missing_image, writer_calls, permission_queries, mark_calls, main_dispatches;
static int launch_calls, alert_calls, removal_calls;
static id written_image, written_target;
static SEL written_selector;
static void *written_context;
static char loaded_path[2048];

static id make(const char *kind) {
 assert(object_count<(int)(sizeof objects/sizeof objects[0]));
 id o=&objects[object_count++]; memset(o,0,sizeof *o);
 o->kind=kind;o->refs=1;o->enabled=1;return o;
}
static id MS(const char *s) { id o=make("NSString");snprintf(o->text,sizeof o->text,"%s",s);return o; }
static Class C(const char *name) {
 if(!strcmp(name,"PHPhotoLibrary")&&!photokit_available)return 0;
 for(int i=0;i<object_count;i++)if(objects[i].kind==name)return &objects[i];
 return make(name);
}
static SEL S(const char *s){return s;}
static id lookup(id o,const char *key) {
 if(!o)return 0;
 for(int i=0;i<o->nentries;i++)if(!strcmp(o->entries[i].key,key))return o->entries[i].value;
 return 0;
}
static void put(id o,const char *key,id v) {
 int i=0;for(;i<o->nentries;i++)if(!strcmp(o->entries[i].key,key))break;
 assert(i<12);if(i==o->nentries)o->nentries++;
 snprintf(o->entries[i].key,sizeof o->entries[i].key,"%s",key);o->entries[i].value=v;
}
static id mock_msgSend(id receiver,SEL selector,...) {
 if(!receiver)return 0;
 assert(receiver->refs>0);
 va_list args;va_start(args,selector);id result=0;
 if(!strcmp(selector,"respondsToSelector:")){result=(id)(uintptr_t)selector_available;}
 else if(!strcmp(selector,"authorizationStatusForAccessLevel:")){
  assert(va_arg(args,long)==1);permission_queries++;result=(id)(intptr_t)authorization;
 } else if(!strcmp(selector,"mainBundle")){result=make("bundle");}
 else if(!strcmp(selector,"bundlePath")){result=MS("/mock/PRIVControl.app");}
 else if(!strcmp(selector,"stringByAppendingPathComponent:")){
  id part=va_arg(args,id);char path[2048];snprintf(path,sizeof path,"%.1000s/%.1000s",receiver->text,part->text);result=MS(path);
 } else if(!strcmp(selector,"imageWithContentsOfFile:")){
  id path=va_arg(args,id);snprintf(loaded_path,sizeof loaded_path,"%s",path->text);
  assert(strstr(path->text,"/mock/PRIVControl.app/Wallpapers/")==path->text);
  assert(!strstr(path->text,"vault")&&!strstr(path->text,"DCIM"));
  result=missing_image?0:make("UIImage");
 } else if(!strcmp(selector,"standardUserDefaults")){result=preferences;}
 else if(!strcmp(selector,"boolForKey:")){id value=lookup(receiver,va_arg(args,id)->text);result=(id)(uintptr_t)(value&&value->number);}
 else if(!strcmp(selector,"setBool:forKey:")){
  int b=va_arg(args,int);id key=va_arg(args,id);id v=make("NSNumber");v->number=b;put(receiver,key->text,v);mark_calls++;
 } else if(!strcmp(selector,"retain")){receiver->refs++;result=receiver;}
 else if(!strcmp(selector,"release")||!strcmp(selector,"drain")){receiver->refs--;}
 else if(!strcmp(selector,"alloc")){result=make(receiver->kind);}
 else if(!strcmp(selector,"init")){result=receiver;}
 else if(!strcmp(selector,"numberWithLong:")){result=make("NSNumber");result->number=va_arg(args,long);}
 else if(!strcmp(selector,"integerValue")){result=(id)(intptr_t)receiver->number;}
 else if(!strcmp(selector,"setObject:forKey:")){id value=va_arg(args,id);id key=va_arg(args,id);put(receiver,key->text,value);}
 else if(!strcmp(selector,"objectForKey:")){result=lookup(receiver,va_arg(args,id)->text);}
 else if(!strcmp(selector,"localizedDescription")){result=receiver->text[0]?MS(receiver->text):0;}
 else if(!strcmp(selector,"performSelectorOnMainThread:withObject:waitUntilDone:")){
  assert(!pending_result);pending_selector=va_arg(args,SEL);pending_result=va_arg(args,id);
  assert(!va_arg(args,int));pending_result->refs++;main_dispatches++;
 } else if(!strcmp(selector,"setTitle:forState:")){
  id title=va_arg(args,id);snprintf(receiver->text,sizeof receiver->text,"%s",title->text);
 } else if(!strcmp(selector,"setEnabled:")){receiver->enabled=va_arg(args,int);}
 else if(!strcmp(selector,"setHidden:")){receiver->hidden=va_arg(args,int);}
 else if(!strcmp(selector,"setAlpha:")||!strcmp(selector,"setTextColor:")){}
 else if(!strcmp(selector,"removeFromSuperview")){receiver->refs--;removal_calls++;}
 else {fprintf(stderr,"Unexpected ObjC selector: %s\n",selector);abort();}
 va_end(args);return result;
}
static void *objc_msgSend=(void*)mock_msgSend;
static id M0(id x,const char *n){return mock_msgSend(x,S(n));}
static id M1(id x,const char *n,id a){return mock_msgSend(x,S(n),a);}
static void V0(id x,const char *n){mock_msgSend(x,S(n));}
static void V1(id x,const char *n,id a){mock_msgSend(x,S(n),a);}
static void V2(id x,const char *n,id a,id b){mock_msgSend(x,S(n),a,b);}
static void VB(id x,const char *n,BOOL a){mock_msgSend(x,S(n),(int)a);}
static void VI(id x,const char *n,long a){(void)x;(void)n;(void)a;}
static id MD(id x,const char *n,double a){(void)x;(void)n;(void)a;return make("font");}
static Rect R(double x,double y,double w,double h){return (Rect){{x,y},{w,h}};}
static id bg,card,soft,green,white,muted,red,g_ctrl,g_scroll;
static void setLabel(id o,const char *s){assert(o&&o->refs>0);snprintf(o->text,sizeof o->text,"%s",s);}
static id view(Class c,Rect r,id color){(void)c;(void)r;(void)color;return make("view");}
static id panel(id p,Rect r){(void)p;(void)r;return make("panel");}
static void add(id p,id c){(void)p;c->refs++;}
static void borderRadius(id p,double r){(void)p;(void)r;}
static id textLabel(id p,const char *s,Rect r,int f,BOOL h,id c){(void)p;(void)r;(void)f;(void)h;(void)c;id o=make("label");o->refs++;setLabel(o,s);return o;}
static id mkButton(id p,const char*s,int t,Rect r,BOOL h){(void)p;(void)t;(void)r;(void)h;id o=make("button");setLabel(o,s);return o;}
static BOOL launchBundle(const char *s){(void)s;launch_calls++;return 1;}
static void alert(const char *s,const char *m){(void)s;(void)m;alert_calls++;}
static void endKeyboard(void){}

#ifndef WP_SOURCE_FILE
#define WP_SOURCE_FILE "../native/WallpaperGallery.inc"
#endif
#include WP_SOURCE_FILE

static void mock_writer(id image,id target,SEL callback,void *context){
 writer_calls++;written_image=image;written_target=target;written_selector=callback;written_context=context;
 assert(!strcmp(callback,"wallpaperImage:didFinishSavingWithError:contextInfo:"));
}
static void mount_gallery(void){
 g_wpRoot=make("gallery");g_wpRoot->refs=2;
 for(int i=0;i<2;i++){g_wpSaveButtons[i]=make("button");g_wpStatusLabels[i]=make("label");}
 for(int i=0;i<3;i++)g_wpNavigation[i]=make("button");
}
static void reset(void){
 object_count=0;preferences=make("preferences");controller=make("controller");g_ctrl=controller;g_scroll=make("home-scroll");
 bg=make("color");card=soft=green=white=muted=red=bg;
 pending_result=0;pending_selector=0;authorization=3;photokit_available=selector_available=1;
 missing_image=writer_calls=permission_queries=mark_calls=main_dispatches=launch_calls=alert_calls=removal_calls=0;
 written_image=written_target=0;written_selector=0;written_context=0;loaded_path[0]=0;
 g_wpBusy=0;g_wpPendingIndex=-1;g_wpPendingImage=0;g_wpPhotosLibrary=g_wpUIKitLibrary=0;
 g_wpSaveImage=mock_writer;mount_gallery();
}
static void pump(void){
 assert(pending_result);assert(!strcmp(pending_selector,"wallpaperSaveFinished:"));
 id result=pending_result;pending_result=0;
 wpSaveFinished(controller,S("wallpaperSaveFinished:"),result);
 V0(result,"release");assert(result->refs==0);
}
static void complete(int fail){
 id error=fail?make("NSError"):0;
 if(error)snprintf(error->text,sizeof error->text,"disk full");
 wpSaveCompleted(written_target,written_selector,written_image,error,written_context);
}
static void assert_idle(void){assert(!g_wpBusy&&g_wpPendingIndex==-1&&!g_wpPendingImage);}
static int scenarios;
#define PASS(name) do {scenarios++;printf("PASS %s\n",name);} while(0)

int main(void){
 (void)wpOpenGallery; /* Layout itself requires UIKit and is not executed here. */
 reset();authorization=1;wpSave(0);assert_idle();assert(!writer_calls&&!mark_calls);assert(strstr(g_wpStatusLabels[0]->text,"restrictions"));PASS("restricted refuses write");
 reset();authorization=2;wpSave(0);assert_idle();assert(!writer_calls&&!mark_calls);assert(strstr(g_wpStatusLabels[0]->text,"refusé"));PASS("denied refuses write");
 reset();authorization=-1;wpSave(0);assert_idle();assert(!writer_calls&&!mark_calls);PASS("unknown authorization fails closed");
 reset();selector_available=0;wpSave(0);assert_idle();assert(!writer_calls&&!mark_calls);PASS("missing authorization API fails closed");
 reset();photokit_available=0;wpSave(0);assert_idle();assert(!writer_calls&&!mark_calls);PASS("missing PhotoKit fails closed");
 reset();missing_image=1;wpSave(0);assert_idle();assert(!writer_calls&&!mark_calls);assert(strstr(g_wpStatusLabels[0]->text,"manque"));PASS("missing bundled asset refuses write");
 reset();g_wpSaveImage=0;wpSave(0);assert_idle();assert(!writer_calls&&!mark_calls);PASS("missing UIKit writer refuses write");
 reset();wpSave(-1);wpSave(2);assert_idle();assert(!writer_calls&&!permission_queries);PASS("invalid index ignored");
 reset();g_wpRoot=0;wpSave(0);assert_idle();assert(!writer_calls);PASS("closed gallery cannot export");
 reset();authorization=0;wpSave(0);assert(writer_calls==1&&g_wpBusy&&!mark_calls);assert(strstr(g_wpStatusLabels[0]->text,"Enregistrement"));PASS("undetermined delegates prompt to UIKit without false success");
 for(int state=3;state<=4;state++){
  reset();authorization=state;wpSave(0);assert(writer_calls==1&&g_wpBusy&&!mark_calls);assert(g_wpPendingImage==written_image&&written_image->refs==2);assert(strstr(loaded_path,"PRIV-Wallpaper-Night.png"));
  wpSave(0);wpSave(1);wpCloseGallery();wpOpenPhotos();wpOpenSettings();assert(writer_calls==1&&g_wpRoot&&!launch_calls&&!removal_calls);
  for(int i=0;i<2;i++){assert(!g_wpSaveButtons[i]->enabled);}
  for(int i=0;i<3;i++){assert(!g_wpNavigation[i]->enabled);}
  complete(0);assert(g_wpBusy&&!mark_calls&&main_dispatches==1);pump();assert_idle();assert(written_image->refs==1);assert(mark_calls==1&&wpDidExport(0)&&!wpDidExport(1));
  for(int i=0;i<2;i++){assert(g_wpSaveButtons[i]->enabled);}
  for(int i=0;i<3;i++){assert(g_wpNavigation[i]->enabled);}
  assert(strstr(g_wpSaveButtons[0]->text,"à nouveau"));assert(strstr(g_wpStatusLabels[0]->text,"Enregistré"));
 }
 PASS("authorized/limited write, duplicate taps and navigation blocked, main completion marks only successful asset");
 reset();wpSave(1);complete(1);assert(g_wpBusy&&!mark_calls);pump();assert_idle();assert(!mark_calls&&!wpDidExport(1));assert(strstr(g_wpStatusLabels[1]->text,"échoué"));wpSave(1);assert(writer_calls==2);complete(0);pump();assert(mark_calls==1&&wpDidExport(1)&&!wpDidExport(0));PASS("write error stays unmarked and can retry successfully");
 reset();authorization=0;wpSave(0);authorization=2;complete(1);pump();assert_idle();assert(!mark_calls&&strstr(g_wpStatusLabels[0]->text,"refusé"));wpSave(0);assert(writer_calls==1);authorization=3;wpSave(0);assert(writer_calls==2);complete(0);pump();assert(mark_calls==1);PASS("denied prompt then Settings recovery rechecks permission");
 reset();wpSave(0);authorization=1;complete(1);pump();assert_idle();assert(!mark_calls&&strstr(g_wpStatusLabels[0]->text,"restrictions"));PASS("restriction at completion displays failure");
 reset();wpSave(0);wpSaveCompleted(controller,0,written_image,0,(void*)&g_wallpapers[1]);pump();assert(g_wpBusy&&!mark_calls);complete(0);pump();assert_idle();assert(mark_calls==1);PASS("mismatched completion cannot mark success");
 reset();wpSave(0);wpSaveCompleted(controller,0,written_image,0,(void*)(uintptr_t)1);assert(g_wpBusy&&!pending_result&&!mark_calls);complete(0);pump();assert(mark_calls==1);PASS("unknown callback context ignored");
 reset();wpSave(0);complete(0);pump();wpCloseGallery();assert(!g_wpRoot&&removal_calls==1&&!g_scroll->hidden);mount_gallery();wpRefreshControls();assert(wpDidExport(0)&&!wpDidExport(1));assert(strstr(g_wpSaveButtons[0]->text,"à nouveau"));wpSave(0);complete(0);pump();assert(writer_calls==2&&mark_calls==2);PASS("close/reopen preserves marker and explicit re-export is possible");
 reset();wpSave(0);id blank_error=make("NSError");wpSaveCompleted(written_target,written_selector,written_image,blank_error,written_context);pump();assert_idle();assert(!mark_calls&&!wpDidExport(0)&&written_image->refs==1);PASS("error without description still fails and releases image");
 reset();wpSave(0);complete(0);pump();assert(mark_calls==1);complete(0);pump();assert_idle();assert(mark_calls==1&&written_image->refs==1);PASS("late repeated callback ignored without double release or extra marker");
 printf("%d host scenarios passed against unmodified production include.\n",scenarios);return 0;
}
