#ifndef SUPPORTFUNCTIONS_H
#define SUPPORTFUNCTIONS_H

#include <QCoreApplication>
#include <QtCore/QCoreApplication>
#include <QtSql>

/******    Global data declarations   *********/
extern QDateTime StartTime;
extern bool ShowDiagnostics, ImmediateDiagnostics, DontActuallyWriteDatabase;
extern QString ConnectionName, CommitTag, DebugConnectionName;

/*********  Global function declarations  ***************/
void DetermineCommitTag(const QString &pathToExecutable, const char *programName);

void saveMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg);
void terminalMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg);
void bothMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg);
QDateTime ShowDiagnosticsSince(const QDateTime startTime);
void FlushDiagnostics();
void DumpDebugInfo();

void addConnectionFromString(const QString &arg, bool DebugConnection = false);
QSqlError addConnection(const QString &driver, const QString &dbName, const QString &host,
                        const QString &user, const QString &passwd, int port, QString connName = "");
QSqlError addDebugConnection(const QString &driver, const QString &dbName, const QString &host,
                        const QString &user, const QString &passwd, int port, QString connName = "");
QString setDbTimeZoneSQL(QTimeZone &theZone, QDateTime atTime);

#endif // SUPPORTFUNCTIONS_H
