*** Dalvik Plugin for Android

Dalvikplugin creates a bridge between the Android browser and the
Dalvik VM. This makes it possible to deploy and execute hybrid Web
applications, where DOM extensions can be implemented in a VM
language (such as Java or Scala) and can be consumed by Javascript
in the browser.

Dalvikplugin is currently in alpha state. At this point, Dalvikplugin
can be deployed in the Android emulator and on rooted Android 2.x
devices. We expect Dalvikplugin to become available on non-rooted
Android 2.x devices in the near future.


*** Build the plugin

Android's Open Source codebase is needed to build the plugin. Please
follow these steps:

1. Switch to the root of the Android codebase and run the usual
commands to build:

	$ . build/envsetup.sh

	$ lunch generic-eng

	$ make

2. Switch to subfolder 'external' and pull the Dalvikplugin source.
This will create a 'dalvikplugin' subfolder:

	$ cd external/

	$ git clone git://github.com/mehrvarz/dalvikplugin.git

3. Switch to 'dalvikplugin' and execute the command to build a local
target:

	$ cd dalvikplugin/

	$ mm

This will create an installable Dalvikplugin archive 'DalvikPlugin.apk'
in folder 'out/target/product/generic/data/app/' (see: 'cp-plugin.sh').


*** Install the plugin

Method 1: use 'adb install' to install your Dalvikplugin in the
Android emulator or on a physical device.

Method 2: Put DalvikPlugin.apk on a webserver and enter it's URL
in the Android browser. A precompiled binary of Dalvikplugin can
also be installed from
'http://lab.vodafone.com/dalvikplugin/DalvikPlugin.apk'.


*** Create your own Dalvik 'Applets'

--- to be completed ---


*** Run Dalvik 'Applets'

Some hybrid Webapp samples can be found here:
'http://lab.vodafone.com/dalvikplugin/'

