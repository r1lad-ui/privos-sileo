#import <UIKit/UIKit.h>
@interface PKAppDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic,strong) UIWindow *window;
@end
int main(int argc,char *argv[]) {
    @autoreleasepool {return UIApplicationMain(argc,argv,nil,NSStringFromClass(PKAppDelegate.class));}
}
