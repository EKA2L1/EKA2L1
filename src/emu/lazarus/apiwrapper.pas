unit APIWrapper;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils;

function SymbianSystemCreate(CpuType: Longint): Longint; cdecl; name 'symbian_system_create'; library EKA2L1API;
function SymbianSystemShutdown(Sys: Longint): Longint; cdecl; name 'symbian_system_shutdown'; library EKA2L1API;

implementation

end.

