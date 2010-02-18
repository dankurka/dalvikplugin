package com.android.dalvikplugin;

import java.lang.reflect.Method;
import java.lang.reflect.Field;
import java.net.*;
import java.io.*;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Random;

import dalvik.system.DexClassLoader;
import dalvik.applet.*;

import android.util.Config;
import android.util.Log;
import android.webkit.WebView;

public class DexLoader
{
  private static final String LOGTAG = "DexLoader";

  private static String fetchFile(String urlString, String tmpPath)
  {
    String newFileName = null;

    try
    {
      // fetch file from 'urlString' and store with its original filename (say "BasicTest.jar") in tmpPath
      // NEW: inject 5-digit randomStr in filename, so that a modified apk does not get loaded by DexClassLoader() from the same filename
      // note: the java classes-jar (apk) gets fetched each and every time
      Random generator = new Random();
      String randomStr = "00000"+generator.nextInt(100000);    // 0 - 99999
      randomStr = randomStr.substring(randomStr.length()-5);
      newFileName = tmpPath + randomStr+"-"+urlString.substring(urlString.lastIndexOf("/")+1);
      if(Config.LOGD) Log.i(LOGTAG, "fetchFile ["+urlString+"] to ["+newFileName+"]");

      int totalSize = 0;
      int bufSize = 8*1024;

      URL url = new URL(urlString);
      InputStream in = url.openStream();
      BufferedInputStream bufIn = new BufferedInputStream(in,bufSize);

      OutputStream fstream = new FileOutputStream(newFileName);
      BufferedOutputStream out = new BufferedOutputStream(fstream,bufSize);

      byte[] buf = new byte[bufSize];
      while(true)
      {
        int count = bufIn.read(buf, 0, bufSize);
        //if(Config.LOGD) Log.i(LOGTAG, "fetchFile read count=["+count+"] ...");
        if(count<1)
          break;
        out.write(buf, 0, count);
        totalSize+=count;
        //if(Config.LOGD) Log.i(LOGTAG, "fetchFile written total=["+totalSize+"] ...");
      }
      out.close();
      bufIn.close();
      if(Config.LOGD) Log.i(LOGTAG, "fetchFile read + written total bytes = ["+totalSize+"] ...");
    }
    catch (MalformedURLException mue)
    {
      if(Config.LOGD) Log.i(LOGTAG, "fetchFile Invalid URL");
      return null;
    }
    catch (IOException ioe)
    {
      if(Config.LOGD) Log.i(LOGTAG, "fetchFile I/O Error - " + ioe);
      return null;
    }
    catch (Exception ex)
    {
      if(Config.LOGD) Log.i(LOGTAG, "fetchFile Exception - " + ex);
      return null;
    }

    return newFileName;
  }


  static ClassLoader dexClassLoader = null;           // always the same
  public static ClassLoader myClassLoader = null;     // always the same - this will be set to the classloader used to load this DexLoader class

  public static Class pluginAppletViewerClass = null; // only needed in base class
  public static Object pluginAppletViewer = null;     // only needed in base class
  static Class appletStubObjectClass = null;
  static Class appletStubClass = null;
  static Object appletStubObject = null;

  static HashMap objectHashMap = null;
  static Object baseApplet = null;
  static volatile boolean initialized = false;
  static volatile boolean aborted = false;

  public static synchronized void clear()
  {
    if(Config.LOGD) Log.i(LOGTAG, "clear()...");
    dexClassLoader = null;
    myClassLoader = null;

    pluginAppletViewerClass = null;
    pluginAppletViewer = null;
    appletStubObjectClass = null;
    appletStubClass = null;
    appletStubObject = null;

    objectHashMap = null;    // holds all instantiated objects-instances referenced by unique-string-id (either package.className or #objaddr)
    baseApplet = null;
    initialized = false;
  }

  public static synchronized Object objectOfClass(String uniqueId)
  {
    if(objectHashMap!=null)
      return (Object)objectHashMap.get(uniqueId);
    return null;
  }

  public static synchronized boolean objectHasProperty(String uniqueId, String fieldName)
  {
    //if(Config.LOGD) Log.i(LOGTAG, "objectHasProperty() uniqueId=["+uniqueId+"] fieldName=["+fieldName+"] ...");
    if(objectHashMap==null)
    {
      if(Config.LOGD) Log.i(LOGTAG, "objectHasProperty() uniqueId=["+uniqueId+"] objectHashMap==null ##############");
    }
    else
    {
      Object obj = (Object)objectHashMap.get(uniqueId);
      if(obj==null)
      {
        if(Config.LOGD) Log.i(LOGTAG, "objectHasProperty() uniqueId=["+uniqueId+"] not found in objectHashMap ##############");
      }
      else
      {
        Class jvmClass = obj.getClass();
        try
        {
          Field field = jvmClass.getField(fieldName);
          if(field!=null)
          {
            //if(Config.LOGD) Log.i(LOGTAG, "objectHasProperty() uniqueId=["+uniqueId+"] fieldName=["+fieldName+"] FOUND");
            return true;
          }
        }
        catch(Exception ex)
        {
          if(Config.LOGD) Log.i(LOGTAG, "objectHasProperty() uniqueId=["+uniqueId+"] jvmClass=["+jvmClass+"] fieldName=["+fieldName+"] ex=["+ex+"] +++++++++++++++");
          // caller may need to jniEnv->ExceptionClear();
        }
      }
    }
    return false;
  }

  public static synchronized boolean objectHasMethod(String uniqueId, String methodName, boolean log)
  {
    //if(Config.LOGD) if(log) Log.i(LOGTAG, "objectHasMethod() uniqueId=["+uniqueId+"] methodName=["+methodName+"] ...");
    if(objectHashMap==null)
    {
      if(Config.LOGD) Log.i(LOGTAG, "objectHasMethod() objectHashMap==null ##############");
    }
    else
    {
      Object obj = (Object)objectHashMap.get(uniqueId);
      if(obj==null)
      {
        if(Config.LOGD) Log.i(LOGTAG, "objectHasMethod() object with uniqueId=["+uniqueId+"] not found in objectHashMap ##############");
      }
      else
      {
        Class jvmClass = obj.getClass();
        Method[] methods = jvmClass.getMethods();
        for(int i=0; i<methods.length; i++)
          if(methods[i].getName().equals(methodName))
          {
            //if(Config.LOGD) if(log) Log.i(LOGTAG, "objectHasMethod() getMethod methodName=["+methodName+"] FOUND");
            return true;
          }
      }
    }
    if(Config.LOGD) Log.i(LOGTAG, "objectHasMethod() getMethod methodName=["+methodName+"] NOT FOUND ###########################");
    return false;
  }


  public static synchronized Method objectHasMethodWithArgs(String uniqueId, String methodName, int argCount, int[] npVariantType, boolean log)
  {
    //if(Config.LOGD) if(log) Log.i(LOGTAG, "objectHasMethodWithArgs() uniqueId=["+uniqueId+"] methodName=["+methodName+"] argCount="+argCount+" ...");
    if(objectHashMap==null)
    {
      if(Config.LOGD) Log.i(LOGTAG, "objectHasMethodWithArgs() objectHashMap==null ##############");
    }
    else
    {
      Object obj = (Object)objectHashMap.get(uniqueId);
      if(obj==null)
      {
        if(Config.LOGD) if(log) Log.i(LOGTAG, "objectHasMethodWithArgs() object with uniqueId=["+uniqueId+"] not found in objectHashMap ##############");
      }
      else
      {
        Class jvmClass = obj.getClass();

        //if(Config.LOGD) if(log) Log.i(LOGTAG, "objectHasMethodWithArgs() getMethod jvmClass=["+jvmClass+"] methodName=["+methodName+"] ...");
        Class classargs[] = new Class[argCount];
        for(int i=0; i<argCount; i++)
        {
          if(Config.LOGD) if(log) Log.i(LOGTAG, "objectHasMethodWithArgs() arg=["+i+"] is=["+npVariantType[i]+"] ...");

          if(npVariantType[i]==0)   // c++ NPVariantType.NPVariantType_Void
          {
            classargs[i]=null; // void ???
            if(Config.LOGD) Log.i(LOGTAG, "objectHasMethodWithArgs() arg=["+i+"] is=["+npVariantType[i]+"] void type not implemented #########################");
          }
          else
          if(npVariantType[i]==1)   // c++ NPVariantType.NPVariantType_Null
          {
            classargs[i]=null; // null ???
            if(Config.LOGD) Log.i(LOGTAG, "objectHasMethodWithArgs() arg=["+i+"] is=["+npVariantType[i]+"] null type not implemented #########################");
          }
          else
          if(npVariantType[i]==2)   // c++ NPVariantType.NPVariantType_Bool
          {
            classargs[i]=Boolean.class;
          }
          else
          if(npVariantType[i]==3)   // c++ NPVariantType.NPVariantType_Int32
          {
            classargs[i]=Integer.class;
          }
          else
          if(npVariantType[i]==4)   // c++ NPVariantType.NPVariantType_Double
          {
            classargs[i]=Double.class;
          }
          else
          if(npVariantType[i]==5)   // c++ NPVariantType.NPVariantType_String
          {
            classargs[i]=String.class;
          }
          else
          if(npVariantType[i]==6)   // c++ NPVariantType.NPVariantType_Object
          {
            classargs[i]=Object.class;
            // TODO: there is an issue with using Object.class, if the actual Java code uses, say, JSObject
            //       maybe I should use getMethods() below, instead of getMethod()
          }
          else
          if(npVariantType[i]==7)   // int
          {
            classargs[i]=int.class;
          }
          else
          {
            if(Config.LOGD) Log.i(LOGTAG, "objectHasMethodWithArgs() arg=["+i+"] is=["+npVariantType[i]+"] not implemented #########################");
          }
        }

        Method methodID = null;
        try
        {
          methodID = jvmClass.getMethod(methodName,classargs);
        }
        catch(Exception ex)
        {
          methodID=null;
          if(Config.LOGD) Log.i(LOGTAG, "objectHasMethodWithArgs() uniqueId=["+uniqueId+"] jvmClass=["+jvmClass+"] methodName=["+methodName+"] ex=["+ex+"] +++++++++++++++");
        }

        if(methodID!=null)
        {
          if(Config.LOGD) if(log) Log.i(LOGTAG, "objectHasMethodWithArgs() uniqueId=["+uniqueId+"] methodName=["+methodName+"] FOUND");
          //if(Config.LOGD) Log.i(LOGTAG, "objectHasMethodWithArgs() uniqueId=["+uniqueId+"] methodID=["+methodID+"] methodType=["+methodID.getReturnType().getName()+"] FOUND");
          // note: the return type of methodID is: methodID.getReturnType().getName()
          return methodID;
        }
      }
    }
    return null;
  }

  public static synchronized String objectTypeOfMethod(Method methodID, boolean log)
  {
    //if(Config.LOGD) if(log) Log.i(LOGTAG, "objectTypeOfMethod() methodID=["+methodID+"]...");
    if(methodID==null)
      return null;

    String typeString = methodID.getReturnType().getName();
    //if(Config.LOGD) if(log) Log.i(LOGTAG, "objectTypeOfMethod() methodID=["+methodID+"] typeString=["+typeString+"]");
    if("boolean".equals(typeString))
      typeString="Z";
    else
    if("int".equals(typeString))
      typeString="I";
    else
    if("byte".equals(typeString))
      typeString="B";
    else
    if("char".equals(typeString))
      typeString="C";
    else
    if("float".equals(typeString))
      typeString="F";
    else
    if("long".equals(typeString))
      typeString="J";
    else
    if("short".equals(typeString))
      typeString="S";
    else
    if("void".equals(typeString))
      typeString="V";
    else
    if("boolean".equals(typeString))
      typeString="Z";
    else
/*
    if("java.lang.Object".equals(typeString))
      typeString="Ljava/lang/Object;";
    else
    if("java.lang.String".equals(typeString))
      typeString="Ljava/lang/String;";
    else
    {
      if(Config.LOGD) Log.i(LOGTAG, "setObjectOfClass() methodID=["+methodID+"] typeString=["+typeString+"] UNKNOWN ########################################");
    }
*/
    {
      if(typeString.indexOf(".")>=0)
      {
        typeString = "L"+typeString.replace('.','/')+";";
      }
    }
    if(Config.LOGD) if(log) Log.i(LOGTAG, "objectTypeOfMethod() methodID=["+methodID+"] typeString=["+typeString+"]");
    return typeString;
  }

  public static synchronized void setObjectOfClass(String uniqueId, Object obj)
  {
    //if(Config.LOGD) Log.i(LOGTAG, "setObjectOfClass() uniqueId=["+uniqueId+"] obj=["+obj+"] +++++++++++++++");
    if(objectHashMap!=null)
      objectHashMap.put(uniqueId,obj);
  }

  public static synchronized Class loadClass(String className)
  {
    if(!initialized)
    {
      if(Config.LOGD) Log.i(LOGTAG, "loadClass() className=["+className+"] initialized==false #################");
      return null;
    }

    if(myClassLoader==null)
    {
      if(Config.LOGD) Log.i(LOGTAG, "loadClass() className=["+className+"] myClassLoader==null #################");
      return null;
    }

    try
    {
      return myClassLoader.loadClass(className);
    }
    catch(java.lang.ClassNotFoundException ex)
    {
      if(Config.LOGD) Log.i(LOGTAG, "loadClass() className=["+className+"] ex=="+ex+" #################");
    }

    return null;
  }

  public static void callStart()
  {
    if(Config.LOGD) Log.i(LOGTAG, "callStart("+baseApplet+")...");
    if(baseApplet!=null)
    {
      ((AppletInterf)baseApplet).start();
      if(Config.LOGD) Log.i(LOGTAG, "callStart("+baseApplet+") done");
    }
    else
    {
      if(Config.LOGD) Log.i(LOGTAG, "callStart() baseApplet==null ##################");
    }
  }

  public static void callStop()
  {
    if(Config.LOGD) Log.i(LOGTAG, "callStop("+baseApplet+")...");
    if(baseApplet!=null)
    {
      ((AppletInterf)baseApplet).stop();
      if(Config.LOGD) Log.i(LOGTAG, "callStop() done");
      return;
    }
    if(Config.LOGD) Log.i(LOGTAG, "callStop() baseApplet==null ##################");
  }

  public static synchronized void shutdown()
  {
    if(Config.LOGD) Log.i(LOGTAG, "shutdown()...");
    callStop();
    if(Config.LOGD) Log.i(LOGTAG, "shutdown() done callStop()...");
    aborted = true;

    if(baseApplet!=null)
    {
      if(Config.LOGD) Log.i(LOGTAG, "shutdown() call destroy()...");
      ((AppletInterf)baseApplet).destroy();
    }
    clear();  // clear all references
    if(Config.LOGD) Log.i(LOGTAG, "shutdown() done");
    System.gc();
    System.gc();
  }

  private static String dexPath = null;
  private static String className = null;
  private static String instanceStr = null;
  private static WebView webView = null;

  static void run()
  {
    String codeBase = dexPath;

    Object jvmObj = null;
    Class jvmClass = null;

    clear();
    aborted = false;
    objectHashMap = new HashMap();    // holds all instantiated classObjects by className
    myClassLoader = DexLoader.class.getClassLoader();
    if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() instanceStr="+instanceStr);

    // TODO: we should dynamically aquire the current tmp-folder
    // note: "Application data dir is /data/data/com.android.browser/app_plugins" in main.cpp::NPP_New()
    // cut of "app_plugins" and concat "cache/"
    String tempDir = "/data/data/com.android.browser/cache/";

    if(dexPath!=null)
    {
      String webPath = webView.getUrl();            // path to the base web document
      if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() dexPath="+dexPath+" webPath="+webPath);

      // in case there was no "protocol://domain/" given for the applet codebase in the html object element, use the one from the web document
      if(!dexPath.startsWith("http://"))
      {
        if(webPath!=null)
        {
          int idxLastSlash = webPath.lastIndexOf('/');
          if(idxLastSlash>=0)
          {
            webPath = webPath.substring(0,idxLastSlash+1);
            if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() webPath=["+webPath+"]");
          }
          dexPath = webPath + dexPath;
        }
      }

      if(dexPath.startsWith("http://") || dexPath.startsWith("https://"))
      {
        // make sure the applet archive is fetched ONLY from "*vodafone.com" or "*timur.mobi" domains
        // todo: some bug here somewhere, if there are only 0 or 1 slash after the domainname
        // todo: "https" not tested
        int idxFirstSlash = dexPath.substring(7).indexOf('/');   // index of first slash after "http://"
        if(idxFirstSlash>0)
        {
          // TODO: verify these domains
          String domainString = dexPath.substring(7,7+idxFirstSlash);
          //if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() domainString=["+domainString+"] idxFirstSlash="+idxFirstSlash);
          if(!domainString.endsWith("vodafone.com")
          && !domainString.endsWith("timur.mobi")
          && !domainString.endsWith("simvelop.dyndns.org")
          && !domainString.endsWith("10.0.2.2")
          && !domainString.endsWith("simvelop.de"))
          {
            dexPath=null;
            if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() applet archive not accepted!!! ["+domainString+"]");
          }
        }
      }

      codeBase = dexPath;
      // fetch archive from full URL string = 'dexPath' and store the archive-file into 'tempDir'
      if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() codeBase="+codeBase+" to tempDir="+tempDir);
      if(dexPath!=null)
      {
        dexPath = fetchFile(dexPath, tempDir);
      }

      if(dexPath!=null)
      {
        //if(Config.LOGD) Log.i(LOGTAG, "new DexClassLoader()...");
        dexClassLoader = new DexClassLoader(dexPath,        // list of jar/apk files containing classes and resources
                                            tempDir,        // dexOutputDir
                                            null,           // binaryLib-Path
                                            myClassLoader); // parent-classloader is the classloader, that was used to load this DexLoader class
        //if(Config.LOGD) Log.i(LOGTAG, "new DexClassLoader() done...");
      }
    }

    if(dexClassLoader!=null && className!=null)
    {
      try
      {
        // load the applet class
        //if(Config.LOGD) Log.i(LOGTAG, "dexClassLoader.loadClass("+className+") jvmClass="+jvmClass+" ...");
        jvmClass = dexClassLoader.loadClass(className);
        if(Config.LOGD) Log.i(LOGTAG, "dexClassLoader.loadClass("+className+") done jvmClass=["+jvmClass+"]");
      }
      catch(Exception ex)
      {
        if(Config.LOGD) Log.i(LOGTAG, "dexClassLoader.loadClass("+className+") ex="+ex);
        jvmClass = null;
        // TODO: how to notify the user?
      }
      catch(IllegalAccessError iae)
      {
        if(Config.LOGD) Log.i(LOGTAG, "dexClassLoader.loadClass("+className+") iae="+iae);
        jvmClass = null;
        // TODO: how to notify the user?
      }
    }

    if(jvmClass!=null)
    {
      // the individual applet class should now have been loaded, we need to instantiate it
      if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() newInstance() jvmClass="+jvmClass);
      try
      {
        // instantiate the applet class
        jvmObj = jvmClass.newInstance();    // Applet object constructor will be called
        //if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() jvmClass.newInstance() done jvmObj="+jvmObj);
        if(jvmObj!=null)
        {
          //objectHashMap.put(className,jvmObj);               // the applet instance
          setObjectOfClass(className,jvmObj);
        }
      }
      catch(java.lang.IllegalAccessException iaex)
      {
        if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() java.lang.IllegalAccessException");
        jvmObj=null;
      }
      catch(java.lang.InstantiationException inex)
      {
        if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() java.lang.InstantiationException");
        jvmObj=null;
      }

      if(jvmObj!=null)
      {
        // TODO: for some reason, the new instance of pluginAppletViewer is not the one being used by the applet (=jvmObj) created above !!!!

        try
        {
          // load and instantiate an individual pluginAppletViewer object
          pluginAppletViewerClass = myClassLoader.loadClass("dalvik.applet.PluginAppletViewer");
          // the following may result in: System.loadLibrary("dalvikplugin");
          pluginAppletViewer = pluginAppletViewerClass.newInstance();

          // in PluginAppletViewer predefine field npp (= arg[3])
          Field field = pluginAppletViewerClass.getDeclaredField("npp");    // window = JSObject.getWindow(AppletImpl.this);
          long val = Long.parseLong(instanceStr.substring(2),16);           // TODO: catch exception
          if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() field npp in pluginAppletViewer set (npp="+val+")...");
          field.setLong(pluginAppletViewer, val);   // NPP

          // call pluginAppletViewer.getWindow() to display npp
          String methodName = "getWindow";
          Class classargs[] = {};
          Method getWindowMethod = pluginAppletViewerClass.getMethod(methodName,classargs); //null);
          getWindowMethod.invoke(pluginAppletViewer);

          // in PluginAppletViewer predefine field webView
          field = pluginAppletViewerClass.getDeclaredField("webView");
          //if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() field webView in pluginAppletViewer set (webView="+webView+")...");
          field.set(pluginAppletViewer, webView);

          // in PluginAppletViewer predefine field codeBase
          field = pluginAppletViewerClass.getDeclaredField("codeBase");
          //if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() field codeBase in pluginAppletViewer set (codeBase="+codeBase+")...");
          field.set(pluginAppletViewer, codeBase);

          // load and instantiate an individual AppletStubObject object
          //if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() loadClass('dalvik.applet.AppletStubObject')...");
          appletStubObjectClass = myClassLoader.loadClass("dalvik.applet.AppletStubObject");
          //if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() appletStubObjectClass.newInstance()...");
          appletStubObject = appletStubObjectClass.newInstance();       // implements AppletStub interface
          //if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() getDeclaredField('appletContext')...");

          // in AppletStubObject predefine field appletContext (= pluginAppletViewer)
          field = appletStubObjectClass.getDeclaredField("appletContext");
          //if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() field.set(appletStubObject,pluginAppletViewer)...");
          field.set(appletStubObject,pluginAppletViewer);

          // load AppletStub class
          // then call the applet's setStub(AppletStub) to attach an AppletStubObject (with a PluginAppletViewer implementing AppletContext)
          //if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() get methodName="+methodName+" jvmObj="+jvmObj);
          Class appletStubClass = myClassLoader.loadClass("dalvik.applet.AppletStub");
          methodName = "setStub";
          Method setStubMethod = jvmClass.getMethod(methodName,appletStubClass);
          if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() setStubMethod.invoke("+methodName+") ...");
          setStubMethod.invoke(jvmObj,appletStubObject);
          //if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() setStubMethod.invoke(jvmObj,appletStubObject); done");


          if(aborted)
          {
            if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() aborted ################################");
          }
          else
          {
            try
            {
              methodName = "init"; /////////////////////////////////////////////////////////////////////// call init()
              //if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() get methodName="+methodName+" jvmObj="+jvmObj);
              //Class classargs[] = {};
              Method initMethod = jvmClass.getMethod(methodName, classargs);

              // note: the following may result in JSObject() being instantiated and JSObject.call("onJavaLoad", args) being executed
              if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() initMethod.invoke("+methodName+")");
              Object parameters[] = {};
              initMethod.invoke(jvmObj,parameters);
              //if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() mLoad.invoke(jvmObj,(String[])null) done");

              // jvmObj is now our instantiated base applet object
              initialized = true;
              baseApplet = jvmObj;
            }
            catch(java.lang.reflect.InvocationTargetException itex)
            {
              if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() mLoad.invoke(jvmObj,(String[])null) failed "+itex);
              // TODO: how to notify javascript?
            }
          }
        }
        catch(java.lang.NoSuchMethodException nsmex)
        {
          if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() java.lang.NoSuchMethodException");
          jvmObj=null;
          // TODO: how to notify javascript?
        }
        catch(java.lang.reflect.InvocationTargetException itex)
        {
          if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() java.lang.reflect.InvocationTargetException ["+itex.getTargetException()+"]");
          jvmObj=null;
          // TODO: how to notify javascript?
        }
        catch(java.lang.IllegalAccessException iaex)
        {
          if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() java.lang.IllegalAccessException");
          jvmObj=null;
          // TODO: how to notify javascript?
        }
        catch(java.lang.InstantiationException inex)
        {
          if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() java.lang.InstantiationException");
          jvmObj=null;
          // TODO: how to notify javascript?
        }
        catch(java.lang.NoSuchFieldException nsfex)
        {
          if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() java.lang.reflect.NoSuchFieldException ["+nsfex+"]");
          jvmObj=null;
          // TODO: how to notify javascript?
        }
        catch(java.lang.ClassNotFoundException cnfex)
        {
          if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() java.lang.ClassNotFoundException ["+cnfex+"]");
          jvmObj=null;
          // TODO: how to notify javascript?
        }
      }
    }
    //if(Config.LOGD) Log.i(LOGTAG, "loadDexRun() done");
  }

  // will be called once from jni-bridge.cpp surfaceCreated() for creating the applet instance
  public static synchronized void loadDexInit(String[] args, WebView setWebView) //throws java.lang.ClassNotFoundException
  {
    // we need to fetch the applet jar (apk!) and instantiate one global DexClassLoader object to act upon the newly fetched files, which are stored in a tempDir
    dexPath = args[0];     // could be something like "http://*.vodafone.com/.../xxx.jar"
    className = args[1];
    instanceStr = args[3];
    webView = setWebView;
    if(Config.LOGD) Log.i(LOGTAG, "loadDexInit() dexPath="+dexPath+" className="+className+" instanceStr="+instanceStr);
/*
    Runnable runnable = new DexLoader();
    Thread thread = new Thread(runnable);
    thread.start();
*/
    run();    // used to be a thread, but now it's just inline
    if(Config.LOGD) Log.i(LOGTAG, "loadDexInit() done");
  }

  public static synchronized Object getObject(String[] args) throws java.lang.ClassNotFoundException
  {
    String className = args[1];     // TODO: not className, but unique-object-identifier-string
    String fieldName = args[2];
    //if(Config.LOGD) Log.i(LOGTAG, "getObject() fieldName=["+fieldName+"] parent className=["+className+"]...");

    if(!initialized)
    {
      if(Config.LOGD) Log.i(LOGTAG, "getObject() fieldName=["+fieldName+"] parent className=["+className+"] initialized==false #################");
      try { Thread.sleep(2); } catch(Exception ex) { };
    }

    if(!initialized)
    {
      if(Config.LOGD) Log.i(LOGTAG, "getObject() fieldName=["+fieldName+"] parent className=["+className+"] initialized==false #################");
      return null;
    }

    if(objectHashMap==null)
    {
      if(Config.LOGD) Log.i(LOGTAG, "getObject() fieldName=["+fieldName+"] parent className=["+className+"] objectHashMap==null #################");
      return null;
    }

    Object jvmObj = (Object)objectHashMap.get(className);
    if(jvmObj==null)
    {
      if(Config.LOGD) Log.i(LOGTAG, "getObject() fieldName=["+fieldName+"] parent className=["+className+"] jvmObj==null #################");
      return null;
    }

    Class jvmClass = jvmObj.getClass();
    if(jvmObj==null)
    {
      if(Config.LOGD) Log.i(LOGTAG, "getObject() fieldName=["+fieldName+"] parent className=["+className+"] jvmObj==null #################");
      return null;
    }
    //if(Config.LOGD) Log.i(LOGTAG, "getObject() fieldName="+fieldName+" jvmClass="+jvmClass+" ...");

    Field field = null;
    // try to access fieldName in className directly over JNI reflection...
    try
    {
      //Field field = jvmClass.getDeclaredField(fieldName);
      field = jvmClass.getField(fieldName);
      //if(Config.LOGD) Log.i(LOGTAG, "getObject() field="+field+" ...");
    }
    catch(java.lang.NoSuchFieldException nsfex)
    {
      if(Config.LOGD) Log.i(LOGTAG, "getObject() fieldName="+fieldName+" java.lang.reflect.NoSuchFieldException ["+nsfex+"] #######################");
      return null;
    }
    catch(java.lang.Exception ex)
    {
      if(Config.LOGD) Log.i(LOGTAG, "getObject() on getField exception ["+ex+"] ##################################");
      return null;
    }

    Object fieldObj = null;
    try
    {
      // step 1: try to access fieldName via reflection
      fieldObj = field.get(jvmObj);
      //if(Config.LOGD) Log.i(LOGTAG, "getObject() via reflection fieldObj=["+fieldObj+"] type=["+field.getType()+"]");
      if(fieldObj==null)
      {
        // hack: call hasProperty() to make sure object is really instantiated (webkit should really do this !?)
        // jvmObj.hasProperty(fieldName);
//        Class typeArgs[] = new Class[1];
//        typeArgs[0] = String.class;
//        Method hasPropertyMethod = jvmClass.getMethod("hasProperty", typeArgs);
        Method hasPropertyMethod = jvmClass.getMethod("hasProperty", new Class[]{String.class});
        //if(Config.LOGD) Log.i(LOGTAG, "getObject() hasPropertyMethod="+hasPropertyMethod+" ...");
        Boolean exists = (Boolean)hasPropertyMethod.invoke(jvmObj,fieldName);
        //if(Config.LOGD) Log.i(LOGTAG, "getObject() hasProperty="+exists+" try reflection again...");
        fieldObj = field.get(jvmObj);
        //if(Config.LOGD) Log.i(LOGTAG, "getObject() via reflection after hasProperty() fieldObj=["+fieldObj+"]");
      }
    }
    catch(java.lang.IllegalAccessException iaex)
    {
      if(Config.LOGD) Log.i(LOGTAG, "getObject() on reflection className="+className+" java.lang.IllegalAccessException "+iaex+" +++++++++++++++++++++++++++++++");
      // ignore if hasProperty() is not implemented
    }
    catch(java.lang.Exception ex)
    {
      if(Config.LOGD) Log.i(LOGTAG, "getObject() on reflection exception ["+ex+"] ignore ++++++++++++++++++++++++++++++");
      // ignore if hasProperty() is not implemented
    }

    if(fieldObj==null)
    {
      // step 2: try to access fieldName via applet.getProperty()...
      try
      {
        //if(Config.LOGD) Log.i(LOGTAG, "getObject() via applet.getProperty("+fieldName+")...");

        // cannot use PluginObjectInterf here
        //fieldObj = ((PluginObjectInterf)jvmObj).getProperty(fieldName);
        String methodName = "getProperty";
        Method getPropertyMethod = jvmClass.getMethod(methodName,String.class);   // todo: or new Class[]{String.class} ???
        if(Config.LOGD) Log.i(LOGTAG, "getObject() getPropertyMethod.invoke("+fieldName+") ...");
        getPropertyMethod.invoke(jvmObj,fieldName);
      }
      catch(java.lang.Exception ex)
      {
        if(Config.LOGD) Log.i(LOGTAG, "getObject() .getProperty("+fieldName+") exception ["+ex+"] ##################################");
        return null;
      }
    }

    //if(Config.LOGD) Log.i(LOGTAG, "getObject() fieldObj="+fieldObj+")...");
    if(fieldObj!=null)
    {
      DexResult dexResult = new DexResult();
      if(field!=null)
        dexResult.fieldTypeDescription = field.getType().toString();
      else
        dexResult.fieldTypeDescription = "class "+fieldObj.getClass().getName();

      if(dexResult.fieldTypeDescription.startsWith("class "))
      {
        // fieldObj is an object value
        if(dexResult.fieldTypeDescription.startsWith("class java.lang.String"))
        {
          dexResult.valueString = (String)fieldObj;
          //if(Config.LOGD) Log.i(LOGTAG, "getObject() stored as dexResult.valueString="+dexResult.valueString+"]");
        }
        else
        if(dexResult.fieldTypeDescription.startsWith("class java.lang.Float"))
        {
          dexResult.valueNativeFloat = ((Float)fieldObj).floatValue();
          //if(Config.LOGD) Log.i(LOGTAG, "getObject() stored as dexResult.valueNativeFloat="+dexResult.valueNativeFloat+"]");
        }
        else
        if(dexResult.fieldTypeDescription.startsWith("class java.lang.Long"))
        {
          dexResult.valueNativeLong = ((Long)fieldObj).longValue();
          //if(Config.LOGD) Log.i(LOGTAG, "getObject() stored as dexResult.valueNativeLong="+dexResult.valueNativeLong+"]");
        }
        else
        if(dexResult.fieldTypeDescription.startsWith("class java.lang.Integer"))
        {
          dexResult.valueNativeInt = ((Integer)fieldObj).intValue();
          //if(Config.LOGD) Log.i(LOGTAG, "getObject() stored as dexResult.valueNativeInteger="+dexResult.valueNativeInt+"]");
        }
        else
        {
          // fieldObj is a complex object
          // store fieldObj in objectHashMap, because it is instantiated already in the VM and we will needed it soon as a container

          String objectIdentifierString = "#"+fieldObj.hashCode();
          dexResult.valueIdentifier = objectIdentifierString;
          //objectHashMap.put(objectIdentifierString,fieldObj);
          setObjectOfClass(objectIdentifierString,fieldObj);
          //if(Config.LOGD) Log.i(LOGTAG, "getObject() object stored with objectIdentifierString="+objectIdentifierString+"]");
        }
      }
      else
      // fieldObj is a primitive value
      if(dexResult.fieldTypeDescription.startsWith("float"))
      {
        dexResult.valueNativeFloat = ((Float)fieldObj).floatValue();
        //if(Config.LOGD) Log.i(LOGTAG, "getObject() stored as dexResult.valueNativeFloat="+dexResult.valueNativeFloat+"]");
      }
      else
      if(dexResult.fieldTypeDescription.startsWith("long"))
      {
        dexResult.valueNativeLong = ((Long)fieldObj).longValue();
        //if(Config.LOGD) Log.i(LOGTAG, "getObject() stored as dexResult.valueNativeLong="+dexResult.valueNativeLong+"]");
      }
      else
      if(dexResult.fieldTypeDescription.startsWith("int"))
      {
        dexResult.valueNativeInt = ((Integer)fieldObj).intValue();
        //if(Config.LOGD) Log.i(LOGTAG, "getObject() stored as dexResult.valueNativeInteger="+dexResult.valueNativeInt+"]");
      }
      else
      if(dexResult.fieldTypeDescription.startsWith("double"))
      {
        dexResult.valueNativeDouble = ((Double)fieldObj).doubleValue();
        //if(Config.LOGD) Log.i(LOGTAG, "getObject() stored as dexResult.valueNativeDouble="+dexResult.valueNativeDouble+"]");
      }
      else
      if(dexResult.fieldTypeDescription.startsWith("long"))
      {
        dexResult.valueNativeLong = ((Long)fieldObj).longValue();
        //if(Config.LOGD) Log.i(LOGTAG, "getObject() stored as dexResult.valueNativeLong="+dexResult.valueNativeLong+"]");
      }
      else
      if(dexResult.fieldTypeDescription.startsWith("short"))
      {
        dexResult.valueNativeShort = ((Short)fieldObj).shortValue();
        //if(Config.LOGD) Log.i(LOGTAG, "getObject() stored as dexResult.valueNativeShort="+dexResult.valueNativeShort+"]");
      }
      else
      if(dexResult.fieldTypeDescription.startsWith("boolean"))
      {
        dexResult.valueNativeBoolean = ((Boolean)fieldObj).booleanValue();
        //if(Config.LOGD) Log.i(LOGTAG, "getObject() stored as dexResult.valueNativeBoolean="+dexResult.valueNativeBoolean+"]");
      }
      else
      if(dexResult.fieldTypeDescription.startsWith("char"))
      {
        dexResult.valueNativeChar = ((Character)fieldObj).charValue();
        //if(Config.LOGD) Log.i(LOGTAG, "getObject() stored as dexResult.valueNativeChar="+dexResult.valueNativeChar+"]");
      }

      return dexResult;
    }

    //if(Config.LOGD) Log.i(LOGTAG, "getObject() className="+className+" jvmObj="+jvmObj+" return null ###################################");
    // TODO: how to notify javascript of this problem?
    return null;
  }
}
