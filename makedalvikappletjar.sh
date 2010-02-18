rm -rf dalvikapplet
mkdir dalvikapplet
cd dalvikapplet
jar xf ../../../out/target/common/obj/APPS/DalvikPlugin_intermediates/classes.jar
rm -rf com META-INF dalvik/applet/PluginObjectInterf.class
jar cf ../dalvikapplet.jar .
cd ..
