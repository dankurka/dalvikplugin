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
//import com.android.sampleplugin.graphics.CubeRenderer;

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

  public class PluginService extends Service
  {
    public class LocalBinder extends Binder
    {
      PluginService getService()
      {
        return PluginService.this;
      }
    }

    private final IBinder binder = new LocalBinder();
/*
    private final IPluginService.Stub binder = new IPluginService.Stub()
    {
      public String getServiceContext() { return getServiceContextImpl(); }
    };
*/

    @Override
    public IBinder onBind(Intent intent)
    {
      //if(Config.LOGD) Log.i(LOGTAG, "onBind()");
      System.out.println("PluginService onBind() ---------------------------");
      return binder;
    }


    private Context       context = null;
    private String        contextString = null;
    private Vibrator      vibrator = null;

/*
    public PluginService()
    {
    }
*/


/*
    @Override
    public int onStartCommand(Intent intent, int flags, int startId)
    {
      System.out.println("PluginService onStartCommand startId="+startId+": " + intent+ "++++++++++++++++++++++++++++++++");
      // We want this service to continue running until it is explicitly
      // stopped, so return sticky.
      return START_STICKY;
    }
*/

/*
//  synchronized private String getServiceContextImpl()
    synchronized public String getServiceContext()
    {
      //if(Config.LOGD) Log.i(LOGTAG, "getServiceContextImpl()");

      System.out.println("PluginService getServiceContextImpl() contextString=["+contextString+"] ++++++++++++++++++++++++++++++++++++++++++++++++++++++");

      if(vibrator!=null)
      {
        //if(Config.LOGD) Log.i(LOGTAG, "onReceive vibrator.vibrate(1000) +++++++++++++++++++++++++++");
        System.out.println("PluginService onReceive vibrator.vibrate(1000) +++++++++++++++++++++++++++");
        vibrator.vibrate(1000);    // buzz
      }

      return contextString;
    }
*/

    @Override
    public void onCreate()
    {
      // is called when the service process is created
      super.onCreate();
      //if(Config.LOGD) Log.i(LOGTAG, "onCreate()");
      System.out.println("PluginService onCreate() *****************************************");

      context = (Context)this;
      contextString = ""+context;
      vibrator = (Vibrator)this.getSystemService(Context.VIBRATOR_SERVICE);

/*
      startServiceMs = new java.util.Date().getTime();

      powerManager     = (PowerManager)this.getSystemService(Context.POWER_SERVICE);
      wakeLock         = powerManager.newWakeLock(PowerManager.SCREEN_BRIGHT_WAKE_LOCK|
                                                  PowerManager.ACQUIRE_CAUSES_WAKEUP|
                                                  PowerManager.ON_AFTER_RELEASE,
                                                  "My Tag");
      wakeLockNoScreen = powerManager.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK,
                                                  "My Tag2");

      notificationManager = (NotificationManager)getSystemService(Context.NOTIFICATION_SERVICE);

      updateWakeOnKeywordsImpl();

      // get 'last_timestamp' property
      preferences = getSharedPreferences(PREFS_NAME, MODE_WORLD_WRITEABLE);
      editor = preferences.edit();

      String lastTimestamp = preferences.getString("last_timestamp", "");
      if(lastTimestamp!=null && lastTimestamp.length()>0)
      {
        try
        {
          lastTimestampMS = new Long(lastTimestamp).longValue();
          if(Config.LOGD) Log.i(LOGTAG, "onCreate() lastTimestampMS="+lastTimestampMS);
        }
        catch(java.lang.NumberFormatException nfex)
        {
          if(Config.LOGD) Log.e(LOGTAG, "onCreate() lastTimestamp nfex=",nfex);
        }
      }

      buzzOnKeyword = preferences.getBoolean("buzzOnKeyword", true);
*/
    }

/*
    @Override
    public void onStart(Intent intent, int startId)
    {
      // osStart() = the service needs data passed into it from the starting process and we don't wish to use IPC
      //if(Config.LOGD) Log.i(LOGTAG, "onStart() startId="+startId);
      System.out.println("PluginService onStart() *****************************************");
    }

    @Override
    public void onLowMemory()
    {
      //if(Config.LOGD) Log.i(LOGTAG, "onLowMemory()");
      System.out.println("PluginService onLowMemory() *****************************************");
    }
*/

    @Override
    public void onDestroy()
    {
      //if(Config.LOGD) Log.i(LOGTAG, "onDestroy()");
      System.out.println("PluginService onDestroy() *****************************************");
    }

  }
