"""Request a real Windows WLAN scan, keeping the handle alive until completion."""
import ctypes as c
import threading

class Notification(c.Structure):
    _fields_=[('source',c.c_ulong),('code',c.c_ulong),('guid',c.c_ubyte*16),('size',c.c_ulong),('data',c.c_void_p)]

w=c.WinDLL('wlanapi');handle=c.c_void_p();version=c.c_ulong();interfaces=c.c_void_p();done=threading.Event();result=[]
CALLBACK=c.WINFUNCTYPE(None,c.POINTER(Notification),c.c_void_p)
@CALLBACK
def notified(pointer,context):
    n=pointer.contents
    if n.source==8 and n.code in (7,8):result.append(n.code);done.set()
w.WlanOpenHandle.argtypes=[c.c_ulong,c.c_void_p,c.POINTER(c.c_ulong),c.POINTER(c.c_void_p)]
w.WlanEnumInterfaces.argtypes=[c.c_void_p,c.c_void_p,c.POINTER(c.c_void_p)]
w.WlanRegisterNotification.argtypes=[c.c_void_p,c.c_ulong,c.c_bool,CALLBACK,c.c_void_p,c.c_void_p,c.POINTER(c.c_ulong)]
w.WlanScan.argtypes=[c.c_void_p,c.c_void_p,c.c_void_p,c.c_void_p,c.c_void_p]
w.WlanFreeMemory.argtypes=[c.c_void_p];w.WlanCloseHandle.argtypes=[c.c_void_p,c.c_void_p]
assert w.WlanOpenHandle(2,None,c.byref(version),c.byref(handle))==0
try:
    assert w.WlanEnumInterfaces(handle,None,c.byref(interfaces))==0
    assert c.c_ulong.from_address(interfaces.value).value>0
    previous=c.c_ulong();assert w.WlanRegisterNotification(handle,8,False,notified,None,None,c.byref(previous))==0
    assert w.WlanScan(handle,c.c_void_p(interfaces.value+8),None,None,None)==0
    assert done.wait(20),'WLAN scan notification timeout'
    assert result[-1]==7,'WLAN scan failed'
    print('PASS: real WLAN scan completed.')
finally:
    if interfaces:w.WlanFreeMemory(interfaces)
    w.WlanCloseHandle(handle,None)
