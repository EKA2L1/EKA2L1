unit MainForm;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Forms, Dialogs, Menus,
  Grids, Controls, StrUtils, symbian, DvcMapping, XMLRead,
  XMLWrite, DOM, JsonConf, LCLType;

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

    procedure AppListButtonClick(Sender: TObject; aCol, aRow: integer);
    procedure AppListDblClick(Sender: TObject);
    procedure AppListSelectCell(Sender: TObject; aCol, aRow: integer;
      var CanSelect: boolean);
    procedure DeviceMappingOptionClick(Sender: TObject);
    procedure FormConstrainedResize(Sender: TObject;
      var MinWidth, MinHeight, MaxWidth, MaxHeight: TConstraintSize);

    procedure FormClose(Sender: TObject; var CloseAction: TCloseAction);
    procedure FormCreate(Sender: TObject);
    procedure FileMenuClick(Sender: TObject);
    procedure InstallROMOptionClick(Sender: TObject);
    procedure InstallSISOptionClick(Sender: TObject);
    procedure QuitOptionClick(Sender: TObject);
    procedure InitAppList;
  end;

  type TSymThread = class(TThread)
  public
    constructor Create(CreateSus: boolean);

  public
    id: Longint;

  protected
    procedure Execute; override;
  end;

var
  EMainForm: TMainForm;
  ESym: TSymbian;
  CMapPath: ansistring;
  EMapPath: ansistring;
  RomPath: ansistring;
  Doc: TXMLDocument;
  oneRun: Boolean;

function GetNode(AppendTo: TDOMNode; Name: ansistring): TDomNode;
function GetTextNode(Name: ansistring): TDomNode;
procedure SaveConfig;
procedure LoadConfig;

implementation

{$R *.lfm}

{ TMainForm }

procedure SaveConfig;
var
  c: TJSONConfig;
begin
  c := TJSONConfig.Create(nil);
  try
    c.FileName := 'config.json';
    c.SetValue('rom', RomPath);
    c.SetValue('cmap', CMapPath);
    c.SetValue('emap', EMapPath);
  finally
    c.Free;
  end;
end;

procedure LoadConfig;
var
  c: TJSONConfig;
begin
  c := TJSONConfig.Create(nil);
  try
     c.Filename := 'config.json';

     CMapPath := c.GetValue('cmap','drives/c/');
     EMapPath := c.GetValue('emap', 'drives/e/');
     RomPath := c.GetValue('rom', 'SYM.ROM');
  finally
    c.Free;
  end;
  
  ESym.MountC(CMapPath);
  ESym.MountE(EMapPath);

  ESym.LoadRom(RomPath);

  SaveConfig;
end;

constructor TSymThread.Create(CreateSus: boolean);
begin
  inherited Create(CreateSus);
  FreeOnTerminate := true;
end;

procedure TSymThread.Execute;
var res: Boolean;
begin
  ESym.Load(id);
  ESym.Loop;

  OneRun:=false;
end;

procedure TMainForm.InitAppList;
var
  total, i: longint;
  id: longword;
  appname: UnicodeString;
begin
  total := ESym.TotalApp;

  for i := 0 to total - 1 do
  begin
    appname := ESym.GetApp(i, @id);
    AppList.Cells[0, i + 1] := '0x' + HexStr(QWord(id), 8);
    AppList.Cells[1, i + 1] := appname;
  end;
end;

procedure TMainForm.FormCreate(Sender: TObject);
var
  CMapNode, EMapNode, DrivesNode, Root: TDomNode;
begin
  Self.OnConstrainedResize := @FormConstrainedResize;

  ESym := TSymbian.Create;
  ESym.Init;

  LoadConfig;
  InitAppList;
end;

function GetNode(AppendTo: TDomNode; Name: ansistring): TDomNode;
var
  RetNode: TDomNode;
begin
  RetNode := AppendTo.FindNode(Name);

  if not Assigned(RetNode) then
  begin
    RetNode := Doc.CreateElement(Name);
    AppendTo.AppendChild(RetNode);
  end;

  exit(RetNode);
end;

function GetTextNode(Name: ansistring): TDomNode;
var
  RetNode: TDomNode;
begin
  RetNode := Doc.DocumentElement.FindNode(Name);

  if not Assigned(RetNode) then
  begin
    RetNode := Doc.CreateTextNode(Name);
  end;

  exit(RetNode);
end;

procedure TMainForm.FormClose(Sender: TObject; var CloseAction: TCloseAction);
begin
end;

procedure TMainForm.FileMenuClick(Sender: TObject);
begin

end;

procedure TMainForm.InstallROMOptionClick(Sender: TObject);
var
  RomNode, RomText: TDOMnode;
begin
  if (OpenDialog.Execute) then
  begin
    RomPath := OpenDialog.Filename;
    ESym.LoadRom(RomPath);
    SaveConfig;
  end;
end;

procedure TMainForm.InstallSISOptionClick(Sender: TObject);
var
  SisFile: ansistring;
begin
  if (OpenDialog.Execute) then
  begin
    SisFile := OpenDialog.Filename;
    if MessageDlg('Drive', 'Yes for C:, No for E:',
       mtConfirmation, [mbYes, mbNo], 0) = mrYes then
    begin
      if (ESym.InstallSIS(0, SisFile)) then
      begin
        ShowMessage('Success!');
      end;
    end else
    begin
      if (ESym.InstallSIS(1, SisFile)) then
      begin
        ShowMessage('Success!');
      end;
    end;
  end;
end;

procedure TMainForm.FormConstrainedResize(Sender: TObject;
  var MinWidth, MinHeight, MaxWidth, MaxHeight: TConstraintSize);
begin
end;

procedure TMainForm.AppListButtonClick(Sender: TObject; aCol, aRow: integer);
var
  id: ansistring;
begin
  id := AppList.Cells[0, aRow];
  ShowMessage(id);
end;

procedure TMainForm.AppListDblClick(Sender: TObject);
var
  pt: TPoint;
  col, row: longint;
  id: longword;
  ids: ansistring;
  thr: TSymThread;
begin
  pt := Mouse.CursorPos;
  pt := (Sender as TStringGrid).ScreenToClient(pt);
  TStringGrid(Sender).MouseToCell(pt.x, pt.y, col, row);

  ids := TStringGrid(Sender).Cells[0, row];
  Delete(ids, 1, 2);

  if (row >= 1) then
    id := Hex2Dec(ids);

  if (not OneRun) then
  begin
    OneRun := true;

    thr := TSymThread.Create(true);
    thr.id:= id;

    thr.Start;
  end;
end;

procedure TMainForm.AppListSelectCell(Sender: TObject; aCol, aRow: integer;
  var CanSelect: boolean);
begin
end;

procedure TMainForm.DeviceMappingOptionClick(Sender: TObject);
var
  DeviceMapperForm: TDeviceMapper;
  DeviceNode, CNode, TextCNode, ENode, TextENode: TDomNode;
begin
  DeviceMapperForm := TDeviceMapper.Create(nil);

  try
    DeviceMapperForm.ShowModal;
  finally
    if (DeviceMapperForm.ShouldSave) then
    begin
      CMapPath := DeviceMapperForm.CPath;
      EMapPath := DeviceMapperForm.EPath;
                  
      ESym.MountC(CMapPath);
      ESym.MountE(EMapPath);

      SaveConfig;
    end;

    DeviceMapperForm.Free;
  end;
end;

procedure TMainForm.QuitOptionClick(Sender: TObject);
begin
  SaveConfig;
  Close;
end;

end.
