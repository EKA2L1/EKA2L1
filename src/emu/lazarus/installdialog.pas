unit InstallDialog;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, FileUtil, Forms, Controls, Graphics, Dialogs, ComCtrls,
  StdCtrls, syncobjs, Math, Symbian
{$ifdef unix}
  ,cthreads,
  cmem
{$endif}
  ;

type

  { TInstallForm }

  TInstallForm = class(TForm)
    InstallingMsg: TLabel;
    InstallingProgress: TProgressBar;

  private
    System: TSymbian;
    FileNames: TStrings;

  public
    procedure Init(Sys: TSymbian; Files: TStrings);
    procedure Install;
  end;
                    
  TInstallSISThr = class(TThread)
  public
    Form: TInstallForm;
    FileNames: TStrings;
    Step: Longint;
    CrrFile: AnsiString;

  public
    constructor Create(CreateSuspend: boolean; Name: TStrings);
    procedure ProgressLoadingBar;
    procedure InstallMsg;
    procedure CloseForm;
    procedure Execute; override;
  end;
var
  InstallForm: TInstallForm;
  InstallCs: TCriticalSection;

procedure InitInstallDialogCS;

implementation

{$R *.lfm}

procedure InitInstallDialogCS;
begin
  InstallCs := TCriticalSection.Create;
end;

procedure TInstallForm.Init(Sys: TSymbian; Files: TStrings);
begin
  System := Sys;
  FileNames := Files;
end;

procedure TInstallForm.Install;
var thr: TInstallSISThr;
begin
    InstallingProgress.Position:= 0;

    thr := TInstallSISThr.Create(true, FileNames);
    thr.Step := 100 div FileNames.Count + 100 mod FileNames.Count;

    thr.Start;
end;

constructor TInstallSISThr.Create(CreateSuspend: boolean; Name: TStrings);
begin
  inherited Create(CreateSuspend);
  FreeOnTerminate := true;
  FileNames := Name;
end;

procedure TInstallSisThr.InstallMsg;
begin
  InstallForm.InstallingMsg.Caption := 'Installing ' + CrrFile + '...';
end;

procedure TInstallSISThr.ProgressLoadingBar;
begin
  InstallForm.InstallingProgress.StepBy(step);
end;

procedure TInstallSISThr.CloseForm;
begin
  InstallForm.Close;
end;

procedure TInstallSISThr.Execute;
var res: boolean;
    i: longint;
begin
    installcs.Acquire;

    try
       for i:=0 to FileNames.Count - 1 do
       begin
          CrrFile := FileNames[i];
          Synchronize(@InstallMsg);

          res := ESym.InstallSIS(0, FileNames[i]);
          Synchronize(@ProgressLoadingBar);
       end;
    finally
       installcs.Release;
    end;

    if not res then
    begin
        ShowMessage('Error with installing this package, skipping!');
    end;

    Synchronize(@CloseForm);
end;

end.

