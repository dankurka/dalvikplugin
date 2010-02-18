package com.android.dalvikplugin;

import android.content.Context;
import android.graphics.PixelFormat;
import android.opengl.GLSurfaceView;
import android.view.SurfaceHolder;        // http://developer.android.com/reference/android/view/SurfaceHolder.html
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;
import android.view.SurfaceHolder.Callback;
import android.view.ViewGroup.LayoutParams;
import android.webkit.PluginStub;         // http://d.android.com/reference/android/webkit/PluginStub.html
import android.widget.FrameLayout;
import android.widget.MediaController;
import android.widget.VideoView;

import android.app.Service;
import android.app.PendingIntent;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.AlarmManager;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.BroadcastReceiver;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Binder;
import android.os.BatteryManager;
import android.os.PowerManager;
import android.os.Vibrator;
//import android.util.Config;
//import android.util.Log;
import java.util.List;
import android.content.ServiceConnection;
import android.content.ComponentName;


// DalvikPluginStub is referenced by
// - jniRegisterNativeMethods() in jni-bridge.cpp
// - kSetPluginStubJavaClassName_ANPSetValue in main.cpp

public class DalvikPluginStub implements PluginStub
{
  static
  {
    System.loadLibrary("dalvikplugin");   // same name as in: Android.mk (but there with a leading "lib" = "libdalvikplugin")
  }

  // native methods implemented in jni-bridge.cpp (in libdalvikplugin.so)
  private native void nativeSurfaceCreated(int npp, View surfaceView);
  private native void nativeSurfaceChanged(int npp, int format, int width, int height);
  private native void nativeSurfaceDestroyed(int npp);
  private native boolean nativeIsFixedSurface(int npp);

  //////////////////////////////////////////////////////////////////////////////////////////


  private static final String LOGTAG = "DalvikPluginStub";

  private Intent serviceIntent = null;
  //private String contextString = null;
  //public static final String TICKER_NEW_ENTRY_BROADCAST_ACTION  = "org.timur.android.service.TickerNewEntry";




/*
          //private IPluginService mService = null;            // interface to service, set by onServiceConnected()
          private PluginService mService = null;

          private ServiceConnection serviceConnection = new ServiceConnection()
          {
            public void onServiceConnected(ComponentName className, IBinder service)
            {
              //if(Config.LOGD) Log.i(LOGTAG, "onServiceConnected() ++++++++++++++++++++++++++++++++++++");
              System.out.println("DalvikPluginStub onServiceConnected()");
              //mService = IPluginService.Stub.asInterface(service);
              PluginService.LocalBinder localService = (PluginService.LocalBinder)service;
              mService = localService.getService();


              if(mService!=null)
              {

//                try
//                {

                  //if(Config.LOGD) Log.i(LOGTAG, "onServiceConnected() call getServiceContext()..");
                  System.out.println("DalvikPluginStub onServiceConnected() call getServiceContext(1)");
                  //String contextString = mService.getServiceContext();
                  //System.out.println("DalvikPluginStub onServiceConnected() call getServiceContext(2b) contextString=["+contextString+"] ++++++++++++++++++++++++++++++++++++");
//                }
//                catch(android.os.RemoteException remex)
//                {
//                  //if(Config.LOGD) Log.e(LOGTAG, "onServiceConnected() remex",remex);
//                  System.out.println("DalvikPluginStub onServiceConnected() remex"+remex);
 //               }
              }

              //if(Config.LOGD) Log.i(LOGTAG, "onServiceConnected() done");
              System.out.println("DalvikPluginStub onServiceConnected() done");
            }

            public void onServiceDisconnected(ComponentName className)
            {
              //if(Config.LOGD) Log.i(LOGTAG, "onServiceDisconnected()");
              System.out.println("DalvikPluginStub onServiceDisconnected()");
              mService = null;
            }
          };
*/

  public View getEmbeddedView(final int npp, final Context context)
  {
    System.out.println("DalvikPluginStub getEmbeddedView() npp="+npp+" contex="+context+"...");

    final SurfaceView view = new SurfaceView(context);
    System.out.println("DalvikPluginStub getEmbeddedView() done new SurfaceView() view="+view+" ...");

    // You can do all sorts of interesting operations on the surface view here
    // We illustrate a few of the important operations below.

    //TODO get pixel format from the subplugin (via jni)
    if(view!=null)
    {
      //view.setVisibility(View.INVISIBLE);
      view.getHolder().setFormat(PixelFormat.RGBA_8888);

      view.getHolder().addCallback(new Callback()
      {
        public void surfaceCreated(SurfaceHolder holder)
        {
          System.out.println("DalvikPluginStub callback surfaceCreated() holder="+holder+" npp="+npp+" view="+view+" ...");
          nativeSurfaceCreated(npp, view);                        // -> surfaceCreated() in jni-bridge.cpp (fetched and instantiated the applet)  // -> surfaceCreated() in PaintPlugin.ccp

/*
          serviceIntent = new Intent(context, PluginService.class);
          ComponentName cmp = serviceIntent.getComponent();
          System.out.println("DalvikPluginStub callback surfaceCreated() cmp.getPackageName()="+cmp.getPackageName()+" cmp.getClassName()="+cmp.getClassName()+" ++++++++++++++++++++++++++++++++++++++++++++");
*/
  /*
            serviceIntent.setComponent(new ComponentName("com.android.dalvikplugin", "com.android.dalvikplugin.PluginService"));
            cmp = serviceIntent.getComponent();
            System.out.println("DalvikPluginStub callback surfaceCreated() cmp.getPackageName()="+cmp.getPackageName()+" cmp.getClassName()="+cmp.getClassName());
            System.out.println("DalvikPluginStub callback surfaceCreated() cmp="+cmp);
            System.out.println("DalvikPluginStub callback surfaceCreated() serviceIntent="+serviceIntent);
  */
/*
          try
          {
            //context.startService(serviceIntent);
            context.bindService(serviceIntent, serviceConnection, Context.BIND_AUTO_CREATE);
          }
          catch(Exception ex)
          {
            System.out.println("DalvikPluginStub callback surfaceCreated() ex="+ex);
            serviceIntent = null;
          }
*/
          System.out.println("DalvikPluginStub callback surfaceCreated() DONE +++++++++++++++++++++++++++++++++++++++++++");
        }

        public void surfaceChanged(SurfaceHolder holder, int format, int width, int height)
        {
          System.out.println("DalvikPluginStub callback surfaceChanged() holder="+holder+" npp="+npp+" format="+format+" width="+width+" height="+height);
          nativeSurfaceChanged(npp, format, width, height);       // -> surfaceChanged() in jni-bridge  // -> surfaceChanged() in PaintPlugin.ccp
        }

        public void surfaceDestroyed(SurfaceHolder holder)
        {
          System.out.println("DalvikPluginStub callback surfaceDestroyed() holder="+holder+" npp="+npp+" ...");
/*
          //if(serviceIntent!=null)
          //  context.stopService(serviceIntent);
          if(serviceConnection!=null)
            context.unbindService(serviceConnection);
*/
          nativeSurfaceDestroyed(npp);                            // -> surfaceDestroyed() in jni-bridge.cpp  //-> surfaceDestroyed() in PaintPlugin.cpp
          System.out.println("DalvikPluginStub callback surfaceDestroyed() done");
        }
      });
    }

    if(nativeIsFixedSurface(npp))                                  // native method implemented in PaintPlugin.cpp
    {
      System.out.println("DalvikPluginStub done nativeIsFixedSurface()...");
      //TODO get the fixed dimensions from the plugin
      //view.getHolder().setFixedSize(width, height);       // TODO
    }

    System.out.println("DalvikPluginStub getEmbeddedView() DONE");
    return view;
  }

  public View getFullScreenView(int npp, Context context)
  {
    System.out.println("DalvikPluginStub getFullScreenView() ########################################");
/*
    FrameLayout layout = new FrameLayout(context);
    LayoutParams fp = new LayoutParams(LayoutParams.FILL_PARENT, LayoutParams.FILL_PARENT);
    layout.setLayoutParams(fp);

    VideoView video = new VideoView(context);
    LayoutParams vp = new LayoutParams(LayoutParams.FILL_PARENT, LayoutParams.FILL_PARENT);
    layout.setLayoutParams(vp);

    GLSurfaceView gl = new GLSurfaceView(context);
    LayoutParams gp = new LayoutParams(LayoutParams.FILL_PARENT, LayoutParams.FILL_PARENT);
    layout.setLayoutParams(gp);

    layout.addView(video);
    layout.addView(gl);

    // We want an 8888 pixel format because that's required for a translucent
    // window. And we want a depth buffer.
    gl.setEGLConfigChooser(8, 8, 8, 8, 16, 0);
    // Tell the cube renderer that we want to render a translucent version
    // of the cube:
    gl.setRenderer(new CubeRenderer(true));
    // Use a surface format with an Alpha channel:
    gl.getHolder().setFormat(PixelFormat.TRANSLUCENT);
    gl.setWindowType(WindowManager.LayoutParams.TYPE_APPLICATION_MEDIA_OVERLAY);


    video.setVideoPath("/sdcard/test_video.3gp");
    video.setMediaController(new MediaController(context));
    video.requestFocus();

    System.out.println("DalvikPluginStub getFullScreenView() done");
    return layout;
*/
    return null;
  }
 }
