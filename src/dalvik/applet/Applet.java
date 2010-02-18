package dalvik.applet;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.net.URL;
import java.net.MalformedURLException;
import java.util.Hashtable;
import java.util.Locale;

import dalvik.awt.*;

public class Applet extends Panel implements AppletInterf
{
  public Applet() //throws HeadlessException
  {
/*
    if (GraphicsEnvironment.isHeadless())
    {
      throw new HeadlessException();
    }
*/
  }

  transient private AppletStub stub = null;      // transient will not be serialized

  /* version ID for serialized form. */
  //private static final long serialVersionUID = -5836846270535785031L;

  private void readObject(ObjectInputStream s) throws ClassNotFoundException, IOException  //, HeadlessException
  {
/*
    if (GraphicsEnvironment.isHeadless())
    {
      throw new HeadlessException();
    }
*/
    s.defaultReadObject();
  }

  public final void setStub(AppletStub stub)
  {
    System.out.println("Applet setStub() stub="+stub+" ...");
/*
    if (this.stub != null)
    {
      SecurityManager s = System.getSecurityManager();
      if (s != null)
      {
        s.checkPermission(new AWTPermission("setAppletStub"));
      }
    }
*/
    this.stub = stub;
  }

  public boolean isActive()
  {
    if(stub != null)
    {
      return stub.isActive();
    }
    else
    {
      // If stub field not filled in, applet never active
      return false;
    }
  }

  public URL getDocumentBase()
  {
    return stub.getDocumentBase();
  }

  public URL getCodeBase()
  {
    return stub.getCodeBase();
  }

   public String getParameter(String name)
   {
     return stub.getParameter(name);
   }

  public AppletContext getAppletContext()
  {
    AppletContext appletContext=null;
    System.out.println("Applet getAppletContext() stub="+stub+" ...");

    // note:
    // - stub = AppletStubObject
    // - set by DexLoader.java via .setStub()
    // - JSObject.java calls .getWindow() on the returned AppletStubObject to get an instance of PluginAppletViewer
    if(stub!=null)
      appletContext = stub.getAppletContext();
    System.out.println("Applet getAppletContext() appletContext="+appletContext);
    return appletContext;
  }

  public void resize(int width, int height)
  {
    Dimension d = size();
    if ((d.width != width) || (d.height != height))
    {
      super.resize(width, height);
      if (stub != null)
      {
        stub.appletResize(width, height);
      }
    }
  }

  public void resize(Dimension d)
  {
    resize(d.width, d.height);
  }

  public void showStatus(String msg)
  {
    getAppletContext().showStatus(msg);
  }

  public Image getImage(URL url)
  {
    return getAppletContext().getImage(url);
  }

  public Image getImage(URL url, String name)
  {
    try
    {
      return getImage(new URL(url, name));
    }
    catch (MalformedURLException e)
    {
      return null;
    }
  }

/*
  public final static AudioClip newAudioClip(URL url) {
      return new sun.applet.AppletAudioClip(url);
  }
*/

/*
  public AudioClip getAudioClip(URL url) {
      return getAppletContext().getAudioClip(url);
  }
*/

/*
  public AudioClip getAudioClip(URL url, String name) {
      try {
          return getAudioClip(new URL(url, name));
      } catch (MalformedURLException e) {
          return null;
      }
  }
*/

  public String getAppletInfo()
  {
    return null;
  }

/*
  public Locale getLocale() {
    Locale locale = super.getLocale();
    if (locale == null) {
      return Locale.getDefault();
    }
    return locale;
  }
*/

  public String[][] getParameterInfo()
  {
    return null;
  }

/*
  public void play(URL url) {
      AudioClip clip = getAudioClip(url);
      if (clip != null) {
          clip.play();
      }
  }
*/

/*
  public void play(URL url, String name) {
      AudioClip clip = getAudioClip(url, name);
      if (clip != null) {
          clip.play();
      }
  }
*/
  public void init() {
  }

  public void start() {
  }

  public void stop() {
  }

  public void destroy() {
  }

// Accessibility support

// AccessibleContext accessibleContext = null;

/*
  public AccessibleContext getAccessibleContext()
  {
    if (accessibleContext == null)
        accessibleContext = new AccessibleApplet();
    return accessibleContext;
  }
*/

/*
  protected class AccessibleApplet extends AccessibleAWTPanel
  {
    //private static final long serialVersionUID = 8127374778187708896L;

    public AccessibleRole getAccessibleRole()
    {
      return AccessibleRole.FRAME;
    }

    public AccessibleStateSet getAccessibleStateSet()
    {
      AccessibleStateSet states = super.getAccessibleStateSet();
      states.add(AccessibleState.ACTIVE);
      return states;
    }
  }
*/
}
