/*!
\file supportfunctions.h
\brief Header file to define support globals and functions.
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
 */
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
void DetermineCommitTag();

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
