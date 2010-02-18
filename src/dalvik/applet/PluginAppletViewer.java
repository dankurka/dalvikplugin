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

package dalvik.applet;

import dalvik.awt.Image;
import java.net.URL;

import android.webkit.WebView;
import android.content.Context;

public class PluginAppletViewer implements AppletContext
{
  static
  {
    System.loadLibrary("dalvikplugin");   // same as in both Android.mk (but with a leading "lib" = "libdalvikplugin")
  }

  private native boolean nativeInvokeNative(int npp, int jsobj_addr, String methodName, Object args[]);

  // DexLoader will patch in these values
  public long npp = 0;                    // npp instance set by DexLoader
  public WebView webView = null;          // webView instance set by DexLoader
  public String codeBase = null;

  public PluginAppletViewer()
  {
    System.out.println("PluginAppletViewer() constructor npp="+npp);
  }

//AudioClip getAudioClip(URL url);

  public Image getImage(URL url)
  {
    return null;
  }

  public Applet getApplet(String name)
  {
    return null;
  }

//Enumeration<Applet> getApplets();

  public void showDocument(URL url)
  {
    // TODO: need testing
    if(webView!=null)
      webView.loadUrl(url.toString());
  }

  public void showDocument(URL url, String target)
  {
    // TODO: need testing
    // TODO: need to implement 'target' ???
    if(webView!=null)
      webView.loadUrl(url.toString());
  }

  public void showStatus(String status)
  {
    // TODO: display string in Java applet logview
  }

//public void setStream(String key, InputStream stream)throws IOException;
//public InputStream getStream(String key);
//public Iterator<String> getStreamKeys();

  public long getWindow()    // called by JSObject.getWindow()
  {
    System.out.println("PluginAppletViewer.java getWindow() npp="+npp+" webView="+webView+" ...");
    return npp;
  }

  public WebView getWebView()
  {
    //System.out.println("PluginAppletViewer.java getWebView() webView="+webView+" ...");
    return webView;
  }

  public Context getContext()
  {
    if(webView!=null)
      return webView.getContext();
    return null;
  }

  // called by JSObject.call()
  public Object call(long jsobj_addr, String methodName, Object args[])
  {
    //System.out.println("____________PluginAppletViewer.java call("+methodName+") myInstance="+myInstance+" native __________...");
    Object ret = null;

    // jsobj_addr = the addr of a some JS object (NPObject)
    // - created by PluginObject.cpp
    //   1. in callJava() when it wants to hand over a JS object (maybe a JS callback function)
    //   2. in pluginSetProperty() when it wants to hand over a JS object (maybe a JS callback function)
    // - or this PluginAppletViewer npp-value, handed to SamplePluginStub::getEmbeddedView() -> given to jni-bridge::surfaceCreated() -> DexLoader.loadDexInit() -> patched here

    boolean b = nativeInvokeNative((int)npp, (int)jsobj_addr, methodName, args);  // -> invokeNative() in jni-bridge.cpp
    //System.out.println("____________PluginAppletViewer.java call("+methodName+") myInstance="+myInstance+" native DONE__________...");
    if(b)
      ret = new Boolean(b);     // this is a hack, see: http://cacao-oj6.sourcearchive.com/documentation/6b14~pre1/PluginAppletViewer_8java-source.html

    // TODO: not sure what object to return
    return ret;
  }
}
