package dalvik.awt;

//import dalvik.awt.image.ImageObserver;

public abstract class Image
{
  protected float accelerationPriority = .5f;
/*
  public abstract int getWidth(ImageObserver observer);
  public abstract int getHeight(ImageObserver observer);
  public abstract Object getProperty(String name, ImageObserver observer);
*/
  public static final Object UndefinedProperty = new Object();

  public static final int SCALE_DEFAULT = 1;
  public static final int SCALE_FAST = 2;
  public static final int SCALE_SMOOTH = 4;
  public static final int SCALE_REPLICATE = 8;
  public static final int SCALE_AREA_AVERAGING = 16;

  public void flush()
  {
/*
    if (surfaceManager != null)
    {
      surfaceManager.flush();
    }
*/
  }

  public void setAccelerationPriority(float priority)
  {
    if (priority < 0 || priority > 1)
      throw new IllegalArgumentException("Priority must be a value between 0 and 1, inclusive");

    accelerationPriority = priority;
  }

  public float getAccelerationPriority()
  {
    return accelerationPriority;
  }
}
