//==========================================================================
// Name:            plot_waterfall.cpp
// Purpose:         Implements a waterfall plot derivative of plot.
// Created:         June 22, 2012
// Authors:         David Rowe, David Witten
// 
// License:
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//==========================================================================
#include <string.h>
#include <algorithm>
#include "wx/wx.h"
#include "main.h"
#include "osx_interface.h"

extern float g_avmag[];                 // av mag spec passed in to draw() 
void clickTune(float frequency); // callback to pass new click freq

BEGIN_EVENT_TABLE(PlotWaterfall, PlotPanel)
    EVT_PAINT           (PlotWaterfall::OnPaint)
    EVT_MOTION          (PlotWaterfall::OnMouseMove)
    EVT_LEFT_DCLICK     (PlotWaterfall::OnMouseLeftDoubleClick)
    EVT_LEFT_UP         (PlotWaterfall::OnMouseLeftUp)
    EVT_MOUSEWHEEL      (PlotWaterfall::OnMouseWheelMoved)
    EVT_SIZE            (PlotWaterfall::OnSize)
    EVT_SHOW            (PlotWaterfall::OnShow)
END_EVENT_TABLE()

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// Class WaterfallPlot
//
// @class   WaterfallPlot
// @author  David Witten
// @date    $(Date)
// @file    $(CurrentFileName).$(CurrentFileExt)
// @brief
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
PlotWaterfall::PlotWaterfall(wxWindow* parent, bool graticule, int colour): PlotPanel(parent)
{

    for(int i = 0; i < 255; i++)
    {
        m_heatmap_lut[i] = heatmap((float)i, 0.0, 255.0);
    }
    m_graticule     = graticule;
    m_colour        = colour;
    m_Bufsz         = GetMaxClientSize();
    m_newdata       = false;
    m_firstPass     = true;
    m_line_color    = 0;
    m_updateOnlyWaterfall = false;
    m_modem_stats_max_f_hz = MODEM_STATS_MAX_F_HZ;

    SetLabelSize(10.0);

    m_max_mag = MAX_MAG_DB;
    m_min_mag = MIN_MAG_DB;
}

// When the window size gets set we can work outthe size of the window
// we plot in and allocate a bit map of the correct size
void PlotWaterfall::OnSize(wxSizeEvent& event) 
{
    m_waterfallBlocks.clear();
    
    // resize bit map

    m_rCtrl  = GetClientRect();

    // m_rGrid is coords of inner window we actually plot to.  We deflate it a bit
    // to leave room for axis labels.

    m_rGrid  = m_rCtrl;
    m_rGrid = m_rGrid.Deflate(PLOT_BORDER + (XLEFT_OFFSET/2), (PLOT_BORDER + (YBOTTOM_OFFSET/2)));

    // we want a bit map the size of m_rGrid

    m_imgHeight = m_rGrid.GetHeight();
    m_imgWidth = m_rGrid.GetWidth();    
    
    m_dT = DT;
    
    event.Skip();
}

//----------------------------------------------------------------
// OnShow()
//----------------------------------------------------------------
void PlotWaterfall::OnShow(wxShowEvent& event)
{
}

//----------------------------------------------------------------
// ~PlotWaterfall()
//----------------------------------------------------------------
PlotWaterfall::~PlotWaterfall()
{
    m_waterfallBlocks.clear();
}

void PlotWaterfall::OnPaint(wxPaintEvent& event)
{
    // Determine whether we're only updating the waterfall area or the whole
    // control.
    m_updateOnlyWaterfall = false;
    wxRegionIterator upd(GetUpdateRegion()); // get the update rect list
    while (upd)
    {
        wxRect rect(upd.GetRect());
        
        fprintf(stderr, "rect(%d,%d,%d,%d) grid(%d,%d,%d,%d)\n", rect.x, rect.y, rect.width, rect.height, m_rGrid.x, m_rGrid.y, m_rGrid.width, m_rGrid.height);
        if (rect.x < m_rGrid.x || rect.x > m_rGrid.x ||
            rect.y < m_rGrid.y || rect.y > m_rGrid.y)
        {
            fprintf(stderr, "Invalidating the entire PlotWaterfall.\n");
            //m_updateOnlyWaterfall = false;
            //break;
        }
        upd++;
    }
    
    PlotPanel::OnPaint(event);
}

//----------------------------------------------------------------
// RefreshWaterfall(): only requests redraw of the waterfall area
// (not graticules).
//----------------------------------------------------------------
void PlotWaterfall::RefreshWaterfallOnly()
{
    wxRect tmpRect(m_rGrid);
    RefreshRect(tmpRect, false);
}

//----------------------------------------------------------------
// heatmap()
// map val to a rgb colour
// from http://eddiema.ca/2011/01/21/c-sharp-heatmaps/
//----------------------------------------------------------------
unsigned PlotWaterfall::heatmap(float val, float min, float max)
{
    unsigned r = 0;
    unsigned g = 0;
    unsigned b = 0;

    val = (val - min) / (max - min);
    if(val <= 0.2)
    {
        b = (unsigned)((val / 0.2) * 255);
    }
    else if(val >  0.2 &&  val <= 0.7)
    {
        b = (unsigned)((1.0 - ((val - 0.2) / 0.5)) * 255);
    }
    if(val >= 0.2 &&  val <= 0.6)
    {
        g = (unsigned)(((val - 0.2) / 0.4) * 255);
    }
    else if(val >  0.6 &&  val <= 0.9)
    {
        g = (unsigned)((1.0 - ((val - 0.6) / 0.3)) * 255);
    }
    if(val >= 0.5)
    {
        r = (unsigned)(((val - 0.5) / 0.5) * 255);
    }
    //printf("%f %x %x %x\n", val, r, g, b);
    return  (b << 16) + (g << 8) + r;
}

bool PlotWaterfall::checkDT(void)
{
    // Check dY is > 1 pixel before proceeding. For small screens
    // and large WATERFALL_SECS_Y we might have less than one
    // block per pixel.  In this case increase m_dT and perform draw
    // less often

    float px_per_sec = (float)m_rGrid.GetHeight() / WATERFALL_SECS_Y;
    float dy = m_dT * px_per_sec;
    
    if (dy < 1.0) {
        m_dT += DT;
        return false;
    }
    else
        return true;
}

//----------------------------------------------------------------
// draw()
//----------------------------------------------------------------
void PlotWaterfall::draw(wxGraphicsContext* gc)
{
    m_rCtrl  = GetClientRect();

    // m_rGrid is coords of inner window we actually plot to.  We deflate it a bit
    // to leave room for axis labels.

    m_rGrid = m_rCtrl;
    m_rGrid = m_rGrid.Deflate(PLOT_BORDER + (XLEFT_OFFSET/2), (PLOT_BORDER + (YBOTTOM_OFFSET/2)));

    // we want a bit map the size of m_rGrid
    m_imgHeight = m_rGrid.GetHeight();
    m_imgWidth = m_rGrid.GetWidth();
    
    if (!m_updateOnlyWaterfall)
    {
        drawGraticule(gc);
    } 
    
    float px_per_sec = (float)m_imgHeight / WATERFALL_SECS_Y;
    int dy = m_dT * px_per_sec;
    int remainingBlackBoxHeight = m_imgHeight - dy * m_waterfallBlocks.size();
    int remainingBlackBoxY = PLOT_BORDER + YBOTTOM_OFFSET + dy * m_waterfallBlocks.size();
    
    if (!m_newdata)
    {
        remainingBlackBoxHeight = m_imgHeight;
        remainingBlackBoxY = PLOT_BORDER + YBOTTOM_OFFSET;
    }
    
    if (remainingBlackBoxHeight > 0)
    {
        wxBrush ltGraphBkgBrush = wxBrush(BLACK_COLOR);
        gc->SetBrush(ltGraphBkgBrush);
        gc->SetPen(wxPen(BLACK_COLOR, 0));
        gc->DrawRectangle(PLOT_BORDER + XLEFT_OFFSET, remainingBlackBoxY, m_imgWidth, remainingBlackBoxHeight);
    }
    
    if(m_newdata)
    {
        m_newdata = false;
        plotPixelData(gc);
        
        int y = 0;
        
        for (auto& bmp : m_waterfallBlocks)
        {
            gc->DrawBitmap(bmp, PLOT_BORDER + XLEFT_OFFSET, y + PLOT_BORDER + YBOTTOM_OFFSET, m_imgWidth, dy);
            y += dy;
        }
        
        m_dT = DT;
    }
}

//-------------------------------------------------------------------------
// drawGraticule()
//-------------------------------------------------------------------------
void PlotWaterfall::drawGraticule(wxGraphicsContext* ctx)
{
    int      x, y, text_w, text_h;
    char     buf[15];
    wxString s;
    float    f, time, freq_hz_to_px, time_s_to_py;

    wxBrush ltGraphBkgBrush;
    ltGraphBkgBrush.SetStyle(wxBRUSHSTYLE_TRANSPARENT);
    ltGraphBkgBrush.SetColour(*wxBLACK);
    ctx->SetBrush(ltGraphBkgBrush);
    ctx->SetPen(wxPen(BLACK_COLOR, 1));
    
    wxGraphicsFont tmpFont = ctx->CreateFont(GetFont(), GetForegroundColour());
    ctx->SetFont(tmpFont);
    
    freq_hz_to_px = (float)m_imgWidth/(MAX_F_HZ-MIN_F_HZ);
    time_s_to_py = (float)m_imgHeight/WATERFALL_SECS_Y;

    // upper LH coords of plot area are (PLOT_BORDER + XLEFT_OFFSET, PLOT_BORDER)
    // lower RH coords of plot area are (PLOT_BORDER + XLEFT_OFFSET + m_rGrid.GetWidth(), 
    //                                   PLOT_BORDER + m_rGrid.GetHeight())

    // Check if small screen size means text will overlap

    int textXStep = STEP_F_HZ*freq_hz_to_px;
    int textYStep = WATERFALL_SECS_STEP*time_s_to_py;
    sprintf(buf, "%4.0fHz", (float)MAX_F_HZ - STEP_F_HZ);
    GetTextExtent(buf, &text_w, &text_h);
    int overlappedText = (text_w > textXStep) || (text_h > textYStep);

    // Major Vertical gridlines and legend
    //dc.SetPen(m_penShortDash);
    for(f=STEP_F_HZ; f<MAX_F_HZ; f+=STEP_F_HZ) 
    {
        x = f*freq_hz_to_px;
        x += PLOT_BORDER + XLEFT_OFFSET;

        if (m_graticule)
            ctx->StrokeLine(x, m_imgHeight + PLOT_BORDER, x, PLOT_BORDER);
        else
            ctx->StrokeLine(x, PLOT_BORDER, x, PLOT_BORDER + YBOTTOM_TEXT_OFFSET + 5);
            
        sprintf(buf, "%4.0fHz", f);
        GetTextExtent(buf, &text_w, &text_h);
        if (!overlappedText)
            ctx->DrawText(buf, x - text_w/2, (YBOTTOM_TEXT_OFFSET/2));
    }

    for(f=STEP_MINOR_F_HZ; f<MAX_F_HZ; f+=STEP_MINOR_F_HZ) 
    {
        x = f*freq_hz_to_px;
        x += PLOT_BORDER + XLEFT_OFFSET;
        ctx->StrokeLine(x, PLOT_BORDER + 5, x, PLOT_BORDER + YBOTTOM_TEXT_OFFSET + 5);
    }
    
    // Horizontal gridlines
    ctx->SetPen(m_penDotDash);
    for(time=0; time<=WATERFALL_SECS_Y; time+=WATERFALL_SECS_STEP) {
       y = m_rGrid.GetHeight() - (WATERFALL_SECS_Y - time)*time_s_to_py;
       y += PLOT_BORDER + YBOTTOM_TEXT_OFFSET;

        if (m_graticule)
            ctx->StrokeLine(PLOT_BORDER + XLEFT_OFFSET, y, 
                        (m_rGrid.GetWidth() + PLOT_BORDER + XLEFT_OFFSET), y);
        sprintf(buf, "%3.0fs", time);
	    GetTextExtent(buf, &text_w, &text_h);
        if (!overlappedText)
            ctx->DrawText(buf, PLOT_BORDER + XLEFT_OFFSET - text_w - XLEFT_TEXT_OFFSET, y-text_h/2);
   }

    // red rx tuning line
    ctx->SetPen(wxPen(RED_COLOR, 2));
    x = m_rxFreq*freq_hz_to_px;
    x += PLOT_BORDER + XLEFT_OFFSET;
    ctx->StrokeLine(x, 0, x, PLOT_BORDER + YBOTTOM_TEXT_OFFSET + 5);
}

//-------------------------------------------------------------------------
// plotPixelData()
//-------------------------------------------------------------------------
void PlotWaterfall::plotPixelData(wxGraphicsContext* gc)
{
    float       spec_index_per_px;
    float       intensity_per_dB;
    float       px_per_sec;
    int         index;
    int         dy;
    unsigned int         dy_blocks;
    int         px;
    int         py;
    int         intensity;

    /*
      Design Notes:

      The height in pixels represents WATERFALL_SECS_Y of data.  Every DT
      seconds we get a vector of MODEM_STATS_NSPEC spectrum samples which we use
      to update the last row.  The height of each row is dy pixels, which
      maps to DT seconds.  We call each dy high rectangle of pixels a
      block.

    */

    // determine dy, the height of one "block"
    px_per_sec = (float)m_imgHeight / WATERFALL_SECS_Y;
    dy = m_dT * px_per_sec;

    // number of dy high blocks in spectrogram
    dy_blocks = m_imgHeight / dy;

    // update min and max amplitude estimates
    float max_mag = MIN_MAG_DB;

    int min_fft_bin=((float)200/m_modem_stats_max_f_hz)*MODEM_STATS_NSPEC;
    int max_fft_bin=((float)2800/m_modem_stats_max_f_hz)*MODEM_STATS_NSPEC;

    for(int i=min_fft_bin; i<max_fft_bin; i++) 
    {
        if (g_avmag[i] > max_mag)
        {
            max_mag = g_avmag[i];
        }
    }

    m_max_mag = BETA*m_max_mag + (1 - BETA)*max_mag;
    m_min_mag = max_mag - 20.0;
    intensity_per_dB  = (float)256 /(m_max_mag - m_min_mag);
    spec_index_per_px = ((float)(MAX_F_HZ)/(float)m_modem_stats_max_f_hz)*(float)MODEM_STATS_NSPEC / (float)m_imgWidth;

    // Draw last line of blocks using latest amplitude data ------------------
    unsigned char dyImageData[3 * dy * m_imgWidth];
    for(py = dy - 1; py >= 0; py--)
    {
        for(px = 0; px < m_imgWidth; px++)
        {
            index = px * spec_index_per_px;
            assert(index < MODEM_STATS_NSPEC);

            intensity = intensity_per_dB * (g_avmag[index] - m_min_mag);
            if(intensity > 255) intensity = 255;
            if (intensity < 0) intensity = 0;

            int pixelPos = (py * m_imgWidth * 3) + (px * 3);
            
            switch (m_colour) {
            case 0:
                dyImageData[pixelPos] = m_heatmap_lut[intensity] & 0xff;
                dyImageData[pixelPos + 1] = (m_heatmap_lut[intensity] >> 8) & 0xff;
                dyImageData[pixelPos + 2] = (m_heatmap_lut[intensity] >> 16) & 0xff;
                break;
            case 1:
                dyImageData[pixelPos] = intensity;
                dyImageData[pixelPos + 1] = intensity;
                dyImageData[pixelPos + 2] = intensity;       
                break;
            case 2:
                dyImageData[pixelPos] = intensity;
                dyImageData[pixelPos + 1] = intensity;
                if (intensity < 127)
                    dyImageData[pixelPos + 2] = intensity*2;
                else
                    dyImageData[pixelPos + 2] = 255;
                        
                break;
            }
        }
    }
    
    // Force main window's color space to be the same as what wxWidgets uses. This only has an effect
    // on macOS due to how it handles color spaces.
    ResetMainWindowColorSpace();

    wxImage* tmpImage = new wxImage(m_imgWidth, dy, (unsigned char*)&dyImageData, true);
    m_waterfallBlocks.push_front(gc->CreateBitmapFromImage(*tmpImage));
    
    if (m_waterfallBlocks.size() > dy_blocks)
    {
        m_waterfallBlocks.pop_back();
    }
    
    delete tmpImage;
}

//-------------------------------------------------------------------------
// OnMouseLeftDown()
//-------------------------------------------------------------------------
void PlotWaterfall::OnMouseLeftDoubleClick(wxMouseEvent& event)
{
    m_mouseDown = true;
    wxClientDC dc(this);

    wxPoint pt(event.GetLogicalPosition(dc));

    // map x coord to edges of actual plot
    pt.x -= PLOT_BORDER + XLEFT_OFFSET;
    pt.y -= PLOT_BORDER;

    // valid click if inside of plot
    if ((pt.x >= 0) && (pt.x <= m_imgWidth) && (pt.y >=0)) 
    {
        float freq_hz_to_px = (float)m_imgWidth/(MAX_F_HZ-MIN_F_HZ);
        float clickFreq = (float)pt.x/freq_hz_to_px;

        // communicate back to other threads
        clickTune(clickFreq);
    }
}


