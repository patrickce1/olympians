 #ifndef FirebaseBridge_mm
#define FirebaseBridge_mm

#import "FirebaseBridge.h"
#import <Foundation/Foundation.h>

/**
 * Initializes the Firebase application.
 *
 * Configures Firebase using the settings provided in GoogleService-Info.plist.
 * This must be called once at application startup, before any Firebase
 * services such as Firestore or Authentication are accessed.
 *
 * Internally calls [FIRApp configure], which is the Objective-C entry
 * point for Firebase initialization.
 */
void initFirebase() {
    Class firApp = NSClassFromString(@"FIRApp");
    if (firApp) {
        [firApp performSelector:@selector(configure)];
    }
}

#endif /* FirebaseBridge_mm */
