unit MainForm;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Forms, Dialogs, Menus,
  Grids, Controls, symbian;

type

  { TMainForm }

  TMainForm = class(TForm)
    AppList: TStringGrid;
    MainMenu: TMainMenu;
    FileMenu: TMenuItem;
    EditMenu: TMenuItem;
    InstallSISOption: TMenuItem;
    InstallROMOption: TMenuItem;
    DeviceMappingOption: TMenuItem;
    MenuItem1: TMenuItem;
    OpenDialog: TOpenDialog;
    QuitOption: TMenuItem;
    FileMenuSep1: TMenuItem;
                                
    procedure FormConstrainedResize(Sender: TObject; var MinWidth, MinHeight,
      MaxWidth, MaxHeight: TConstraintSize);

    procedure FormClose(Sender: TObject; var CloseAction: TCloseAction);
    procedure FormCreate(Sender: TObject);
    procedure FileMenuClick(Sender: TObject);
    procedure InstallSISOptionClick(Sender: TObject);
    procedure QuitOptionClick(Sender: TObject);
    procedure InitAppList;
  end;

var
  EMainForm: TMainForm;  
  ESym: TSymbian;

implementation

{$R *.lfm}

{ TMainForm }

procedure TMainForm.InitAppList;
var
  total, i: longint;
  id: Longword;
  appname: UnicodeString;
begin
  total := ESym.TotalApp;

  for i:=0 to total-1 do
  begin
     appname := ESym.GetApp(i, @id);
     AppList.Cells[0, i + 1] := '0x' + HexStr(QWord(id), 8);
     AppList.Cells[1, i + 1] := appname;
  end;
end;

procedure TMainForm.FormCreate(Sender: TObject);
begin
  Self.OnConstrainedResize:= @FormConstrainedResize;

  ESym := TSymbian.Create;
  ESym.Init;

  InitAppList;
end;

procedure TMainForm.FormClose(Sender: TObject; var CloseAction: TCloseAction);
begin
  ESym.Destroy;
end;

procedure TMainForm.FileMenuClick(Sender: TObject);
begin

end;

procedure TMainForm.InstallSISOptionClick(Sender: TObject);
var
  SisFile: ansistring;
begin
  if (OpenDialog.Execute) then
  begin
    SisFile := OpenDialog.Filename;
  end;
end;

procedure TMainForm.FormConstrainedResize(Sender: TObject; var MinWidth, MinHeight,
  MaxWidth, MaxHeight: TConstraintSize);
begin
  MinWidth := 583;
  MinHeight := 836;
  MaxHeight:= 836;
  MaxWidth := 583;
end;

procedure TMainForm.QuitOptionClick(Sender: TObject);
begin
  Close;
end;

end.
