unit MainForm;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, FileUtil, Forms, Controls, Graphics, Dialogs, Menus;

type

  { TMainForm }

  TMainForm = class(TForm)
    MainMenu: TMainMenu;
    FileMenu: TMenuItem;
    EditMenu: TMenuItem;
    InstallSISOption: TMenuItem;
    InstallROMOption: TMenuItem;
    DeviceMappingOption: TMenuItem;
    MenuItem1: TMenuItem;
    QuitOption: TMenuItem;
    FileMenuSep1: TMenuItem;
    procedure FormCreate(Sender: TObject);
    procedure FileMenuClick(Sender: TObject);
    procedure QuitOptionClick(Sender: TObject);
  private

  public

  end;

var
  MainForm: TMainForm;

implementation

{$R *.lfm}

{ TMainForm }

procedure TMainForm.FormCreate(Sender: TObject);
begin

end;

procedure TMainForm.FileMenuClick(Sender: TObject);
begin

end;

procedure TMainForm.QuitOptionClick(Sender: TObject);
begin
    self.Qui
end;

end.

