package dalvik.applet;

import java.net.URL;
import android.webkit.WebView;
import android.content.Context;

public class AppletStubObject implements AppletStub   // see: http://java.sun.com/j2se/1.4.2/docs/api/java/applet/AppletStub.html
{
  public AppletContext appletContext = null;    // is being set to the pluginAppletViewer instance

  public boolean isActive()
  {
    // TODO
    return false;
  }

  public URL getDocumentBase()    // URL of the base web document
  {
    if(appletContext!=null)
    {
      WebView webView = ((PluginAppletViewer)appletContext).getWebView();
      try
      {
        String originalUrlString = webView.getOriginalUrl();
        return new URL(originalUrlString);
      }
      catch(java.net.MalformedURLException ex)
      {
        System.out.println("AppletStubObject.java getDocumentBase() ex="+ex+" ######################");
      }
    }
    return null;
  }

  public URL getCodeBase()      // URL of the applet jar
  {
    if(appletContext!=null)
    {
      String codeBase = ((PluginAppletViewer)appletContext).codeBase;
      //System.out.println("____________ AppletStubObject.java getCodeBase() codeBase="+codeBase+" __________...");
      try
      {
        return new URL(codeBase);
      }
      catch(java.net.MalformedURLException ex)
      {
        System.out.println("AppletStubObject.java getCodeBase() ex="+ex);
      }
    }
    return null;
  }

  public String getParameter(String name)
  {
    // todo
    return "";
  }

  public AppletContext getAppletContext()
  {
    // note: JSObject will cast this to PluginAppletViewer and will then call PluginAppletViewer.getWindow()
    System.out.println("AppletStubObject getAppletContext() appletContext="+appletContext);
    return appletContext;
  }

  public void appletResize(int width, int height)
  {
    // todo: ???
  }

  // tm: added functionality
  public Context getContext()     // the android context
  {
    return null;  // todo???
  }

  // tm: added functionality (what about other browsers than webkit?)
  public WebView getWebView()
  {
    if(appletContext!=null)
      return ((PluginAppletViewer)appletContext).getWebView();;
    return null;
  }
}
