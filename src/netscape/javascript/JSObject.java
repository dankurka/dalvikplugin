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

package netscape.javascript;

import dalvik.applet.Applet;

import java.security.AccessControlContext;
import java.security.AccessControlException;
import java.security.AccessController;
import java.security.BasicPermission;
import dalvik.applet.PluginAppletViewer;
import android.webkit.WebView;
import android.content.Context;

public final class JSObject
{
  private static PluginAppletViewer pluginAppletViewer = null;
  private long   jsobj_addr = 0;   // value handed over by constructor
                                   // JSObject's are created using static getWindow(Applet applet) -> return new JSObject( ((PluginAppletViewer)(applet.getAppletContext())).getWindow() );

  private static void initClass()
  {
  }

/*
  static
  {
  }

  public JSObject()
  {
    this((long) 0);
  }

  public void init()
  {
  }
*/

  /**
   * TODO: it is illegal to construct a JSObject manually
   */
  public JSObject(int jsobj_addr)
  {
    // may be called by PluginObject.cpp, jsobj_addr = the addr of a JS object (NPObject)
    // 1. in callJava() when it wants to hand over a JS object (maybe a JS callback function)
    // 2. in pluginSetProperty() when it wants to hand over a JS object (maybe a JS callback function)
    //
    // may be called by .getWindow(Applet), jsobj_addr = pluginAppletViewer.getWindow();  // npp or 0???
    // npp was handed to SamplePluginStub::getEmbeddedView() -> given to jni-bridge::surfaceCreated() -> DexLoader.loadDexInit() -> patched into PluginAppletViewer
    this((long) jsobj_addr);
  }

  public JSObject(long jsobj_addr)
  {
/*
    // todo: See if the caller has permission

    try
    {
      AccessController.getContext().checkPermission(new JSObjectCreatePermission());
    }
    catch (AccessControlException ace)
    {
      // If not, only caller with JSObject.getWindow on the stack may
      // make this call unprivileged.

      // Although this check is inefficient, it should happen only once
      // during applet init, so we look the other way

      StackTraceElement[] stack =  Thread.currentThread().getStackTrace();
      boolean mayProceed = false;

      for (int i=0; i < stack.length; i++)
      {
        if (stack[i].getClassName().equals("netscape.javascript.JSObject") &&
            stack[i].getMethodName().equals("getWindow")) {
          mayProceed = true;
        }
      }

      if (!mayProceed) throw ace;
    }

    PluginDebug.debug ("JSObject long CONSTRUCTOR");
*/
    this.jsobj_addr = jsobj_addr;
    System.out.println("________JSObject constructor jsobj_addr="+jsobj_addr+" ...");
  }

  public long getNpp()
  {
    return jsobj_addr;
  }

  /**
   * Retrieves a named member of a JavaScript object.
   * Equivalent to "this.<i>name</i>" in JavaScript.
   */
  public Object getMember(String name)
  {
    Object o = null;
/*
    PluginDebug.debug ("JSObject.getMember " + name);

    Object o = PluginAppletViewer.getMember(jsobj_addr, name);
    PluginDebug.debug ("JSObject.getMember GOT " + o);
*/
    return o;
  }


//public Object   getMember(int index) { return getSlot(index); }

  public Object getSlot(int index)
  {
    Object o = null;
//  PluginDebug.debug ("JSObject.getSlot " + index);
//  return PluginAppletViewer.getSlot(jsobj_addr, index);
    return o;
  }


  /**
   * Sets a named member of a JavaScript object.
   * Equivalent to "this.<i>name</i> = <i>value</i>" in JavaScript.
   */
  public void     setMember(String name, Object value)
  {
//  PluginDebug.debug ("JSObject.setMember " + name + " " + value);
//  PluginAppletViewer.setMember(jsobj_addr, name, value);
  }

//public void     setMember(int index, Object value)
//{
//  setSlot(index, value);
//}

  public void     setSlot(int index, Object value)
  {
//  PluginDebug.debug ("JSObject.setSlot " + index + " " + value);
//  PluginAppletViewer.setSlot(jsobj_addr, index, value);
  }

  // TODO: toString, finalize.
  public void     removeMember(String name)
  {
//  PluginDebug.debug ("JSObject.removeMember " + name);
//  PluginAppletViewer.removeMember(jsobj_addr, name);
  }

  public Object call(String methodName, Object args[])
  {
    if(pluginAppletViewer!=null)
    {
      if(args==null)
        args=new Object[0];
      //System.out.println("JSObject call(methodName="+methodName+")...");
      return pluginAppletViewer.call(jsobj_addr, methodName, args);   // -> nativeInvokeNative() -> jni-bridge.cpp::invokeNative()
    }
    return null;
  }

  public WebView getWebView()
  {
    return pluginAppletViewer.getWebView();
  }

  public Context getContext()
  {
    if(pluginAppletViewer!=null)
    {
      WebView webView = pluginAppletViewer.getWebView();
      if(webView!=null)
        return webView.getContext();
    }
    return null;
  }

  public Object eval(String s)
  {
    Object o = null;
//  PluginDebug.debug("JSObject.eval " + s);
//  return PluginAppletViewer.eval(jsobj_addr, s);
    return o;
  }

/*
  public String        toString()
  {
    return PluginAppletViewer.javascriptToString(jsobj_addr);
  }
*/

  public static JSObject getWindow(Applet applet)
  {
    long jsobj_addr = 0l;

//  if(pluginAppletViewer==null)
    {
      System.out.println("JSObject getWindow() pluginAppletViewer==null -> applet.getAppletContext() ...");
      pluginAppletViewer = (PluginAppletViewer)(applet.getAppletContext());
    }
//  else
//  {
//    System.out.println("JSObject getWindow() pluginAppletViewer!=null ...");
//  }

    if(pluginAppletViewer!=null)
    {
      //System.out.println("________JSObject.java pluginAppletViewer.getWindow()______...");
      jsobj_addr = pluginAppletViewer.getWindow();
      //System.out.println("________JSObject.java getWindow() jsobj_addr="+jsobj_addr);
      return new JSObject(jsobj_addr);

/*
      JSObject jsObject = null;
      try
      {
        Class  jsObjectClass = pluginAppletViewer.clLoader.loadClass("netscape.javascript.JSObject");
        jsObject = (JSObject)jsObjectClass.newInstance();      // note: pluginAppletViewer will use the same classloader to load (get access to) the native c++ methods in libdalvikplugin
        jsObject.jsobj_addr = jsobj_addr;
      }
      catch(java.lang.ClassNotFoundException cnfex)
      {
        System.out.println("JSObject getWindow() java.lang.ClassNotFoundException");
        jsObject=null;
      }
      catch(java.lang.IllegalAccessException iaex)
      {
        System.out.println("JSObject getWindow() java.lang.IllegalAccessException");
        jsObject=null;
      }
      catch(java.lang.InstantiationException inex)
      {
        System.out.println("JSObject getWindow() java.lang.InstantiationException");
        jsObject=null;
      }

      return jsObject;
*/
    }
    return null;
  }

  protected void  finalize()
  {
//  PluginDebug.debug("JSObject.finalize ");
//  PluginAppletViewer.JavaScriptFinalize(jsobj_addr);
  }

  public boolean equals(Object obj)
  {
    return false;   // todo ???
  }
}
