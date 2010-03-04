#ifndef PluginObject__DEFINED
#define PluginObject__DEFINED

#include "main.h"
#include <jni.h>

class SubPlugin {
public:
    SubPlugin(NPP inst) : m_inst(inst) {}
    virtual ~SubPlugin() {}
    virtual int16 handleEvent(const ANPEvent* evt) = 0;
    virtual bool supportsDrawingModel(ANPDrawingModel) = 0;

    NPP inst() const { return m_inst; }

private:
    NPP m_inst;
};

class SurfaceSubPlugin : public SubPlugin {
public:
    virtual bool isFixedSurface() = 0;
    virtual void surfaceCreated(JNIEnv*, jobject) = 0;
    virtual void surfaceChanged(int format, int width, int height) = 0;
    virtual void surfaceDestroyed() = 0;
};

static int     pluginObjectChildrenMax = 256;

typedef struct PluginObject {
    NPObject   header;
    NPP        npp;
    NPWindow*  window;

    char*      dynamicDexArchive;      // set in main.cpp by NPP_New(), e.g.: "http://timur.mobi/andr/BasicTest.jar";
                                       //  or in PluginObject.cpp by pluginGetProperty()

    char*      jobjectUniqueIdString;  // a unique identifier string for the instantiated java object instance
                                       //                    may be 1. the package.class string
                                       //                    or     2. #objaddr

    JavaVM*    vm;                     // set in main.cpp NPP_New()
    jobject    webviewGlob;            // set in jni-bridge surfaceCreated()
    jclass     dexLoaderClass;         // set in jni-bridge surfaceCreated()
    jclass     dexResultClass;         // set in main.cpp

    PluginObject** children;
    int            childrenCount;

    PluginObject*  rootNpObject;        // always points to the root PluginObject
                                        // allows all other PluginObject's to add themselfs to the root children-array

    char* applicationDataDirectory;

    bool unavailableHasPropertyAppletMethod;
    bool unavailableGetPropertyAppletMethod;
    bool unavailableHasMethodAppletMethod;

} PluginObject;

NPClass* getPluginClass(NPP instance);

#endif // PluginObject__DEFINED
