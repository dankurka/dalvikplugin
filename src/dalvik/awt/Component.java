package dalvik.awt;

public class Component
{
  public Component() 
  {
  }

  int width;
  int height;

  //@Deprecated   // use getSize()
  public Dimension size() 
  {
    return new Dimension(width, height);
  }

  //@Deprecated   // use setSize()
  public void resize(int width, int height) 
  {
/*
    synchronized(getTreeLock()) 
    {
      setBoundsOp(ComponentPeer.SET_SIZE);
      setBounds(x, y, width, height);
    }
*/
  }

/*
  public Locale getLocale() 
  {
    Locale locale = this.locale;
    if (locale != null) 
      return locale;

    Container parent = this.parent;
    if (parent == null) 
      throw new IllegalComponentStateException("This component must have a parent in order to determine its locale");
    else
      return parent.getLocale();
  }
*/

  public Container getParent()
  {
    System.err.println("dalvik.awt.Component.getParent() NOT YET IMPLEMENTED ##############################################");
    return null;    // ???
  }
}
