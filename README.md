# README.md
# QtSupportRoutines
\author Thomas A. DeMay
\date   2015

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


A copy of the license can be found in LICENSE.txt in this source directory.

This repository contains a .h and .cpp file with routines that are useful across
many Qt projects.  These functions are very Qt specific.
To use these routines, add both files to your Qt project.

There are three kinds of routines.

First, there are functions to interface with Qt's debugging facilities
to extract more information than is usually seen.  These functions can
optionally save a bunch of debug messages in internal arrays and then
store them in a database table or print them to \a stderr.  The other option
is to send diagnostics immediately to \a stderr.

The ability to store diagnostics in a database table means that the
program can run silently and if an anomoly is detected, the debug
info can be examined up to 2 days later.  An example SQL for retrieving
diagnostic information from the database is given below.

Second, there are functions for connecting to the database(s).

Third, there is a function to generate SQL to set the database
timezone to the timezone given.


/*********************************
SQL to extract records from the DebugInfo table starting from 2310 yesterday.
**********************************/
SELECT Time
, RIGHT(ArchiveTag,8) as TAG    /* Only show 8 characters of the ArchiveTag */
, Severity
  /* Extract and display just the function name; the field contains the whole function signature. */
, SUBSTRING_INDEX(SUBSTRING_INDEX(SUBSTRING_INDEX(FunctionName, '::', -1), '(', 1), ' ', -1) AS Function
, SourceLineNo AS Line
  /* Only show first 240 chars of message, 
     replacing \r with "\r" and \n with "\n" to keep message all on one line. */
, LEFT(REPLACE(REPLACE(Message,'\r','\\r'),'\n','\\n'), 240) AS Message /* ## */
 FROM DebugInfo  /* ## */
 WHERE Time > TIMESTAMPADD(MINUTE, -50, DATE(NOW()))      /* 50 minutes before midnight today. */
 /* AND ArchiveTag LIKE 'notset' */ /* To see only program starts. */
;
/* ## comment protects " " between words to avoid SQL errors. */
