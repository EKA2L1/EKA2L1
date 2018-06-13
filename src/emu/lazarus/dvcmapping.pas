unit dvcmapping;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, FileUtil, Forms, Controls, Graphics, Dialogs, StdCtrls,
  ExtCtrls, JsonConf;

type

  { TDeviceMapper }

  TDeviceMapper = class(TForm)
    Button1: TButton;
    SaveButton: TButton;
    EPathBrowse: TButton;
    EPathEdit: TEdit;
    ELabel: TLabel;
    CPathBrowse: TButton;
    CLabel: TLabel;
    CPathEdit: TEdit;
    SelectDirDialog: TSelectDirectoryDialog;
    StaticText1: TStaticText;
    procedure Button1Click(Sender: TObject);
    procedure CPathBrowseClick(Sender: TObject);
    procedure EPathBrowseClick(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure SaveButtonClick(Sender: TObject);    

  private
    RequestSave: Boolean;

  public
     CPath: AnsiString;
     EPath: AnsiString;

     property ShouldSave: Boolean read RequestSave;
  end;

var
  DeviceMapper: TDeviceMapper;

implementation

{$R *.lfm}

{ TDeviceMapper }

procedure TDeviceMapper.CPathBrowseClick(Sender: TObject);
begin
  if (SelectDirDialog.Execute) then
  begin
    CPath := SelectDirDialog.FileName;
    CPathEdit.Text := CPath;
  end;
end;

procedure TDeviceMapper.Button1Click(Sender: TObject);
begin
  Close;
end;

procedure TDeviceMapper.EPathBrowseClick(Sender: TObject);
begin
  if (SelectDirDialog.Execute) then
  begin
    EPath := SelectDirDialog.FileName;
    EPathEdit.Text := EPath;
  end;
end;

procedure TDeviceMapper.FormCreate(Sender: TObject);
var c: TJsonConfig;
begin
  c := TJSONConfig.Create(nil);
  try
     c.Filename := 'config.json';

     CPathEdit.Text := ExtractFilePath(c.GetValue('cmap','drives/c/'));
     EPathEdit.Text := ExtractFilePath(c.GetValue('emap', 'drives/e/'));
  finally
    c.Free;
  end;
end;

procedure TDeviceMapper.SaveButtonClick(Sender: TObject);
begin
  RequestSave:=true;
  Close;
end;

end.

