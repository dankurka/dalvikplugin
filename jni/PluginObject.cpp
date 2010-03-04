#include <stdlib.h>
#include <stdio.h>
#include <utils/Vector.h>
#include <jni.h>

#include "main.h"
#include "PluginObject.h"
extern ANPLogInterfaceV0       gLogI;

static void pluginInvalidate(NPObject *obj);
static bool pluginHasProperty(NPObject *obj, NPIdentifier name);
static bool pluginHasMethod(NPObject *obj, NPIdentifier name);
static bool pluginGetProperty(NPObject *obj, NPIdentifier name, NPVariant *variant);
static bool pluginSetProperty(NPObject *obj, NPIdentifier name, const NPVariant *variant);
static bool pluginInvoke(NPObject *obj, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result);
static bool pluginInvokeDefault(NPObject *obj, const NPVariant *args, uint32_t argCount, NPVariant *result);
static NPObject* pluginAllocate(NPP npp, NPClass *theClass);
static void pluginDeallocate(NPObject *obj);
static bool pluginRemoveProperty(NPObject *npobj, NPIdentifier name);
static bool pluginEnumerate(NPObject *npobj, NPIdentifier **value, uint32_t *count);
static bool pluginConstruct(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);

static NPClass pluginClass = {
    NP_CLASS_STRUCT_VERSION,        // as defined in /external/webkit/V8Binding/npapi/npruntime.h or /external/webkit/WebCore/bridge/npruntime.h as 3
    pluginAllocate,
    pluginDeallocate,
    pluginInvalidate,
    pluginHasMethod,
    pluginInvoke,
    pluginInvokeDefault,
    pluginHasProperty,
    pluginGetProperty,
    pluginSetProperty,
    pluginRemoveProperty,
    pluginEnumerate,
    pluginConstruct
};

static NPP myNppInstance = NULL;

NPClass* getPluginClass(NPP instance)
{
  // this is called by main.cpp
  myNppInstance = instance;
  //if(myNppInstance!=NULL) { gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject getPluginClass()"); }
  return &pluginClass;
}

static jclass globalStringClass = NULL;
static jobject globalDexArchiveString = NULL;
static jmethodID jObjectOfID = NULL;
static jmethodID jObjectHasPropertyID = NULL;
static jmethodID jObjectHasMethodID = NULL;
static jmethodID jObjectHasMethodWithArgsID = NULL;
static jmethodID jObjectTypeOfMethodID = NULL;
static jmethodID setObjectOfClassID = NULL;
static jmethodID loadClassID = NULL;
static char* signatureJavaLangString = "Ljava/lang/String;";

static jmethodID dexGetObjectMethod = NULL;

static int callJava(JNIEnv* jniEnv, NPObject* obj, const char* dexMethod, uint32_t argCount, const NPVariant* args, NPVariant* result, bool throwExceptionOnMethodNotFound, bool log)
{
  int ret = -1;   // unspecified error

  if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava dexMethod=%s with numberOfArgs=%i ...",dexMethod,argCount);
  PluginObject* plugin = (PluginObject *)obj;
  //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava AttachCurrentThread plugin=%p dexMethod=%s argCount=%i ...",plugin,dexMethod,argCount);

  // try to get the java object instance for obj...
  // native code is now attached to the JVM and we got a valid jniENV
  //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava GetStaticMethodID 'objectOfClass'...");
  if(jObjectOfID==NULL)
  {
    jObjectOfID = jniEnv->GetStaticMethodID(plugin->dexLoaderClass, "objectOfClass", "(Ljava/lang/String;)Ljava/lang/Object;");
    if(jObjectOfID==NULL)
    {
      gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject callJava GetStaticMethodID() 'objectOfClass' not found ####################");
      return -2; // this is a fatal error, since DexLoader.objectOfClass() should always be available
    }
  }


  //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava CallStaticObjectMethod(objectOfClass) class='%s' ...",plugin->jobjectUniqueIdString);
  jstring jDynamicDexClass = jniEnv->NewStringUTF(plugin->jobjectUniqueIdString);

  // try to get the object from classname via objectOfClass hashmap in
  //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava jDynamicDexClass='%p' ...",jDynamicDexClass);
  jobject jvmObj = (jobject)(jniEnv->CallStaticObjectMethod(plugin->dexLoaderClass, jObjectOfID, jDynamicDexClass));
  // this is equivelent to calling:  (Object)objectHashMap.get(jDynamicDexClass);
  //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava jvmObj='%p' (the java instance for this scriptable obj) ...",jvmObj);
  if(jvmObj)
  {
    // a java object for this scriptable obj was instantiated!

    //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava got jvmObj, jvmClass = GetObjectClass(jvmObj) ...");
    jclass jvmClass = jniEnv->GetObjectClass(jvmObj);
    //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava use direct Java reflection to check if the named method '%s' is available...",dexMethod);
    if(jObjectHasMethodWithArgsID==NULL)
    {
      jObjectHasMethodWithArgsID = jniEnv->GetStaticMethodID(plugin->dexLoaderClass, "objectHasMethodWithArgs", "(Ljava/lang/String;Ljava/lang/String;I[IZ)Ljava/lang/reflect/Method;");
      if(jObjectHasMethodWithArgsID==NULL)
      {
        gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject callJava 'objectHasMethodWithArgs' not found ####################");
        //return -2; // this is a fatal error, since DexLoader.objectOfClass() should always be available
      }
    }

    jobject jMethodObject = NULL;
    char returnTypeSig[256];
    jint npVariantTypeArray[argCount];

    if(jObjectHasMethodWithArgsID!=NULL)
    {
      //jstring jDynamicDexClass = jniEnv->NewStringUTF(plugin->jobjectUniqueIdString);
      // try to get the java.lang.reflect.Method for dexMethod in class jDynamicDexClass from objectOfClass hashmap in DexLoader
      if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava jObjectHasMethodWithArgsID jDynamicDexClass='%p' dexMethod=%s ...",jDynamicDexClass,dexMethod);
      for(uint32_t i=0; i<argCount; i++)
      {
        //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava arg[%i] = %d",i,args[i].type);
        npVariantTypeArray[i] = (jint)args[i].type;
      }
      jintArray npVariantTypeArray2 = jniEnv->NewIntArray(argCount);
      jniEnv->SetIntArrayRegion(npVariantTypeArray2, 0, argCount, npVariantTypeArray);
      jMethodObject = jniEnv->CallStaticObjectMethod(plugin->dexLoaderClass, jObjectHasMethodWithArgsID, jDynamicDexClass, jniEnv->NewStringUTF(dexMethod),argCount,npVariantTypeArray2, log);
      if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava jObjectHasMethodWithArgsID dexMethod=%s reflectMethod=%i +++++++++++++++++++++++++++++++++",dexMethod,jMethodObject);


      // autoboxing!
      // todo: this autoboxing code is too trivial. it needs to go argument by argument (instead of converting multiple args together)
      if(!jMethodObject)
      {
        // try to find same method but all args of type NPVariantType_Double instead with type NPVariantType_Int32
        if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava try to find same method but with NPVariantType_Int32 args instead of NPVariantType_Double...");
        for(uint32_t i=0; i<argCount; i++)
        {
          if(npVariantTypeArray[i]==NPVariantType_Double)   // 4
            npVariantTypeArray[i]=NPVariantType_Int32;      // 3
        }
        jniEnv->SetIntArrayRegion(npVariantTypeArray2, 0, argCount, npVariantTypeArray);
        jMethodObject = jniEnv->CallStaticObjectMethod(plugin->dexLoaderClass, jObjectHasMethodWithArgsID, jDynamicDexClass, jniEnv->NewStringUTF(dexMethod),argCount,npVariantTypeArray2, log);
        if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava jObjectHasMethodWithArgsID dexMethod=%s reflectMethod=%i +++++++++++++++++++++++++++++++++",dexMethod,jMethodObject);

        if(!jMethodObject)
        {
          // try to find same method but all args of type NPVariantType_Int32 instead with type int
          if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava try to find same method but with int args instead of NPVariantType_Int32...");
          for(uint32_t i=0; i<argCount; i++)
          {
            if(npVariantTypeArray[i]==NPVariantType_Int32)  // 3
              npVariantTypeArray[i]=7;                      // 7 = int
          }
          jniEnv->SetIntArrayRegion(npVariantTypeArray2, 0, argCount, npVariantTypeArray);
          jMethodObject = jniEnv->CallStaticObjectMethod(plugin->dexLoaderClass, jObjectHasMethodWithArgsID, jDynamicDexClass, jniEnv->NewStringUTF(dexMethod),argCount,npVariantTypeArray2, log);
          if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava jObjectHasMethodWithArgsID dexMethod=%s reflectMethod=%i +++++++++++++++++++++++++++++++++",dexMethod,jMethodObject);
        }
      }

      if(jMethodObject)
      {
        if(jObjectTypeOfMethodID==NULL)
        {
          jObjectTypeOfMethodID = jniEnv->GetStaticMethodID(plugin->dexLoaderClass, "objectTypeOfMethod", "(Ljava/lang/reflect/Method;Z)Ljava/lang/String;");
          if(jObjectTypeOfMethodID==NULL)
          {
            gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject callJava 'objectTypeOfMethod' not found ####################");
            //return -2; // this is a fatal error, since DexLoader.objectOfClass() should always be available
          }
        }

        if(jObjectTypeOfMethodID!=NULL)
        {
          //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava jObjectTypeOfMethodID to get typeString of method=%s ...",dexMethod);
          jstring jStr = (jstring)jniEnv->CallStaticObjectMethod(plugin->dexLoaderClass, jObjectTypeOfMethodID, jMethodObject, log);
          const char* cValueString = jniEnv->GetStringUTFChars(jStr, NULL);
          if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava jObjectTypeOfMethodID typeString=%s ...",cValueString);
          strcpy(returnTypeSig,cValueString);
        }
      }
      else
        jniEnv->ExceptionClear();   // erase possible java.lang.NoSuchMethodException
    }

    if(!jMethodObject)
    {
      return -10; // the named method is not available in the applet code
    }



    // construct a method-signature based on the args-array for call to CallxxxxxMethodA(jvmClass,dexMethod,jargs)
    char signatureBase[512];
    strcpy(signatureBase,"(");
    for(uint32_t i=0; i<argCount; i++)
    {
      //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava args[%i].type=%i",i,args[i].type);

      if(npVariantTypeArray[i]==NPVariantType_Bool)
      {
        //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava args[%i].type=Bool",i);
        strcat(signatureBase,"Ljava/lang/Boolean;");
      }
      else
      if(npVariantTypeArray[i]==NPVariantType_Int32)
      {
        //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava args[%i].type=Int32",i);
        strcat(signatureBase,"Ljava/lang/Integer;");
      }
      else
      if(npVariantTypeArray[i]==NPVariantType_Double)
      {
        //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava args[%i].type=Double",i);
        strcat(signatureBase,"Ljava/lang/Double;");
      }
      else
      if(npVariantTypeArray[i]==NPVariantType_String)
      {
        //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava args[%i].type=String",i);
        strcat(signatureBase,signatureJavaLangString);
      }
      else
      if(npVariantTypeArray[i]==NPVariantType_Object)
      {
        //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava args[%i].type=Object",i);
        strcat(signatureBase,"Ljava/lang/Object;");
      }
      else
      if(npVariantTypeArray[i]==7)
      {
        //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava args[%i].type=Object",i);
        strcat(signatureBase,"I");
      }
      else
      {
        gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava args[%i].type=%i ###################### UNHANDLED",i,args[i].type);
      }
    }
    strcat(signatureBase,")");

/*
    // attempt to find the one method with the right return type...
    char signature[512];
    char returnTypeSig[256];
    strcpy(returnTypeSig,"Z");
    sprintf(signature,"%s%s",signatureBase,returnTypeSig);
    //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava GetMethodID(%s) signature='%s' ...",dexMethod,signature);
    jmethodID jMethodID = jniEnv->GetMethodID(jvmClass, dexMethod, signature);
    //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava GetMethodID() signature='%s' jMethodID=%i ...",signature,jMethodID);
    if(jMethodID==0)
    {
      jniEnv->ExceptionClear();
      strcpy(returnTypeSig,"V");
      sprintf(signature,"%s%s",signatureBase,returnTypeSig);
      //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava GetMethodID(%s) signature='%s' ...",dexMethod,signature);
      jMethodID = jniEnv->GetMethodID(jvmClass, dexMethod, signature);
      //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava GetMethodID() signature='%s' jMethodID=%i ...",signature,jMethodID);
      if(jMethodID==0)
      {
        jniEnv->ExceptionClear();
        strcpy(returnTypeSig,"S");
        sprintf(signature,"%s%s",signatureBase,returnTypeSig);
        //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava GetMethodID(%s) signature='%s' ...",dexMethod,signature);
        jMethodID = jniEnv->GetMethodID(jvmClass, dexMethod, signature);
        //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava GetMethodID() signature='%s' jMethodID=%i ...",signature,jMethodID);
        if(jMethodID==0)
        {
          jniEnv->ExceptionClear();
          strcpy(returnTypeSig,"J");
          sprintf(signature,"%s%s",signatureBase,returnTypeSig);
          //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava GetMethodID(%s) signature='%s' ...",dexMethod,signature);
          jMethodID = jniEnv->GetMethodID(jvmClass, dexMethod, signature);
          //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava GetMethodID() signature='%s' jMethodID=%i ...",signature,jMethodID);
          if(jMethodID==0)
          {
            jniEnv->ExceptionClear();
            strcpy(returnTypeSig,"I");
            sprintf(signature,"%s%s",signatureBase,returnTypeSig);
            //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava GetMethodID() signature='%s' ...",signature);
            jMethodID = jniEnv->GetMethodID(jvmClass, dexMethod, signature);
            //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava GetMethodID() signature='%s' jMethodID=%i ...",signature,jMethodID);
            if(jMethodID==0)
            {
              jniEnsignatureBasev->ExceptionClear();
              strcpy(returnTypeSig,"F");
              sprintf(signature,"%s%s",signatureBase,returnTypeSig);
              //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava GetMethodID() signature='%s' ...",signature);
              jMethodID = jniEnv->GetMethodID(jvmClass, dexMethod, signature);
              //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava GetMethodID() signature='%s' jMethodID=%i ...",signature,jMethodID);
              if(jMethodID==0)
              {
                jniEnv->ExceptionClear();
                strcpy(returnTypeSig,"D");
                sprintf(signature,"%s%s",signatureBase,returnTypeSig);
                //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava GetMethodID() signature='%s' ...",signature);
                jMethodID = jniEnv->GetMethodID(jvmClass, dexMethod, signature);
                //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava GetMethodID() signature='%s' jMethodID=%i ...",signature,jMethodID);
                if(jMethodID==0)
                {
                  jniEnv->ExceptionClear();
                  strcpy(returnTypeSig,"C");
                  sprintf(signature,"%s%s",signatureBase,returnTypeSig);
                  //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava GetMethodID() signature='%s' ...",signature);
                  jMethodID = jniEnv->GetMethodID(jvmClass, dexMethod, signature);
                  //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava GetMethodID() signature='%s' jMethodID=%i ...",signature,jMethodID);
                  if(jMethodID==0)
                  {
                    jniEnv->ExceptionClear();
                    strcpy(returnTypeSig,"B");
                    sprintf(signature,"%s%s",signatureBase,returnTypeSig);
                    //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava GetMethodID() signature='%s' ...",signature);
                    jMethodID = jniEnv->GetMethodID(jvmClass, dexMethod, signature);
                    //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava GetMethodID() signature='%s' jMethodID=%i ...",signature,jMethodID);
                    if(jMethodID==0)
                    {
                      jniEnv->ExceptionClear();
                      strcpy(returnTypeSig,"Ljava/lang/Object;");
                      sprintf(signature,"%s%s",signatureBase,returnTypeSig);
                      gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava GetMethodID() signature='%s' ...",signature);
                      jMethodID = jniEnv->GetMethodID(jvmClass, dexMethod, signature);
                      gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava GetMethodID() signature='%s' jMethodID=%i ...",signature,jMethodID);
                      if(jMethodID==0)
                      {
                        jniEnv->ExceptionClear();
                        //strcpy(returnTypeSig,"Ljava/lang/String;");
                        strcpy(returnTypeSig,signatureJavaLangString);
                        sprintf(signature,"%s%s",signatureBase,returnTypeSig);
                        //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava GetMethodID() signature='%s' ...",signature);
                        jMethodID = jniEnv->GetMethodID(jvmClass, dexMethod, signature);
                        //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava GetMethodID() signature='%s' jMethodID=%i ...",signature,jMethodID);
                        if(jMethodID==0)
                        {
                          jniEnv->ExceptionClear();
                          strcpy(returnTypeSig,"[Ljava/lang/Object;");    // object array
                          sprintf(signature,"%s%s",signatureBase,returnTypeSig);
                          //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava GetMethodID() signature='%s' ...",signature);
                          jMethodID = jniEnv->GetMethodID(jvmClass, dexMethod, signature);
                          //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava GetMethodID() signature='%s' jMethodID=%i ...",signature,jMethodID);
                          if(jMethodID==0)
                          {
                            jniEnv->ExceptionClear();     // todo: or would it be better to NOT clear any exception here?

                            if(throwExceptionOnMethodNotFound)
                            {
                              gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava throwExceptionOnMethodNotFound, need to fetch windowObject");
                              NPObject* windowObject = NULL;
                              NPError error = browser->getvalue(myNppInstance, NPNVWindowNPObject, &windowObject);
                              if(!error && windowObject)
                              {
                                char str[256]; sprintf(str,"method '%s' not found",dexMethod);
                                gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava browser->setexception(%s)",str);
                                browser->setexception(windowObject, str);
                                browser->releaseobject(windowObject);
                                windowObject=NULL;
                              }
                              else
                              {
                                gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject callJava failed to fetch windowObject error=%d ##################################",error);
                              }
                            }

                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
*/

    char signature[512];
    sprintf(signature,"%s%s",signatureBase,returnTypeSig);
    if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava GetMethodID(%s) signature='%s' ...",dexMethod,signature);
    jmethodID jMethodID = jniEnv->GetMethodID(jvmClass, dexMethod, signature);
    if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava GetMethodID(%s) signature='%s' jMethodID=%i ...",dexMethod,signature,jMethodID);
    if(!jMethodID)
      return -10; // the named method is not available in the applet code

    // the named method with the expected arguments is available in the applet class

    // we may need to create Java Objects via loadClassID, in order to hand them over as arguments
    if(loadClassID==NULL)
    {
      loadClassID = jniEnv->GetStaticMethodID(plugin->dexLoaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
      if(loadClassID==NULL)
      {
        gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject callJava GetStaticMethodID() 'loadClass' not found ####################");
        return -3; // this is fatal, since DexLoader.loadClass() should always be available
      }
    }

    // construct our method arguments array...
    jvalue jargs[argCount];
    void* allocArray[argCount];
    int allocArrayCount=0;
    for(uint32_t i=0; i<argCount; i++)
    {
      if(npVariantTypeArray[i]==NPVariantType_Bool)
      {
        bool bval = NPVARIANT_TO_BOOLEAN(args[i]);
        //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava NPVARIANT_TO_BOOLEAN bval=[%i] ...",bval);
        jclass javaBooleanClass = (jclass)(jniEnv->CallStaticObjectMethod(plugin->dexLoaderClass, loadClassID, jniEnv->NewStringUTF("java.lang.Boolean")));
        //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava NPVARIANT_TO_BOOLEAN javaBooleanClass=%p ...",javaBooleanClass);
        if(javaBooleanClass)
        {
          // get the constructor of the java.lang.Boolean class
          jmethodID mID = jniEnv->GetMethodID(javaBooleanClass, "<init>", "(Z)V");   // constructor
          //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava NPVARIANT_TO_BOOLEAN mID=%p ...",mID);
          if(mID!=NULL)
          {
            // create a java.lang.Boolean object by calling the constructor and handing over the address of npObj
            //jargs[i].l = jniEnv->NewObject(javaBooleanClass, mID, bval);
            jobject localRefCls = jniEnv->NewObject(javaBooleanClass, mID, bval);
            jargs[i].l = jniEnv->NewGlobalRef(localRefCls);
            jniEnv->DeleteLocalRef(localRefCls);
          }
        }
      }

      else
      if(npVariantTypeArray[i]==NPVariantType_Double)
      {
        double dblval = NPVARIANT_TO_DOUBLE(args[i]);
        //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava NPVARIANT_TO_DOUBLE dblval=[%f] ...",dblval);
        jclass javaDoubleClass = (jclass)(jniEnv->CallStaticObjectMethod(plugin->dexLoaderClass, loadClassID, jniEnv->NewStringUTF("java.lang.Double")));
        //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava NPVARIANT_TO_OBJECT javaDoubleClass=%p ...",javaDoubleClass);
        if(javaDoubleClass)
        {
          // get the constructor of the java.lang.Double class
          jmethodID mID = jniEnv->GetMethodID(javaDoubleClass, "<init>", "(D)V");   // constructor
          //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava NPVARIANT_TO_OBJECT mID=%p ...",mID);
          if(mID!=NULL)
          {
            // create a java.lang.Double object by calling the constructor and handing over the address of npObj
            //jargs[i].l = jniEnv->NewObject(javaDoubleClass, mID, dblval);
            jobject localRefCls = jniEnv->NewObject(javaDoubleClass, mID, dblval);
            jargs[i].l = jniEnv->NewGlobalRef(localRefCls);
            jniEnv->DeleteLocalRef(localRefCls);
          }
        }
      }

      else
      if(npVariantTypeArray[i]==NPVariantType_String)
      {
        NPString npStr = NPVARIANT_TO_STRING(args[i]);
        const NPUTF8* npUtf8 = npStr.UTF8Characters;
        char* str = (char*)malloc(npStr.UTF8Length+1);
        strncpy(str,npUtf8,npStr.UTF8Length);
        *(str+npStr.UTF8Length)=0;
        allocArray[allocArrayCount++]=str;
        jobject localRefCls = jniEnv->NewStringUTF(str);
        jargs[i].l = jniEnv->NewGlobalRef(localRefCls);     // todo: this works fine, why do we NOT have to create a Java object?
        jniEnv->DeleteLocalRef(localRefCls);
        //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava NPVARIANT_TO_STRING str=[%s] npStr.UTF8Length=%i ...",str,npStr.UTF8Length);
      }

      else
      if(npVariantTypeArray[i]==NPVariantType_Object)
      {
        // instantiate an JSObject and store npObj-addr as npp (instance / internal)
        jobject jsObj = NULL;
        NPObject* npObj = NPVARIANT_TO_OBJECT(args[i]);
        //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava NPVARIANT_TO_OBJECT npObj=%p ...",npObj);
        jclass jsObjectClass = (jclass)(jniEnv->CallStaticObjectMethod(plugin->dexLoaderClass, loadClassID, jniEnv->NewStringUTF("netscape.javascript.JSObject")));
        //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava NPVARIANT_TO_OBJECT jsObjectClass=%p ...",jsObjectClass);
        if(jsObjectClass)
        {
          // get the constructor of the JSObject class
          jmethodID constructurMethodID = jniEnv->GetMethodID(jsObjectClass, "<init>", "(I)V");
          //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava NPVARIANT_TO_OBJECT constructurMethodID=%p ...",constructurMethodID);
          if(constructurMethodID!=NULL)
          {
            browser->retainobject(npObj);     // this may for example be a JS function callback, TODO: when should this be released??? probably never.

            // create a JSObject object by calling the jsObjectClass constructor and handing over the address of npObj as an int
            jsObj = jniEnv->NewObject(jsObjectClass, constructurMethodID, (int)npObj);
            jargs[i].l = jniEnv->NewGlobalRef(jsObj);
            jniEnv->DeleteLocalRef(jsObj);
            //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava NPVARIANT_TO_OBJECT done");
          }
        }
      }

      else
      if(npVariantTypeArray[i]==7)  // int
      {
// original type:  args[i].type
// boxed type:     npVariantTypeArray[i]
        if(args[i].type==NPVariantType_Double)
        {
          double dblval = NPVARIANT_TO_DOUBLE(args[i]);
          jargs[i].i = (int)dblval;
          gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava jargs[%i] to int NPVARIANT_TO_DOUBLE cast int %i ++++++++++++++++++++++++++++++++++++++++++++++",i,jargs[i].i);
        }
        else
        if(args[i].type==NPVariantType_Int32)
        {
          jargs[i].i = NPVARIANT_TO_INT32(args[i]);
          gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava jargs[%i] to int NPVARIANT_TO_INT32 %i ++++++++++++++++++++++++++++++++++++++++++++++",i,jargs[i].i);
        }
        else
        {
          gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject callJava jargs[%i] to int NOT HANDLED args[%i].type = %i #############################",i,args[i].type);
        }
      }

      else
      {
        //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava NPVARIANT_TO_BOOLEAN bval=[%i] ...",bval);
      }
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////
    // now call the named applet method...
    // TODO: list of returntype cases may not be complete
    //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava returnTypeSig=%s ...",returnTypeSig);
    if(!strcmp(returnTypeSig,"Z"))  // Boolean
    {
      jboolean retBool = JNI_FALSE;
      //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava CallBooleanMethodA() ...");
      retBool = jniEnv->CallBooleanMethodA(jvmObj, jMethodID, jargs);
      if(jniEnv->ExceptionCheck())
      {
        gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject callJava CallBooleanMethodA() EXCEPTION #############################");
        jniEnv->ExceptionDescribe();
        // todo: throw JS exception
        jniEnv->ExceptionClear();
      }
      else
      {
        if(retBool==JNI_TRUE)
        {
          ret = 1;    // processing OK, methode available
          //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava CallBooleanMethod() ret=1");
        }
        else
        {
          ret = 0;    // processing OK, method not available
          //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava CallBooleanMethod() ret=0");
        }
      }
    }
    else
    if(!strcmp(returnTypeSig,"I"))  // int
    {
      jint retInt = 0;
      //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava CallIntMethodA() ...");
      retInt = jniEnv->CallIntMethodA(jvmObj, jMethodID, jargs);
      if(jniEnv->ExceptionCheck())
      {
        gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject callJava CallIntMethodA() EXCEPTION #############################");
        jniEnv->ExceptionDescribe();
        // todo: throw JS exception
        jniEnv->ExceptionClear();
      }
      else
      {
        if(result)
        {
          INT32_TO_NPVARIANT(retInt, *result);
          ret = 0;  // processing OK
        }
        else
        {
          gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject callJava CallIntMethodA() retInt=[%i] no resultObject ################",retInt);
        }
      }
    }
    else
    if(!strcmp(returnTypeSig,"V"))  // void
    {
      //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava CallVoidMethodA() ...");
      jniEnv->CallVoidMethodA(jvmObj, jMethodID, jargs);
      if(jniEnv->ExceptionCheck())
      {
        gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject callJava CallVoidMethodA() EXCEPTION #############################");
        jniEnv->ExceptionDescribe();
        // todo: throw JS exception
        jniEnv->ExceptionClear();
      }
      else
      {
        ret = 0;      // processing OK
      }
    }
    else
    if(!strcmp(returnTypeSig,signatureJavaLangString))
    {
      //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava jstring=CallObjectMethodA() ...");
      jstring jStr = (jstring)jniEnv->CallObjectMethodA(jvmObj, jMethodID, jargs);
      if(jniEnv->ExceptionCheck())
      {
        gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject callJava CallObjectMethodA() for String EXCEPTION #############################");
        jniEnv->ExceptionDescribe();
        // todo: throw JS exception
        jniEnv->ExceptionClear();
      }
      else
      {
        const char* cValueString = jniEnv->GetStringUTFChars(jStr, NULL);
        //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava CallObjectMethodA() cValueString=[%s]",cValueString);
        if(result)
        {
          STRINGZ_TO_NPVARIANT(cValueString, *result);
          ret = 0;  // processing OK
        }
        else
        {
          gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject callJava CallObjectMethodA() for String cValueString=[%s] no resultObject ################",cValueString);
        }
      }
    }
    else
//    if(!strcmp(returnTypeSig,"Ljava/lang/Object;")
//    || !strcmp(returnTypeSig,"[Ljava/lang/Object;")     // TODO: not sure about this
//      )
    {
      // treat everything else as an object

      //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava jobject=CallObjectMethodA() ...");
      jobject jObj = (jobject)jniEnv->CallObjectMethodA(jvmObj, jMethodID, jargs);
      if(jniEnv->ExceptionCheck())
      {
        gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject callJava CallObjectMethodA() EXCEPTION #############################");
        jniEnv->ExceptionDescribe();
        // todo: throw JS exception
        jniEnv->ExceptionClear();
      }
      else
      if(jObj)
      {
        //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava CallObjectMethodA() jObj=[%p]",jObj);

        NPObject* newScriptableObject = (NPObject*)browser->createobject(myNppInstance, &pluginClass);
        if(newScriptableObject)
        {
          browser->retainobject(newScriptableObject);

          PluginObject* thisPluginObject = (PluginObject *)newScriptableObject;
          PluginObject* parentPlugin = (PluginObject *)obj;

          thisPluginObject->vm = parentPlugin->vm;
          thisPluginObject->dexLoaderClass = parentPlugin->dexLoaderClass;
          thisPluginObject->dexResultClass = parentPlugin->dexResultClass;
//          thisPluginObject->dexGetObjectMethod = parentPlugin->dexGetObjectMethod;
          thisPluginObject->window = parentPlugin->window;
          thisPluginObject->webviewGlob = parentPlugin->webviewGlob;
          thisPluginObject->rootNpObject = parentPlugin->rootNpObject;

          // add this npobject to it's root-parent's npobject children array
          if(thisPluginObject->rootNpObject->childrenCount<pluginObjectChildrenMax)
            thisPluginObject->rootNpObject->children[thisPluginObject->rootNpObject->childrenCount++] = thisPluginObject;

          jclass jObjClass = jniEnv->GetObjectClass(jObj);
          //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava jObjClass=%p...",jObjClass);

          jmethodID jHashCodeMethodID = jniEnv->GetMethodID(jvmClass, "hashCode", "()I");
          //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava jHashCodeMethodID=%p...",jHashCodeMethodID);

          jint jHashCode = (jint)(jniEnv->CallIntMethod(jObj, jHashCodeMethodID, NULL));
          //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava jHashCode=%i...",jHashCode);

          char* valueIdentifier = (char*)malloc(256);
          sprintf(valueIdentifier,"#%i",jHashCode);
          // todo: put this object into DexLoader.objectHashMap.put()
          // objectOfClass

          if(setObjectOfClassID==NULL)
          {
            //gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject callJava GetStaticMethodID() 'setObjectOfClass'...");
            setObjectOfClassID = jniEnv->GetStaticMethodID(plugin->dexLoaderClass, "setObjectOfClass", "(Ljava/lang/String;Ljava/lang/Object;)V");
            if(setObjectOfClassID==NULL)
            {
              gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject callJava GetStaticMethodID() 'setObjectOfClass' not found ####################");
              return -4; // this is fatal, since DexLoader.setObjectOfClass() should always be available
            }
          }

          //gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject callJava setObjectOfClassID=%p CallStaticVoidMethod() ...",setObjectOfClassID);
          jstring jValueIdentifier = jniEnv->NewStringUTF(valueIdentifier);
          jniEnv->CallStaticVoidMethod(plugin->dexLoaderClass, setObjectOfClassID, jValueIdentifier, jObj);

          //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava valueIdentifier=%s...",valueIdentifier);
          ((PluginObject*)newScriptableObject)->jobjectUniqueIdString = valueIdentifier;

          //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava uniqueIdString=[%s] OBJECT_TO_NPVARIANT...",((PluginObject*)newScriptableObject)->jobjectUniqueIdString);
          if(result)
          {
            OBJECT_TO_NPVARIANT(newScriptableObject, *result);
            ret = 0;  // processing OK
          }
          else
          {
            gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject callJava OBJECT_TO_NPVARIANT no resultObject ###########################");
          }

          //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava OBJECT_TO_NPVARIANT DONE");
        }
      }
      else
      {
        //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava jObj==NULL -> *result=NULL");
        NULL_TO_NPVARIANT(*result);
      }
    }
/*
    else
    {
      gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject callJava UNHANDLED returnTypeSig=[%s] #############################",returnTypeSig);
    }
*/

    // free all global refs
    for(uint32_t i=0; i<argCount; i++)
      jniEnv->DeleteGlobalRef(jargs[i].l);

    // free all allocated memory
    if(allocArrayCount>0)
    {
      for(int i=0; i<allocArrayCount; i++)
        free(allocArray[i]);
      //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava %i free(allocArray) done",allocArrayCount);
    }
  }

  // todo: do we need to release jDynamicDexClass ??? maybe just deleteLocalReference?
  //jniEnv->ReleaseStringUTFChars(jDynamicDexClass,plugin->jobjectUniqueIdString);
  return ret;
}


static jobject getJavaObject(NPObject* obj, NPUTF8* idName, JNIEnv* jniEnv)
{
  // call DexLoader.java:getObject(plugin->dynamicDexArchive,plugin->jobjectUniqueIdString,idName)
  // and get the property with the name idName in the Java object-instance of class plugin->jobjectUniqueIdString of the given scriptable obj
  // example:            getObject("JilApplet-debug.apk","org.timur.jil.Jil","Device")

  PluginObject* plugin = (PluginObject *)obj;
  if(idName!=NULL && jniEnv!=NULL && plugin!=NULL)
  {
    //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject getJavaObject plugin->dexLoaderClass=%p plugin->jobjectUniqueIdString=%s", plugin->dexLoaderClass,plugin->jobjectUniqueIdString);
    jniEnv->ExceptionClear();

    if(plugin->dexLoaderClass!=NULL)
    {
      // we got access to com.android.dexloader.DexLoader

      if(/*plugin->*/dexGetObjectMethod==NULL)
      {
        const char* dexGetObjectMethodName = "getObject";
        char signature[256];
        sprintf(signature, "([Ljava/lang/String;)Ljava/lang/Object;");
        //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject getJavaObject GetStaticMethodID(%s) signature=[%s]",dexGetObjectMethodName,signature);
        /*plugin->*/ dexGetObjectMethod = jniEnv->GetStaticMethodID(plugin->dexLoaderClass, dexGetObjectMethodName, signature);
        if(/*plugin->*/ dexGetObjectMethod == NULL)
        {
          gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject getJavaObject JavaVM unable to find %s() in dexLoaderClass ##################################", dexGetObjectMethodName);
        }
      }

      if(/*plugin->*/dexGetObjectMethod==NULL)
      {
        gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject getJavaObject dexGetObjectMethod==NULL ##################################");
      }
      else
      {
        //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject getJavaObject dexGetObjectMethod!=NULL globalStringClass=%p",globalStringClass);

        if(globalStringClass==NULL)
        {
          jclass stringClass = jniEnv->FindClass("java/lang/String");
          if(stringClass)
            globalStringClass = (jclass)jniEnv->NewGlobalRef(stringClass);
        }

        if(globalDexArchiveString==NULL)
        {
          jobject localDexArchiveString = jniEnv->NewStringUTF(plugin->dynamicDexArchive);
          if(localDexArchiveString)
            globalDexArchiveString = jniEnv->NewGlobalRef(localDexArchiveString);
        }

        jobjectArray argArray = jniEnv->NewObjectArray(3, globalStringClass, globalDexArchiveString);    // 1st arg = remote jar-archive URL-string
        if(argArray==NULL)
        {
          gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject getJavaObject argArray==NULL ##################################");
        }
        else
        {
          //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject getJavaObject set argArray ++++++++++++++++");
          jobjectArray globalArgArray = (jobjectArray)jniEnv->NewGlobalRef(argArray);
          jniEnv->DeleteLocalRef(argArray);

          jobject localUTFUniqueIdString = jniEnv->NewStringUTF(plugin->jobjectUniqueIdString);
          jobject localUTFidName = jniEnv->NewStringUTF(idName);

          jniEnv->SetObjectArrayElement(globalArgArray, 1, localUTFUniqueIdString);  // 2nd arg = className-string   TODO: this should be UNiQUE
          jniEnv->SetObjectArrayElement(globalArgArray, 2, localUTFidName);          // 3rd arg = fieldname

          // todo: why is plugin->dynamicDexArchive = (null) in some cases?
          //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject getJavaObject use arguments [%s] [%s] [%s] ...",plugin->dynamicDexArchive,plugin->jobjectUniqueIdString,idName);
          ////jniEnv->ExceptionClear();

          // receive a DexResult.java struct containing the object, a string descriing it's type and a possible error-code
          //return (jobject)(jniEnv->CallStaticObjectMethod(plugin->dexLoaderClass, plugin->dexGetObjectMethod, globalArgArray));
          jobject localDexResult = (jobject)(jniEnv->CallStaticObjectMethod(plugin->dexLoaderClass, /*plugin->*/dexGetObjectMethod, globalArgArray));
          jobject globalDexResult = NULL;
          if(localDexResult)
          {
            globalDexResult = jniEnv->NewGlobalRef(localDexResult);
            jniEnv->DeleteLocalRef(localDexResult);
          }

          jniEnv->DeleteLocalRef(localUTFidName);
          jniEnv->DeleteLocalRef(localUTFUniqueIdString);
          jniEnv->DeleteGlobalRef(globalArgArray);

          //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject getJavaObject return globalDexResult=%p",globalDexResult);
          return globalDexResult;  // caller must jniEnv->DeleteGlobalRef(globalDexResult)
        }
      }
    }
  }
  return NULL;
}

static bool pluginHasProperty(NPObject* obj, NPIdentifier npIdentifier)
{
  bool log=true;

  bool ret=false;
  if(obj==NULL)
  {
    if(myNppInstance)
      gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject pluginHasProperty obj=NULL ##############");
    return ret;
  }

  PluginObject* plugin = (PluginObject *)obj;
  if(plugin==NULL || plugin->rootNpObject==NULL || plugin->rootNpObject->dexLoaderClass==NULL)
  {
    // seems to be called after shutdown (that is: after surfaceDestroyed())
    if(myNppInstance)
      gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject pluginHasProperty plugin->rootNpObject->dexLoaderClass==NULL (called after shutdown?) ##############");
    return ret;
  }

  NPUTF8* idName = NULL;
  int idInt = -1;

  if(browser->identifierisstring(npIdentifier))
    idName = browser->utf8fromidentifier(npIdentifier);
  else
    idInt = browser->intfromidentifier(npIdentifier);

  if(myNppInstance)
  {
    if(idName!=NULL)
    {
      if(!strcmp(idName,"dbg") || !strcmp(idName,"hasProperty") || !strcmp(idName,"hasMethod")) log=false;
      if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginHasProperty name=[%s] ...",idName);
    }
    else
    {
      gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginHasProperty id=%d ... +++++++++++++++++++++++++++++",idInt);
    }

    JNIEnv* jniEnv = NULL;
    jint attachResult = plugin->vm->AttachCurrentThread(&jniEnv, NULL);
    if(attachResult!=JNI_OK)
    {
      gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject pluginHasProperty AttachCurrentThread attachResult!=JNI_OK %d #########################",attachResult);
    }
    else
    if(jniEnv==NULL)
    {
      gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject pluginHasProperty AttachCurrentThread jniEnv==NULL ###############################");
    }
    else
    {
      jniEnv->ExceptionClear();

/*
          {
//    gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginHasProperty contextString=[%s]",com.android.dalvikplugin.DalvikPluginStub.contextString);
// tmtmtm

            jfieldID fieldID = jniEnv->GetFieldID(jvmClass, idName, "Ljava/lang/Object;");      // todo: must try several signatures?
            if(fieldID)

            //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty get fieldTypeDescriptionID...");
            jfieldID fieldTypeDescriptionID = jniEnv->GetFieldID(plugin->dexResultClass, "contextString", "Ljava/lang/String;");
            //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty get jObjDescription...");
            jstring jObjDescription = (jstring)jniEnv->GetObjectField(jObj, fieldTypeDescriptionID);
          }
*/

      int errCode = -1;
      if(!plugin->unavailableHasPropertyAppletMethod)
      {
        // try to call applet "hasProperty()"-methode to find out if idName is available
        NPVariant* callArgs = new NPVariant[1];
        callArgs[0].type = NPVariantType_String;
        NPString npString;
        npString.UTF8Characters = idName;
        npString.UTF8Length = strlen(idName);
        callArgs[0].value.stringValue = npString;
        errCode = callJava(jniEnv, obj, "hasProperty", 1, callArgs, NULL, false, log);
        if(errCode==-10)
        {
          // hasProperty() is not implemented in applet code, don't try this again
          if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginHasProperty is not implemented in applet code, I will not try this again ++++++++++++++");
          plugin->unavailableHasPropertyAppletMethod = true;
          // use direct JNI-reflection below
        }
        else
        if(errCode<0)
        {
          // a freakin error in callJava() accured, probably something like...
          // static DexLoader.objectOfClass() is not accessible
          // static DexLoader.loadClass() is not accessible
          // static DexLoader.setObjectOfClass() is not accessible()
          // use direct JNI-reflection below
        }
        else
        if(errCode==0)
        {
          // hasProperty() IS implemented in applet code, but the named method+argsSignature is NOT accessible in the applet code
          ret=false;
        }
        else
        //if(errCode>0)
        {
          // hasProperty() IS implemented in applet code, and the named method+argsSignature IS accessible in the applet code
          ret=true;
        }
      }

      //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginHasProperty errCode=%d",errCode);
      if(errCode<0)
      {
/*
        // use direct JNI-reflection to check if the named property 'idName' is available
        gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginHasProperty use direct JNI-reflection to check if the named property 'idName' is available...");
        if(jObjectOfID==NULL)
        {
          jObjectOfID = jniEnv->GetStaticMethodID(plugin->dexLoaderClass, "objectOfClass", "(Ljava/lang/String;)Ljava/lang/Object;");
          if(jObjectOfID==NULL)
          {
            gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject pluginHasProperty GetStaticMethodID() 'objectOfClass' not found ####################");
          }
        }

        if(jObjectOfID!=NULL)
        {
          jstring jDynamicDexClass = jniEnv->NewStringUTF(plugin->jobjectUniqueIdString);
          // try to get the object from classname via objectOfClass hashmap in
          gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginHasProperty jObjectOfID jDynamicDexClass='%p' ...",jDynamicDexClass);
          jobject jvmObj = (jobject)(jniEnv->CallStaticObjectMethod(plugin->dexLoaderClass, jObjectOfID, jDynamicDexClass));
          // this is equivelent to calling:  (Object)objectHashMap.get(jDynamicDexClass);
          gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginHasProperty jvmObj='%p' (the java instance for this scriptable obj) ...",jvmObj);
          if(jvmObj)
          {
            gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginHasProperty got jvmObj ...");
            jclass jvmClass = jniEnv->GetObjectClass(jvmObj);

            gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginHasProperty GetFieldID idName...");
            jfieldID fieldID = jniEnv->GetFieldID(jvmClass, idName, "Ljava/lang/Object;");      // todo: tmtmtm must try several signatures?
            if(fieldID)
              ret = true;   // field exists
            else
              jniEnv->ExceptionClear();
            gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginHasProperty GetFieldID idName ret=%i",ret);
          }
        }
*/

        // we use direct Java reflection to check if the named property idName is available in ...
        // we dont use JNI reflection, because we don't know the type of idName and cannot construct a signature for it
        if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginHasProperty use direct Java reflection to check if the named property '%s' is available...",idName);
        if(jObjectHasPropertyID==NULL)
        {
          jObjectHasPropertyID = jniEnv->GetStaticMethodID(plugin->dexLoaderClass, "objectHasProperty", "(Ljava/lang/String;Ljava/lang/String;)Z");
          if(jObjectHasPropertyID==NULL)
          {
            if(log) gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject pluginHasProperty 'objectHasProperty' not found ####################");
            //return -2; // this is a fatal error, since DexLoader.objectOfClass() should always be available
          }
        }

        if(jObjectHasPropertyID!=NULL)
        {
          jstring jDynamicDexClass = jniEnv->NewStringUTF(plugin->jobjectUniqueIdString);
          // try to get the object from classname via objectOfClass hashmap in
//          gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginHasProperty jObjectHasPropertyID jDynamicDexClass='%p' idName=%s ...",jDynamicDexClass,idName);
          jboolean retBool = jniEnv->CallStaticBooleanMethod(plugin->dexLoaderClass, jObjectHasPropertyID, jDynamicDexClass, jniEnv->NewStringUTF(idName));
          if(retBool==JNI_TRUE)
            ret=true;
          else
            jniEnv->ExceptionClear();   // erase possible java.lang.NoSuchFieldException
        }

      }

      //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginHasProperty ret=%d",ret);
      if(ret)
      {
        if(idName!=NULL)
        {
          //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginHasProperty name=[%s] ret=%d set",idName,ret);
        }
        else
        {
          gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginHasProperty id=%d ret=%bd set ++++++++++++++++++++++++++++",idInt,ret);
        }
      }
      else
      {
        if(idName!=NULL)
        {
          //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginHasProperty name=[%s] ret=%d unset",idName,ret);
        }
        else
        {
          gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginHasProperty id=%d ret=%bd unset ++++++++++++++++++++++++++++",idInt,ret);
        }
      }
    }
  }

  if(idName!=NULL)
    browser->memfree(idName);
  return ret;
}


static bool pluginHasMethod(NPObject* obj, NPIdentifier npIdentifier)
{
  bool log=true;

  bool ret=false;
  if(obj==NULL)
  {
    if(myNppInstance)
      gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject pluginHasMethod obj=NULL ##############");
    return ret;
  }

  PluginObject* plugin = (PluginObject *)obj;

  if(plugin==NULL || plugin->rootNpObject==NULL || plugin->rootNpObject->dexLoaderClass==NULL)
  {
    // called after shutdown (after surfaceDestroyed())
    if(myNppInstance)
      gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject pluginHasMethod plugin->rootNpObject->dexLoaderClass==NULL ##############");
    return ret;
  }

  NPUTF8* idName = NULL;
  int idInt = -1;

  if(browser->identifierisstring(npIdentifier))
    idName = browser->utf8fromidentifier(npIdentifier);
  else
    idInt = browser->intfromidentifier(npIdentifier);

  if(myNppInstance)
  {
    if(idName!=NULL)
    {
      if(!strcmp(idName,"dbg") || !strcmp(idName,"hasProperty") || !strcmp(idName,"hasMethod")) log=false;
      if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginHasMethod name=[%s]",idName);
    }
    else
    {
      gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginHasMethod id=%d ###################################",idInt);
    }

    JNIEnv* jniEnv = NULL;
    jint attachResult = plugin->vm->AttachCurrentThread(&jniEnv, NULL);
    if(attachResult!=JNI_OK)
    {
      gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject pluginHasMethod AttachCurrentThread attachResult!=JNI_OK %d #########################",attachResult);
    }
    else
    if(jniEnv==NULL)
    {
      gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject pluginHasMethod AttachCurrentThread jniEnv==NULL ###############################");
    }
    else
    {
      int errCode = -1;
      if(!plugin->unavailableHasMethodAppletMethod)
      {
        NPVariant* callArgs = new NPVariant[1];
        callArgs[0].type = NPVariantType_String;
        NPString npString;
        npString.UTF8Characters = idName;
        npString.UTF8Length = strlen(idName);
        callArgs[0].value.stringValue = npString;
        errCode = callJava(jniEnv,obj, "hasMethod", 1, callArgs, NULL, false, log);
        if(errCode==-10)
        {
          // hasMethod() is not implemented in applet code, don't try this again
          plugin->unavailableHasMethodAppletMethod = true;
          // use direct JNI-reflection below
        }
        else
        if(errCode<0)
        {
          // some freakin error in callJava(), probably things like...
          // static DexLoader.objectOfClass() is not accessible
          // static DexLoader.loadClass() is not accessible
          // static DexLoader.setObjectOfClass() is not accessible()
          // use direct JNI-reflection below
        }
        else
        if(errCode==0)
        {
          // hasMethod() IS implemented in applet code, but the named method+argsSignature is NOT accessible in the applet code
          ret=false;
        }
        else
        //if(errCode>0)
        {
          // hasMethod() IS implemented in applet code, and the named method+argsSignature IS accessible in the applet code
          ret=true;
        }
      }

      if(errCode<0)
      {
        // use direct JNI-reflection to find out if named method 'idName' is accessible in applet code
        if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginHasMethod use direct JNI-reflection to check if the named method '%s' is available... +++++++++++++++++++++++++++++",idName);
        if(jObjectOfID==NULL)
        {
          jObjectOfID = jniEnv->GetStaticMethodID(plugin->dexLoaderClass, "objectOfClass", "(Ljava/lang/String;)Ljava/lang/Object;");
          if(jObjectOfID==NULL)
          {
            gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject pluginHasMethod GetStaticMethodID() 'objectOfClass' not found ####################");
          }
        }

        if(jObjectOfID!=NULL)
        {
          jstring jDynamicDexClass = jniEnv->NewStringUTF(plugin->jobjectUniqueIdString);
          // try to get the object from classname via objectOfClass hashmap in
          //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginHasMethod jObjectOfID jDynamicDexClass='%p' ...",jDynamicDexClass);
          jobject jvmObj = (jobject)(jniEnv->CallStaticObjectMethod(plugin->dexLoaderClass, jObjectOfID, jDynamicDexClass));
          // this is equivelent to calling:  (Object)objectHashMap.get(jDynamicDexClass);
          //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginHasMethod jvmObj='%p' (the java instance for this scriptable obj) ...",jvmObj);
          if(jvmObj)
          {
            //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginHasMethod got jvmObj ...");
            jclass jvmClass = jniEnv->GetObjectClass(jvmObj);

            if(jObjectHasMethodID==NULL)
            {
              jObjectHasMethodID = jniEnv->GetStaticMethodID(plugin->dexLoaderClass, "objectHasMethod", "(Ljava/lang/String;Ljava/lang/String;Z)Z");
              if(jObjectHasMethodID==NULL)
              {
                gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject callJava 'objectHasMethod' not found ####################");
              }
            }

            if(jObjectHasMethodID!=NULL)
            {
              // try to get the java.lang.reflect.Method for dexMethod in class jDynamicDexClass from objectOfClass hashmap in DexLoader
              //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava jObjectHasMethodID jDynamicDexClass='%p' idName=%s ...",jDynamicDexClass,idName);
              jboolean found = jniEnv->CallStaticBooleanMethod(plugin->dexLoaderClass, jObjectHasMethodID, jDynamicDexClass, jniEnv->NewStringUTF(idName), log);
              if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava jObjectHasMethodID idName=%s found=%i +++++++++++++++++++++++++++++++++",idName,found);
              if(found)
                ret = true;
            }
          }
        }
      }

      if(idName!=NULL)
      {
        //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginHasMethod name=%s ret=%d",idName,ret);
      }
      else
      {
        gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginHasMethod id=%d ret=%d ################################",idInt,ret);
      }
    }
  }

  if(idName!=NULL)
    browser->memfree(idName);
  return ret;
}


static bool pluginGetProperty(NPObject* obj, NPIdentifier npIdentifier, NPVariant* variant)
{
  // we call getJavaObject() to get the jObj (= DexResult) for the property with name npIdentifier from the scriptable obj
  // we then return the object-value in the DexResult via variant to JS

  // note: on page reload, the browsers skips calling pluginHasProperty() and jumps right into pluginGetProperty()
  //       this is why in DexLoader.getObject() we call hasProperty(), when we cannot find the java instance for npIdentifier
  bool log=true;

  bool ret = false;
  if(obj==NULL)
  {
    if(myNppInstance)
      gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject pluginGetProperty obj=NULL ##############");
    return ret;
  }

  PluginObject* plugin = (PluginObject *)obj;

  if(plugin==NULL || plugin->rootNpObject==NULL || plugin->rootNpObject->dexLoaderClass==NULL)
  {
    // called after shutdown (after surfaceDestroyed())
    if(myNppInstance)
      gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject pluginGetProperty plugin->rootNpObject->dexLoaderClass==NULL ##############");
    return ret;
  }

  NPUTF8* idName = NULL;
  int idInt = -1;

  if((npIdentifier))
    idName = browser->utf8fromidentifier(npIdentifier);
  else
    idInt = browser->intfromidentifier(npIdentifier);

  if(myNppInstance)
  {
    if(idName!=NULL)
    {
      if(!strcmp(idName,"dbg") || !strcmp(idName,"hasProperty") || !strcmp(idName,"hasMethod")) log=false;
      //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty name=[%s] obj=[%p] myNppInstance=[%p]",idName,obj,myNppInstance);
    }
    else
    {
      gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty id=%d #################################### not supported",idInt);
    }

    if(idName!=NULL)
    {
      // check, if field idName exists in the current class and if it's object was already instantiated in the VM
      //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty plugin->dexLoaderClass=[%p]",plugin->dexLoaderClass);

      JNIEnv* jniEnv = NULL;
      jint result = plugin->vm->AttachCurrentThread(&jniEnv, NULL);
      if(result==JNI_OK && jniEnv!=NULL)
      {
        jobject jObj = getJavaObject(obj, idName, jniEnv);     // this will call DexLoader.java:getObject() and returns jObj = DexResult (as a GlobalRef)
        //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty after getJavaObject() jObj=%p ...",jObj);
        if(jObj!=NULL)
        {
          // we got a DexResult object here
          //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty got jObj plugin->dexResultClass=%p ...",plugin->dexResultClass);

          if(plugin->dexResultClass!=NULL)
          {
            //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty get fieldTypeDescriptionID...");
            jfieldID fieldTypeDescriptionID = jniEnv->GetFieldID(plugin->dexResultClass, "fieldTypeDescription", signatureJavaLangString); //"Ljava/lang/String;");
            //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty get jObjDescription...");
            jstring jObjDescription = (jstring)jniEnv->GetObjectField(jObj, fieldTypeDescriptionID);
            //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty get cObjDescription...");

            jstring jValueString = NULL;
            const char* cValueString = NULL;
            const char* cObjDescription = jniEnv->GetStringUTFChars(jObjDescription, NULL);
            if(cObjDescription!=NULL)
            {
              //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty cObjDescription=[%s]",cObjDescription);
              // length of cObjDescription can be 0 after forced shutdown
              if(strlen(cObjDescription)>0)
              {
                if(!strncmp(cObjDescription,"class java.lang.String",22))
                {
                  //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty GOT STRING");
                  jfieldID fieldValueStringID = jniEnv->GetFieldID(plugin->dexResultClass, "valueString", signatureJavaLangString); //"Ljava/lang/String;");
                  if(fieldValueStringID)
                  {
                    //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty got fieldValueStringID");
                    jValueString = (jstring)jniEnv->GetObjectField(jObj, fieldValueStringID);
                    //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty got jValueString");
                    cValueString = jniEnv->GetStringUTFChars(jValueString, NULL);
                    //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty got cValueString");
                    //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty got cValueString=%s",cValueString);

                    char* valueString = (char*)malloc(strlen(cValueString)+1);
                    strcpy(valueString,cValueString);
                    jniEnv->ReleaseStringUTFChars(jValueString, cValueString);

                    STRINGZ_TO_NPVARIANT(valueString, *variant);
                    //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty done STRINGZ_TO_NPVARIANT +++++++++++++++++++");
                    ret = true;
                  }
                }

// todo: maybe we want to treat Float, Long and Integer just like any object (other than String!(?))
                else
                if(!strncmp(cObjDescription,"class java.lang.Float",21))
                {
                  //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty GOT Float +++++++++++++++++++");
                  jfieldID fieldValueFloatID = jniEnv->GetFieldID(plugin->dexResultClass, "valueNativeFloat", "F");
                  if(fieldValueFloatID!=NULL)
                  {
                    jfloat jValueFloat = jniEnv->GetFloatField(jObj, fieldValueFloatID);
                    //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty GOT Float jValueFloat=%f",jValueFloat);
                    DOUBLE_TO_NPVARIANT(jValueFloat,*variant);
                    ret = true;
                  }
                }

                else
                if(!strncmp(cObjDescription,"class java.lang.Long",20))
                {
                  //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty GOT Long +++++++++++++++++++");
                  jfieldID fieldValueLongID = jniEnv->GetFieldID(plugin->dexResultClass, "valueNativeLong", "J");
                  if(fieldValueLongID!=NULL)
                  {
                    jlong jValueLong = jniEnv->GetLongField(jObj, fieldValueLongID);
                    //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty GOT Long jValueLong=%i",jValueLong);
                    INT32_TO_NPVARIANT(jValueLong,*variant);
                    ret = true;
                  }
                }

                else
                if(!strncmp(cObjDescription,"class java.lang.Integer",23))
                {
                  //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty GOT Integer +++++++++++++++++++");
                  jfieldID fieldValueIntID = jniEnv->GetFieldID(plugin->dexResultClass, "valueNativeInt", "I");
                  if(fieldValueIntID!=NULL)
                  {
                    jint jValueInt = jniEnv->GetIntField(jObj, fieldValueIntID);
                    //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty GOT Integer jValueInt=%i",jValueInt);
                    INT32_TO_NPVARIANT(jValueInt,*variant);
                    ret = true;
                  }
                }

                else
                if(!strncmp(cObjDescription,"class ",6))
                {
                  // if the idName object was instantiated already, getObject() in DexLoader.java now has stored a reference to it in it's objectHashMap
                  //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty GOT OBJECT browser->createobject(myNppInstance, pluginClass)...");
                  NPObject* newScriptableObject = (NPObject*)browser->createobject(myNppInstance, &pluginClass);
                  if(newScriptableObject)
                  {
                    browser->retainobject(newScriptableObject);

                    PluginObject* thisPluginObject = (PluginObject *)newScriptableObject;
                    PluginObject* parentPlugin = (PluginObject *)obj;

                    thisPluginObject->vm = parentPlugin->vm;
                    thisPluginObject->dexLoaderClass = parentPlugin->dexLoaderClass;
                    thisPluginObject->dexResultClass = parentPlugin->dexResultClass;
//                    thisPluginObject->dexGetObjectMethod = parentPlugin->dexGetObjectMethod;
                    thisPluginObject->window = parentPlugin->window;
                    thisPluginObject->webviewGlob = parentPlugin->webviewGlob;
                    thisPluginObject->rootNpObject = parentPlugin->rootNpObject;

                    // add this npobject to it's root-parent's npobject children array
                    if(thisPluginObject->rootNpObject->childrenCount<pluginObjectChildrenMax)
                      thisPluginObject->rootNpObject->children[thisPluginObject->rootNpObject->childrenCount++] = thisPluginObject;

                    // use the unique identifier string provided by DexResult for jobjectUniqueIdString
                    jfieldID fieldValueIdentifierID = jniEnv->GetFieldID(plugin->dexResultClass, "valueIdentifier", signatureJavaLangString); //"Ljava/lang/String;");
                    jstring jValueIdentifier = (jstring)jniEnv->GetObjectField(jObj, fieldValueIdentifierID);
                    //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty get cObjDescription...");
                    const char* cValueIdentifier = jniEnv->GetStringUTFChars(jValueIdentifier, NULL);
                    char* valueIdentifier = (char*)malloc(256);
                    strcpy(valueIdentifier,cValueIdentifier);
                    jniEnv->ReleaseStringUTFChars(jValueIdentifier, cValueIdentifier);
                    ((PluginObject*)newScriptableObject)->jobjectUniqueIdString = valueIdentifier;

                    gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty new className=[%s]",((PluginObject*)newScriptableObject)->jobjectUniqueIdString);
                    OBJECT_TO_NPVARIANT(newScriptableObject, *variant);

                    //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty OBJECT_TO_NPVARIANT DONE");
                    ret = true;
                  }
                }

                else
                if(!strncmp(cObjDescription,"float",5))   // OK
                {
                  //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty GOT native float +++++++++++++++++++");
                  jfieldID fieldValueFloatID = jniEnv->GetFieldID(plugin->dexResultClass, "valueNativeFloat", "F");
                  if(fieldValueFloatID!=NULL)
                  {
                    jfloat jValueFloat = jniEnv->GetFloatField(jObj, fieldValueFloatID);
                    //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty GOT native float jValueFloat=%f for %s",jValueFloat,idName);
                    DOUBLE_TO_NPVARIANT(jValueFloat,*variant);
                    ret = true;
                  }
                }

                else
                if(!strncmp(cObjDescription,"double",6))    // todo: fail?
                {
                  //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty GOT native double +++++++++++++++++++");
                  jfieldID fieldValueDoubleID = jniEnv->GetFieldID(plugin->dexResultClass, "valueNativeDouble", "D");
                  if(fieldValueDoubleID!=NULL)
                  {
                    jdouble jValueDouble = jniEnv->GetDoubleField(jObj, fieldValueDoubleID);
                    //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty GOT native float jValueFloat=%f",jValueDouble);
                    DOUBLE_TO_NPVARIANT(jValueDouble,*variant);
                    ret = true;
                  }
                }

                else
                if(!strncmp(cObjDescription,"long",4))    // todo: fail?
                {
                  //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty GOT native long +++++++++++++++++++");
                  jfieldID fieldValueLongID = jniEnv->GetFieldID(plugin->dexResultClass, "valueNativeLong", "J");
                  if(fieldValueLongID!=NULL)
                  {
                    jlong jValueLong = jniEnv->GetLongField(jObj, fieldValueLongID);
                    //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty GOT native long jValueLong=%i",(long)jValueLong);
                    INT32_TO_NPVARIANT((long)jValueLong,*variant);
                    ret = true;
                  }
                }

                else
                if(!strncmp(cObjDescription,"int",3))   // OK
                {
                  //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty GOT native int +++++++++++++++++++");
                  jfieldID fieldValueIntID = jniEnv->GetFieldID(plugin->dexResultClass, "valueNativeInt", "I");
                  if(fieldValueIntID!=NULL)
                  {
                    jint jValueInt = jniEnv->GetIntField(jObj, fieldValueIntID);
                    //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty GOT native int jValueInt=%i",(int)jValueInt);
                    INT32_TO_NPVARIANT((int)jValueInt,*variant);
                    ret = true;
                  }
                }

                else
                if(!strncmp(cObjDescription,"short",5))   // todo: fail?
                {
                  //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty GOT native short +++++++++++++++++++");
                  jfieldID fieldValueShortID = jniEnv->GetFieldID(plugin->dexResultClass, "valueNativeShort", "S");
                  if(fieldValueShortID!=NULL)
                  {
                    jint jValueShort = jniEnv->GetIntField(jObj, fieldValueShortID);
                    //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty GOT native int jValueShort=%i",(int)jValueShort);
                    INT32_TO_NPVARIANT((int)jValueShort,*variant);
                    ret = true;
                  }
                }

                else
                if(!strncmp(cObjDescription,"boolean",7))   // todo: fail?
                {
                  //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty GOT native boolean +++++++++++++++++++");
                  jfieldID fieldValueBooleanID = jniEnv->GetFieldID(plugin->dexResultClass, "valueNativeBoolean", "Z");
                  if(fieldValueBooleanID!=NULL)
                  {
                    jint jValueBoolean = jniEnv->GetBooleanField(jObj, fieldValueBooleanID);
                    //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty GOT native int jValueBoolean=%i",(int)jValueBoolean);
                    BOOLEAN_TO_NPVARIANT((int)jValueBoolean,*variant);
                    ret = true;
                  }
                }

                else
                if(!strncmp(cObjDescription,"char",4))    // todo: fail?
                {
                  //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty GOT native char +++++++++++++++++++");
                  jfieldID fieldValueCharID = jniEnv->GetFieldID(plugin->dexResultClass, "valueNativeChar", "C");
                  if(fieldValueCharID!=NULL)
                  {
                    jint jValueChar = jniEnv->GetCharField(jObj, fieldValueCharID);
                    //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty GOT native int jValueChar=%i",(int)jValueChar);
                    INT32_TO_NPVARIANT((int)jValueChar,*variant);
                    ret = true;
                  }
                }

                else
                if(!strncmp(cObjDescription,"byte",4))    // todo: fail?
                {
                  //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty GOT native byte +++++++++++++++++++");
                  jfieldID fieldValueByteID = jniEnv->GetFieldID(plugin->dexResultClass, "valueNativeByte", "B");
                  if(fieldValueByteID!=NULL)
                  {
                    jint jValueByte = jniEnv->GetByteField(jObj, fieldValueByteID);
                    //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty GOT native int jValueByte=%i",(int)jValueByte);
                    INT32_TO_NPVARIANT((int)jValueByte,*variant);
                    ret = true;
                  }
                }
              }

              /*if(cValueString && jValueString)
              {
                if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty ReleaseStringUTFChars jValueString...");
                jniEnv->ReleaseStringUTFChars(jValueString, cValueString);
              }*/


              //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty ReleaseStringUTFChars jObjDescription...");
              jniEnv->ReleaseStringUTFChars(jObjDescription, cObjDescription);

            }

            jniEnv->DeleteLocalRef(jObjDescription);
          }
          jniEnv->DeleteGlobalRef(jObj);
        }
/*
// todo ???
        else
        {
          plugin->getProperty();  ???
        }
*/
      }

      //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty browser->memfree(idName)");
      browser->memfree(idName);
    }
  }

  //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginGetProperty ret=%i",ret);
  return ret;
}


static bool pluginSetProperty(NPObject* obj, NPIdentifier npIdentifier, const NPVariant* setVariant)
{
  bool log=true;
  bool ret=false;

  if(obj==NULL)
  {
    if(myNppInstance!=NULL)
      gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject pluginSetProperty obj=NULL ##############");
    return ret;
  }

  PluginObject* plugin = (PluginObject *)obj;

  if(plugin==NULL || plugin->rootNpObject==NULL || plugin->rootNpObject->dexLoaderClass==NULL)
  {
    // called after shutdown (after surfaceDestroyed())
    if(myNppInstance!=NULL)
      gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject pluginSetProperty plugin->rootNpObject->dexLoaderClass==NULL ##############");
    return ret;
  }

  NPUTF8* idName = NULL;
  int idInt = -1;

  if(browser->identifierisstring(npIdentifier))
    idName = browser->utf8fromidentifier(npIdentifier);
  else
    idInt = browser->intfromidentifier(npIdentifier);

  if(myNppInstance!=NULL)
  {
    if(idName!=NULL)
    {
      gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginSetProperty name=[%s]",idName);
    }
    else
    {
      gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginSetProperty id=%d +++++++++++++++++++",idInt);
    }

    if(idName!=NULL)
    {
      // check, if field idName exists in the current class and if it's object was already instantiated in the VM
      //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginSetProperty plugin->dexLoaderClass=[%p]",plugin->dexLoaderClass);

      JNIEnv* jniEnv = NULL;
      jint result = plugin->vm->AttachCurrentThread(&jniEnv, NULL);
      if(result==JNI_OK && jniEnv!=NULL)
      {
        // try to get the java object instance for obj...

        // DexLoader.loadClass() in order to load a java Object class using the proper classloader
        //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginSetProperty GetStaticMethodID 'loadClass'...");
        jmethodID loadClassID = jniEnv->GetStaticMethodID(plugin->dexLoaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
        if(loadClassID==NULL)
        {
          gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject pluginSetProperty GetStaticMethodID() 'loadClass' not found ####################");
          return false;
        }

        if(jObjectOfID==NULL)
        {
          //gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginSetProperty GetStaticMethodID 'objectOfClass'...");
          jObjectOfID = jniEnv->GetStaticMethodID(plugin->dexLoaderClass, "objectOfClass", "(Ljava/lang/String;)Ljava/lang/Object;");
          if(jObjectOfID==NULL)
          {
            gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject pluginSetProperty GetStaticMethodID() 'objectOfClass' not found ####################");
            return false;
          }
        }

        //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava CallStaticObjectMethod(objectOfClass) class='%s' ...",plugin->jobjectUniqueIdString);
        jstring jDynamicDexClass = jniEnv->NewStringUTF(plugin->jobjectUniqueIdString);

        // try to get the object from classname via objectOfClass hashmap in
        //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginSetProperty jDynamicDexClass='%p' ...",jDynamicDexClass);
        jobject jvmObj = (jobject)(jniEnv->CallStaticObjectMethod(plugin->dexLoaderClass, jObjectOfID, jDynamicDexClass));
        // this is equivelent to calling:  (Object)objectHashMap.get(jDynamicDexClass);
        //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginSetProperty jvmObj='%p' (the java instance for this scriptable obj) ...",jvmObj);
        if(jvmObj)
        {
          // a java object for this scriptable obj was already instantiated!

          //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava got jvmObj, jvmClass = GetObjectClass(jvmObj) ...");
          jclass jvmClass = jniEnv->GetObjectClass(jvmObj);
          //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginSetProperty got jvmClass=%p, GetMethodID 'setProperty' ...",jvmClass);

          jmethodID setPropertyID = jniEnv->GetMethodID(jvmClass, "setProperty", "(Ljava/lang/String;Ljava/lang/Object;)Z");
          if(setPropertyID==0)
          {
            gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject pluginSetProperty GetMethodID 'setProperty' fail ################## ");
          }
          else
          {
            if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginSetProperty create jargs for CallBooleanMethodA...");
            jvalue jargs[2];
            void* alloc = NULL;

            //jargs[0].l = jniEnv->NewStringUTF(idName);
            jobject localRefCls = jniEnv->NewStringUTF(idName);
            jargs[0].l = jniEnv->NewGlobalRef(localRefCls);
            jniEnv->DeleteLocalRef(localRefCls);

            if(setVariant->type==NPVariantType_Bool)
            {
              bool val = NPVARIANT_TO_BOOLEAN(*setVariant);
              //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginSetProperty NPVARIANT_TO_BOOLEAN setVariant->type==NPVariantType_Bool=%i",val);

              jclass javaBooleanClass = (jclass)(jniEnv->CallStaticObjectMethod(plugin->dexLoaderClass, loadClassID, jniEnv->NewStringUTF("java.lang.Boolean")));
              //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginSetProperty NPVARIANT_TO_BOOLEAN javaBooleanClass=%p ...",javaBooleanClass);
              if(javaBooleanClass)
              {
                // get the constructor of the java.lang.Boolean class
                jmethodID mID = jniEnv->GetMethodID(javaBooleanClass, "<init>", "(Z)V");   // constructor
                if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginSetProperty NPVARIANT_TO_BOOLEAN mID=%p ...",mID);
                if(mID!=NULL)
                {
                  // create a java.lang.Boolean object by calling the constructor and handing over the address of npObj
                  //jargs[1].l = jniEnv->NewObject(javaBooleanClass, mID, val);
                  jobject localRefCls = jniEnv->NewObject(javaBooleanClass, mID, val);
                  jargs[1].l = jniEnv->NewGlobalRef(localRefCls);
                  jniEnv->DeleteLocalRef(localRefCls);
                }
              }
            }
            else
            if(setVariant->type==NPVariantType_Double)
            {
              double val = NPVARIANT_TO_DOUBLE(*setVariant);
              if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginSetProperty NPVARIANT_TO_DOUBLE setVariant->type==NPVariantType_Double=%f",val);

              jclass javaDoubleClass = (jclass)(jniEnv->CallStaticObjectMethod(plugin->dexLoaderClass, loadClassID, jniEnv->NewStringUTF("java.lang.Double")));
              if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginSetProperty NPVARIANT_TO_DOUBLE javaDoubleClass=%p ...",javaDoubleClass);
              if(javaDoubleClass)
              {
                // get the constructor of the java.lang.Double class
                jmethodID mID = jniEnv->GetMethodID(javaDoubleClass, "<init>", "(D)V");   // constructor
                if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginSetProperty NPVARIANT_TO_DOUBLE mID=%p ...",mID);
                if(mID!=NULL)
                {
                  // create a java.lang.Double object by calling the constructor and handing over the address of npObj
//                  jargs[1].l = jniEnv->NewObject(javaDoubleClass, mID, val);
                  jobject localRefCls = jniEnv->NewObject(javaDoubleClass, mID, val);
                  jargs[1].l = jniEnv->NewGlobalRef(localRefCls);
                  jniEnv->DeleteLocalRef(localRefCls);
                }
              }
            }
            else
            if(setVariant->type==NPVariantType_String)
            {
              NPString npStr = NPVARIANT_TO_STRING(*setVariant);
              const NPUTF8* npUtf8 = npStr.UTF8Characters;
              char* str = (char*)malloc(npStr.UTF8Length+1);
              strncpy(str,npUtf8,npStr.UTF8Length);
              *(str+npStr.UTF8Length)=0;
              alloc=str;

              jobject localRefCls = jniEnv->NewStringUTF(str);
              jargs[1].l = jniEnv->NewGlobalRef(localRefCls);
              jniEnv->DeleteLocalRef(localRefCls);
            }
            else
            if(setVariant->type==NPVariantType_Object)
            {
              // instantiate an JSObject and store the addr of npObj as npp (instance / internal) in it
              jobject jsObj = NULL;
              NPObject* npObj = NPVARIANT_TO_OBJECT(*setVariant);
              //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginSetProperty NPVARIANT_TO_OBJECT npObj=%p %p...",npObj,*npObj);
              jclass jsObjectClass = (jclass)(jniEnv->CallStaticObjectMethod(plugin->dexLoaderClass, loadClassID, jniEnv->NewStringUTF("netscape.javascript.JSObject")));
              //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginSetProperty NPVARIANT_TO_OBJECT jsObjectClass=%p ...",jsObjectClass);
              if(jsObjectClass)
              {
                // get the constructor of the JSObject class
                jmethodID mID = jniEnv->GetMethodID(jsObjectClass, "<init>", "(I)V");
                //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginSetProperty NPVARIANT_TO_OBJECT mID=%p (int)npObj=%i ...",mID,(int)npObj);
                if(mID!=NULL)
                {
                  browser->retainobject(npObj);
                  // create a JSObject object by calling the constructor and handing over the address of npObj
                  jsObj = jniEnv->NewObject(jsObjectClass, mID, (int)npObj);
                  jargs[1].l = jniEnv->NewGlobalRef(jsObj);
                  jniEnv->DeleteLocalRef(jsObj);

                  //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginSetProperty NPVARIANT_TO_OBJECT ...");
                }
              }
            }

            jboolean retBool = JNI_FALSE;
            //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject callJava CallBooleanMethodA() ...");
            retBool = jniEnv->CallBooleanMethodA(jvmObj, setPropertyID, jargs);
            if(retBool==JNI_TRUE)
            {
              ret=true;
              //if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginSetProperty CallBooleanMethodA() ret=true");
            }
            else
            {
              gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginSetProperty CallBooleanMethodA() ret=false ################");
            }

            jniEnv->DeleteGlobalRef(jargs[1].l);
            jniEnv->DeleteGlobalRef(jargs[0].l);

            if(alloc)
              free(alloc);
          }
        }
      }

      if(idName!=NULL)
        browser->memfree(idName);
    }
  }

  //if(myNppInstance)
  //{
  //  if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginSetProperty done ret=%i",ret);
  //}
  return ret;
}


static bool pluginInvoke(NPObject* obj, NPIdentifier npIdentifier,
                         const NPVariant* args, uint32_t argCount,
                         NPVariant* result)
{
  bool log=true;
  bool ret=false;
  if(obj==NULL)
  {
    if(myNppInstance)
      gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject pluginInvoke obj=NULL ##############");
    return ret;
  }

  PluginObject* plugin = (PluginObject *)obj;

  if(plugin==NULL || plugin->rootNpObject==NULL || plugin->rootNpObject->dexLoaderClass==NULL)
  {
    // called after shutdown (after surfaceDestroyed())
    if(myNppInstance)
      gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject pluginInvoke plugin->rootNpObject->dexLoaderClass==NULL ##############");
    return ret;
  }

  NPUTF8* idName = NULL;
  int idInt = -1;

  if((npIdentifier))
    idName = browser->utf8fromidentifier(npIdentifier);
  else
    idInt = browser->intfromidentifier(npIdentifier);

  if(myNppInstance)
  {
    if(idName!=NULL)
    {
      if(!strcmp(idName,"dbg") || !strcmp(idName,"hasProperty") || !strcmp(idName,"hasMethod")) log=false;
      if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginInvoke name=[%s]",idName);
    }
    else
    {
      gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject pluginInvoke id=%d ###################################",idInt);
    }

    if(idName!=NULL)
    {
      JNIEnv* jniEnv = NULL;
      jint attachResult = plugin->vm->AttachCurrentThread(&jniEnv, NULL);
      if(attachResult!=JNI_OK)
      {
        gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject pluginInvoke AttachCurrentThread attachResult!=JNI_OK %d #########################",attachResult);
      }
      else
      if(jniEnv==NULL)
      {
        gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject pluginInvoke AttachCurrentThread jniEnv==NULL ###############################");
      }
      else
      {
        // hand over JS-args to Java
        if(log) gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginInvoke argCount=%i ... callJava ...",argCount);
        int errCode = callJava(jniEnv,obj,idName,argCount,args,result,true,log);
        if(errCode<0)
        {
          // some freakin error in callJava(), probably things like...
          // method idName NOT available in applet code
          // static DexLoader.objectOfClass() is not accessible
          // static DexLoader.loadClass() is not accessible
          // static DexLoader.setObjectOfClass() is not accessible()
          // use direct JNI-reflection below
        }
        else
        {
          // processing OK, return code of method idName in result
          ret=true;
        }
      }
    }
  }

  if(idName!=NULL)
    browser->memfree(idName);
  return ret;
}

static bool pluginInvokeDefault(NPObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  bool ret=false;

  if(obj==NULL)
  {
    if(myNppInstance)
      gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject pluginInvokeDefault() obj=NULL ##############");
    return ret;
  }

  PluginObject* plugin = (PluginObject *)obj;

  if(plugin==NULL || plugin->rootNpObject==NULL || plugin->rootNpObject->dexLoaderClass==NULL)
  {
    // called after shutdown (after surfaceDestroyed())
    if(myNppInstance)
      gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject pluginInvokeDefault() plugin->rootNpObject->dexLoaderClass==NULL ##############");
    return ret;
  }

  // TODO!

  //if(myNppInstance)
  // gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject pluginInvokeDefault()... #########################################");
  return false;
}

static void pluginInvalidate(NPObject* obj)
{
  if(myNppInstance!=NULL)
    gLogI.log(myNppInstance, kError_ANPLogType, "PluginObject pluginInvalidate() #########################################");  // TODO???
  // TODO???
  // TODO: Release any remaining references to JavaScript objects.
}

static NPObject* pluginAllocate(NPP npp, NPClass* theClass)
{
  if(myNppInstance)
    myNppInstance = npp;
  // note: npp and myNppInstance are (i.e. should be) the SAME

  gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginAllocate() npp=%p myNppInstance=%p",npp,myNppInstance);
  PluginObject* newInstance = (PluginObject*)malloc(sizeof(PluginObject));

  // newInstance->header is NPObject
  bzero(newInstance, sizeof(PluginObject));
  newInstance->header._class = theClass;
  newInstance->header.referenceCount = 1;
  newInstance->npp = npp;

  // we only return the NPObject
  return &newInstance->header;
}

static void pluginDeallocate(NPObject* obj)
{
  if(myNppInstance)
    gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginDeallocate() obj=%p +++++++++++++++++++++++++",obj);
  if(browser && obj)
    browser->memfree(obj);
}

static bool pluginRemoveProperty(NPObject* npobj, NPIdentifier name)
{
  if(myNppInstance)
    gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginRemoveProperty() +++++++++++++++++++++++++");
  return false;
}

static bool pluginEnumerate(NPObject* npobj, NPIdentifier** value, uint32_t* count)
{
  if(myNppInstance)
    gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginEnumerate() +++++++++++++++++++++++++");
  return false;
}

static bool pluginConstruct(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
  if(myNppInstance)
    gLogI.log(myNppInstance, kDebug_ANPLogType, "PluginObject pluginConstruct() +++++++++++++++++++++++++");
  return false;
}
