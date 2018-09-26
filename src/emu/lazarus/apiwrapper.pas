unit APIWrapper;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils;

const EKA2L1API = 'eka2l1_api';
      EPOC6 = $65243205;
      EPOC9 = $65243209;
      UNICORN = 0;
      DYNARMIC = 1;
      GLFW = $50535054;
      OPENGL = $10000000;
      VFS_ATTRIB_HIDDEN = $100;         
      VFS_ATTRIB_WRITE_PROTECTED = $200;
      VFS_ATTRIB_INTERNAL = $400;
      VFS_ATTRIB_REMOVEABLE = $800;
      VFS_DRIVE_A = $00;
      VFS_DRIVE_B = $01;
      VFS_DRIVE_C = $02;
      VFS_DRIVE_D = $03;
      VFS_DRIVE_E = $04;
      VFS_MEDIA_PHYSICAL = $02;
      VFS_MEDIA_ROM = $04;

function SymbianSystemCreate(WinType: Longint; RenderType: Longint; CpuType: Longint): Longint; cdecl; external EKA2L1API name 'create_symbian_system';
function SymbianSystemShutdown(Sys: Longint): Longint; cdecl; external EKA2L1API name 'shutdown_symbian_system';
function SymbianSystemFree(Sys: Longint): Longint; cdecl; external EKA2L1API name 'free_symbian_system';
function SymbianSystemInit(Sys: Longint): Longint; cdecl; external EKA2L1API name 'init_symbian_system';
function SymbianSystemLoad(Sys: Longint; AppID: Longword): Longint; cdecl; external EKA2L1API name 'load_process';
function SymbianSystemLoop(Sys: Longint): Longint; cdecl; external EKA2L1API name 'loop_system';
function SymbianSystemMount(Sys: Longint; Attrib: Longint; MediaType: Longint; Drive: Longint; RealPath: PChar): Longint; cdecl; external EKA2L1API name 'mount_symbian_system';
function SymbianSystemLoadRom(Sys: Longint; Path: PChar): Longint; cdecl; external EKA2L1API name 'load_rom';
function SymbianSystemSetCurrentSymbianUse(Sys: Longint; Ver: Longword): Longint; cdecl; external EKA2L1API name 'set_current_symbian_use';
function SymbianSystemGetCurrentSymbianUse(Sys: Longint; Ver: PLongWord): Longint; cdecl; external EKA2L1API name 'get_current_symbian_use';
function SymbianSystemGetTotalAppInstalled(Sys: Longint): Longint; cdecl; external EKA2L1API name 'get_total_app_installed';
function SymbianSystemGetAppInstalled(Sys: Longint; Idx: Longint; Name: PWideChar; NameLength: PLongint; UID: PLongWord): Longint; external EKA2L1API name 'get_app_installed';
function SymbianSystemInstall(Sys: Longint; drive: Longint; path: PChar): Longint;external EKA2L1API name 'install_sis';
function SymbianSystemReset(Sys: Longint): Longint; cdecl; external EKA2L1API name 'reinit_system';

implementation

end.

