// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <e32base.h>
#include <f32file.h>

class RPlaySession : public RSessionBase {
public:
    TInt Connect() { return CreateSession(_L("PlayServer"), TVersion(1, 0, 0), 4); }
};

class RPlaySubSession : public RSubSessionBase {
public:
    TInt Open(RPlaySession &session, TInt type) { return CreateSubSession(session, type); }
    TInt Request(TInt op, const TIpcArgs &args) { return SendReceive(op, args); }
    TInt Notify(TInt op) { return Send(op); }
    void Close() { CloseSubSession(1); }
};

struct TGameFilter {
    TInt selector;
    TInt offset;
    TInt limit;
};

static void WriteWordL(RFile &file, TUint32 value) {
    User::LeaveIfError(file.Write(TPtrC8(reinterpret_cast<const TUint8 *>(&value), sizeof(value))));
}

static void ListGamesL(RPlaySubSession &games, RFile &output) {
    TPckgBuf<TGameFilter> filter;
    filter().selector = 9;
    filter().offset = 0;
    filter().limit = 256;
    const TDesC8 &query = filter;
    TPckgBuf<TInt> count(0);
    TInt error;
    for (TInt attempt = 0; ; ++attempt) {
        error = games.Request(0x500002, TIpcArgs(&query, &count));
        if (error != KErrNotReady || attempt == 99) { break; }
        User::After(100000);
    }
    User::LeaveIfError(error);
    if (count() < 0 || count() > filter().limit) { User::Leave(KErrCorrupt); }
    if (count() == 0) { WriteWordL(output, 0); return; }

    RBuf8 uids;
    uids.CreateL(count() * 4);
    CleanupClosePushL(uids);
    User::LeaveIfError(games.Request(0x500003, TIpcArgs(&uids)));
    if (uids.Length() != count() * 4) { User::Leave(KErrCorrupt); }

    const TDesC8 &ids = uids;
    TPckgBuf<TInt> infoCount(count());
    User::LeaveIfError(games.Request(0x500005, TIpcArgs(&ids, &infoCount)));
    if (infoCount() != count()) { User::Leave(KErrCorrupt); }
    // GamesList transfers TGameInfo records: UID, inline TBuf<128>, then status fields.
    const TInt recordSize = 328;
    RBuf8 info;
    info.CreateL(infoCount() * recordSize);
    CleanupClosePushL(info);
    User::LeaveIfError(games.Request(0x500006, TIpcArgs(&info)));
    if (info.Length() != infoCount() * recordSize) { User::Leave(KErrCorrupt); }

    WriteWordL(output, count());
    for (TInt i = 0; i < count(); ++i) {
        const TUint32 *record = reinterpret_cast<const TUint32 *>(info.Ptr() + i * recordSize);
        const TUint32 length = record[1] & 0x0FFFFFFF;
        if (record[0] != reinterpret_cast<const TUint32 *>(uids.Ptr())[i]
            || record[2] != 128 || length > 128) { User::Leave(KErrCorrupt); }
        WriteWordL(output, record[0]);
        WriteWordL(output, length);
        User::LeaveIfError(output.Write(TPtrC8(reinterpret_cast<const TUint8 *>(record + 3), length * 2)));
    }
    CleanupStack::PopAndDestroy(2, &uids);
}

static void RequestL() {
    TBuf<64> command;
    User::CommandLine(command);
    TLex arguments(command);
    const TPtrC operation = arguments.NextToken();
    TUint uid = 0;
    if (operation == _L("launch")) {
        TLex number(arguments.NextToken());
        User::LeaveIfError(number.Val(uid, EHex));
        if (!uid) { User::Leave(KErrArgument); }
    } else if (operation != _L("list")) {
        User::Leave(KErrArgument);
    }

    RPlaySession session;
    TInt error = KErrNotFound;
    // Games starts PlayServer asynchronously, after its own process is running.
    for (TInt attempt = 0; attempt < 100; ++attempt) {
        error = session.Connect();
        if (error != KErrNotFound && error != KErrServerTerminated && error != KErrNotReady) { break; }
        User::After(100000);
    }
    User::LeaveIfError(error);
    CleanupClosePushL(session);
    RPlaySubSession games;
    User::LeaveIfError(games.Open(session, 11));
    CleanupClosePushL(games);
    RFs fs;
    User::LeaveIfError(fs.Connect());
    CleanupClosePushL(fs);
    const TInt directoryError = fs.MkDirAll(_L("C:\\private\\e000ea02\\"));
    if (directoryError != KErrAlreadyExists) { User::LeaveIfError(directoryError); }
    RFile output;
    User::LeaveIfError(output.Replace(fs, _L("C:\\private\\e000ea02\\response.bin"), EFileWrite));
    CleanupClosePushL(output);
    WriteWordL(output, 0x32474E45);
    WriteWordL(output, 1);
    if (operation == _L("list")) {
        ListGamesL(games, output);
    } else {
        RPlaySubSession mode;
        User::LeaveIfError(mode.Open(session, 4));
        CleanupClosePushL(mode);
        User::LeaveIfError(mode.Notify(0x40000B));
        const TPckgBuf<TUint> game(uid);
        User::LeaveIfError(games.Request(0x50000E, TIpcArgs(&game, 0)));
        CleanupStack::PopAndDestroy(&mode);
        WriteWordL(output, 0);
    }
    CleanupStack::PopAndDestroy(4, &session);
}

GLDEF_C TInt E32Main() {
    CTrapCleanup *cleanup = CTrapCleanup::New();
    if (!cleanup) { return KErrNoMemory; }
    TRAPD(error, RequestL());
    delete cleanup;
    return error;
}
