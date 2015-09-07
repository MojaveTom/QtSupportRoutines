/*!
\file supportfunctions.cpp
\brief Definitions of support functions.
\author Thomas A. DeMay
\date 2015
\par    Copyright (C) 2015  Thomas A. DeMay

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

\details
There are three kinds of functions in this file.

First, there are functions to interface with Qt's debugging facilities
to extract more information than is usually seen.  These functions can
optionally save a bunch of debug messages in internal arrays and then
store them in a database table or print them to \a stderr.  The other option
is to send diagnostics immediately to \a stderr.

The ability to store diagnostics in a database table means that the
program can run silently and if an anomoly is detected, the debug
info can be examined up to 2 days later.  Notes.txt has example SQL
for retrieving diagnostic information from the database.

Second, there are functions for connecting to the database(s).

Third, there is a function to generate SQL to set the database
timezone to the timezone given.

 */
#include "supportfunctions.h"
#include <unistd.h>
#include <stdlib.h>


/******    Global data declarations   *********/
QString CommitTag = QString("NotSet"); //!< String containing the Git commit tag for this project.
QVariantList DebugInfoTime;     //!< Array of times of calls to debugging output routines.
QVariantList DebugInfoSeverity; //!< Array of severities of calls to debugging output routines.
QVariantList DebugInfoGitTag;   //!< Array of Git tags of calls to debugging output routines.
QVariantList DebugInfoFile;     //!< Array of file names of calls to debugging output routines.
QVariantList DebugInfoFunction; //!< Array of function signatures of calls to debugging output routines.
QVariantList DebugInfoLineNo;   //!< Array of line numbers of calls to debugging output routines.
QVariantList DebugInfoMessage;  //!< Array of diagnostic messages of calls to debugging output routines.
QDateTime StartTime;            //!< Time of last output for ShowDiagnosticsSince.

//!< Initially set to the start time of the program; updated each time ShowDiagnosticsSince is called.
bool ShowDiagnostics=false;             //!< Flag to print diagnostics to terminal.
bool ImmediateDiagnostics=false;        //!< Flag to ONLY print diagnostics to terminal.
bool DontActuallyWriteDatabase=false;   //!< Flag to not actually write any recors to database.

//!< This flag does not affect saving diagnostics to the database.
QString ConnectionName;         //!< Connection name for accessing the database.
QString DebugConnectionName;    //!< Connection name for accessing the DEBUG database.

/*! Local global function declarations. */
void DumpDebugInfoToTerminal();
void DumpDebugInfoToDatabase(QSqlDatabase &dbConn);

/***********  Global function definitions   *************/

/*!
 * \brief saveMessageOutput -- Save diagnostic information to the variant lists.
 *
 * Capture diagnostic information to global variant lists; to be written to the database
 * or terminal at a later time.
 * Fatal messages abort the program after dumping the diagnostics arrays.
 * \param type      The severity indicator.
 * \param context   Contains file, function, and line number.
 * \param msg       The user's diagnostic message.
 */
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
    /*! IF the arrays get big, dump the debug info to its destination and clear arrays. */
    if (DebugInfoTime.size() >= 10000)     // Keep arrays from getting toooo large.
    {
        // Switch to terminal message handling to prevent getting here recursively.
        QtMessageHandler prevMsgHandler = qInstallMessageHandler(terminalMessageOutput);
        DumpDebugInfo();
        qInstallMessageHandler(prevMsgHandler);
    }
}

/*!
 * \brief terminalMessageOutput -- Send debug info to \a stderr in some nice format.
 *
 * If there are any saved up diagnostics; they are printed first, and the arrays cleared.
 * The function is not re-entrant, and is protected from re-entry.
 * Fatal messages abort the program.
 * \param type      Message severity.
 * \param context   Context contains the file, function, and line number.
 * \param msg       The diagnostic message.
 */
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

/*!
 * \brief DumpDebugInfoToTerminal -- Send contents of debug info arrays to \a stderr.
 */
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

/*!
 * \brief DumpDebugInfoToDatabase -- Send contents of debug info arrays to database.
 *
 * Purges database entries older than 2 days.
 * \param dbConn    The database connection for debug info.
 */
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

    /* Purge old diagnostic data from database. */
    if (!query.exec(QString("DELETE FROM DebugInfo WHERE Time < '%1'")
                    .arg(QDate::currentDate().addDays(-2).toString("yyyy-MM-dd"))))
        qCritical() << "Error deleting old debug info from database: " << query.lastError() << "\nQuery: " << query.lastQuery();
    //        qDebug() << "Deleted old debug info from database: " <<  query.lastQuery();


    qDebug() << "Return";
    return;
}

/*!
 * \brief DumpDebugInfo -- Send saved diagnostics to database.
 *
 * If database is not available, send to terminal via \a stderr.
 */
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

/*!
 * \brief FlushDiagnostics -- Convenience function to show unseen diagnostics.
 *
 * Only does anything if we're supposed to show diagnostics, but not seeing
 * diagnostics immediately.
 */
void FlushDiagnostics()
{
    if (!ShowDiagnostics || ImmediateDiagnostics)
        return;
    StartTime = ShowDiagnosticsSince(StartTime);
}

/*!
 * \brief ShowDiagnosticsSince -- Retrieve diagnostics from database and print.
 *
 * Dump any saved diagnostics first, then query the database for diagnostics
 * that have been entered since \a startTime.  If the return value is used as
 * the startTime argument for the next call to this function, a view
 * of the diagnostics without time breaks will be presented at programmed intervals.
 * \param startTime     Beginning of time for which to show diagnostics.
 * \return              The current time.
 */
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

    /* Turn off saving of diagnostics while querying the database.
     * Diagnostics will be sent to the default Qt message handler.
     */
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

    /* Resotore previous message handler. */
    qInstallMessageHandler(prevMsgHandler);
    QDateTime timeNow = QDateTime::currentDateTime();
    qDebug() << "Return" << timeNow;
    return timeNow;
}

/*!
 * \brief DetermineCommitTag -- Get latest Git commit tag.
 *
 * This function should be called very near the beginning
 * of the main function so that diagnostics will have the
 * correct tag.  Until this function is called, all diagnostics
 * will have the tag: "NotSet".
 *
 * The function uses the value of the macro SOURCE_PATH to
 * find the ArchiveTag.txt file or the .git directory to
 * determine the commit tag per 3 or 4 below.  The SOURCE_PATH
 * macro is defined in the .pro file with the statement:
 * DEFINES += SOURCE_DIR=\'\"$$_PRO_FILE_PWD_\"\'
 *
 * The function uses \a qApp->applicationName() to determine the
 * program name.
 *
 * Four sources:
 *  1.  Default value "NotSet"
 *  2.  A pre-existing value in CommitTag (other than "NotSet")
 *  3.  File "ArchiveTag.txt" in the source directory.
 *  4.  Running a Git command in the source directory.
 *          Save tag to "ArchiveTag.txt".
 *
 * If the database connection name has not been set, this function sets it
 * to the program name.
 *
 */
#ifndef SOURCE_DIR
#error "You must define SOURCE_DIR macro in the .pro file.  Use the command:  DEFINES += SOURCE_DIR=\'\"$$_PRO_FILE_PWD_\"\'  "
#endif

void DetermineCommitTag()
{
    qInfo() << "Begin";
    QFileInfo fileInfo;
    fileInfo.setFile(".");
    QString sourcePath(SOURCE_DIR);
    qDebug("Path to sources is \"%s\"", qUtf8Printable(sourcePath));
    QString programName = qApp->applicationName();
    qDebug("The program name is \"%s\"", qUtf8Printable(programName));

    /*  Set connection names to program name if not already set. */
    if (ConnectionName.isEmpty())
    {
        ConnectionName = programName;
        qDebug() << "ConnectionName set:" << ConnectionName;
    }
    if (DebugConnectionName.isEmpty())
    {
        DebugConnectionName = ConnectionName;
        qDebug() << "DebugConnectionName set:" << DebugConnectionName;
    }

    if ((!CommitTag.isEmpty()) && (CommitTag != "NotSet"))
    {
        qDebug() << "Return -- CommitTag already set:" << CommitTag;
        return;
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

/*!
 * \brief addConnection -- Make a connection to the database.
 * \param driver    Database server identifier, "QMYSQL" for this program.
 * \param dbName    Name of the database schema to which to connect.
 * \param host      Host of the database server.
 * \param user      Username for database access.
 * \param passwd    Password for database access.
 * \param port      Tcp/Ip port to use for the connection.
 * \param connName  Name to apply to the connection.  Saved in global ConnectionName.
 * \return Error indication.
 */
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

/*!
 * \brief addConnectionFromString -- Parse string to make connection.
 * \param arg               String like "QMYSQL://user:password@host:port/schema"
 * \param DebugConnection   True if we're making a connection to the debug database.
 */
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

/*!
 * \brief addDebugConnection -- Make a connection to the database for debug info.
 * \param driver    Database server identifier, "QMYSQL" for this program.
 * \param dbName    Name of the database schema to which to connect.
 * \param host      Host of the database server.
 * \param user      Username for database access.
 * \param passwd    Password for database access.
 * \param port      Tcp/Ip port to use for the connection.
 * \param connName  Name to apply to the connection.  Saved in global DebugConnectionName.
 * \return Error indication.
 */
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
    {
        qInfo() << "The debug database connection " << DebugConnectionName << " is open.  Test for DebugInfo table.";
        QSqlQuery query(db);
        if (!query.exec("SELECT COUNT(*) FROM DebugInfo"))
        {
            qDebug("Assume an error occurred because the DebugInfo table does not exist.");
            qDebug("Last query was \"%s\"; the error description is \"%s\"."
                   , qUtf8Printable(query.lastQuery())
                   , qUtf8Printable(query.lastError().text())
                   );
            if (!query.exec(
                        "CREATE TABLE `DebugInfo` ("
                          "`idDebugInfo` int(11) NOT NULL AUTO_INCREMENT,"
                          "`Time` varchar(30) DEFAULT NULL COMMENT 'Time when debug info was generated.',"
                          "`Severity` varchar(8) DEFAULT NULL,"
                          "`ArchiveTag` varchar(40) DEFAULT NULL COMMENT 'Id of this source code in the source control archive.',"
                          "`FilePath` text COMMENT 'Path to source file where info was logged.',"
                          "`FunctionName` text COMMENT 'Name of function in which info was logged.',"
                          "`SourceLineNo` int(11) DEFAULT NULL COMMENT 'Line number in source file.',"
                          "`Message` text COMMENT 'Body of info message.',"
                          "PRIMARY KEY (`idDebugInfo`)"
                        ") ENGINE=InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET=utf8"))
            {
                qDebug("Creating DebugInfo table failed.  Assume table already exists.");
                qDebug("Error was \"%s\"", qUtf8Printable(query.lastError().text()));
            }
            else
            {
                qInfo("Successfully created DebugInfo table in database %s.", qUtf8Printable(db.databaseName()));
            }
        }
        else
        {
            qInfo("The database has a DebugInfo table.");
        }
    }
    qInfo() << "Return" << err;
    return err;
}

/*!
 * \brief setDbTimeZoneSQL -- Create SQL string to set timezone.
 *
 * If the \a atTime argument is during daylight savings time, you may
 * get a different result than if it is during standard time.
 *
 * \param theZone   The timezone.
 * \param atTime    DateTime to use.
 * \return
 */
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

