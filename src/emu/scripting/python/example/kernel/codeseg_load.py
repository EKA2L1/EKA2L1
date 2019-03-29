import symemu
import symemu2.events

def scriptEntry():
    # Load EUSER DLL.
    seg = symemu.loadCodeseg('euser.dll')  
    
    symemu.emulog('Runtime code address: 0x{:X}, size: 0x{:X}'.format(seg.codeRunAddress(), seg.codeSize()))
    symemu.emulog('Runtime data address: 0x{:X}, size: 0x{:X}'.format(seg.dataRunAddress(), seg.dataSize()))
    symemu.emulog('Runtime bss address: 0x{:X}, size: 0x{:X}'.format(seg.bssRunAddress(), seg.bssSize()))
    symemu.emulog('Total exports: {}'.format(seg.exportCount()))
    symemu.emulog('Export 649 loaded address: 0x{:X}'.format(seg.lookup(649)))
