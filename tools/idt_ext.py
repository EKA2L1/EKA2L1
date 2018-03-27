import idaapi
import idc
import types
import os

idt = GetIdbPath()

idt = idt.replace('.idb','.idt')
idt = idt.replace('.i64','.idt')

dll = GetInputFile()

f = open(idt, 'wb')
f.write("0 Name = %s\n" % (dll))
for i in xrange(idaapi.get_entry_qty()):
    fn = idaapi.getn_func(i)
    a = fn.startEA
    if a != BADADDR:
       eo = GetEntryOrdinal(i)
       nm = GetFunctionName(GetEntryPoint(eo))
       if nm!='':
          f.write("%d Name=%s\n" % (eo,nm))
f.close()
