// AppDelegate.mm — UIKit application delegate for DroidGfxSim iOS

#import <UIKit/UIKit.h>

@interface DroidGfxSimViewController : UIViewController
@end

@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property (strong, nonatomic) UIWindow *window;
@property (strong, nonatomic) DroidGfxSimViewController *viewController;
@end

@implementation AppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    self.viewController = [[DroidGfxSimViewController alloc] init];
    self.window.rootViewController = self.viewController;
    [self.window makeKeyAndVisible];
    return YES;
}

- (void)applicationWillResignActive:(UIApplication *)application {
    // Pause simulation timers and throttle rendering when losing active focus
}

- (void)applicationDidEnterBackground:(UIApplication *)application {
    // Release unneeded resources and flush caches
}

- (void)applicationWillEnterForeground:(UIApplication *)application {
    // Restore simulation states and reload active shaders
}

- (void)applicationDidBecomeActive:(UIApplication *)application {
    // Resume simulation loop
}

- (void)applicationWillTerminate:(UIApplication *)application {
    // Tear down engine context and persist telemetry if needed
}

@end

int main(int argc, char * argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
    }
}
