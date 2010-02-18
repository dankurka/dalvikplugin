package dalvik.applet;

import java.net.URL;

import android.webkit.WebView;
import android.content.Context;

public interface AppletStub
{
  boolean isActive();
  URL getDocumentBase();
  URL getCodeBase();
  String getParameter(String name);
  AppletContext getAppletContext();
  void appletResize(int width, int height);
  public WebView getWebView();
  public Context getContext();
}
