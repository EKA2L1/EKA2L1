unit symbian;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Dialogs, APIWrapper;

type
  TSymbian = class(TObject)
    System: longint;
    SymVer: longint;

  public
    constructor Create;
    destructor Destroy;

    procedure SetVer(ver: longint);
    procedure Init;

    function TotalApp: Longint;
    function GetApp(idx: longint; id: PLongWord): UnicodeString;

    procedure Load(id: Longword);
    function InstallSIS(dr: Longint; path: AnsiString): boolean;

    function LoadRom(path: AnsiString): boolean;
    procedure MountC(path: AnsiString);
    procedure MountE(path: AnsiString);

    procedure Loop;
    procedure Shut;

    procedure Reset;

    property Sys: Longint read system;
  end;

implementation

procedure TSymbian.Reset;
begin
  SymbianSystemReset(system);
end;

function TSymbian.InstallSIS(dr: longint; path: AnsiString): boolean;
begin
  exit(SymbianSystemInstall(system, dr, PChar(path)) = 0);
end;

procedure TSymbian.Load(id: Longword);
begin
  SymbianSystemLoad(system, id);
end;

procedure TSymbian.Loop;
begin
  SymbianSystemLoop(system);
end;

procedure TSymbian.Shut;
begin
  if (system > -1) then
  begin
    SymbianSystemShutdown(system);
    system := -1;
  end;
end;

function TSymbian.LoadRom(path: AnsiString): boolean;
var res: longint;
begin
  res := SymbianSystemLoadRom(system, PChar(path));

  if (res = -1) then
     exit(false);

  exit(true);
end;

procedure TSymbian.MountC(path: AnsiString);
begin
  SymbianSystemMount(system, 'C:', PChar(path));
end;

procedure TSymbian.MountE(path: AnsiString);
begin
  SymbianSystemMount(system, 'E:', PChar(path));
end;

function TSymbian.GetApp(idx: longint; id: PLongWord): UnicodeString;
var name: PWideChar;
    namelen, res, i: longint;
    ret: UnicodeString;
begin
  name := getmem(200);
  res := SymbianSystemGetAppInstalled(System, idx, name, @namelen, id);

  if (res = -1) then
  begin
    exit('');
  end;

  for i:=1 to namelen do
  begin
    ret += (name + i - 1)^;
  end;

  freemem(name);

  exit(ret);
end;

function TSymbian.TotalApp: Longint;
begin
   exit(SymbianSystemGetTotalAppInstalled(System));
end;

procedure TSymbian.Init;
var res: longint;
begin
  res:=SymbianSystemInit(System);

  if (res = -1) then
  begin
    ShowMessage('Failed to initialize the System!');
    exit;
  end;
end;

procedure TSymbian.SetVer(ver: longint);
begin
  SymVer := ver;
  SymbianSystemSetCurrentSymbianUse(System, ver);
end;

constructor TSymbian.Create;
begin
  System := SymbianSystemCreate(GLFW, OPENGL, UNICORN);

  if (System = -1) then
  begin
    ShowMessage('Successfully create Symbian system!');
    exit;
  end;
end;

destructor TSymbian.Destroy;
var
  res: longint;
begin
  Shut;

  inherited;
end;

end.
