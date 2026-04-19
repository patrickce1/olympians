#ifndef FirebaseBridge_h
#define FirebaseBridge_h

/**
 * Initializes the Firebase application.
 *
 * This function configures Firebase using the settings provided in
 * GoogleService-Info.plist. It must be called once before any other
 * Firebase services (e.g. Firestore, Auth) are used.
 */
void initFirebase();

#endif /* FirebaseBridge_h */
