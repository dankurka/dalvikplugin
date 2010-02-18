package com.android.dalvikplugin;

public class DexResult
{
  public String   fieldTypeDescription;
  // if field is not String, Float, Long or Int, it is a complex object that can be retrieved via getObject()

  public String   valueString;
  public float    valueNativeFloat;
  public long     valueNativeLong;
  public int      valueNativeInt;

  public double   valueNativeDouble;
  public short    valueNativeShort;
  public boolean  valueNativeBoolean;
  public char     valueNativeChar;
  public byte     valueNativeByte;

  public String   valueIdentifier;
}
