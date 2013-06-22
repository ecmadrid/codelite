#include "cc_box_tip_window.h"
#include <wx/bitmap.h>
#include <wx/dcmemory.h>
#include "ieditor.h"
#include <wx/settings.h>
#include <wx/dcbuffer.h>
#include "bitmap_loader.h"
#include <wx/tokenzr.h>
#include <wx/spinctrl.h>
#include "Markup.h"
#include "event_notifier.h"
#include "editor_config.h"
#include <wx/stc/stc.h>

const wxEventType wxEVT_TIP_BTN_CLICKED_UP   = wxNewEventType();
const wxEventType wxEVT_TIP_BTN_CLICKED_DOWN = wxNewEventType();

CCBoxTipWindow::CCBoxTipWindow(wxWindow* parent, const wxString& tip)
    : wxPopupWindow(parent)
    , m_tip(tip)
    , m_positionedToRight(false)
{
    DoInitialize(tip, 1, true);
}

CCBoxTipWindow::CCBoxTipWindow(wxWindow* parent, const wxString &tip, size_t numOfTips, bool simpleTip)
    : wxPopupWindow(parent)
    , m_positionedToRight(true)
{
    DoInitialize(tip, numOfTips, simpleTip);
}

CCBoxTipWindow::~CCBoxTipWindow()
{
}

void CCBoxTipWindow::DoInitialize(const wxString& tip, size_t numOfTips, bool simpleTip)
{
    m_tip = tip;
    m_numOfTips = numOfTips;
    
    //Invalidate the rectangles
    m_leftTipRect = wxRect();
    m_rightTipRect = wxRect();

    if ( !simpleTip && m_numOfTips > 1 )
        m_tip.Prepend(wxT("\n\n")); // Make room for the arrows

    Hide();

    wxBitmap bmp(1, 1);
    wxMemoryDC dc(bmp);

    wxSize size;
    
    LexerConfPtr cppLex = EditorConfigST::Get()->GetLexer("C++");
    if ( cppLex ) {
        // use the lexer default font
        m_codeFont = cppLex->GetFontForSyle(0);
        
    } else {
        m_codeFont    = wxFont(10, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
        
    }
    
    m_commentFont = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);

    wxString codePart, commentPart;
    wxString strippedTip = DoStripMarkups();
    
    size_t from = 0;
    int hr_count = 0;
    size_t hrPos = strippedTip.find("<hr>");
    while ( hrPos != wxString::npos ) {
        ++hr_count;
        from = hrPos + 4;
        hrPos = strippedTip.find("<hr>", from);
    }
    
    int where= strippedTip.Find("<hr>");
    if ( where != wxNOT_FOUND ) {
        codePart    = strippedTip.Mid(0, where);
        commentPart = strippedTip.Mid(where + 5);

    } else {
        codePart = strippedTip;
    }

    int commentWidth  = 0;
    int codeWidth     = 0;

    // Use bold font for measurements
    m_codeFont.SetWeight(wxFONTWEIGHT_BOLD);
    m_commentFont.SetWeight(wxFONTWEIGHT_BOLD);
    
    if ( !simpleTip ) {
        dc.GetMultiLineTextExtent(codePart,    &codeWidth,    NULL, NULL, &m_codeFont);
        dc.GetMultiLineTextExtent(commentPart, &commentWidth, NULL, NULL, &m_commentFont);
        
    } else {
        dc.GetMultiLineTextExtent(strippedTip, &codeWidth,    NULL, NULL, &m_commentFont);

    }
    
    m_codeFont.SetWeight(wxFONTWEIGHT_NORMAL);
    m_commentFont.SetWeight(wxFONTWEIGHT_NORMAL);

    // Set the width
    commentWidth > codeWidth ? size.x = commentWidth : size.x = codeWidth;

    dc.GetTextExtent(wxT("Tp"), NULL, &m_lineHeight, NULL, NULL, &m_codeFont);
    int nLineCount = ::wxStringTokenize(m_tip, wxT("\r\n"), wxTOKEN_RET_EMPTY_ALL).GetCount();
    
    size.y = nLineCount * m_lineHeight;
    size.y += (hr_count * 10) + 10; // each <hr> uses 10 pixels height
    size.x += 40;
    SetSize(size);

    Connect(wxEVT_PAINT, wxPaintEventHandler(CCBoxTipWindow::OnPaint), NULL, this);
    Connect(wxEVT_ERASE_BACKGROUND, wxEraseEventHandler(CCBoxTipWindow::OnEraseBG), NULL, this);
    Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(CCBoxTipWindow::OnMouseLeft), NULL, this);
}

void CCBoxTipWindow::PositionRelativeTo(wxWindow* win, IEditor* focusEdior)
{
    m_positionedToRight = true;
    // When shown, set the focus back to the editor
    wxPoint pt = win->GetScreenPosition();
    pt.x += win->GetSize().x;

    // Check for overflow
    wxSize  size = ::wxGetDisplaySize();
    if ( pt.x + GetSize().x > size.x ) {
        // Move the tip to the left
        pt = win->GetScreenPosition();
        pt.x -= GetSize().x;
        m_positionedToRight = false;

        if ( pt.x < 0 ) {
            // the window can not fit to the screen, restore its position
            // to the right side
            pt = win->GetScreenPosition();
            pt.x += win->GetSize().x;
            m_positionedToRight = true;
        }
    }

    if ( focusEdior ) {
        // Check that the tip Y coord is inside the editor
        // this is to prevent some zombie tips appearing floating in no-man-land
        wxRect editorRect = focusEdior->GetSTC()->GetScreenRect();
        if ( editorRect.GetTopLeft().y > pt.y ) {
            m_positionedToRight = true;
            return;
        }
    }
    
    SetSize(wxRect(pt, GetSize()));
    Show();

    if( focusEdior ) {
        focusEdior->SetActive();
    }
}

void CCBoxTipWindow::OnEraseBG(wxEraseEvent& e)
{
    wxUnusedVar(e);
}

void CCBoxTipWindow::OnPaint(wxPaintEvent& e)
{
    wxBufferedPaintDC dc(this);

    wxColour penColour   = DrawingUtils::GetThemeBorderColour();
    wxColour brushColour = DrawingUtils::GetThemeTipBgColour();

    dc.SetBrush( brushColour   );
    dc.SetPen  ( penColour );

    wxRect rr = GetClientRect();
    dc.DrawRectangle(rr);
    
    // Draw left-right arrows
    m_leftTipRect = wxRect();
    m_rightTipRect = wxRect();
    //if ( m_numOfTips > 1 && m_leftbmp.IsOk() && m_rightbmp.IsOk() ) {
    //    
    //    // If the tip is positioned on the right side of the completion box
    //    // place the <- -> buttons on the LEFT side
    //    int leftButtonOffset = 5;
    //    if ( !m_positionedToRight ) {
    //        // Place the buttons on the RIGHT side
    //        leftButtonOffset = rr.GetWidth() - m_leftbmp.GetWidth() - m_rightbmp.GetWidth() - 10; // 10 pixels padding
    //    }
    //    
    //    m_leftTipRect  = wxRect(leftButtonOffset, 5, m_leftbmp.GetWidth(), m_leftbmp.GetHeight());
    //    wxPoint rightBmpPt = m_leftTipRect.GetTopRight();
    //    rightBmpPt.x += 5;
    //    m_rightTipRect = wxRect(rightBmpPt, wxSize(m_rightbmp.GetWidth(), m_rightbmp.GetHeight()));
    //
    //    dc.DrawBitmap(m_leftbmp,  m_leftTipRect.GetTopLeft());
    //    dc.DrawBitmap(m_rightbmp, m_rightTipRect.GetTopLeft());
    //    
    //}

    dc.SetFont(m_commentFont);
    dc.SetTextForeground( DrawingUtils::GetThemeTextColour() );

    ::setMarkupLexerInput(m_tip);
    wxString curtext;

    wxPoint pt(5, 5);
    while ( true ) {
        int type = ::MarkupLex();

        if ( type == 0 )  // EOF
            break;

        switch (type) {
        case BOLD_START: {
            // Before we change the font, draw the buffer
            DoPrintText(dc, curtext, pt);
            wxFont f = dc.GetFont();
            f.SetWeight(wxFONTWEIGHT_BOLD);
            dc.SetFont(f);
            break;
        }
        case BOLD_END: {
            // Before we change the font, draw the buffer
            DoPrintText(dc, curtext, pt);

            wxFont f = dc.GetFont();
            f.SetWeight(wxFONTWEIGHT_NORMAL);
            dc.SetFont(f);
            break;
        }
        case ITALIC_START: {
            // Before we change the font, draw the buffer
            DoPrintText(dc, curtext, pt);
            wxFont f = dc.GetFont();
            f.SetStyle(wxFONTSTYLE_ITALIC);
            dc.SetFont(f);
            break;
        }
        case ITALIC_END: {
            // Before we change the font, draw the buffer
            DoPrintText(dc, curtext, pt);

            wxFont f = dc.GetFont();
            f.SetStyle(wxFONTSTYLE_NORMAL);
            dc.SetFont(f);
            break;
        }
        case CODE_START: {
            // Before we change the font, draw the buffer
            DoPrintText(dc, curtext, pt);
            dc.SetFont(m_codeFont);
            break;
        }
        case CODE_END: {
            // Before we change the font, draw the buffer
            DoPrintText(dc, curtext, pt);
            dc.SetFont(m_commentFont);
            break;
        }

        case NEW_LINE: {
            // New line, print the content
            DoPrintText(dc, curtext, pt);
            pt.y += m_lineHeight;

            // reset the drawing point to the start of the next line
            pt.x = 5;
            break;
        }
        case HORIZONTAL_LINE: {
            // Draw the buffer
            curtext.Clear();
            pt.y += 5;
            dc.DrawLine(wxPoint(0, pt.y), wxPoint(rr.GetWidth(), pt.y));
            pt.y += 5;
            pt.x = 5;
            break;
        }
        case COLOR_START: {
            DoPrintText(dc, curtext, pt);
            wxString colorname = ::MarkupText();
            colorname = colorname.AfterFirst(wxT('"'));
            colorname = colorname.BeforeLast(wxT('"'));
            dc.SetTextForeground(wxColour(colorname));
            break;
        }
        case COLOR_END: {
            DoPrintText(dc, curtext, pt);
            // restore default colour
            dc.SetTextForeground( DrawingUtils::GetThemeTextColour() );
            break;
        }
        //case PARAM_ARG: {
        //    // print what we got so far and move on to the next line
        //    DoPrintText(dc, curtext, pt);
        //    pt.y += m_lineHeight;
        //    pt.x = 5;
        //    
        //    // Now print: Parameter
        //    wxFont f = dc.GetFont();
        //    wxFontWeight curweight = f.GetWeight();
        //    f.SetWeight(wxFONTWEIGHT_BOLD);
        //    dc.SetFont(f);
        //    wxString tmp = "Parameter";
        //    DoPrintText(dc, tmp, pt);
        //    // restore the font's weight
        //    f.SetWeight(curweight);
        //    dc.SetFont(f);
        //    pt.y += m_lineHeight;
        //    pt.x = 5;
        //    break;
        //}
        default:
            curtext << ::MarkupText();
            break;
        }
    }

    if ( curtext.IsEmpty() == false ) {
        DoPrintText(dc, curtext, pt);
    }

    ::MarkupClean();
}

void CCBoxTipWindow::DoPrintText(wxDC& dc, wxString& text, wxPoint& pt)
{
    if ( text.IsEmpty() == false ) {
        wxSize sz = dc.GetTextExtent(text);
        dc.DrawText(text, pt);
        pt.x += sz.x;
        text.Clear();
    }
}

void CCBoxTipWindow::OnMouseLeft(wxMouseEvent& e)
{
    if ( m_leftTipRect.Contains( e.GetPosition() ) )  {
        wxCommandEvent evt(wxEVT_TIP_BTN_CLICKED_UP);
        EventNotifier::Get()->AddPendingEvent(evt);

    } else if ( m_rightTipRect.Contains( e.GetPosition() ) ) {
        wxCommandEvent evt(wxEVT_TIP_BTN_CLICKED_DOWN);
        EventNotifier::Get()->AddPendingEvent(evt);

    } else {
        e.Skip();
    }
}

wxString CCBoxTipWindow::DoStripMarkups()
{
    ::setMarkupLexerInput(m_tip);
    wxString text;
    while ( true ) {
        int type = ::MarkupLex();

        if ( type == 0 )  // EOF
            break;
        switch (type) {
        case BOLD_START:
        case BOLD_END:
        case CODE_START:
        case CODE_END:
        case COLOR_START:
        case COLOR_END:
            break;
        case HORIZONTAL_LINE:
            text << "<hr>";
        case NEW_LINE:
            text << "\n";
            break;
        default:
            text << ::MarkupText();
            break;
        }
    }

    ::MarkupClean();
    return text;
}

void CCBoxTipWindow::PositionAt(const wxPoint& pt, IEditor* focusEdior)
{
    SetSize(wxRect(pt, GetSize()));
    Show();

    if( focusEdior ) {
        focusEdior->SetActive();
    }
}
