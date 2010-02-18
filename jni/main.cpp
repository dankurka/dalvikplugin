/*
 * Copyright (c) 2009 Timur Mehrvarz <timur.mehrvarz@web.de>
 * Copyright 2008, The Android Open Source Project
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "main.h"
#include "PluginObject.h"

NPNetscapeFuncs* browser;
#define EXPORT __attribute__((visibility("default")))

NPError NPP_New(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc, char* argn[], char* argv[], NPSavedData* saved);
NPError NPP_Destroy(NPP instance, NPSavedData** save);
NPError NPP_SetWindow(NPP instance, NPWindow* window);
NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16* stype);
NPError NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason);
int32   NPP_WriteReady(NPP instance, NPStream* stream);
int32   NPP_Write(NPP instance, NPStream* stream, int32 offset, int32 len, void* buffer);
void    NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname);
void    NPP_Print(NPP instance, NPPrint* platformPrint);
int16   NPP_HandleEvent(NPP instance, void* event);
void    NPP_URLNotify(NPP instance, const char* URL, NPReason reason, void* notifyData);
NPError NPP_GetValue(NPP instance, NPPVariable variable, void *value);
NPError NPP_SetValue(NPP instance, NPNVariable variable, void *value);

extern "C" {
EXPORT NPError NP_Initialize(NPNetscapeFuncs* browserFuncs, NPPluginFuncs* pluginFuncs, void* java_env, void *application_context);
EXPORT NPError NP_GetValue(NPP instance, NPPVariable variable, void *value);
EXPORT const char* NP_GetMIMEDescription(void);
EXPORT void NP_Shutdown(void);
};

ANPAudioTrackInterfaceV0    gSoundI;
ANPBitmapInterfaceV0        gBitmapI;
ANPCanvasInterfaceV0        gCanvasI;
ANPLogInterfaceV0           gLogI;
ANPPaintInterfaceV0         gPaintI;
ANPPathInterfaceV0          gPathI;
ANPSurfaceInterfaceV0       gSurfaceI;
ANPSystemInterfaceV0        gSystemI;
ANPTypefaceInterfaceV0      gTypefaceI;
ANPWindowInterfaceV0        gWindowI;

#define ARRAY_COUNT(array)      (sizeof(array) / sizeof(array[0]))

static JavaVM* jvm = NULL;
//static bool    needDexInit=true;

NPError NP_Initialize(NPNetscapeFuncs* browserFuncs, NPPluginFuncs* pluginFuncs, void* java_env, void* application_context)
{
  if(!pluginFuncs)
    return NPERR_GENERIC_ERROR;

  if(!browserFuncs)
    return NPERR_GENERIC_ERROR;

  if(browserFuncs->size < sizeof(NPNetscapeFuncs))
    return NPERR_GENERIC_ERROR;

  browser = (NPNetscapeFuncs*)malloc(sizeof(NPNetscapeFuncs));
  if(browser)
  {
    memcpy(browser, browserFuncs, sizeof(NPNetscapeFuncs));
    // we could check here, if browser->pluginthreadasynccall != NULL
  }

  pluginFuncs->version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
  pluginFuncs->size = sizeof(pluginFuncs);
  pluginFuncs->newp = NPP_New;
  pluginFuncs->destroy = NPP_Destroy;
  pluginFuncs->setwindow = NPP_SetWindow;
  pluginFuncs->newstream = NPP_NewStream;
  pluginFuncs->destroystream = NPP_DestroyStream;
  pluginFuncs->asfile = NPP_StreamAsFile;
  pluginFuncs->writeready = NPP_WriteReady;
  pluginFuncs->write = (NPP_WriteProcPtr)NPP_Write;
  pluginFuncs->print = NPP_Print;
  pluginFuncs->event = NPP_HandleEvent;
  pluginFuncs->urlnotify = NPP_URLNotify;
  pluginFuncs->getvalue = NPP_GetValue;
  pluginFuncs->setvalue = NPP_SetValue;

  static const struct {
    NPNVariable     v;
    uint32_t        size;
    ANPInterface*   i;
  } gPairs[] = {
    { kAudioTrackInterfaceV0_ANPGetValue,   sizeof(gSoundI),    &gSoundI },
    { kBitmapInterfaceV0_ANPGetValue,       sizeof(gBitmapI),   &gBitmapI },
    { kCanvasInterfaceV0_ANPGetValue,       sizeof(gCanvasI),   &gCanvasI },
    { kLogInterfaceV0_ANPGetValue,          sizeof(gLogI),      &gLogI },
    { kPaintInterfaceV0_ANPGetValue,        sizeof(gPaintI),    &gPaintI },
    { kPathInterfaceV0_ANPGetValue,         sizeof(gPathI),     &gPathI },
    { kSurfaceInterfaceV0_ANPGetValue,      sizeof(gSurfaceI),  &gSurfaceI },
    { kSystemInterfaceV0_ANPGetValue,       sizeof(gSystemI),   &gSystemI },
    { kTypefaceInterfaceV0_ANPGetValue,     sizeof(gTypefaceI), &gTypefaceI },
    { kWindowInterfaceV0_ANPGetValue,       sizeof(gWindowI),   &gWindowI },
  };

  for(size_t i = 0; i < ARRAY_COUNT(gPairs); i++)
  {
    gPairs[i].i->inSize = gPairs[i].size;
    NPError err = browser->getvalue(NULL, gPairs[i].v, gPairs[i].i);
                // calls to getValue() end up in /external/webkit/WebCore/plugins/android/PluginViewAndroid.cpp/getValue()
    if(err)
      return err;
  }

  JNIEnv* jniEnv = (JNIEnv *)java_env;
  if(jniEnv->GetJavaVM(&jvm) < 0)
  {
    jvm=NULL;
    return -1;
  }
  // jvm now accesible

  return NPERR_NO_ERROR;
}

void NP_Shutdown(void)
{
  //gLogI.log(NULL, kError_ANPLogType, "main NP_Shutdown");
}

const char* NP_GetMIMEDescription(void)
{
  return "application/x-dalvik-applet:tst:Dalvik plugin mimetype is application/x-dalvik-applet";
}


NPError NPP_New(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc, char* argn[], char* argv[], NPSavedData* saved)
{
  if(instance)
    gLogI.log(instance, kDebug_ANPLogType, "main NPP_New instance=%p browser=%p",instance,browser);

  char* archive = NULL;
  char* classid = NULL;

  // retrieve the <object> attributes
  for(int i=0; i<argc; i++)
  {
    if(instance)
      gLogI.log(instance, kDebug_ANPLogType, "main NPP_New arg %d [%s] [%s]",i,argn[i],argv[i]);
    if(!strcmp(argn[i], "archive"))
    {
      archive = argv[i];
    }
    else
    if(!strcmp(argn[i], "classid"))
    {
      classid = argv[i];
    }
  }

  PluginObject* pluginObj = NULL;

  if(instance)
  {
    // Scripting functions appeared in NPAPI version 14
    if(browser && browser->version >= 14)
    {
      gLogI.log(instance, kDebug_ANPLogType, "main NPP_New browser->version >= 14");
      instance->pdata = browser->createobject(instance, getPluginClass(instance));
      // note: pluginAllocate(), initializeIdentifiers() will be called on PluginObject
      pluginObj = static_cast<PluginObject*>(instance->pdata);
      pluginObj->children = new PluginObject*[pluginObjectChildrenMax];
      pluginObj->childrenCount = 0;
      gLogI.log(instance, kDebug_ANPLogType, "main NPP_New root pluginObj=%p",pluginObj);

      //bzero(pluginObj, sizeof(*pluginObj));
    }
    else
    {
      gLogI.log(instance, kDebug_ANPLogType, "main NPP_New browser->version < 14 no scripting support");
    }
  }

  if(!pluginObj)
  {
    // TODO:
    gLogI.log(instance, kError_ANPLogType, "main NPP_New vm pluginObj NULL ###################################");
    return -1;
  }

  // NEEDED FOR DRAWING
  // pluginObj->activePlugin = new PaintPlugin(instance);      // TODO rename PaintPlugin!

  pluginObj->dynamicDexArchive = archive;

  if(classid)
  {
    // cut off leading "java:"
    // TODO: make sure it really starts with "java:"
    if(*(classid+4)==':')
      classid +=5;
    // cut off trailing ".class"
    // TODO: make sure it really ends with ".class"
    *(classid+strlen(classid)-6)=0;
  }

  pluginObj->jobjectUniqueIdString = classid;
  if(instance)
    gLogI.log(instance, kDebug_ANPLogType, "main NPP_New pluginObj->jobjectUniqueIdString=%s",pluginObj->jobjectUniqueIdString);

//  pluginObj->rootNpObject = pluginObj;

  ANPDrawingModel anpDrawingModel = kSurface_ANPDrawingModel;
//ANPDrawingModel anpDrawingModel = kBitmap_ANPDrawingModel;

  // notify the plugin API of the location of the java interface
  // kSetPluginStubJavaClassName_ANPSetValue defined in "/external/webkit/WebKit/android/plugins/android_npapi.h"
  // -> Set the name of the Java class found in the plugin's apk that implements the PluginStub interface.
  // -> This value must be set prior to selecting the Surface_ANPDrawingModel or requesting to enter full-screen mode.
  if(browser && instance)
  {
    char* className = "com.android.dalvikplugin.DalvikPluginStub";        // todo: change this package- and classname
    gLogI.log(instance, kDebug_ANPLogType, "main NPP_New browser->setvalue() kSetPluginStubJavaClassName_ANPSetValue [%s]",className);
    NPError npErr = browser->setvalue(instance, kSetPluginStubJavaClassName_ANPSetValue, reinterpret_cast<void*>(className));
                // calls to setValue() end up in /external/webkit/WebCore/plugins/android/PluginViewAndroid.cpp/platformSetValue()
    if(npErr)
    {
      gLogI.log(instance, kError_ANPLogType, "main NPP_New ERROR set class %d", npErr);
      return npErr;
    }

    // notify the plugin API of the drawing model we wish to use. This must be
    // done prior to creating certain subPlugin objects (e.g. surfaceViews)
    //gLogI.log(instance, kDebug_ANPLogType, "main NPP_New browser->setvalue() kRequestDrawingModel_ANPSetValue");
    NPError err = browser->setvalue(instance, kRequestDrawingModel_ANPSetValue, reinterpret_cast<void*>(anpDrawingModel));
    if(err)
    {
      gLogI.log(instance, kError_ANPLogType, "main NPP_New request model %d err %d", anpDrawingModel, err);
      return err;
    }
  }

  //gLogI.log(instance, kDebug_ANPLogType, "main NPP_New gSystemI.getApplicationDataDirectory()");
  const char* path = gSystemI.getApplicationDataDirectory();
  if (path) {
    if(instance)
      gLogI.log(instance, kDebug_ANPLogType, "main NPP_New Application data dir is %s", path);
  } else {
    if(instance)
      gLogI.log(instance, kError_ANPLogType, "main NPP_New Can't find Application data dir");
  }


  gLogI.log(instance, kError_ANPLogType, "main NPP_New jvm=%p +++++++++++++++++++++++++++++++++",jvm);
  if(jvm)
  {
    PluginObject* plugin = pluginObj;
    pluginObj->vm = jvm;

    JNIEnv* jniEnv = NULL;
    jint result = pluginObj->vm->AttachCurrentThread(&jniEnv, NULL);
    if(result!=JNI_OK || jniEnv==NULL)
    {
      gLogI.log(instance, kError_ANPLogType, "main NPP_New jniEnv==NULL ###################################");
    }
    else
    {
      gLogI.log(instance, kError_ANPLogType, "main NPP_New jniEnv=%p",jniEnv);

/*
// we would like to initialize the VM here, but due to lack of 'pluginObj->vm' we do it in jni-bridge::createSurface()
//  gLogI.log(instance, kDebug_ANPLogType, "main NPP_New jniEnv=%p thiz=%p instance=%p surface=%p",jniEnv,thiz,instance,surface);

      // Get the WebView pointer. Note that this MUST execute on either
      // the browser main thread or a worker thread and only
      // from a script-invoked method call.
      jobject webview = NULL;
      NPError err = browser->getvalue(instance, NPNVnetscapeWindow, &webview);
      gLogI.log(instance, kDebug_ANPLogType, "main NPP_New NPNVnetscapeWindow err=%i",err);
      if(err!=NPERR_NO_ERROR)
      {
        gLogI.log(instance, kError_ANPLogType, "main NPP_New failed to retrieve webview #################################################");
        // TODO
      }
      else
      {
        //gLogI.log(instance, kDebug_ANPLogType, "main NPP_New err==NPERR_NO_ERROR webview=%p plugin=%p",webview,plugin);
        plugin->webviewGlob = (jobject)jniEnv->NewGlobalRef(webview);    // protected from garbage-collecting
        //gLogI.log(instance, kDebug_ANPLogType, "main NPP_New NewGlobalRef() webview=%p plugin->webviewGlob=%p",webview,plugin->webviewGlob);

        //////////////// retrieve and cache the plugin->dexLoaderClass, so that it can be used in PluginObject ///////////////
        if(plugin->dexLoaderClass==NULL)    // TODO ???
        {
          const char* dexLoaderClassName = "com/android/dalvikplugin/DexLoader";
          //gLogI.log(instance, kDebug_ANPLogType, "main NPP_New jniEnv->FindClass(%s) ...\n",dexLoaderClassName);
          jclass dexLoaderClass = jniEnv->FindClass(dexLoaderClassName);
          if(dexLoaderClass==NULL)
          {
            gLogI.log(instance, kError_ANPLogType, "main NPP_New FAIL dexLoaderClass==NULL #################################################");
          }
          else
          {
            //gLogI.log(instance, kDebug_ANPLogType, "main NPP_New dexLoaderClass!=NULL plugin=%p",plugin);
            plugin->dexLoaderClass = (jclass)jniEnv->NewGlobalRef(dexLoaderClass);    // protected from garbage-collecting

            {
              const char* dexResultClassName = "com/android/dalvikplugin/DexResult";
              //gLogI.log(instance, kDebug_ANPLogType, "main NPP_New jniEnv->FindClass(%s) ...\n",dexResultClassName);
              jclass dexResultClass = jniEnv->FindClass(dexResultClassName);
              if(dexResultClass!=NULL)
              {
                //gLogI.log(instance, kDebug_ANPLogType, "main NPP_New dexResultClass!=NULL plugin=%p",plugin);
                plugin->dexResultClass = (jclass)jniEnv->NewGlobalRef(dexResultClass);    // protected from garbage-collecting
              }
              else
              {
                gLogI.log(instance, kError_ANPLogType, "main NPP_New FAIL dexResultClass==NULL ###################################################");
              }
            }
          }
        }

        //gLogI.log(instance, kDebug_ANPLogType, "main NPP_New if plugin->dexLoaderClass!=NULL plugin=%p",plugin);
        if(plugin->dexLoaderClass!=NULL)
        {
          gLogI.log(instance, kDebug_ANPLogType, "main NPP_New plugin->dexLoaderClass=%p plugin->dynamicDexArchive=%s plugin->jobjectUniqueIdString=%s",
                                                      plugin->dexLoaderClass,plugin->dynamicDexArchive,plugin->jobjectUniqueIdString);
          // we got access to com.android.dalvikplugin.DexLoader
          // (4) now call invokeDex()

          const char* dexInvokerMethodName = "loadDexInit";
          //gLogI.log(instance, kDebug_ANPLogType, "main NPP_New GetStaticMethodID(%s)...",dexInvokerMethodName);
          jmethodID  dexLoaderMethod = jniEnv->GetStaticMethodID(plugin->dexLoaderClass, dexInvokerMethodName, "([Ljava/lang/String;Landroid/webkit/WebView;)V");
          if(dexLoaderMethod == NULL)
          {
            gLogI.log(instance, kError_ANPLogType, "main NPP_New JavaVM unable to find %s() in dexLoaderClass ################################################", dexLoaderMethod);
          }
          else
          {
            jobjectArray argArray = jniEnv->NewObjectArray(5, jniEnv->FindClass("java/lang/String"), jniEnv->NewStringUTF(plugin->dynamicDexArchive));    // 1st arg = remote jar-archive URL-string
            if(argArray != NULL)
            {
              jniEnv->SetObjectArrayElement(argArray, 1, jniEnv->NewStringUTF(plugin->jobjectUniqueIdString));   // 2nd arg = className-string
              jniEnv->SetObjectArrayElement(argArray, 2, jniEnv->NewStringUTF("-"));                       // 3rd arg = metnodeName-string  not needed for loadDex

              // prepare forwarding of the npp instance to DexLoader -> PluginAppletViewer -> Applet -> jni-bridge::invokeNative()
              char buffer [50];
              sprintf (buffer, "%p", instance);
              //gLogI.log(instance, kDebug_ANPLogType, "main NPP_New arg3=instance=%s",buffer);
              jniEnv->SetObjectArrayElement(argArray, 3, jniEnv->NewStringUTF(buffer));                   // 4th arg = npp

              //gLogI.log(instance, kDebug_ANPLogType, "main NPP_New use arguments [%s] [%s] [%s] [%s]...",plugin->dynamicDexArchive,plugin->jobjectUniqueIdString,"-",buffer);
              jniEnv->CallStaticVoidMethod(plugin->dexLoaderClass, dexLoaderMethod, argArray, plugin->webviewGlob);

              // loadDex() will load the applet, call init()
              // applet.init() may call "onJavaLoad()" in JS
              plugin->needDexInit=false;
            }
          }

        }
        else
        {
          gLogI.log(instance, kError_ANPLogType, "main NPP_New dexLoaderClass==NULL ################################################");
          // TODO
        }
      }
*/
    }
  }

  gLogI.log(instance, kDebug_ANPLogType, "main NPP_New done");

  return NPERR_NO_ERROR;
}

NPError NPP_Destroy(NPP instance, NPSavedData** save)
{
  if(instance)
  {
    gLogI.log(instance, kDebug_ANPLogType, "main NPP_Destroy() instance=%p",instance);

    NPError npErr = browser->setvalue(instance,kSetPluginStubJavaClassName_ANPSetValue,reinterpret_cast<void*>(NULL));
    if(npErr!=0)
      gLogI.log(instance, kDebug_ANPLogType, "main NPP_Destroy() kSetPluginStubJavaClassName_ANPSetValue=NULL err=%i #################",npErr);

    PluginObject* pluginObj = (PluginObject*)instance->pdata;
    gLogI.log(instance, kDebug_ANPLogType, "main NPP_Destroy() pluginObj=%p",pluginObj);
    if(pluginObj && browser)
    {
      JNIEnv* jniEnv = NULL;
      jint result = pluginObj->vm->AttachCurrentThread(&jniEnv, NULL);
      if(result==JNI_OK && jniEnv!=NULL)
      {
        jniEnv->ExceptionClear();   // todo: needed ??

        if(pluginObj->dexLoaderClass)
        {
/*
          // TODO: before calling DexLoader.shutdown(), we must make sure DexLoader.callStop() was called
          if(plugin->dexLoaderClass && !plugin->needDexInit)
          {
            const char* stopMethodName = "callStop";
            gLogI.log(instance, kDebug_ANPLogType, "jni-bridge surfaceDestroyed() GetStaticMethodID(%s)...",stopMethodName);
            jmethodID  stopMethod = jniEnv->GetStaticMethodID(plugin->dexLoaderClass, stopMethodName, "()V");
            if(stopMethod == NULL)
            {
              gLogI.log(instance, kError_ANPLogType, "jni-bridge surfaceCreated() JavaVM unable to find %s() via dexLoaderClass ################################################", stopMethodName);
            }
            else
            {
              jniEnv->CallStaticVoidMethod(plugin->dexLoaderClass, stopMethod, NULL);
            }
          }
*/

          // call DexLoader.shutdown()...
          gLogI.log(instance, kDebug_ANPLogType, "main NPP_Destroy() call DexLoader.shutdown...");

          const char* dexInvokerMethodName = "shutdown";
          gLogI.log(instance, kDebug_ANPLogType, "main NPP_Destroy() GetStaticMethodID dexInvokerMethodName=%s",dexInvokerMethodName);
          jmethodID  dexLoaderMethod = jniEnv->GetStaticMethodID(pluginObj->dexLoaderClass, dexInvokerMethodName, "()V");
          if(dexLoaderMethod == NULL)
          {
            gLogI.log(instance, kError_ANPLogType, "main NPP_Destroy() JavaVM unable to find %s() in dexLoaderClass #####################", dexInvokerMethodName);
          }
          else
          {
            jobjectArray argArray = jniEnv->NewObjectArray(5, jniEnv->FindClass("java/lang/String"), jniEnv->NewStringUTF(pluginObj->dynamicDexArchive));
            if(argArray != NULL)
            {
              gLogI.log(instance, kDebug_ANPLogType, "main NPP_Destroy() CallStaticVoidMethod [%s] ...", dexInvokerMethodName);
              jniEnv->CallStaticVoidMethod(pluginObj->dexLoaderClass, dexLoaderMethod, argArray, NULL);
              gLogI.log(instance, kDebug_ANPLogType, "main NPP_Destroy() CallStaticVoidMethod [%s] DONE", dexInvokerMethodName);
            }
          }

          gLogI.log(instance, kDebug_ANPLogType, "main NPP_Destroy() DeleteGlobalRef(pluginObj->dexLoaderClass) ...");
          jniEnv->DeleteGlobalRef(pluginObj->dexLoaderClass);
          //gLogI.log(instance, kDebug_ANPLogType, "main NPP_Destroy() pluginObj->dexLoaderClass=NULL ...");
          pluginObj->dexLoaderClass = NULL;
        }

        gLogI.log(instance, kDebug_ANPLogType, "main NPP_Destroy() pluginObj->dexResultClass=%p ...",pluginObj->dexResultClass);
        if(pluginObj->dexResultClass!=NULL)
        {
          jniEnv->DeleteGlobalRef(pluginObj->dexResultClass);
          //gLogI.log(instance, kDebug_ANPLogType, "main NPP_Destroy() pluginObj->dexResultClass=NULL ...");
          pluginObj->dexResultClass = NULL;
        }

        gLogI.log(instance, kDebug_ANPLogType, "main NPP_Destroy() pluginObj->webviewGlob=%p ...",pluginObj->webviewGlob);
        if(pluginObj->webviewGlob!=NULL)
        {
          jniEnv->DeleteGlobalRef(pluginObj->webviewGlob);
          //gLogI.log(instance, kDebug_ANPLogType, "main NPP_Destroy() pluginObj->webviewGlob=NULL ...");
          pluginObj->webviewGlob = NULL;
        }

        jniEnv->ExceptionClear();
        pluginObj->vm = NULL;
      }

      //gLogI.log(instance, kDebug_ANPLogType, "main NPP_Destroy() release sleep()...");
      //sleep(3);

      while(pluginObj->childrenCount-- > 0)
      {
        NPObject* npPluginObj = (NPObject*)pluginObj->children[pluginObj->childrenCount];
        gLogI.log(instance, kDebug_ANPLogType, "main NPP_Destroy() release child pluginObj[%i]=%p referenceCount=%u",pluginObj->childrenCount,npPluginObj,npPluginObj->referenceCount);
        browser->releaseobject(npPluginObj);
      }

      // release pluginObj object more than once, in case we retainobject pluginObj again for NPPVpluginScriptableNPObject
      NPObject* npPluginObj=(NPObject*)pluginObj;
      gLogI.log(instance, kDebug_ANPLogType, "main NPP_Destroy() release root pluginObj=%p referenceCount=%u",npPluginObj,npPluginObj->referenceCount);
      browser->releaseobject(npPluginObj);

      instance->pdata = NULL;
      gLogI.log(instance, kDebug_ANPLogType, "main NPP_Destroy() instance->pdata=%p set to NULL for instance=%p",instance->pdata,instance);
    }

    gLogI.log(instance, kDebug_ANPLogType, "main NPP_Destroy() done");
  }

  return NPERR_NO_ERROR;
}

NPError NPP_SetWindow(NPP instance, NPWindow* window)
{
  if(instance)
  {
    gLogI.log(instance, kDebug_ANPLogType, "main NPP_SetWindow instance=%p window=%p",instance,window);
    PluginObject* pluginObj = (PluginObject*)instance->pdata;
    // Do nothing if browser didn't support NPN_CreateObject which would have created the PluginObject.
    if(pluginObj)
    {
      pluginObj->window = window;

      if(browser)
      {
        if(window)
        {
          gLogI.log(instance, kDebug_ANPLogType, "main NPP_SetWindow invalidaterect x=%i y=%i w=%i h=%i",window->x,window->y,window->width,window->height);
          gLogI.log(instance, kDebug_ANPLogType, "main NPP_SetWindow invalidaterect clip top=%i left=%i bottom=%i right=%i",window->clipRect.top,window->clipRect.left,window->clipRect.bottom,window->clipRect.right);
          browser->invalidaterect(instance, &window->clipRect);
        }
        else
        {
          browser->invalidaterect(instance, NULL);
        }
        // -> DalvikPluginStub.java -> getEmbeddedView() -> nativeIsFixedSurface() -> jni-bridge.cpp call isFixedSurface()
      }

      gLogI.log(instance, kDebug_ANPLogType, "main NPP_SetWindow done");
      return NPERR_NO_ERROR;
    }

    gLogI.log(instance, kError_ANPLogType, "main NPP_SetWindow pluginObj==NULL BAD");
  }

  return NPERR_GENERIC_ERROR;
}

NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16* stype)
{
  *stype = NP_ASFILEONLY;
  return NPERR_NO_ERROR;
}

NPError NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason)
{
  return NPERR_NO_ERROR;
}

int32 NPP_WriteReady(NPP instance, NPStream* stream)
{
  return 0;
}

int32 NPP_Write(NPP instance, NPStream* stream, int32 offset, int32 len, void* buffer)
{
  return 0;
}

void NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname)
{
}

void NPP_Print(NPP instance, NPPrint* platformPrint)
{
}

int16 NPP_HandleEvent(NPP instance, void* event)
{
  //gLogI.log(instance, kDebug_ANPLogType, "main NPP_HandleEvent instance=%p ...",instance);
  PluginObject* pluginObj = reinterpret_cast<PluginObject*>(instance->pdata);
  const ANPEvent* evt = reinterpret_cast<const ANPEvent*>(event);

/*
  if(evt)
  {
    switch (evt->eventType)
    {
      case kDraw_ANPEventType:
        if (evt->data.draw.model == kBitmap_ANPDrawingModel)
        {
          static ANPBitmapFormat currentFormat = -1;
          if (evt->data.draw.data.bitmap.format != currentFormat)
          {
            currentFormat = evt->data.draw.data.bitmap.format;
            if(instance)
              gLogI.log(instance, kDebug_ANPLogType, "main NPP_HandleEvent instance=%p Draw (bitmap) clip=%d,%d,%d,%d format=%d", instance,
                        evt->data.draw.clip.left,
                        evt->data.draw.clip.top,
                        evt->data.draw.clip.right,
                        evt->data.draw.clip.bottom,
                        evt->data.draw.data.bitmap.format);
          }
        }
        break;

      case kKey_ANPEventType:
        if(instance)
          gLogI.log(instance, kDebug_ANPLogType, "main NPP_HandleEvent instance==%p Key action=%d code=%d vcode=%d unichar=%d repeat=%d mods=%x", instance,
                    evt->data.key.action,
                    evt->data.key.nativeCode,
                    evt->data.key.virtualCode,
                    evt->data.key.unichar,
                    evt->data.key.repeatCount,
                    evt->data.key.modifiers);
          break;

      case kLifecycle_ANPEventType:
        if(evt)
          //if(evt->data)
            //if(evt->data.lifecycle)
              if(instance)
                gLogI.log(instance, kDebug_ANPLogType, "main NPP_HandleEvent instance=%p Lifecycle action=%d", instance, evt->data.lifecycle.action);
        break;

      case kTouch_ANPEventType:
        if(instance)
          gLogI.log(instance, kDebug_ANPLogType, "main NPP_HandleEvent instance=%p Touch action=%d [%d %d]", instance, evt->data.touch.action, evt->data.touch.x, evt->data.touch.y);
        break;

      case kMouse_ANPEventType:
        if(instance)
          gLogI.log(instance, kDebug_ANPLogType, "main NPP_HandleEvent instance=%p Mouse action=%d [%d %d]", instance, evt->data.mouse.action, evt->data.mouse.x, evt->data.mouse.y);
        break;

//       case kVisibleRect_ANPEventType:
//            gLogI.log(instance, kDebug_ANPLogType, "NPP_HandleEvent %p VisibleRect [%d %d %d %d]",
//                      instance, evt->data.visibleRect.rect.left, evt->data.visibleRect.rect.top,
//                      evt->data.visibleRect.rect.right, evt->data.visibleRect.rect.bottom);
//            break;

      default:
        if(instance)
          gLogI.log(instance, kDebug_ANPLogType, "main NPP_GetValue instance=%p Unknown Event [%d]", instance, evt->eventType);
        break;
    }
  }
*/

/* tmtm ???
  if(instance)
    gLogI.log(instance, kDebug_ANPLogType, "NPP_HandleEvent if(!pluginObj->activePlugin)... pluginObj=%p",pluginObj);
  if(!pluginObj->activePlugin)
  {
    if(instance)
      gLogI.log(instance, kError_ANPLogType, "NPP_HandleEvent the active plugin is null +++++++++++++++++");
    return 0; // unknown or unhandled event
  }

  // TODO: not call this if browser session is about to shut down? tmtm
  if(instance && evt && pluginObj && pluginObj->activePlugin)
  {
    gLogI.log(instance, kDebug_ANPLogType, "NPP_HandleEvent return pluginObj->activePlugin->handleEvent(evt)");
    int16 ret = pluginObj->activePlugin->handleEvent(evt);
    gLogI.log(instance, kDebug_ANPLogType, "NPP_HandleEvent return ret=%i",ret);
    return ret;
  }
*/

  //gLogI.log(instance, kDebug_ANPLogType, "main NPP_HandleEvent NO EVT PROCESSING return 0");
  return 0;
}

void NPP_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData)
{
  if(instance)
    gLogI.log(instance, kError_ANPLogType, "main NPP_URLNotify()...");
}

EXPORT NPError NP_GetValue(NPP instance, NPPVariable variable, void* value)
{
  if(instance)
    gLogI.log(instance, kError_ANPLogType, "main NP_GetValue()...");

  if(variable == NPPVpluginNameString)
  {
    const char **str = (const char **)value;
    *str = "Dalvik Plugin";
    return NPERR_NO_ERROR;
  }

  if(variable == NPPVpluginDescriptionString)
  {
    const char **str = (const char **)value;
    *str = "Description of Dalvik Plugin";        // TODO: better description string?
    return NPERR_NO_ERROR;
  }

  if(instance)
    gLogI.log(instance, kError_ANPLogType, "main NP_GetValue NPERR_GENERIC_ERROR");
  return NPERR_GENERIC_ERROR;
}

NPError NPP_GetValue(NPP instance, NPPVariable variable, void* value)
{
  if(!instance)
    return NPERR_GENERIC_ERROR;

  if(variable == NPPVpluginScriptableNPObject)
  {
    // A plugin that wishes to be scriptable using this new mechanism needs to return the appropriate NPObject
    gLogI.log(instance, kDebug_ANPLogType, "main NPP_GetValue NPPVpluginScriptableNPObject... (15 -> BROWSER CHECKS IF THIS PLUGIN SUPPORTS SCRIPTING ACCESS? YES)");
    PluginObject* pluginObj = (PluginObject*)instance->pdata;    // as created in NPP_New by browser->createobject()
    if(pluginObj)
    {
      void** v = (void**)value;
      *v = pluginObj;
      if(!browser)
        return NPERR_GENERIC_ERROR;
      browser->retainobject((NPObject*)pluginObj);
      gLogI.log(instance, kDebug_ANPLogType, "main NPP_GetValue NPPVpluginScriptableNPObject browser->retainobject done obj=%p",*v);
      return NPERR_NO_ERROR;
    }
  }

/*
  // TODO ?
    case NPPVpluginNameString:
        *((char **)value) = STRINGS_PRODUCTNAME;
        return NPERR_NO_ERROR;
    case NPPVpluginDescriptionString:    // Plugin description
        *((char **)value) = STRINGS_FILEDESCRIPTION;
        return NPERR_NO_ERROR;
    case NPPVpluginWindowBool:
        *((PRBool *)value) = this->isWindowed;
        return NPERR_NO_ERROR;
*/

  gLogI.log(instance, kDebug_ANPLogType, "main NPP_GetValue variable=%d not handled +++++++++++++++++++++++++++++",variable);
  return NPERR_GENERIC_ERROR;
}

NPError NPP_SetValue(NPP instance, NPNVariable variable, void *value)
{
  if(instance)
    gLogI.log(instance, kError_ANPLogType, "main NPP_SetValue NPERR_GENERIC_ERROR +++++++++++++++++++++++++++++++++++");
  return NPERR_GENERIC_ERROR;
}
