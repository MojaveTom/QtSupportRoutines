#include "supportfunctions.h"
#include <unistd.h>
#include <stdlib.h>


/******    Global data declarations   *********/
QString CommitTag("NotSet");
QVariantList DebugInfoTime;
QVariantList DebugInfoSeverity;
QVariantList DebugInfoGitTag;
QVariantList DebugInfoFile;
QVariantList DebugInfoFunction;
QVariantList DebugInfoLineNo;
QVariantList DebugInfoMessage;
QDateTime StartTime;
bool ShowDiagnostics=false, ImmediateDiagnostics=false, DontActuallyWriteDatabase=false;
QString ConnectionName;
QString DebugConnectionName;

void DumpDebugInfoToTerminal();
void DumpDebugInfoToDatabase(QSqlDatabase &dbConn);

/***********  Global function definitions   *************/
void saveMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    DebugInfoTime.append(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz t"));
    DebugInfoGitTag.append(CommitTag);
    DebugInfoFile.append(context.file);
    DebugInfoFunction.append(context.function);
    DebugInfoLineNo.append(context.line);
    DebugInfoMessage.append(msg);
    switch (type) {
    case QtInfoMsg:
        DebugInfoSeverity.append("Info");
        break;
    case QtDebugMsg:
        DebugInfoSeverity.append("Debug");
        break;
    case QtWarningMsg:
        DebugInfoSeverity.append("Warning");
        break;
    case QtCriticalMsg:
        DebugInfoSeverity.append("Critical");
        break;
    case QtFatalMsg:
        DebugInfoSeverity.append("Fatal");
        DumpDebugInfo();
        abort();
    }
    if (DebugInfoTime.size() >= 10000)     // Keep arrays from getting toooo large.
    {
        // Switch to terminal message handling to prevent getting here recursively.
        QtMessageHandler prevMsgHandler = qInstallMessageHandler(terminalMessageOutput);
        DumpDebugInfo();
        qInstallMessageHandler(prevMsgHandler);
    }
}

void terminalMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    static bool reentered = false;
    if (reentered)
        return;
    // Send any saved messages to terminal first.
    if (!DebugInfoTime.isEmpty())
    {  // This should only happen once.
        reentered = true;       // reentered prevents DumpDebugInfoToTerminal from causing infinite recursion
        DumpDebugInfoToTerminal();
        // and clear saved.
        DebugInfoTime.clear();
        DebugInfoGitTag.clear();
        DebugInfoSeverity.clear();
        DebugInfoFile.clear();
        DebugInfoFunction.clear();
        DebugInfoLineNo.clear();
        DebugInfoMessage.clear();
        reentered = false;
    }
    QFileInfo tempFileName(context.file);
    QString tempFuncName(context.function);
    int funcNameEnd = tempFuncName.indexOf("(");
    int funcNameBegin = tempFuncName.lastIndexOf(":", funcNameEnd);
    if (funcNameBegin < 0)
        funcNameBegin = tempFuncName.lastIndexOf(" ", funcNameEnd);
    funcNameBegin++;        // skip found char, or inc -1 to 0 if no char found.
    QString severity;
    switch (type) {
    case QtDebugMsg:
        severity = "Debug";
        break;
    case QtInfoMsg:
        severity = "Info";
        break;
    case QtWarningMsg:
        severity = "Warning";
        break;
    case QtCriticalMsg:
        severity = "Critical";
        break;
    case QtFatalMsg:
        severity = "Fatal";
        break;
    }

    fprintf(stderr, "%-8s\t%12s\t%30s\t%6d\t%s\n"
            , qPrintable(severity)
            , qPrintable(tempFileName.fileName())
            , qPrintable(tempFuncName.mid(funcNameBegin, funcNameEnd - funcNameBegin))
            , context.line      // context.line is an integer
            , qPrintable(msg)
            );
    fflush(stderr);
    if (type == QtFatalMsg)
        abort();
}

void DumpDebugInfoToTerminal()
{
    qDebug() << "Begin";
    for (int i = 0; i < DebugInfoTime.size(); ++i)
    {
        QFileInfo tempFileName(DebugInfoFile.at(i).toString());
        QString tempFuncName = DebugInfoFunction.at(i).toString();
        int funcNameEnd = tempFuncName.indexOf("(");
        int funcNameBegin = tempFuncName.lastIndexOf(":", funcNameEnd);
        if (funcNameBegin < 0)
            funcNameBegin = tempFuncName.lastIndexOf(" ", funcNameEnd);
        funcNameBegin++;        // skip found char, or inc -1 to 0 if no char found.

        fprintf(stderr, "%-8s\t%12s\t%30s\t%6d\t%s\n"
                , qPrintable(DebugInfoSeverity.at(i).toString())
                , qPrintable(tempFileName.fileName())
                , qPrintable(tempFuncName.mid(funcNameBegin, funcNameEnd - funcNameBegin))
                , DebugInfoLineNo.at(i).toInt()
                , qPrintable(DebugInfoMessage.at(i).toString())
                );
    }
    qDebug() << "Return";
    return;
}

void DumpDebugInfoToDatabase(QSqlDatabase &dbConn)
{
    qDebug() << "Begin";
    QSqlQuery query(dbConn);
    for (int i = 0; i < DebugInfoTime.size(); ++i)
    {
        if (!query.exec(QString("INSERT INTO DebugInfo "
                                "(Time, Severity, ArchiveTag, FilePath, FunctionName, SourceLineNo, Message) "
                                "VALUES ('%1', '%2', '%3', '%4', '%5', %6, '%7')")
                        .arg(DebugInfoTime.at(i).toString())
                        .arg(DebugInfoSeverity.at(i).toString())
                        .arg(DebugInfoGitTag.at(i).toString())
                        .arg(DebugInfoFile.at(i).toString())
                        .arg(DebugInfoFunction.at(i).toString())
                        .arg(DebugInfoLineNo.at(i).toInt(), 0, 10)
                        .arg(DebugInfoMessage.at(i).toString().replace("'", ""))
                        ))
            qCritical() << "Error inserting DebugInfo record in database: " << query.lastError() << "\nQuery: " << query.lastQuery();
    }
    /* This didn't work -- complains that the integer for SourceLineNo is bad.
    query.prepare("INSERT INTO DebugInfo "
                  "(Time, Severity, ArchiveTag, FilePath, FunctionName, SourceLineNo, Message) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?)");
    query.bindValue(0, DebugInfoTime);
    query.bindValue(1, DebugInfoSeverity);
    query.bindValue(2, DebugInfoGitTag);
    query.bindValue(3, DebugInfoFile);
    query.bindValue(4, DebugInfoFunction);
    query.bindValue(5, DebugInfoLineNo);
    query.bindValue(6, DebugInfoMessage);

    bool success = query.exec();
    */

    if (!query.exec(QString("DELETE FROM DebugInfo WHERE Time < '%1'")
                    .arg(QDate::currentDate().addDays(-2).toString("yyyy-MM-dd"))))
        qCritical() << "Error deleting old debug info from database: " << query.lastError() << "\nQuery: " << query.lastQuery();
    //        qDebug() << "Deleted old debug info from database: " <<  query.lastQuery();


    qDebug() << "Return";
    return;
}

void DumpDebugInfo()
{
    qDebug() << "Begin";
    if (DebugInfoTime.size() == 0)
    {
        qDebug() << "Return -- nothing to dump.";
        return;
    }
    qDebug() << "Using DebugConnectionName: " << DebugConnectionName;
    QSqlDatabase dbConn = QSqlDatabase::database(DebugConnectionName);
    if (!dbConn.isValid())
        qCritical("%s is NOT valid.", qUtf8Printable(DebugConnectionName));
    if (!dbConn.isOpen())
        qCritical("%s is NOT open.", qUtf8Printable(DebugConnectionName));

    if (!dbConn.isOpen())
        DumpDebugInfoToTerminal();
    else
        DumpDebugInfoToDatabase(dbConn);
    DebugInfoTime.clear();
    DebugInfoGitTag.clear();
    DebugInfoSeverity.clear();
    DebugInfoFile.clear();
    DebugInfoFunction.clear();
    DebugInfoLineNo.clear();
    DebugInfoMessage.clear();
    qDebug() << "Return";
}

void FlushDiagnostics()
{
    if (!ShowDiagnostics || ImmediateDiagnostics)
        return;
    StartTime = ShowDiagnosticsSince(StartTime);
}

QDateTime ShowDiagnosticsSince(const QDateTime startTime)
{
    qDebug() << "Begin";
    if (ImmediateDiagnostics)   // If nothing to show from arrays, just return.
    {
        qDebug() << "Return -- already sending to terminal.";
        return QDateTime::currentDateTime();;
    }
    DumpDebugInfo();        // dump arrays to database.
    QSqlDatabase dbConn = QSqlDatabase::database(DebugConnectionName);

    if (!dbConn.isOpen())
        return QDateTime::currentDateTime();         // No diagnostics stored in database to retrieve.
    QSqlQuery query(dbConn);

    QtMessageHandler prevMsgHandler = qInstallMessageHandler(0);

    if (query.exec(QString("SELECT CONCAT(Time, '   '"
                           ", RPAD(RIGHT(ArchiveTag, 8), 10, ' ')"
                           ", RPAD(Severity, 10, ' ')"
                           ", RPAD(LPAD(SourceLineNo, 4, ' '), 6, ' ')"
                           ", RPAD(substring_index(substring_index(substring_index(FunctionName, '::', -1), '(', 1), ' ', -1), 25, ' '), ' '"
                           ", LEFT(REPLACE(REPLACE(Message, '\r', '\\\\r'), '\n', '\\\\n'), 250))"
                           " FROM DebugInfo WHERE Time >= '%1';").arg(startTime.toString("yyyy-MM-dd hh:mm:ss.zzz"))))
        while (query.next())
            qInfo("%s", qUtf8Printable(query.value(0).toString()));
    else
        qDebug() << "Diag extraction error:" << query.lastQuery() << query.lastError();
    qInstallMessageHandler(prevMsgHandler);
    QDateTime timeNow = QDateTime::currentDateTime();
    qDebug() << "Return" << timeNow;
    return timeNow;
}

void DetermineCommitTag(const QString &pathToExecutable, const char *programName)
{
    qInfo() << "Begin";

    if ((!CommitTag.isEmpty()) && (CommitTag != "NotSet"))
    {
        qDebug() << "Return -- CommitTag already set:" << CommitTag;
        return;
    }

    QFileInfo fileInfo(pathToExecutable);
    QString fp = fileInfo.canonicalPath();
    QString sourcePath;
    qDebug() << "file path is " << fp;
    while (fp.size() > 2)
    {
        fileInfo.setFile(fp + "/" + programName);
        sourcePath = fileInfo.absolutePath();
        qDebug() << "Source Path becomes " << sourcePath;
        if (fileInfo.exists())
            break;
        //        fp = fp.left(fp.lastIndexOf('/'));      // This gets the "/" added above.
        fp = fp.left(fp.lastIndexOf('/'));      // This gets the one before that.
        qDebug() << "file path is " << fp;
    }
    if (sourcePath == "")
    {
        qWarning() << "Source path not found.";
        CommitTag = "Source path not found";
        qInfo() << "Return";
        return;
    }


    qDebug() << "Source path is" << sourcePath;
    if (ConnectionName.isEmpty())
    {
        fileInfo.setFile(sourcePath);
        ConnectionName = fileInfo.baseName();
        qDebug() << "ConnectionName set:" << ConnectionName;
    }
    if (DebugConnectionName.isEmpty())
    {
        DebugConnectionName = ConnectionName;
        qDebug() << "DebugConnectionName set:" << DebugConnectionName;
    }

    fileInfo.setFile(sourcePath + "/ArchiveTag.txt");
    QFile archiveTagFile(fileInfo.absoluteFilePath());
    if (fileInfo.exists())
    {
        if (archiveTagFile.open(QFile::ReadOnly)) {
            QTextStream f(&archiveTagFile);
            CommitTag = f.readLine();
            qInfo() << "Return with CommitTag from ArchiveTag.txt:" << CommitTag;
            return;
        }
    }
    fileInfo.setFile(sourcePath + "/.git");
    sourcePath = fileInfo.absoluteFilePath();
    qDebug() << "Path to .git directory " << sourcePath;
    if (fileInfo.isDir())
    {
        QProcess getGit;
        QString git_command = QString("bash -c \"a='%1'; "
                                      "if [ -d \"$a\" ]; "
                                      "then git --git-dir=$a log -1 --format='%H'; "
                                      "else echo Not in a git archive; "
                                      "fi\"").arg(sourcePath);
        qDebug() << "git command" << git_command;

        getGit.start(git_command);
        if (getGit.waitForFinished())
        {
            CommitTag = QString(getGit.readAllStandardOutput());
            QString errText(getGit.readAllStandardError());
            if (errText.size() > 0)
                qDebug() << "stderr output is: " << errText;
        }
        getGit.close();
        CommitTag.chop(1);
        if (archiveTagFile.open(QFile::WriteOnly)) {
            QTextStream f(&archiveTagFile);
            f << CommitTag;
            qDebug() << "Wrote CommitTag to ArchiveTag.txt";
        }
    }
    else
    {
        CommitTag = ".git not found";
        qWarning() << "Git archive not found; path is " << sourcePath;
    }
    qInfo() << "Return:" << CommitTag;
}

QSqlError addConnection(const QString &driver, const QString &dbName, const QString &host,
                        const QString &user, const QString &passwd, int port, QString connName)
{
    qInfo() << "Begin";
    qInfo() << driver << dbName << host << user << passwd << port;
    QSqlError err;
    if (!connName.isEmpty())
        ConnectionName = connName;
    QSqlDatabase db = QSqlDatabase::addDatabase(driver, ConnectionName);
    if (!db.isValid())
    {
        err = db.lastError();
        qWarning() << "Unable to addDatabase" << driver << err;
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(ConnectionName);
        qInfo() << "Return" << err;
        return err;
    }
    db.setDatabaseName(dbName);
    db.setHostName(host);
    db.setPort(port);
    if (!db.open(user, passwd))
    {
        qWarning() << "Unable to open database" << driver << dbName << host << port;
        err = db.lastError();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(ConnectionName);
    }
    else
        qInfo() << "The database connection " << ConnectionName << " is open.";
    qInfo() << "Return" << err;
    return err;
}

void addConnectionFromString(const QString &arg, bool DebugConnection)
{
    qInfo() << "Begin";
    QUrl url(arg, QUrl::TolerantMode);
    if (!url.isValid()) {
        qWarning("Invalid URL: %s", qPrintable(arg));
        qInfo() << "Return";
    }
    if (DebugConnection)
    {
        QSqlError err = addDebugConnection(url.scheme().toUpper(), url.path().mid(1), url.host(),
                                      url.userName(), url.password(), url.port(-1), QString("Debug%1").arg(ConnectionName));
        if (err.type() != QSqlError::NoError)
            qWarning() << "Unable to open debug database connection:" << err;
    }
    else
    {
        QSqlError err = addConnection(url.scheme().toUpper(), url.path().mid(1), url.host(),
                                      url.userName(), url.password(), url.port(-1));
        if (err.type() != QSqlError::NoError)
            qWarning() << "Unable to open connection:" << err;
    }
    qInfo() << "Return";
}

QSqlError addDebugConnection(const QString &driver, const QString &dbName, const QString &host,
                        const QString &user, const QString &passwd, int port, QString connName)
{
    qInfo() << "Begin";
    qInfo() << driver << dbName << host << user << passwd << port;
    QSqlError err;
    if (!connName.isEmpty())
        DebugConnectionName = connName;
    QSqlDatabase db = QSqlDatabase::addDatabase(driver, DebugConnectionName);
    if (!db.isValid())
    {
        err = db.lastError();
        qWarning() << "Unable to addDatabase" << driver << err;
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(DebugConnectionName);
        qInfo() << "Return" << err;
        return err;
    }
    db.setDatabaseName(dbName);
    db.setHostName(host);
    db.setPort(port);
    if (!db.open(user, passwd))
    {
        qWarning() << "Unable to open database" << driver << dbName << host << port;
        err = db.lastError();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(DebugConnectionName);
    }
    else
        qInfo() << "The database connection " << DebugConnectionName << " is open.";
    qInfo() << "Return" << err;
    return err;
}

QString setDbTimeZoneSQL(QTimeZone &theZone, QDateTime atTime)
{
    qDebug("Begin");
    int tzOffset = theZone.offsetFromUtc(atTime);
    int tzOffsetHours = abs(tzOffset) / 3600, tzOffsetMinutes = (abs(tzOffset) / 60) % 60;
    QChar plusMinus = tzOffset < 0 ? '-' : '+';
    QString retVal = QString("SET time_zone = '%1%2:%3'")
            .arg(plusMinus)
            .arg(tzOffsetHours, 2, 10, QChar('0'))
            .arg(tzOffsetMinutes, 2, 10, QChar('0'));
    qDebug() << "Return" << retVal;
    return retVal;
}

