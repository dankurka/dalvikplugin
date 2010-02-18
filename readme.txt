cp ../../out/target/product/generic/data/app/DalvikPlugin.apk .

aidl src/com/android/dalvikplugin/IPluginService.aidl

javap -s -p bin/classes/org/timur/jil/Jil

adb logcat | grep -E '/Dex|/plugin|/System.out|browser|signal|/System.err|/webcore|Accelero|Jil|DeviceImpl|Position'


