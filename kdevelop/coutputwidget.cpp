/***************************************************************************
                      coutputwidget.cpp - the output window in KDevelop
                             -------------------                                         

    begin                : 5 Aug 1998                                        
    copyright            : (C) 1998 by Sandy Meier
    email                : smeier@rz.uni-potsdam.de                                     
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   * 
 *                                                                         *
 ***************************************************************************/
#include "coutputwidget.h"

#include <kapp.h>
#include <kdebug.h>

#include <qpainter.h>
#include <qpixmap.h>
#include <qfont.h>

COutputWidget::COutputWidget(QWidget* parent, const char* name) :
//  KEdit(parent,name)
  QMultiLineEdit(parent, name)
{
}

void COutputWidget::insertAtEnd(const QString& s)
{
  int row = (numLines() < 1)? 0 : numLines()-1;
  // correct workaround for QMultilineEdit
  //  the string inside could be NULL, and so QMultilineEdit fails
#if (QT_VERSION < 300)
  int col = qstrlen(textLine(row));
#else
  int col = 0;
#endif
  if (s.left(1) == "\n" && row == 0 && col == 0)
    insertAt(" "+s, row, col);
  else
    insertAt(s, row, col);
}

// ---------------------------------------------------------------------------
// This widget processes the makefile output and displays the error lines in
// garish colours.
//
// All lines output from make to strerr are blue. If _any_ line contains a
// file:linenumber then they are red.
//
// Keys F4 and SHIFT-F4 go  to next/previous error lines.
// Pressing ENTER or RETURN or a left mouse click on an error line will display
// the file/linenumber in the editor.
//
// By default we display the last output in the widget. When the user moves off the
// last character then the display is "held" at that point. Moving the cursor to the
// last character will start the widget following the last text again.
//
// It was useful to use the QMultiLineEdit code as it supports text selection within
// the widget, especially multiline text selection. Using a QListView as we were
// previously, was also a problem as it slowed down significantly when the list
// became large and we were displaying the newly appended text.
//
// Not the most wonderful code in the world :((
// A big chunk of QMultiLineEdit was copied here and modified so that we can
// have coloured output lines. Unfortunately, I couldn't find any other way to
// achieve this as QMultiLineEdit changes pens internally and when trying to modify
// the colorgroup externally it just seemed to change all text rather than the line :((
// I'm sure it is possible but I gave in...
// ---------------------------------------------------------------------------

CMakeOutputWidget::CMakeOutputWidget(QWidget* parent, const char* name) :
  QMultiLineEdit(parent, name),
  m_buf(),
  m_enterDir("[^\n]*: Entering directory `(.*)'\n"),
  m_leaveDir("[^\n]*: Leaving directory `([^\n]*)'\n"),
  m_errorGcc("([^: \t]+):([0-9]+)[:,].*")
{
  it = m_errorMap.begin();
  setReadOnly(true);        // -JROC uncommented again
  setWordWrap(WidgetWidth); // -JROC
  setWrapPolicy(Anywhere); // -JROC
}

// ---------------------------------------------------------------------------

void CMakeOutputWidget::insertAtEnd(const QString& text, MakeOutputErrorType defaultType)
{
  m_buf += text;
  int pos;

  while ( (pos = m_buf.find('\n')) != -1)
  {
    QString line = m_buf.left(pos+1);
    m_buf.remove(0, pos+1);

    // extract file info from this line - if any
    processLine(line, defaultType);

    // add to the end of the text - highlighting is done in the
    // paint routine.
    int row = (numLines() < 1)? 0 : numLines()-1;
#if (QT_VERSION < 300)
    int col = qstrlen(textLine(row));
#else
    int col = 0;
#endif

    bool displayAdditions=atEnd();
    insertAt(line, row, col);
    if (displayAdditions)
      setCursorPosition(numLines()+1,0);
  }
}

// ---------------------------------------------------------------------------

void CMakeOutputWidget::start()
{
  clear();
  m_dirStack.clear();
  m_errorMap.clear();

  // dummy needed ?
  ErrorDetails errorDetails("", -1, Normal);
  m_errorMap.insert(-1, errorDetails);
  it = m_errorMap.begin();
}

// ---------------------------------------------------------------------------

void CMakeOutputWidget::processLine(const QString& line, MakeOutputErrorType type)
{
  QString fileInErr = QString::null;
  int lineInErr = -1;

  const int errorGccFileGroup = 1;
  const int errorGccRowGroup = 2;

  if (m_enterDir.match(line))
  {
    QString *dir = new QString(m_enterDir.group(1));
    m_dirStack.push(dir);
    return;
  }

  if (m_leaveDir.match(line))
  {
    QString *dir = m_dirStack.pop();
    delete dir;
    return;
  }

  if (m_errorGcc.match(line))
  {
    type = Error;
    fileInErr = m_errorGcc.group(errorGccFileGroup);
    lineInErr = QString(m_errorGcc.group(errorGccRowGroup)).toInt()-1;
    if (fileInErr.left(1) != "/")
      if (m_dirStack.top())
        fileInErr.prepend("/").prepend(*m_dirStack.top());
  }

  if (type != Normal)
  {
    // add the error keyed on the line number in the make output widget
    ErrorDetails errorDetails(fileInErr, lineInErr, type);
    m_errorMap.insert(numLines()-1, errorDetails);
  }
}

// ---------------------------------------------------------------------------

void CMakeOutputWidget::viewNextError()
{
  if (!m_errorMap.isEmpty())
  {
    while (++it != m_errorMap.end())
    {
      ErrorDetails errorDetails = it.data();
      if (!errorDetails.m_fileName.isEmpty() && errorDetails.m_lineNumber >= 0)
      {
        selectLine(it.key());
        emit switchToFile(errorDetails.m_fileName, errorDetails.m_lineNumber);
        return;
      }
    }
  }
  kapp->beep();
}

// ---------------------------------------------------------------------------

void CMakeOutputWidget::viewPreviousError()
{
  if (!m_errorMap.isEmpty())
  {
    while (--it != m_errorMap.begin())
    {
      ErrorDetails errorDetails = it.data();
      if (!errorDetails.m_fileName.isEmpty() && errorDetails.m_lineNumber >= 0)
      {
        selectLine(it.key());
        emit switchToFile(errorDetails.m_fileName, errorDetails.m_lineNumber);
        return;
      }
    }
  }
  kapp->beep();
}

// --------------------------------------------------------------------------------

void CMakeOutputWidget::selectLine(int line)
{
  setCursorPosition( line, 255, false );
  setCursorPosition( line, 0, true );
}

// --------------------------------------------------------------------------------

// override from QMultiLineEdit
void CMakeOutputWidget::keyPressEvent( QKeyEvent* event )
{
  switch (event->key())
  {
    case Key_Enter:
    case Key_Return:
      checkForError();
      break;

    default:
      QMultiLineEdit::keyPressEvent(event);
      break;
  }
}

// --------------------------------------------------------------------------------

void CMakeOutputWidget::mouseReleaseEvent(QMouseEvent* event)
{
  QMultiLineEdit::mouseReleaseEvent(event);
  checkForError();
}

// --------------------------------------------------------------------------------

// Check the current line for errors _and_ do the switch to the file:linenumber.
void CMakeOutputWidget::checkForError()
{
  int line,col;
  getCursorPosition(&line,&col);

  ErrorMap::Iterator tmp_it;
  if ((tmp_it = m_errorMap.find(line)) != m_errorMap.end())
  {
    ErrorDetails errorDetails = tmp_it.data();
    if (!errorDetails.m_fileName.isEmpty() && errorDetails.m_lineNumber >= 0)
    {
      selectLine(line);
      emit switchToFile(errorDetails.m_fileName, errorDetails.m_lineNumber);
      it = tmp_it;
      return;
    }
  }

  tmp_it = m_errorMap.begin();
  // if unchecked this can cause a nasty endless loop
  if (tmp_it != m_errorMap.end()) tmp_it++;

  if (line > tmp_it.key())
    it = m_errorMap.end();
  else
    it = m_errorMap.begin();
}

// --------------------------------------------------------------------------------

CMakeOutputWidget::MakeOutputErrorType CMakeOutputWidget::lineType(int line)
{
  ErrorMap::Iterator tmp_it;
  if ((tmp_it = m_errorMap.find(line)) != m_errorMap.end())
  {
    ErrorDetails errorDetails = tmp_it.data();
    return errorDetails.m_type;
  }
  return Normal;
}

// --------------------------------------------------------------------------------
// Below is modified code from QMultiLineEdit that will set the appropriate colours
// for the text lines.
//
// My apoligies for this extremly ugly hacked code :(
// --------------------------------------------------------------------------------

static int defTabStop = 8;

static int tabStopDist( const QFontMetrics &fm )
{
    return defTabStop*fm.width( QChar('x') );
}

static int textWidthWithTabs( const QFontMetrics &fm, const QString &s, uint start, uint nChars, int align )
{
  if ( s.isEmpty() )
    return 0;

  int dist = -fm.leftBearing( s[(int)start] );
  int i = start;
  int tabDist = -1; // lazy eval
  while ( (uint)i < s.length() && (uint)i < start+nChars )
  {
    if ( s[i] == '\t' && align == Qt::AlignLeft )
    {
      if ( tabDist<0 )
        tabDist = tabStopDist(fm);
    dist = ( (dist+tabDist+1)/tabDist ) * tabDist;
    i++;
    }
    else
    {
      int ii = i;
      while ( (uint)i < s.length() && (uint)i < start + nChars && ( align != Qt::AlignLeft || s[i] != '\t' ) )
        i++;
      dist += fm.width( s.mid(ii,i-ii) );
    }
  }
  return dist;
}

static QPixmap *buffer = 0;

static void cleanupMLBuffer()
{
    delete buffer;
    buffer = 0;
}

static QPixmap *getCacheBuffer( QSize sz )
{
    if ( !buffer ) {
        qAddPostRoutine( cleanupMLBuffer );
        buffer = new QPixmap;
    }

    if ( buffer->width() < sz.width() || buffer->height() < sz.height() )
        buffer->resize( sz );
    return buffer;
}

/*!
  Computes the pixel position in line \a line which corresponds to
  character position \a xIndex
*/

int CMakeOutputWidget::mapToView( int xIndex, int line )
{
#if (QT_VERSION < 300)
    int lr_marg = hMargin();
    int align = alignment();

    QString s = stringShown( line );
    xIndex = QMIN( (int)s.length(), xIndex );
    QFontMetrics fm( font() );
    int wcell = cellWidth() - 2 * lr_marg;// - d->marg_extra;
    int wrow = textWidth(s);
    int w = textWidthWithTabs( fm, s, 0, xIndex, align ) - 1;
    if ( align == Qt::AlignCenter || align == Qt::AlignHCenter )
      w += (wcell - wrow) / 2;
    else
      if ( align == Qt::AlignRight )
        w += wcell - wrow;
    return lr_marg + w;
#else
    return 0; // dummy, 'cause never used
#endif
}

// --------------------------------------------------------------------------------
// This is where we paint the lines different colours depending on the type.
// --------------------------------------------------------------------------------

void CMakeOutputWidget::paintCell(QPainter* painter, int row, int /*col*/)
{
#if (QT_VERSION < 300)
  int lr_marg = hMargin();
  bool markIsOn = hasMarkedText();
  int align = alignment();
  int cursorX = 0;
  int cursorY = 0;
  cursorPosition(&cursorY,&cursorX);

  const QColorGroup & g = colorGroup();
  QFontMetrics fm( painter->font() );
  QString s = stringShown(row);
  if ( s.isNull() )
  {
    qWarning( "QMultiLineEdit::paintCell: (%s) no text at line %d",
    name( "unnamed" ), row );
    return;
  }

  QRect updateR = cellUpdateRect();
  QPixmap *buffer = getCacheBuffer( updateR.size() );
  ASSERT(buffer);
  buffer->fill ( g.base() );

  QPainter p( buffer );
  p.setFont( painter->font() );
  p.translate( -updateR.left(), -updateR.top() );

  p.setTabStops( tabStopDist(fm) );

  int yPos = 0;
  int markX1, markX2;        // in x-coordinate pixels
  markX1 = markX2 = 0;      // avoid gcc warning
  if ( markIsOn )
  {
    int markBeginX, markBeginY;
    int markEndX, markEndY;
    getMarkedRegion( &markBeginY, &markBeginX, &markEndY, &markEndX );
    if ( row >= markBeginY && row <= markEndY )
    {
      if ( row == markBeginY )
      {
        markX1 = markBeginX;
        if ( row == markEndY )     // both marks on same row
          markX2 = markEndX;
        else
          markX2 = s.length();  // mark till end of line
      }
      else
      {
        if ( row == markEndY )
        {
          markX1 = 0;
          markX2 = markEndX;
        }
        else
        {
          markX1 = 0;      // whole line is marked
          markX2 = s.length();  // whole line is marked
        }
      }
    }
  }

  switch (lineType(row))
  {
    case Error:       p.setPen( Qt::darkRed );    break;
    case Diagnostic:  p.setPen( Qt::darkBlue );   break;
    default:          p.setPen( g.text());        break;
  }

  int wcell = cellWidth() - 2*lr_marg;// - marg_extra;
  int wrow = textWidth( s );
  int x = lr_marg - p.fontMetrics().leftBearing(s[0]);
  if ( align == Qt::AlignCenter || align == Qt::AlignHCenter )
    x += (wcell - wrow) / 2;
  else
    if ( align == Qt::AlignRight )
      x += wcell - wrow;
  p.drawText( x,  yPos, cellWidth()-lr_marg-x, cellHeight(),
              align == AlignLeft?ExpandTabs:0, s );
//  if ( !r->newline && BREAK_WITHIN_WORDS )
//    p.drawPixmap( x + wrow - lr_marg - marg_extra, yPos, d->arrow );
#if 0
  if ( r->newline )
    p.drawLine( lr_marg,  yPos+cellHeight()-2, cellWidth() - lr_marg, yPos+cellHeight()-2);
#endif
  if ( markX1 != markX2 )
  {
    int sLength = s.length();
    int xpos1   =  mapToView( markX1, row );
    int xpos2   =  mapToView( markX2, row );
    int fillxpos1 = xpos1;
    int fillxpos2 = xpos2;
    if ( markX1 == 0 )
      fillxpos1 -= 2;
    if ( markX2 == sLength )
      fillxpos2 += 3;
    p.setClipping( TRUE );
    p.setClipRect( fillxpos1 - updateR.left(), 0,
         fillxpos2 - fillxpos1, cellHeight(row) );
    p.fillRect( fillxpos1, 0, fillxpos2 - fillxpos1, cellHeight(row),
      g.brush( QColorGroup::Highlight ) );
    p.setPen( g.highlightedText() );
    p.drawText( x,  yPos, cellWidth()-lr_marg-x, cellHeight(),
      align == AlignLeft?ExpandTabs:0, s );
    p.setClipping( FALSE );
  }

  if ( row == cursorY && cursorOn && !isReadOnly() )
  {
    int cursorPos = QMIN( (int)s.length(), cursorX );
    int cXPos   = mapToView( cursorPos, row );
    int cYPos   = 0;
    if ( hasFocus() ) // || d->dnd_forcecursor )
    {
      p.setPen( g.text() );
      /* styled?
       p.drawLine( cXPos - 2, cYPos,
       cXPos + 2, cYPos );
      */
      p.drawLine( cXPos, cYPos,
      cXPos, cYPos + fm.height() - 2);
      /* styled?
       p.drawLine( cXPos - 2, cYPos + fm.height() - 2,
       cXPos + 2, cYPos + fm.height() - 2);
      */

#ifndef QT_NO_TRANSFORMATIONS
      // TODO: set it other times, eg. when scrollbar moves view
      QWMatrix wm = painter->worldMatrix();
      setMicroFocusHint( int(wm.dx()+cXPos),
           int (wm.dy()+cYPos),
           1, fm.ascent() );
#else
      setMicroFocusHint( cXPos,
           cYPos,
           1, fm.ascent() );
#endif
    }
  }
  p.end();
  painter->drawPixmap( updateR.left(), updateR.top(), *buffer,
     0, 0, updateR.width(), updateR.height() );
#endif // QT_VERSION < 300
}

#include "coutputwidget.moc"
