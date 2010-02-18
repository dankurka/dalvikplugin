package dalvik.applet;

import java.net.URL;

public interface AppletInterf
{
  //public final void setStub(AppletStub stub);
  public boolean isActive();
  public URL getDocumentBase();
  public URL getCodeBase();
  public String getParameter(String name);
  public AppletContext getAppletContext();
  //public void resize(int width, int height);
  //public void resize(Dimension d);
  public void showStatus(String msg);
  //public Image getImage(URL url);
  //public Image getImage(URL url, String name);
  public String getAppletInfo();
  public String[][] getParameterInfo();
  public void init();
  public void start();
  public void stop();
  public void destroy();
}
