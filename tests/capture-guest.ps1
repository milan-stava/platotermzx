# Optional manual network reproduction. Public guest fields only, no password.
$ErrorActionPreference='Stop'
Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Net.Sockets;
using System.Diagnostics;
public class GuestProbe {
 static int phase,word,index;
 static void Reply(NetworkStream s,int value) {
  value &= 127;
  if(value==0x60)value=1;
  else if(value==0x70)value=12;
  else if(value==0x71)value=1;
  else if(value==0x72)value=0;
  else if(value==0x73)value=64;
  else if(value==0x7d)value=127;
  else if(value==0x7b)return;
  int key=128|value;
  byte[] reply={27,(byte)(64|(key&63)),(byte)(96|((key>>6)&15))};
  s.Write(reply,0,3);
 }
 static byte[] Read(NetworkStream s) {
  var result=new MemoryStream();var total=Stopwatch.StartNew();var idle=Stopwatch.StartNew();
  while(total.ElapsedMilliseconds<15000 && idle.ElapsedMilliseconds<2500) {
   int b;
   try { b=s.ReadByte(); } catch(IOException) {continue;}
   if(b<0)break;
   idle.Restart();result.WriteByte((byte)b);b &=127;
   if(phase==2) { word|=(b&63)<<(index*6);if(++index==3){Reply(s,word);phase=0;} }
   else if(phase==1) {phase=b==89?2:0;word=0;index=0;}
   else if(b==27)phase=1;
  }
  return result.ToArray();
 }
 public static void Run(string path) {
  using(var c=new TcpClient()) {
   var wait=c.BeginConnect("irata.online",8005,null,null);
   if(!wait.AsyncWaitHandle.WaitOne(5000))throw new Exception("Connect timeout");
   c.EndConnect(wait);var s=c.GetStream();s.ReadTimeout=300;
   File.WriteAllBytes(Path.Combine(path,"guest-start.bin"),Read(s));
   byte[] name=System.Text.Encoding.ASCII.GetBytes("guest");
   s.Write(name,0,name.Length);
   File.WriteAllBytes(Path.Combine(path,"guest-typed.bin"),Read(s));
   s.WriteByte(13);
   File.WriteAllBytes(Path.Combine(path,"guest-enter.bin"),Read(s));
   s.Write(name,0,name.Length);
   File.WriteAllBytes(Path.Combine(path,"group-typed.bin"),Read(s));
   s.WriteByte(13);
   File.WriteAllBytes(Path.Combine(path,"group-enter.bin"),Read(s));
  }
 }
}
'@
[GuestProbe]::Run((Join-Path (Split-Path $PSScriptRoot) 'build-STD'))
Get-Item "$PSScriptRoot/../build-STD/guest-*.bin" | Select-Object Name,Length
