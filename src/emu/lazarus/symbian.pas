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
  end;

implementation

function TSymbian.GetApp(idx: longint; id: PLongWord): UnicodeString;
var name: PWideChar;
    namelen, res, i: longint;
    ret: UnicodeString;
    a: char;
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
  System := SymbianSystemCreate(UNICORN);

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
  if (System > -1) then
  begin
    res := SymbianSystemShutdown(System);

    if (res = -1) then
    begin
      ShowMessage('Fail to shutdown the System!');
    end;
  end;

  inherited;
end;

end.
