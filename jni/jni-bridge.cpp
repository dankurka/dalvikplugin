/*
 * Copyright (C) 2009 Timur Mehrvarz <timur.mehrvarz@web.de>
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <string.h>
#include <jni.h>
#include <JNIHelp.h>
#include <utils/Log.h>

#include <stdlib.h>
#include "main.h"
#include "PluginObject.h"
extern ANPLogInterfaceV0        gLogI;

#include "PluginObject.h"

#define EXPORT __attribute__((visibility("default")))

static bool    needDexInit=true;

static void surfaceCreated(JNIEnv* jniEnv, jobject thiz, jint npp, jobject surface)
{
  NPP instance = (NPP)npp;
  if(!instance)
    return;

  gLogI.log(instance, kDebug_ANPLogType, "jni-bridge surfaceCreated() jniEnv=%p thiz=%p instance=%p surface=%p",jniEnv,thiz,instance,surface);

  //gLogI.log(instance, kDebug_ANPLogType, "jni-bridge surfaceCreated() vm=%p",vm);
  PluginObject* plugin = static_cast<PluginObject*>(instance->pdata);
  //gLogI.log(instance, kDebug_ANPLogType, "jni-bridge surfaceCreated() plugin=%p",plugin);
  if(!plugin)
  {
    gLogI.log(instance, kError_ANPLogType, "jni-bridge surfaceCreated() plugin is NULL ##################################");
    return;
  }

  if(/*plugin->*/needDexInit)
  {
    JavaVM* vm = NULL;
    jniEnv->GetJavaVM(&vm);
    if(!vm)
    {
      gLogI.log(instance, kError_ANPLogType, "jni-bridge surfaceCreated() vm is NULL ###################################");
      return;
    }

/* */
    plugin->vm = vm;
    gLogI.log(instance, kDebug_ANPLogType, "jni-bridge surfaceCreated() plugin->vm set vm=%p",vm);


    // Get the WebView pointer. Note that this MUST execute on either
    // the browser main thread or a worker thread and only
    // from a script-invoked method call.
    jobject webview = NULL;
    NPError err = browser->getvalue(instance, NPNVnetscapeWindow, &webview);
    gLogI.log(instance, kDebug_ANPLogType, "jni-bridge surfaceCreated() NPNVnetscapeWindow err=%i",err);
    if(err==NPERR_NO_ERROR)
    {
      //gLogI.log(instance, kDebug_ANPLogType, "jni-bridge surfaceCreated() err==NPERR_NO_ERROR webview=%p plugin=%p",webview,plugin);
      plugin->webviewGlob = (jobject)jniEnv->NewGlobalRef(webview);    // protected from garbage-collecting
      //gLogI.log(instance, kDebug_ANPLogType, "jni-bridge surfaceCreated() NewGlobalRef() webview=%p plugin->webviewGlob=%p",webview,plugin->webviewGlob);

      // todo: do we have to releaseobject(webview) ???
    }
    else
    {
      gLogI.log(instance, kError_ANPLogType, "jni-bridge surfaceCreated() failed to retrieve webview via NPNVnetscapeWindow #################################################");
      return;
    }

    //////////////// retrieve and cache the plugin->dexLoaderClass, so that it can be used in PluginObject ///////////////
    //if(plugin->dexLoaderClass==NULL)    // TODO ???
    {
      const char* dexLoaderClassName = "com/android/dalvikplugin/DexLoader";
      //gLogI.log(instance, kDebug_ANPLogType, "jni-bridge surfaceCreated() jniEnv->FindClass(%s) ...\n",dexLoaderClassName);
      jclass dexLoaderClass = jniEnv->FindClass(dexLoaderClassName);
      if(dexLoaderClass!=NULL)
      {
        //gLogI.log(instance, kDebug_ANPLogType, "jni-bridge surfaceCreated() dexLoaderClass!=NULL plugin=%p",plugin);
        plugin->dexLoaderClass = (jclass)jniEnv->NewGlobalRef(dexLoaderClass);    // protected from garbage-collecting
      }
      else
      {
        gLogI.log(instance, kError_ANPLogType, "jni-bridge surfaceCreated() FAIL dexLoaderClass==NULL #################################################");
        // TODO
      }

      const char* dexResultClassName = "com/android/dalvikplugin/DexResult";
      //gLogI.log(instance, kDebug_ANPLogType, "jni-bridge surfaceCreated() jniEnv->FindClass(%s) ...\n",dexResultClassName);
      jclass dexResultClass = jniEnv->FindClass(dexResultClassName);
      if(dexResultClass!=NULL)
      {
        //gLogI.log(instance, kDebug_ANPLogType, "jni-bridge surfaceCreated() dexResultClass!=NULL plugin=%p",plugin);
        plugin->dexResultClass = (jclass)jniEnv->NewGlobalRef(dexResultClass);    // protected from garbage-collecting
      }
      else
      {
        gLogI.log(instance, kError_ANPLogType, "jni-bridge surfaceCreated() FAIL dexResultClass==NULL ###################################################");
        // TODO
      }
    }

    //gLogI.log(instance, kDebug_ANPLogType, "jni-bridge surfaceCreated() if plugin->dexLoaderClass!=NULL plugin=%p",plugin);
    if(plugin->dexLoaderClass!=NULL)
    {
      gLogI.log(instance, kDebug_ANPLogType, "jni-bridge surfaceCreated() plugin->dexLoaderClass=%p plugin->dynamicDexArchive=%s plugin->jobjectUniqueIdString=%s",
                                                  plugin->dexLoaderClass,plugin->dynamicDexArchive,plugin->jobjectUniqueIdString);
      // we got access to com.android.dexloader.DexLoader
      // (4) now call invokeDex()

      const char* dexInvokerMethodName = "loadDexInit";
      //gLogI.log(instance, kDebug_ANPLogType, "jni-bridge surfaceCreated() GetStaticMethodID(%s)...",dexInvokerMethodName);
      jmethodID  dexLoaderMethod = jniEnv->GetStaticMethodID(plugin->dexLoaderClass, dexInvokerMethodName, "([Ljava/lang/String;Landroid/webkit/WebView;)V");
      if(dexLoaderMethod == NULL)
      {
        gLogI.log(instance, kError_ANPLogType, "jni-bridge surfaceCreated() JavaVM unable to find %s() in dexLoaderClass ################################################", dexLoaderMethod);
      }
      else
      {
        jobjectArray argArray = jniEnv->NewObjectArray(5, jniEnv->FindClass("java/lang/String"), jniEnv->NewStringUTF(plugin->dynamicDexArchive));    // 1st arg = remote jar-archive URL-string
        if(argArray == NULL)
        {
          gLogI.log(instance, kError_ANPLogType, "jni-bridge surfaceCreated() argArray = jniEnv->NewObjectArray() == NULL ################################################");
        }
        else
        {
          jniEnv->SetObjectArrayElement(argArray, 1, jniEnv->NewStringUTF(plugin->jobjectUniqueIdString));   // 2nd arg = className-string
          jniEnv->SetObjectArrayElement(argArray, 2, jniEnv->NewStringUTF("-"));                       // 3rd arg = metnodeName-string  not needed for loadDex

          // prepare forwarding of the npp instance to DexLoader -> PluginAppletViewer -> Applet -> jni-bridge::invokeNative()
          char buffer [50];
          sprintf (buffer, "%p", instance);
          gLogI.log(instance, kDebug_ANPLogType, "jni-bridge surfaceCreated() arg3=instance=%s instance=%i",buffer,instance);
          jniEnv->SetObjectArrayElement(argArray, 3, jniEnv->NewStringUTF(buffer));                   // 4th arg = npp

          //gLogI.log(instance, kDebug_ANPLogType, "jni-bridge use arguments [%s] [%s] [%s] [%s]...",plugin->dynamicDexArchive,plugin->jobjectUniqueIdString,"-",buffer);
          jniEnv->CallStaticVoidMethod(plugin->dexLoaderClass, dexLoaderMethod, argArray, plugin->webviewGlob);

          // loadDex() will load the applet, call init()
          //           applet.init() will call "onJavaLoad()" in JS
          /*plugin->*/needDexInit=false;
        }
      }
    }
    else
    {
      gLogI.log(instance, kError_ANPLogType, "jni-bridge surfaceCreated() dexLoaderClass==NULL ################################################");
      // TODO
    }
/* */

    if(plugin->dexLoaderClass && !/*plugin->*/needDexInit)
    {
      // call applet start()
      const char* startMethodName = "callStart";
      gLogI.log(instance, kDebug_ANPLogType, "jni-bridge surfaceCreated() GetStaticMethodID(%s)...",startMethodName);
      jmethodID  startMethod = jniEnv->GetStaticMethodID(plugin->dexLoaderClass, startMethodName, "()V");
      if(startMethod == NULL)
      {
        gLogI.log(instance, kError_ANPLogType, "jni-bridge surfaceCreated() JavaVM unable to find %s() via dexLoaderClass ################################################", startMethodName);
      }
      else
      {
        jniEnv->CallStaticVoidMethod(plugin->dexLoaderClass, startMethod, NULL);
      }
    }
  }

  //// NEEDED FOR DRAWING
  //SurfaceSubPlugin* obj = getPluginObject(npp);
  //if(obj && jniEnv)
  //  obj->surfaceCreated(jniEnv, surface);    // call PaintPlugin.cpp surfaceCreated()

//  plugin->surfaceActive = true;

  gLogI.log(instance, kDebug_ANPLogType, "jni-bridge surfaceCreated() done");
}


static void surfaceChanged(JNIEnv* jniEnv, jobject thiz, jint npp, jint format, jint width, jint height)
{
  NPP instance = (NPP)npp;
  if(!instance)
    return;
  gLogI.log(instance, kDebug_ANPLogType, "jni-bridge surfaceChanged()...");

  //// NEEDED FOR DRAWING
  //SurfaceSubPlugin* obj = getPluginObject(npp);
  //if(obj)
  //  obj->surfaceChanged(format, width, height);
  //gLogI.log(instance, kDebug_ANPLogType, "jni-bridge surfaceChanged() done");
}

static void surfaceDestroyed(JNIEnv* jniEnv, jobject thiz, jint npp)
{
  // give up all handles to the VM and any VM objects!

  NPP instance = (NPP)npp;
  if(!instance)
    return;

  gLogI.log(instance, kDebug_ANPLogType, "jni-bridge surfaceDestroyed() instance=%p ...",instance);

  if(instance->pdata!=NULL)
  {
    PluginObject* plugin = static_cast<PluginObject*>(instance->pdata);
    gLogI.log(instance, kDebug_ANPLogType, "jni-bridge surfaceDestroyed() plugin=%p",plugin);
  }

  gLogI.log(instance, kDebug_ANPLogType, "jni-bridge surfaceDestroyed done");
}


// this is being called from DalvikPluginStub.java
static jboolean isFixedSurface(JNIEnv* env, jobject thiz, jint npp)
{
  NPP instance = (NPP)npp;
  if(!instance)
    return false;

  return false;
}




////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct InvokeNativeObject {
  NPP npp;              // npp instance from surfaceCreated()
  NPObject* jsobj;      // JS object
  char methodName[256];
//  jobject[] args;
} InvokeNativeObject;

static void invokeNativeAsync(void* param)
{
  if(param && browser)
  {
    InvokeNativeObject* invokeNativeObject = (InvokeNativeObject*)param;
    NPP instance = invokeNativeObject->npp;   // root instance, npp from surfaceCreated()

    if(instance)
    {
      NPObject* jsobj = invokeNativeObject->jsobj;
      const char* methodName = invokeNativeObject->methodName;
      //jobject[] args = invokeNativeObject->args;    // TODO

      if(methodName)
        if(strlen(methodName)<1)
          methodName=NULL;
/*
      if(methodName)
      {
        gLogI.log(instance, kDebug_ANPLogType, "jni-bridge invokeNativeAsync browser=%p methodname=%s ...GODDO GODDO +++++++++++++++++++++++++++++",browser,methodName);
      }
      else
      {
        gLogI.log(instance, kDebug_ANPLogType, "jni-bridge invokeNativeAsync browser=%p ...GODDO GODDO +++++++++++++++++++++++++++++",browser);
      }
*/

      NPError error = 0;
      NPObject* windowObject = NULL;
      bool fetchedWindowObject = false;
      if(instance==(NPP)jsobj)
      {
        // need to create windowObject
        //gLogI.log(instance, kDebug_ANPLogType, "jni-bridge invokeNativeAsync need to fetch windowObject +++++++++++++++++++++++++++++");
        error = browser->getvalue(instance, NPNVWindowNPObject, &windowObject);       // todo: to be fetched each time?
        if(!error)
        {
          fetchedWindowObject = true;
          //gLogI.log(instance, kDebug_ANPLogType, "jni-bridge invokeNativeAsync fetched windowObject=%p windowObject->referenceCount=%i +++++++++++++++++++++++++++++",windowObject,windowObject->referenceCount);
        }
        else
        {
          gLogI.log(instance, kError_ANPLogType, "jni-bridge invokeNativeAsync fetched windowObject=%p error=%d ##################################",windowObject,error);
        }
      }
      else
        windowObject = jsobj;

      if(error || !windowObject)
      {
        gLogI.log(instance, kError_ANPLogType, "jni-bridge invokeNativeAsync with methodName=%s Unable to retrieve DOM Window windowObject==NULL ################################", methodName);
      }
      else
      {
        // call back into JavaScript
        NPVariant result;

        if(methodName)
        {
          //NPVariant alertMessage;
          //STRINGZ_TO_NPVARIANT("Success!", alertMessage);
          //browser->invoke(instance, windowObject, browser->getstringidentifier("alert"), &alertMessage, 1, &result);      // TODO: use this to implement handing over arguments

          //gLogI.log(instance, kDebug_ANPLogType, "jni-bridge invokeNativeAsync browser->invoke() jsobj=%p methodName=%s",jsobj,methodName);
          browser->invoke(instance,                                  // npp
                          windowObject,                                     // NPObject* windowScriptObject
                          browser->getstringidentifier(methodName),  // NPIdentifier methodName
                          0,                                         // const NPVariant* args      // TODO: handing over arguments when calling JS
                          0,                                         // uint32_t argCount          // TODO
                          &result);                                  // NPVariant*
          //gLogI.log(instance, kDebug_ANPLogType, "jni-bridge invokeNativeAsync browser->invoke() result type=%d intval=%d ...",result.type,result.value.intValue);
        }
        else
        {
          //gLogI.log(instance, kDebug_ANPLogType, "jni-bridge invokeNativeAsync without methodName browser->invokeDefault() jsobj=%p",jsobj);
          browser->invokeDefault(instance,                           // npp
                          windowObject,                                     // NPObject* windowScriptObject
                          0,                                         // const NPVariant* args      // TODO: handing over arguments when calling JS
                          0,                                         // uint32_t argCount          // TODO
                          &result);                                  // NPVariant*
          //gLogI.log(instance, kDebug_ANPLogType, "jni-bridge invokeNativeAsync without methodName browser->invokeDefault() result type=%d intval=%d ...",result.type,result.value.intValue);
        }

        browser->releasevariantvalue(&result);

        if(fetchedWindowObject)
        {
          //gLogI.log(instance, kDebug_ANPLogType, "jni-bridge invokeNativeAsync release fetched windowObject windowObject->referenceCount=%i... +++++++++++++++++++++++++++++",windowObject->referenceCount);
          browser->releaseobject(windowObject);
          windowObject=NULL;
        }
      }
    }

    free(param);
    if(instance)
    {
      //gLogI.log(instance, kDebug_ANPLogType, "jni-bridge invokeNativeAsync done");
    }
  }
}

/*
typedef struct _NPP
{
    void*    pdata;            // plug-in private data
    void*    ndata;            // netscape private data
} NPP_t;

typedef NPP_t*    NPP;
*/

// called by PluginAppletViewer.java nativeInvokeNative()
static jboolean invokeNative(JNIEnv* jniEnv,
                             jobject thiz,            // TODO ???
                             jint jnpp,               // root npp instance given to jni-bridge::surfaceCreated() -> handed over to DexLoader.loadDexInit()
                             jint jsobj_addr,         // via JsObject from pluginAppletViewer.getWindow();
                             jstring jmethodName,     // may be null
                             jobject args[])
{
  NPP npp = (NPP)jnpp;
  if(npp)
  {
    if(jniEnv && browser && browser->pluginthreadasynccall)
    {
      InvokeNativeObject* invokeNativeObject = (InvokeNativeObject*)malloc(sizeof(InvokeNativeObject));
      if(invokeNativeObject)
      {
        const char* methodName = NULL;
        if(jmethodName)
          methodName = jniEnv->GetStringUTFChars(jmethodName, NULL);

//        if(methodName)
//          gLogI.log(npp, kDebug_ANPLogType, "jni-bridge invokeNative() npp=%p jsobj_addr=%p methodname=%s ...",npp,jsobj_addr,methodName);
//        else
//          gLogI.log(npp, kDebug_ANPLogType, "jni-bridge invokeNative() npp=%p jsobj_addr=%p ...",npp,jsobj_addr);

        bzero(invokeNativeObject, sizeof(InvokeNativeObject));
        invokeNativeObject->npp = npp;
        invokeNativeObject->jsobj = (NPObject*)jsobj_addr;
        if(methodName)
          strcpy(invokeNativeObject->methodName,methodName);
        //invokeNativeObject->args = args;    // todo

        //gLogI.log(npp, kDebug_ANPLogType, "jni-bridge invokeNative() npp=%p jsobj=%p use pluginthreadasynccall()...",npp,invokeNativeObject->jsobj);
        browser->pluginthreadasynccall(npp, &invokeNativeAsync, (void*)invokeNativeObject);
        // invokeNativeObject may now be freed
        //gLogI.log(npp, kDebug_ANPLogType, "jni-bridge invokeNative() called ... ");

        if(jmethodName)
          jniEnv->ReleaseStringUTFChars(jmethodName, methodName);
        //gLogI.log(npp, kDebug_ANPLogType, "jni-bridge invokeNative() done");
        return true;
      }
    }

    gLogI.log(npp, kDebug_ANPLogType, "jni-bridge invokeNative() done fail");
  }
  return false;
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// JNI registration: register the native functions, which will be called by DalvikPluginStub.java

static JNINativeMethod gJavaDalvikPluginStubMethods[] =
{
  { "nativeSurfaceCreated", "(ILandroid/view/View;)V", (void*)surfaceCreated },
  { "nativeSurfaceChanged", "(IIII)V", (void*)surfaceChanged },
  { "nativeSurfaceDestroyed", "(I)V", (void*)surfaceDestroyed },
  { "nativeIsFixedSurface", "(I)Z", (void*)isFixedSurface }
};

static JNINativeMethod gJavaDalvikPluginStubMethods2[] =
{
  { "nativeInvokeNative", "(IILjava/lang/String;[Ljava/lang/Object;)Z", (void*)invokeNative }     // String methodName, Object args[]
};

EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
  JNIEnv* jniEnv = NULL;

  if(vm->GetEnv((void**) &jniEnv, JNI_VERSION_1_4) != JNI_OK)
    return -1;

  // class DalvikPluginStub.java will now be able to call the native methods from gJavaDalvikPluginStubMethods
  jniRegisterNativeMethods(jniEnv, "com/android/dalvikplugin/DalvikPluginStub",
                           gJavaDalvikPluginStubMethods, NELEM(gJavaDalvikPluginStubMethods));

  // class PluginAppletViewer.java will now be able to call the native methods from gJavaDalvikPluginStubMethods2
  jniRegisterNativeMethods(jniEnv, "dalvik/applet/PluginAppletViewer",
                           gJavaDalvikPluginStubMethods2, NELEM(gJavaDalvikPluginStubMethods2));

  return JNI_VERSION_1_4;
}
