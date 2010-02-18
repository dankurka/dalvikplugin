/*
 * Copyright (c) 2009 Timur Mehrvarz <timur.mehrvarz@web.de>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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

    bool unavailableHasPropertyAppletMethod;
    bool unavailableGetPropertyAppletMethod;
    bool unavailableHasMethodAppletMethod;

} PluginObject;

NPClass* getPluginClass(NPP instance);

#endif // PluginObject__DEFINED
