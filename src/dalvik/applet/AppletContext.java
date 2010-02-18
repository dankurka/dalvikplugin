package dalvik.applet;

import dalvik.awt.Image;
import java.net.URL;

public interface AppletContext
{
//  AudioClip getAudioClip(URL url);
    Image getImage(URL url);
    Applet getApplet(String name);
//  Enumeration<Applet> getApplets();
    void showDocument(URL url);
    public void showDocument(URL url, String target);
    void showStatus(String status);
//  public void setStream(String key, InputStream stream)throws IOException;
//  public InputStream getStream(String key);
//  public Iterator<String> getStreamKeys();
}
