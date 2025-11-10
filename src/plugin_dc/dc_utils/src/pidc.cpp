/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  Layer to perform wxDC drawing using wxDC or opengl
 *
 ***************************************************************************
 *   Copyright (C) 2011 by Sean D'Epagnier.
 *   Copyright (C) 2023 Jon Gough.
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.             *
 ***************************************************************************
 *
 */

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#ifdef __MSVC__
//#include <windows.h>
#endif

#ifdef ocpnUSE_GL
#include <wx/glcanvas.h>
#endif

#include <wx/graphics.h>
#include <wx/dcclient.h>

#include <vector>

#include "pidc.h"

#include "linmath.h"
#include "pi_shaders.h"

#ifdef __ANDROID__
#include "qdebug.h"
#endif

#if wxUSE_GRAPHICS_CONTEXT
#pragma message "Compiling with wxUSE_GRAPHICS_CONTEXT"
#endif

static float GLMinSymbolLineWidth;
static wxArrayPtrVoid pi_gTesselatorVertices;

#ifdef USE_ANDROID_GLES2
extern GLint pi_color_tri_shader_program;
extern GLint pi_circle_filled_shader_program;
#endif

//#ifdef USE_ANDROID_GLES2
//#include "pi_shaders.h"
//#include <gl2.h>
//#include "linmath.h"
//#endif

//#include "georef.h"
#ifndef PI
#define PI 3.1415926535897931160E0 /* pi */
#endif

#define __CALL_CONVENTION

// These are only global to this module to allow tessellation callbacks to
// access them. Tessellation does not handle classes and methods
bool g_bTexture2D;
int g_iTextureHeight;
int g_iTextureWidth;
GLint g_textureId;

#ifdef USE_ANDROID_GLES2
int g_min_x;
int g_min_y;
int g_max_x;
int g_max_y;
#endif

#ifdef ocpnUSE_GL
static int NextPow2(int size) {
  int n = size - 1;  // compute dimensions needed as next larger power of 2
  int shift = 1;
  while ((n + 1) & n) {
    n |= n >> shift;
    shift <<= 1;
  }

  return n + 1;
}

void checkGlError(const char* op, const char* filename, int linenumber) {
#ifdef ANDROID
    bool berror = false;

    wxString l_ErrorTxt = "";
    for (GLint error = glGetError(); error; error = glGetError()) {
        berror = true;
        switch(error) {
            case GL_INVALID_ENUM:
                l_ErrorTxt +=_("GL_INVALID_ENUM ");
                break;
            case GL_INVALID_VALUE:
                l_ErrorTxt += _("GL_INVALID_VALUE ");
                break;
            case GL_INVALID_OPERATION:
                l_ErrorTxt += _("GL_INVALID_OPERATION ");
                break;
            case GL_INVALID_FRAMEBUFFER_OPERATION:
                l_ErrorTxt += _("GL_INVALID_FRAMEBUFFER_OPERATION ");
                break;
            case GL_OUT_OF_MEMORY:
                l_ErrorTxt += _("GL_OUT_OF_MEMORY ");
                break;
        }
    }
#ifdef _DEBUG
    if(berror == true)
        wxLogMessage( _("%s:%i %s(), %s\n"), filename, linenumber, op, l_ErrorTxt);
#endif
#endif
}

#endif

//----------------------------------------------------------------------------
/* pass the dc to the constructor, or NULL to use opengl */
piDC::piDC(wxGLContext *context)
    : glcontext(context), dc(NULL), m_pen(wxNullPen), m_brush(wxNullBrush) {
  Init();
}

piDC::piDC(wxDC &pdc)
    : glcontext(NULL), dc(&pdc), m_pen(wxNullPen), m_brush(wxNullBrush) {
  Init();
#if wxUSE_GRAPHICS_CONTEXT
  pgc = NULL;
  wxMemoryDC *pmdc = wxDynamicCast(dc, wxMemoryDC);
  if (pmdc)
    pgc = wxGraphicsContext::Create(*pmdc);
  else {
    wxClientDC *pcdc = wxDynamicCast(dc, wxClientDC);
    if (pcdc) pgc = wxGraphicsContext::Create(*pcdc);
  }
#endif
}

piDC::piDC()
    : glcontext(NULL), dc(NULL), m_pen(wxNullPen), m_brush(wxNullBrush) {
  Init();
}

piDC::~piDC() {
#if wxUSE_GRAPHICS_CONTEXT
  if (pgc) delete pgc;
#endif

#ifdef USE_ANDROID_GLES2
  free(s_odc_tess_work_buf);
  free(s_odc_tess_tex_buf);
#endif
}

void piDC::Init() {
  m_buseTex = GetLocaleCanonicalName().IsSameAs(_T("en_US"));

#if wxUSE_GRAPHICS_CONTEXT
  pgc = NULL;
#endif

#ifdef ocpnUSE_GL
  g_bTexture2D = false;
  m_textforegroundcolour = wxColour(0, 0, 0);
#endif

  m_textbackgroundcolour = wxTransparentColour;
  workBuf = NULL;
  workBufSize = 0;

#ifdef USE_ANDROID_GLES2
  s_odc_tess_work_buf = NULL;
  s_odc_tess_tex_buf = NULL;
  s_odc_tess_vertex_idx = 0;
  s_odc_tess_vertex_idx_this = 0;
  s_odc_tess_buf_len = 0;

  s_odc_tess_work_buf = (GLfloat *)malloc(100 * sizeof(GLfloat));
  s_odc_tess_tex_buf = (GLfloat *)malloc(100 * sizeof(GLfloat));
  s_odc_tess_buf_len = 100;
#endif

  g_textureId = -1;
  m_tobj = NULL;
#ifdef ocpnUSE_GL
  if (glcontext) {
    GLint parms[2];
    glGetIntegerv(GL_SMOOTH_LINE_WIDTH_RANGE, &parms[0]);
    GLMinSymbolLineWidth = wxMax(parms[0], 1);

    pi_loadShaders();
  }
#endif
}

void piDC::SetVP(PlugIn_ViewPort *vp) {
#ifdef USE_ANDROID_GLES2
  configureShaders(vp->pix_width, vp->pix_height);
#endif
  m_vpSize = wxSize(vp->pix_width, vp->pix_height);
}

void piDC::Clear() {
  if (dc)
    dc->Clear();
//  else {
//#ifdef ocpnUSE_GL
//    wxBrush tmpBrush = m_brush;
//    int w, h;
//    SetBrush(wxBrush(glcanvas->GetBackgroundColour()));
//    glcanvas->GetSize(&w, &h);
//    DrawRectangle(0, 0, w, h);
//    SetBrush(tmpBrush);
//#endif
//  }
}

void piDC::SetBackground(const wxBrush &brush) {
  if (dc)
    dc->SetBackground(brush);
  else {
//#ifdef ocpnUSE_GL
//    glcanvas->SetBackgroundColour(brush.GetColour());
//#endif
  }
}

void piDC::SetGLAttrs(bool highQuality) {
#ifdef ocpnUSE_GL

  //      Enable anti-aliased polys, at best quality
  if (highQuality) {
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_POLYGON_SMOOTH);
    glEnable(GL_BLEND);
  } else {
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_POLYGON_SMOOTH);
    glDisable(GL_BLEND);
  }

#endif
}

void piDC::SetPen(const wxPen &pen) {
  if (dc) {
    if (pen == wxNullPen)
      dc->SetPen(*wxTRANSPARENT_PEN);
    else
      dc->SetPen(pen);
  } else {
    m_pen = pen;

#ifndef USE_ANDROID_GLES2
    if (m_pen.IsOk()) {
      switch (m_pen.GetStyle()) {
        case wxPENSTYLE_DOT:
        case wxPENSTYLE_LONG_DASH:
        case wxPENSTYLE_SHORT_DASH:
        case wxPENSTYLE_DOT_DASH:
          break;

        default:
          glDisable(GL_LINE_STIPPLE);
          break;
      }
    }
#endif
  }
}

void piDC::SetBrush(const wxBrush &brush) {
  if (dc)
    dc->SetBrush(brush);
  else
    m_brush = brush;
}

void piDC::SetTextForeground(const wxColour &colour) {
  if (dc)
    dc->SetTextForeground(colour);
  else
    m_textforegroundcolour = colour;
}

void piDC::SetTextBackground( const wxColour &colour )
{
    if( dc ) dc->SetTextBackground( colour );
    else
        m_textbackgroundcolour = colour;
}

void piDC::SetFont(const wxFont &font) {
  if (dc)
    dc->SetFont(font);
  else
    m_font = font;
}

const wxPen &piDC::GetPen() const {
  if (dc) return dc->GetPen();
  return m_pen;
}

const wxBrush &piDC::GetBrush() const {
  if (dc) return dc->GetBrush();
  return m_brush;
}

const wxFont &piDC::GetFont() const {
  if (dc) return dc->GetFont();
  return m_font;
}

void piDC::GetSize(wxCoord *width, wxCoord *height) const {
  if (dc)
    dc->GetSize(width, height);
  else {
//#ifdef ocpnUSE_GL
//    glcanvas->GetSize(width, height);
//#endif
  }
}

void piDC::SetGLStipple() const {
#ifdef ocpnUSE_GL
#ifndef USE_ANDROID_GLES2

  switch (m_pen.GetStyle()) {
    case wxPENSTYLE_DOT: {
      glLineStipple(1, 0xF8F8);
      glEnable(GL_LINE_STIPPLE);
      break;
    }
    case wxPENSTYLE_LONG_DASH: {
      glLineStipple(2, 0x3FFF);
      glEnable(GL_LINE_STIPPLE);
      break;
    }
    case wxPENSTYLE_SHORT_DASH: {
      glLineStipple(1, 0x0FFF);
      glEnable(GL_LINE_STIPPLE);
      break;
    }
    case wxPENSTYLE_DOT_DASH: {
      glLineStipple(2, 0xDEDE);
      glEnable(GL_LINE_STIPPLE);
      break;
    }
    default:
      break;
  }
#endif  // USE_ANDROID_GLES2
#endif  // ocpnUSE_GL
}

#ifdef ocpnUSE_GL
/* draw a half circle using triangles */
void piDC::DrawEndCap(float x1, float y1, float t1, float angle) {
#ifndef USE_ANDROID_GLES2
  const int steps = 16;
  float xa, ya;
  bool first = true;
  for (int i = 0; i <= steps; i++) {
    float a = angle + M_PI / 2 + M_PI / steps * i;

    float xb = x1 + t1 / 2 * cos(a);
    float yb = y1 + t1 / 2 * sin(a);
    if (first)
      first = false;
    else {
      glVertex2f(x1, y1);
      glVertex2f(xa, ya);
      glVertex2f(xb, yb);
    }
    xa = xb, ya = yb;
  }
#endif
}
#endif  // ocpnUSE_GL

// Draws a line between (x1,y1) - (x2,y2) with a start thickness of t1
void piDC::DrawGLThickLine(float x1, float y1, float x2, float y2, wxPen pen,
                           bool b_hiqual) {
#ifdef ocpnUSE_GL

  float angle = atan2f(y2 - y1, x2 - x1);
  float t1 = pen.GetWidth();
  float t2sina1 = t1 / 2 * sinf(angle);
  float t2cosa1 = t1 / 2 * cosf(angle);

#ifndef USE_ANDROID_GLES2
  glBegin(GL_TRIANGLES);

  //    n.b.  The dwxDash interpretation for GL only allows for 2 elements in
  //    the dash table. The first is assumed drawn, second is assumed space
  wxDash *dashes;
  int n_dashes = pen.GetDashes(&dashes);
  if (n_dashes) {
    float lpix = sqrtf(powf((float)(x1 - x2), 2) + powf((float)(y1 - y2), 2));
    float lrun = 0.;
    float xa = x1;
    float ya = y1;
    float ldraw = t1 * dashes[0];
    float lspace = t1 * dashes[1];

    while (lrun < lpix) {
      //    Dash
      float xb = xa + ldraw * cosf(angle);
      float yb = ya + ldraw * sinf(angle);

      if ((lrun + ldraw) >= lpix)  // last segment is partial draw
      {
        xb = x2;
        yb = y2;
      }

      glVertex2f(xa + t2sina1, ya - t2cosa1);
      glVertex2f(xb + t2sina1, yb - t2cosa1);
      glVertex2f(xb - t2sina1, yb + t2cosa1);

      glVertex2f(xb - t2sina1, yb + t2cosa1);
      glVertex2f(xa - t2sina1, ya + t2cosa1);
      glVertex2f(xa + t2sina1, ya - t2cosa1);

      xa = xb;
      ya = yb;
      lrun += ldraw;

      //    Space
      xb = xa + lspace * cos(angle);
      yb = ya + lspace * sin(angle);

      xa = xb;
      ya = yb;
      lrun += lspace;
    }
  } else {
    glVertex2f(x1 + t2sina1, y1 - t2cosa1);
    glVertex2f(x2 + t2sina1, y2 - t2cosa1);
    glVertex2f(x2 - t2sina1, y2 + t2cosa1);

    glVertex2f(x2 - t2sina1, y2 + t2cosa1);
    glVertex2f(x1 - t2sina1, y1 + t2cosa1);
    glVertex2f(x1 + t2sina1, y1 - t2cosa1);

    /* wx draws a nice rounded end in dc mode, so replicate
       this for opengl mode, should this be done for the dashed mode case? */
    if (pen.GetCap() == wxCAP_ROUND) {
      DrawEndCap(x1, y1, t1, angle);
      DrawEndCap(x2, y2, t1, angle + M_PI);
    }
  }

  glEnd();
#else
  //    n.b.  The dwxDash interpretation for GL only allows for 2 elements in
  //    the dash table. The first is assumed drawn, second is assumed space
  wxDash *dashes;
  int n_dashes = pen.GetDashes(&dashes);
  if (n_dashes) {
    float lpix = sqrtf(powf((float)(x1 - x2), 2) + powf((float)(y1 - y2), 2));
    float lrun = 0.;
    float xa = x1;
    float ya = y1;
    float ldraw = t1 * dashes[0];
    float lspace = t1 * dashes[1];

    GLint program = pi_color_tri_shader_program;
    glUseProgram(program);

    float vert[12];

    // Disable VBO's (vertex buffer objects) for attributes.
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    GLint pos = glGetAttribLocation(program, "position");
    glEnableVertexAttribArray(pos);
    glVertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), vert);

    // Build Transform matrix
    mat4x4 I;
    mat4x4_identity(I);

    GLint matloc = glGetUniformLocation(program, "TransformMatrix");
    glUniformMatrix4fv(matloc, 1, GL_FALSE, (const GLfloat *)I);

    wxColor c = pen.GetColour();
    float colorv[4];
    colorv[0] = c.Red() / float(256);
    colorv[1] = c.Green() / float(256);
    colorv[2] = c.Blue() / float(256);
    colorv[3] = c.Alpha() / float(256);

    GLint colloc = glGetUniformLocation(program, "color");
    glUniform4fv(colloc, 1, colorv);

    while (lrun < lpix) {
      //    Dash
      float xb = xa + ldraw * cosf(angle);
      float yb = ya + ldraw * sinf(angle);

      if ((lrun + ldraw) >= lpix)  // last segment is partial draw
      {
        xb = x2;
        yb = y2;
      }

      vert[0] = xa + t2sina1;
      vert[1] = ya - t2cosa1;
      vert[2] = xb + t2sina1;
      vert[3] = yb - t2cosa1;
      vert[4] = xb - t2sina1;
      vert[5] = yb + t2cosa1;
      vert[6] = xb - t2sina1;
      vert[7] = yb + t2cosa1;
      vert[8] = xa - t2sina1;
      vert[9] = ya + t2cosa1;
      vert[10] = xa + t2sina1;
      vert[11] = ya - t2cosa1;

      glDrawArrays(GL_TRIANGLES, 0, 6);

      xa = xb;
      ya = yb;
      lrun += ldraw;

      //    Space
      xb = xa + lspace * cos(angle);
      yb = ya + lspace * sin(angle);

      xa = xb;
      ya = yb;
      lrun += lspace;
    }

    glUseProgram(0);
  } else {
    float vert[12];
    vert[0] = x1 + t2sina1;
    vert[1] = y1 - t2cosa1;
    vert[2] = x2 + t2sina1;
    vert[3] = y2 - t2cosa1;
    vert[4] = x2 - t2sina1;
    vert[5] = y2 + t2cosa1;
    vert[6] = x2 - t2sina1;
    vert[7] = y2 + t2cosa1;
    vert[8] = x1 - t2sina1;
    vert[9] = y1 + t2cosa1;
    vert[10] = x1 + t2sina1;
    vert[11] = y1 - t2cosa1;

    GLint program = pi_color_tri_shader_program;
    glUseProgram(program);

    // Disable VBO's (vertex buffer objects) for attributes.
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    GLint pos = glGetAttribLocation(program, "position");
    glEnableVertexAttribArray(pos);
    glVertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), vert);

    // Build Transform matrix
    mat4x4 I;
    mat4x4_identity(I);

    GLint matloc = glGetUniformLocation(program, "TransformMatrix");
    glUniformMatrix4fv(matloc, 1, GL_FALSE, (const GLfloat *)I);

    wxColor c = pen.GetColour();
    float colorv[4];
    colorv[0] = c.Red() / float(256);
    colorv[1] = c.Green() / float(256);
    colorv[2] = c.Blue() / float(256);
    colorv[3] = c.Alpha() / float(256);

    GLint colloc = glGetUniformLocation(program, "color");
    glUniform4fv(colloc, 1, colorv);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    /* wx draws a nice rounded end in dc mode, so replicate
     *           this for opengl mode, should this be done for the dashed mode
     * case? */
    //         if(pen.GetCap() == wxCAP_ROUND) {
    //             DrawEndCap( x1, y1, t1, angle);
    //             DrawEndCap( x2, y2, t1, angle + M_PI);
    //         }
    //
    glUseProgram(0);
  }

#endif

#endif  // ocpnUSE_GL
}

void piDC::DrawLine(wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2,
                    bool b_hiqual) {
  if (dc) {
    dc->DrawLine(x1, y1, x2, y2);
  }
#ifdef ocpnUSE_GL
  else if (ConfigurePen()) {
    bool b_draw_thick = false;

    float pen_width = wxMax(GLMinSymbolLineWidth, m_pen.GetWidth());

    //      Enable anti-aliased lines, at best quality
    if (b_hiqual) {
      SetGLStipple();

#ifndef __WXQT__
      glEnable(GL_BLEND);
      glEnable(GL_LINE_SMOOTH);
#endif

      if (pen_width > 1.0) {
        GLint parms[2];
        glGetIntegerv(GL_SMOOTH_LINE_WIDTH_RANGE, &parms[0]);
        if (glGetError()) glGetIntegerv(GL_ALIASED_LINE_WIDTH_RANGE, &parms[0]);
        if (pen_width > parms[1]) {
          b_draw_thick = true;
        } else
          glLineWidth(pen_width);
      } else {
        glLineWidth(pen_width);
      }
    } else {
      if (pen_width > 1) {
        GLint parms[2];
        glGetIntegerv(GL_ALIASED_LINE_WIDTH_RANGE, &parms[0]);
        if (pen_width > parms[1])
          b_draw_thick = true;
        else
          glLineWidth(pen_width);
      } else
        glLineWidth(pen_width);
    }

#ifdef USE_ANDROID_GLES2
    if (b_draw_thick) {
      DrawGLThickLine(x1, y1, x2, y2, m_pen, b_hiqual);
    } else {
      checkGlError("Before glUseProgram", "piDC", __LINE__);
      GLint program = pi_color_tri_shader_program;
      glUseProgram(program);
      checkGlError("After glUseProgram", "piDC", __LINE__);

      float fBuf[4];
      GLint pos = glGetAttribLocation(program, "position");
      glVertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                            fBuf);
      glEnableVertexAttribArray(pos);

      // GLint matloc = glGetUniformLocation(program,"MVMatrix");
      // glUniformMatrix4fv( matloc, 1, GL_FALSE, (const
      // GLfloat*)cc1->GetpVP()->vp_transform);

      float colorv[4];
      colorv[0] = m_pen.GetColour().Red() / float(256);
      colorv[1] = m_pen.GetColour().Green() / float(256);
      colorv[2] = m_pen.GetColour().Blue() / float(256);
      colorv[3] = 1.0;

      GLint colloc = glGetUniformLocation(program, "color");
      glUniform4fv(colloc, 1, colorv);

      wxDash *dashes;
      int n_dashes = m_pen.GetDashes(&dashes);
      if (n_dashes) {
        float angle = atan2f((float)(y2 - y1), (float)(x2 - x1));
        float cosa = cosf(angle);
        float sina = sinf(angle);
        float t1 = m_pen.GetWidth();

        float lpix = sqrtf(powf(x1 - x2, 2) + powf(y1 - y2, 2));
        float lrun = 0.;
        float xa = x1;
        float ya = y1;
        float ldraw = t1 * dashes[0];
        float lspace = t1 * dashes[1];

        ldraw = wxMax(ldraw, 4.0);
        lspace = wxMax(lspace, 4.0);
        lpix = wxMin(lpix, 2000.0);

        while (lrun < lpix) {
          //    Dash
          float xb = xa + ldraw * cosa;
          float yb = ya + ldraw * sina;

          if ((lrun + ldraw) >= lpix)  // last segment is partial draw
          {
            xb = x2;
            yb = y2;
          }

          fBuf[0] = xa;
          fBuf[1] = ya;
          fBuf[2] = xb;
          fBuf[3] = yb;

          glDrawArrays(GL_LINES, 0, 2);

          xa = xa + (lspace + ldraw) * cosa;
          ya = ya + (lspace + ldraw) * sina;
          lrun += lspace + ldraw;
        }
      } else  // not dashed
      {
        fBuf[0] = x1;
        fBuf[1] = y1;
        fBuf[2] = x2;
        fBuf[3] = y2;

        glDrawArrays(GL_LINES, 0, 2);
      }

      glUseProgram(0);
    }

#else
    if (b_draw_thick) {
      DrawGLThickLine(x1, y1, x2, y2, m_pen, b_hiqual);
    } else {
      wxDash *dashes;
      int n_dashes = m_pen.GetDashes(&dashes);
      if (n_dashes) {
        float angle = atan2f((float)(y2 - y1), (float)(x2 - x1));
        float cosa = cosf(angle);
        float sina = sinf(angle);
        float t1 = m_pen.GetWidth();

        float lpix = sqrtf(powf(x1 - x2, 2) + powf(y1 - y2, 2));
        float lrun = 0.;
        float xa = x1;
        float ya = y1;
        float ldraw = t1 * dashes[0];
        float lspace = t1 * dashes[1];

        ldraw = wxMax(ldraw, 4.0);
        lspace = wxMax(lspace, 4.0);
        lpix = wxMin(lpix, 2000.0);

        glBegin(GL_LINES);
        while (lrun < lpix) {
          //    Dash
          float xb = xa + ldraw * cosa;
          float yb = ya + ldraw * sina;

          if ((lrun + ldraw) >= lpix)  // last segment is partial draw
          {
            xb = x2;
            yb = y2;
          }

          glVertex2f(xa, ya);
          glVertex2f(xb, yb);

          xa = xa + (lspace + ldraw) * cosa;
          ya = ya + (lspace + ldraw) * sina;
          lrun += lspace + ldraw;
        }
        glEnd();
      } else  // not dashed
      {
        glBegin(GL_LINES);
        glVertex2i(x1, y1);
        glVertex2i(x2, y2);
        glEnd();
      }
    }
#endif
    glDisable(GL_LINE_STIPPLE);

    if (b_hiqual) {
      glDisable(GL_LINE_SMOOTH);
      glDisable(GL_BLEND);
    }
  }
#endif  // ocpnUSE_GL
}

// Draws thick lines from triangles
void piDC::DrawGLThickLines(int n, wxPoint points[], wxCoord xoffset,
                            wxCoord yoffset, wxPen pen, bool b_hiqual) {
#ifdef ocpnUSE_GL
  if (n < 2) return;

#ifdef USE_ANDROID_GLES2
  wxPoint p0 = points[0];
  for (int i = 1; i < n; i++) {
    DrawGLThickLine(p0.x + xoffset, p0.y + yoffset, points[i].x + xoffset,
                    points[i].y + yoffset, pen, b_hiqual);
    p0 = points[i];
  }
  return;
#else
  /* for dashed case, for now just draw thick lines */
  wxDash *dashes;
  if (pen.GetDashes(&dashes)) {
    wxPoint p0 = points[0];
    for (int i = 1; i < n; i++) {
      DrawGLThickLine(p0.x + xoffset, p0.y + yoffset, points[i].x + xoffset,
                      points[i].y + yoffset, pen, b_hiqual);
      p0 = points[i];
    }
    return;
  }

  /* cull zero segments */
  wxPoint *cpoints = new wxPoint[n];
  cpoints[0] = points[0];
  int c = 1;
  for (int i = 1; i < n; i++) {
    if (points[i].x != points[i - 1].x || points[i].y != points[i - 1].y)
      cpoints[c++] = points[i];
  }

  /* nicer than than rendering each segment separately, this is because thick
     line segments drawn as rectangles which have different angles have
     rectangles which overlap and also leave a gap.
     This code properly calculates vertexes for adjoining segments */
  float t1 = pen.GetWidth();

  float x0 = cpoints[0].x, y0 = cpoints[0].y, x1 = cpoints[1].x,
        y1 = cpoints[1].y;
  float a0 = atan2f(y1 - y0, x1 - x0);

  // It is also possible to use triangle strip, (and triangle fan for endcap)
  // to reduce vertex count.. is it worth it?
  glBegin(GL_TRIANGLES);

  float t2sina0 = t1 / 2 * sinf(a0);
  float t2cosa0 = t1 / 2 * cosf(a0);

  for (int i = 1; i < c; i++) {
    float x2, y2;
    float a1;

    if (i < c - 1) {
      x2 = cpoints[i + 1].x, y2 = cpoints[i + 1].y;
      a1 = atan2f(y2 - y1, x2 - x1);
    } else {
      x2 = x1, y2 = y1;
      a1 = a0;
    }

    float aa = (a0 + a1) / 2;
    float diff = fabsf(a0 - a1);
    if (diff > M_PI) diff -= 2 * (float)M_PI;
    float rad = t1 / 2 / wxMax(cosf(diff / 2), .4);

    float t2sina1 = rad * sinf(aa);
    float t2cosa1 = rad * cosf(aa);

    glVertex2f(x1 + t2sina1, y1 - t2cosa1);
    glVertex2f(x1 - t2sina1, y1 + t2cosa1);
    glVertex2f(x0 + t2sina0, y0 - t2cosa0);

    glVertex2f(x0 - t2sina0, y0 + t2cosa0);
    glVertex2f(x0 + t2sina0, y0 - t2cosa0);

    float dot = t2sina0 * t2sina1 + t2cosa0 * t2cosa1;
    if (dot > 0)
      glVertex2f(x1 - t2sina1, y1 + t2cosa1);
    else
      glVertex2f(x1 + t2sina1, y1 - t2cosa1);

    x0 = x1, x1 = x2;
    y0 = y1, y1 = y2;
    a0 = a1;
    t2sina0 = t2sina1, t2cosa0 = t2cosa1;
  }

  if (pen.GetCap() == wxCAP_ROUND) {
    DrawEndCap(x0, y0, t1, a0);
    DrawEndCap(x0, y0, t1, a0 + M_PI);
  }

  glEnd();

  glPopAttrib();

  delete[] cpoints;

#endif
#endif  // ocpnUSE_GL
}

void piDC::DrawLines(int n, wxPoint points[], wxCoord xoffset, wxCoord yoffset,
                     bool b_hiqual) {
  if (dc) dc->DrawLines(n, points, xoffset, yoffset);
#ifdef ocpnUSE_GL
  else if (ConfigurePen()) {
#ifdef __WXQT__
    SetGLAttrs(false);  // Some QT platforms (Android) have trouble with
                        // GL_BLEND / GL_LINE_SMOOTH
#else
    SetGLAttrs(b_hiqual);
#endif
    bool b_draw_thick = false;

    glDisable(GL_LINE_STIPPLE);
    SetGLStipple();

    //      Enable anti-aliased lines, at best quality
    if (b_hiqual) {
      glEnable(GL_BLEND);
      if (m_pen.GetWidth() > 1) {
        GLint parms[2];
        glGetIntegerv(GL_SMOOTH_LINE_WIDTH_RANGE, &parms[0]);
        if (glGetError()) glGetIntegerv(GL_ALIASED_LINE_WIDTH_RANGE, &parms[0]);

        if (m_pen.GetWidth() > parms[1])
          b_draw_thick = true;
        else
          glLineWidth(wxMax(GLMinSymbolLineWidth, m_pen.GetWidth()));
      } else
        glLineWidth(wxMax(GLMinSymbolLineWidth, 1));
    } else {
      if (m_pen.GetWidth() > 1) {
        GLint parms[2];
        glGetIntegerv(GL_ALIASED_LINE_WIDTH_RANGE, &parms[0]);
        if (m_pen.GetWidth() > parms[1])
          b_draw_thick = true;
        else
          glLineWidth(wxMax(GLMinSymbolLineWidth, m_pen.GetWidth()));
      } else
        glLineWidth(wxMax(GLMinSymbolLineWidth, 1));
    }

    if (b_draw_thick) {
      DrawGLThickLines(n, points, xoffset, yoffset, m_pen, b_hiqual);
    } else {
      if (b_hiqual) {
        glEnable(GL_LINE_SMOOTH);
        glDisable(GL_LINE_STIPPLE);
        glDisable(GL_POLYGON_SMOOTH);
        glDisable(GL_BLEND);
      }
    }
#ifndef USE_ANDROID_GLES2
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < n; i++) {
      glVertex2i(points[i].x + xoffset, points[i].y + yoffset);
    }
    glEnd();
#else
    //  Grow the work buffer as necessary
    if (workBufSize < (size_t)n * 2) {
      workBuf = (float *)realloc(workBuf, (n * 4) * sizeof(float));
      workBufSize = n * 4;
    }

    for (int i = 0; i < n; i++) {
      workBuf[i * 2] = points[i].x + xoffset;
      workBuf[(i * 2) + 1] = points[i].y + yoffset;
    }

    GLint program = pi_color_tri_shader_program;
    glUseProgram(program);

    GLint pos = glGetAttribLocation(program, "position");
    glVertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                          workBuf);
    glEnableVertexAttribArray(pos);

    float colorv[4];
    colorv[0] = m_pen.GetColour().Red() / float(256);
    colorv[1] = m_pen.GetColour().Green() / float(256);
    colorv[2] = m_pen.GetColour().Blue() / float(256);
    colorv[3] = m_pen.GetColour().Alpha() / float(256);

    GLint colloc = glGetUniformLocation(program, "color");
    glUniform4fv(colloc, 1, colorv);

    glDrawArrays(GL_LINE_STRIP, 0, n);
    glUseProgram(0);

#endif
    if (b_hiqual) {
      glDisable(GL_LINE_STIPPLE);
      glDisable(GL_POLYGON_SMOOTH);
      glDisable(GL_BLEND);
    }

    SetGLAttrs(false);
  }
#endif  // ocpnUSE_GL
}

void piDC::DrawArc(wxCoord xc, wxCoord yc, wxCoord x1, wxCoord y1, wxCoord x2,
                   wxCoord y2, bool b_hiqual) {
#ifndef USE_ANDROID_GLES2
  if (dc) dc->DrawArc(x1, y1, x2, y2, xc, yc);
#ifdef ocpnUSE_GL
  else if (ConfigurePen()) {
    bool b_draw_thick = false;

    float pen_width = wxMax(GLMinSymbolLineWidth, m_pen.GetWidth());

    //      Enable anti-aliased lines, at best quality
    if (b_hiqual) {
      SetGLStipple();

#ifndef __WXQT__
      glEnable(GL_BLEND);
      glEnable(GL_LINE_SMOOTH);
#endif

      if (pen_width > 1.0) {
        GLint parms[2];
        glGetIntegerv(GL_SMOOTH_LINE_WIDTH_RANGE, &parms[0]);
        if (pen_width > parms[1])
          b_draw_thick = true;
        else
          glLineWidth(pen_width);
      } else
        glLineWidth(pen_width);
    } else {
      if (pen_width > 1) {
        GLint parms[2];
        glGetIntegerv(GL_ALIASED_LINE_WIDTH_RANGE, &parms[0]);
        if (pen_width > parms[1])
          b_draw_thick = true;
        else
          glLineWidth(pen_width);
      } else
        glLineWidth(pen_width);
    }

    if (b_draw_thick) {
      DrawGLThickLine(x1, y1, x2, y2, m_pen, b_hiqual);
    } else {
      wxDash *dashes;
      int n_dashes = m_pen.GetDashes(&dashes);
      if (n_dashes) {
        float angle = atan2f((float)(y2 - y1), (float)(x2 - x1));
        float cosa = cosf(angle);
        float sina = sinf(angle);
        float t1 = m_pen.GetWidth();

        float lpix = sqrtf(powf(x1 - x2, 2) + powf(y1 - y2, 2));
        float lrun = 0.;
        float xa = x1;
        float ya = y1;
        float ldraw = t1 * dashes[0];
        float lspace = t1 * dashes[1];

        glBegin(GL_LINES);
        while (lrun < lpix) {
          //    Dash
          float xb = xa + ldraw * cosa;
          float yb = ya + ldraw * sina;

          if ((lrun + ldraw) >= lpix)  // last segment is partial draw
          {
            xb = x2;
            yb = y2;
          }

          glVertex2f(xa, ya);
          glVertex2f(xb, yb);

          xa = xa + (lspace + ldraw) * cosa;
          ya = ya + (lspace + ldraw) * sina;
          lrun += lspace + ldraw;
        }
        glEnd();
      } else  // not dashed
      {
        glBegin(GL_LINES);
        glVertex2i(x1, y1);
        glVertex2i(x2, y2);
        glEnd();
      }
    }

    glDisable(GL_LINE_STIPPLE);

    if (b_hiqual) {
      glDisable(GL_LINE_SMOOTH);
      glDisable(GL_BLEND);
    }
  }
#endif  // ocpnUSE_GL
#endif
}
void piDC::DrawSector(wxCoord xc, wxCoord yc, wxCoord x1, wxCoord y1,
                      wxCoord x2, wxCoord y2, wxCoord x3, wxCoord y3,
                      wxCoord x4, wxCoord y4) {
  if (dc) {
    double y1yc, x1xc, y4yc, x4xc;
    y1yc = y1 - yc;
    x1xc = x1 - xc;
    y4yc = y4 - yc;
    x4xc = x4 - xc;
    wxDouble l_dFirstAngle;
    l_dFirstAngle = atan2(y1yc, x1xc);

    wxDouble l_dSecondAngle;
    l_dSecondAngle = atan2(y4yc, x4xc);

    wxDouble l_OuterRadius = sqrt(pow((y2 - yc), 2.0) + pow((x2 - xc), 2.0));
    wxDouble l_InnerRadius = sqrt(pow((y1 - yc), 2.0) + pow((x1 - xc), 2.0));

#if wxUSE_GRAPHICS_CONTEXT
    wxGraphicsContext *wxGC = NULL;
    wxMemoryDC *pmdc = wxDynamicCast(GetDC(), wxMemoryDC);
    if (pmdc)
      wxGC = wxGraphicsContext::Create(*pmdc);
    else {
      wxClientDC *pcdc = wxDynamicCast(GetDC(), wxClientDC);
      if (pcdc) wxGC = wxGraphicsContext::Create(*pcdc);
    }
    if (wxGC) {
      wxGC->SetPen(dc->GetPen());
      wxGC->SetBrush(dc->GetBrush());
      wxGraphicsPath gpath = wxGC->CreatePath();

      gpath.MoveToPoint(x1, y1);
      gpath.AddLineToPoint(x2, y2);
      gpath.AddArc(xc, yc, l_OuterRadius, l_dFirstAngle, l_dSecondAngle, true);
      gpath.MoveToPoint(x3, y3);
      gpath.AddLineToPoint(x4, y4);
      gpath.AddArc(xc, yc, l_InnerRadius, l_dSecondAngle, l_dFirstAngle, false);

      wxGC->FillPath(gpath);
    }
#else
    wxPoint *points;
    int numpoints =
        ArcSectorPoints(*&points, xc, yc, x1, y1, x2, y2, x3, y3, x4, y4, true);
    DrawPolygon(numpoints, points);
    delete[] points;
#endif
  }
#ifdef ocpnUSE_GL
  else {
    wxPoint *points;
    int numpoints =
        ArcSectorPoints(*&points, xc, yc, x1, y1, x2, y2, x3, y3, x4, y4, true);
    DrawLines(numpoints, points);
    DrawPolygon(numpoints, points, 0, 0);
    delete[] points;
  }
#endif  // ocpnUSE_GL
}

void piDC::StrokeLine(wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2) {
#if wxUSE_GRAPHICS_CONTEXT
  if (pgc) {
    pgc->SetPen(dc->GetPen());
    pgc->StrokeLine(x1, y1, x2, y2);

    dc->CalcBoundingBox(x1, y1);
    dc->CalcBoundingBox(x2, y2);
  } else
#endif
    DrawLine(x1, y1, x2, y2, true);
}

void piDC::StrokeLines(int n, wxPoint *points) {
  if (n < 2) /* optimization and also to avoid assertion in pgc->StrokeLines */
    return;

#if wxUSE_GRAPHICS_CONTEXT
  if (pgc) {
    wxPoint2DDouble *dPoints =
        (wxPoint2DDouble *)malloc(n * sizeof(wxPoint2DDouble));
    for (int i = 0; i < n; i++) {
      dPoints[i].m_x = points[i].x;
      dPoints[i].m_y = points[i].y;
    }
    pgc->SetPen(dc->GetPen());
    pgc->StrokeLines(n, dPoints);
    free(dPoints);
  } else
#endif
    DrawLines(n, points, 0, 0, true);
}

void piDC::DrawGLLineArray(int n, float *vertex_array, float *color_array,
                           bool b_hiqual) {
#ifdef ocpnUSE_GL
  if (ConfigurePen()) {
#ifdef __WXQT__
    SetGLAttrs(false);  // Some QT platforms (Android) have trouble with
                        // GL_BLEND / GL_LINE_SMOOTH
#else
    SetGLAttrs(b_hiqual);
#endif
    // bool b_draw_thick = false;

    glDisable(GL_LINE_STIPPLE);
    SetGLStipple();

    //      Enable anti-aliased lines, at best quality
    if (b_hiqual) {
      if (m_pen.GetWidth() > 1) {
        // GLint parms[2];
        // glGetIntegerv( GL_SMOOTH_LINE_WIDTH_RANGE, &parms[0] );
        // if(glGetError())
        // glGetIntegerv( GL_ALIASED_LINE_WIDTH_RANGE, &parms[0] );

        glLineWidth(wxMax(GLMinSymbolLineWidth, m_pen.GetWidth()));
      } else
        glLineWidth(wxMax(GLMinSymbolLineWidth, 1));
    } else {
      if (m_pen.GetWidth() > 1) {
        // GLint parms[2];
        // glGetIntegerv( GL_ALIASED_LINE_WIDTH_RANGE, &parms[0] );
        glLineWidth(wxMax(GLMinSymbolLineWidth, m_pen.GetWidth()));
      } else
        glLineWidth(wxMax(GLMinSymbolLineWidth, 1));
    }

#ifndef USE_ANDROID_GLES2

    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < n; i++)
      glVertex2f(vertex_array[2 * i], vertex_array[2 * i + 1]);
    glEnd();

#else
    glUseProgram(pi_colorv_tri_shader_program);

    GLint pos = glGetAttribLocation(pi_colorv_tri_shader_program, "position");
    glVertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                          vertex_array);
    glEnableVertexAttribArray(pos);

    GLint colloc = glGetAttribLocation(pi_colorv_tri_shader_program, "colorv");
    glVertexAttribPointer(colloc, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          color_array);
    glEnableVertexAttribArray(colloc);

    glDrawArrays(GL_LINES, 0, n);
    glUseProgram(0);
#endif
    if (b_hiqual) {
      glDisable(GL_LINE_STIPPLE);
      glDisable(GL_POLYGON_SMOOTH);
    }
  }
#endif  // ocpnUSE_GL
}

void piDC::StrokeArc(wxCoord xc, wxCoord yc, wxCoord x1, wxCoord y1, wxCoord x2,
                     wxCoord y2) {
#if wxUSE_GRAPHICS_CONTEXT
  if (pgc) {
    pgc->SetPen(dc->GetPen());
    pgc->SetBrush(dc->GetBrush());

    wxGraphicsPath gpath = pgc->CreatePath();
    wxDouble l_dFirstAngle = atan2((y1 - yc), (x1 - xc));
    wxDouble l_dSecondAngle = atan2((y2 - yc), (x2 - xc));
    wxDouble l_radius = sqrt(pow((y2 - yc), 2.0) + pow((x2 - xc), 2.0));
    gpath.MoveToPoint(x1, y1);
    gpath.AddArc(xc, yc, l_radius, l_dFirstAngle, l_dSecondAngle, true);
    pgc->DrawPath(gpath);

    dc->CalcBoundingBox(x1, y1);
    dc->CalcBoundingBox(x2, y2);
  } else
#endif
    DrawArc(xc, yc, x1, y1, x2, y2, true);
}

void piDC::StrokeSector(wxCoord xc, wxCoord yc, wxCoord x1, wxCoord y1,
                        wxCoord x2, wxCoord y2, wxCoord x3, wxCoord y3,
                        wxCoord x4, wxCoord y4) {
#if wxUSE_GRAPHICS_CONTEXT
  if (pgc) {
    pgc->SetPen(dc->GetPen());
    pgc->SetBrush(dc->GetBrush());

    double y1yc, x1xc, y3yc, x3xc;
    y1yc = y1 - yc;
    x1xc = x1 - xc;
    y3yc = y3 - yc;
    x3xc = x3 - xc;
    wxGraphicsPath gpath = pgc->CreatePath();
    wxDouble l_dFirstAngle;
    l_dFirstAngle = atan2(y1yc, x1xc);

    wxDouble l_dSecondAngle;
    l_dSecondAngle = atan2(y3yc, x3xc);

    wxDouble l_radius = sqrt(pow((y2 - yc), 2.0) + pow((x2 - xc), 2.0));
    gpath.MoveToPoint(x1, y1);
    gpath.AddLineToPoint(x2, y2);
    gpath.AddArc(xc, yc, l_radius, l_dFirstAngle, l_dSecondAngle, true);
    gpath.MoveToPoint(x3, y3);
    gpath.AddLineToPoint(x4, y4);
    l_radius = sqrt(pow((y1 - yc), 2.0) + pow((x1 - xc), 2.0));
    gpath.AddArc(xc, yc, l_radius, l_dSecondAngle, l_dFirstAngle, false);
    pgc->StrokePath(gpath);
    pgc->FillPath(gpath);

    dc->CalcBoundingBox(x1, y1);
    dc->CalcBoundingBox(x3, y3);
  } else
#endif
    DrawSector(xc, yc, x1, y1, x2, y2, x3, y3, x4, y4);
}

void piDC::DrawRectangle(wxCoord x, wxCoord y, wxCoord w, wxCoord h) {
  if (dc) dc->DrawRectangle(x, y, w, h);
#ifdef ocpnUSE_GL
  else {
#ifndef USE_ANDROID_GLES2
    if (ConfigureBrush()) {
      glBegin(GL_QUADS);
      glVertex2i(x, y);
      glVertex2i(x + w, y);
      glVertex2i(x + w, y + h);
      glVertex2i(x, y + h);
      glEnd();
    }

    if (ConfigurePen()) {
      glBegin(GL_LINE_LOOP);
      glVertex2i(x, y);
      glVertex2i(x + w, y);
      glVertex2i(x + w, y + h);
      glVertex2i(x, y + h);
      glEnd();
    }
#else
    DrawRoundedRectangle(x, y, w, h, 0);
#endif
  }
#endif  // ocpnUSE_GL
}

/* draw the arc along corners */
static void drawrrhelper(wxCoord x0, wxCoord y0, wxCoord r, int quadrant,
                         int steps) {
#ifndef USE_ANDROID_GLES2
#ifdef ocpnUSE_GL
  float step = 1.0 / steps, rs = 2.0 * r * step, rss = rs * step, x, y, dx, dy,
        ddx, ddy;
  switch (quadrant) {
    case 0:
      x = r, y = 0, dx = 0, dy = -rs, ddx = -rss, ddy = rss;
      break;
    case 1:
      x = 0, y = -r, dx = -rs, dy = 0, ddx = rss, ddy = rss;
      break;
    case 2:
      x = -r, y = 0, dx = 0, dy = rs, ddx = rss, ddy = -rss;
      break;
    case 3:
      x = 0, y = r, dx = rs, dy = 0, ddx = -rss, ddy = -rss;
      break;
    default:
      return;  // avoid unitialized compiler warnings
  }

  for (int i = 0; i < steps; i++) {
    glVertex2i(x0 + floor(x), y0 + floor(y));
    x += dx + ddx / 2, y += dy + ddy / 2;
    dx += ddx, dy += ddy;
  }
  glVertex2i(x0 + floor(x), y0 + floor(y));
#endif  // ocpnUSE_GL
#endif
}

void piDC::drawrrhelperGLES2(wxCoord x0, wxCoord y0, wxCoord r, int quadrant,
                             int steps) {
#ifdef ocpnUSE_GL
  float step = 1.0 / steps, rs = 2.0 * r * step, rss = rs * step, x, y, dx, dy,
        ddx, ddy;
  switch (quadrant) {
    case 0:
      x = r, y = 0, dx = 0, dy = -rs, ddx = -rss, ddy = rss;
      break;
    case 1:
      x = 0, y = -r, dx = -rs, dy = 0, ddx = rss, ddy = rss;
      break;
    case 2:
      x = -r, y = 0, dx = 0, dy = rs, ddx = rss, ddy = -rss;
      break;
    case 3:
      x = 0, y = r, dx = rs, dy = 0, ddx = -rss, ddy = -rss;
      break;
    default:
      return;  // avoid unitialized compiler warnings
  }

  for (int i = 0; i < steps; i++) {
    workBuf[workBufIndex++] = x0 + floor(x);
    workBuf[workBufIndex++] = y0 + floor(y);

    x += dx + ddx / 2, y += dy + ddy / 2;
    dx += ddx, dy += ddy;
  }

  workBuf[workBufIndex++] = x0 + floor(x);
  workBuf[workBufIndex++] = y0 + floor(y);
#endif  // ocpnUSE_GL
}
void piDC::DrawRoundedRectangle(wxCoord x, wxCoord y, wxCoord w, wxCoord h,
                                wxCoord r) {
  if (dc) dc->DrawRoundedRectangle(x, y, w, h, r);
#ifdef ocpnUSE_GL
  else {
    r++;
    int steps = ceil(sqrt((float)r));

    wxCoord x1 = x + r, x2 = x + w - r;
    wxCoord y1 = y + r, y2 = y + h - r;
#ifdef USE_ANDROID_GLES2
    //  Grow the work buffer as necessary
    size_t bufReq = steps * 8 * 2 * sizeof(float);  // large, to be sure

    if (workBufSize < bufReq) {
      workBuf = (float *)realloc(workBuf, bufReq);
      workBufSize = bufReq;
    }
    workBufIndex = 0;

    drawrrhelperGLES2(x2, y1, r, 0, steps);
    drawrrhelperGLES2(x1, y1, r, 1, steps);
    drawrrhelperGLES2(x1, y2, r, 2, steps);
    drawrrhelperGLES2(x2, y2, r, 3, steps);

    GLint program = pi_color_tri_shader_program;
    checkGlError("Before glUseProgram", "piDC", __LINE__);
    glUseProgram(program);
    checkGlError("After glUseProgram", "piDC", __LINE__);

    // Get pointers to the attributes in the program.
    GLint mPosAttrib = glGetAttribLocation(program, "position");

    // Disable VBO's (vertex buffer objects) for attributes.
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glVertexAttribPointer(mPosAttrib, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                          workBuf);
    glEnableVertexAttribArray(mPosAttrib);

    //  Border color
    float bcolorv[4];
    bcolorv[0] = m_brush.GetColour().Red() / float(256);
    bcolorv[1] = m_brush.GetColour().Green() / float(256);
    bcolorv[2] = m_brush.GetColour().Blue() / float(256);
    bcolorv[3] = m_brush.GetColour().Alpha() / float(256);

    GLint bcolloc = glGetUniformLocation(program, "color");
    glUniform4fv(bcolloc, 1, bcolorv);

    float angle = 0.;
    float xoffset = 0;
    float yoffset = 0;

    // Rotate
    mat4x4 I, Q;
    mat4x4_identity(I);
    mat4x4_rotate_Z(Q, I, angle);

    // Translate
    Q[3][0] = xoffset;
    Q[3][1] = yoffset;

    GLint matloc = glGetUniformLocation(program, "TransformMatrix");
    glUniformMatrix4fv(matloc, 1, GL_FALSE, (const GLfloat *)Q);

    // Perform the actual drawing.
    glDrawArrays(GL_TRIANGLE_FAN, 0, workBufIndex / 2);

    // Restore the per-object transform to Identity Matrix
    mat4x4 IM;
    mat4x4_identity(IM);
    GLint matlocf = glGetUniformLocation(program, "TransformMatrix");
    glUniformMatrix4fv(matlocf, 1, GL_FALSE, (const GLfloat *)IM);
    glUseProgram(0);

#else

    if (ConfigureBrush()) {
      glBegin(GL_TRIANGLE_FAN);
      drawrrhelper(x2, y1, r, 0, steps);
      drawrrhelper(x1, y1, r, 1, steps);
      drawrrhelper(x1, y2, r, 2, steps);
      drawrrhelper(x2, y2, r, 3, steps);
      glEnd();
    }

    if (ConfigurePen()) {
      glBegin(GL_LINE_LOOP);
      drawrrhelper(x2, y1, r, 0, steps);
      drawrrhelper(x1, y1, r, 1, steps);
      drawrrhelper(x1, y2, r, 2, steps);
      drawrrhelper(x2, y2, r, 3, steps);
      glEnd();
    }
#endif
  }
#endif  // ocpnUSE_GL
}

void piDC::DrawCircle(wxCoord x, wxCoord y, wxCoord radius) {
#ifdef USE_ANDROID_GLES2

  //      Enable anti-aliased lines, at best quality
  glEnable(GL_BLEND);

  float coords[8];
  coords[0] = x - radius;
  coords[1] = y + radius;
  coords[2] = x + radius;
  coords[3] = y + radius;
  coords[4] = x - radius;
  coords[5] = y - radius;
  coords[6] = x + radius;
  coords[7] = y - radius;

  GLint program = pi_circle_filled_shader_program;
  checkGlError("Before glUseProgram", "piDC", __LINE__);
  glUseProgram(program);
  checkGlError("After glUseProgram", "piDC", __LINE__);

  // Get pointers to the attributes in the program.
  GLint mPosAttrib = glGetAttribLocation(program, "aPos");

  // Disable VBO's (vertex buffer objects) for attributes.
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  glVertexAttribPointer(mPosAttrib, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                        coords);
  glEnableVertexAttribArray(mPosAttrib);

  //  Circle radius
  GLint radiusloc = glGetUniformLocation(program, "circle_radius");
  glUniform1f(radiusloc, radius);

  //  Circle center point
  GLint centerloc = glGetUniformLocation(program, "circle_center");
  float ctrv[2];
  ctrv[0] = x;
  ctrv[1] = GetCanvasByIndex(0)->GetSize().y - y;
  glUniform2fv(centerloc, 1, ctrv);

  //  Circle color
  float colorv[4];
  colorv[0] = m_brush.GetColour().Red() / float(256);
  colorv[1] = m_brush.GetColour().Green() / float(256);
  colorv[2] = m_brush.GetColour().Blue() / float(256);
  colorv[3] = (m_brush.GetStyle() == wxBRUSHSTYLE_TRANSPARENT) ? 0.0 : 1.0;

  GLint colloc = glGetUniformLocation(program, "circle_color");
  glUniform4fv(colloc, 1, colorv);

  //  Border color
  float bcolorv[4];
  bcolorv[0] = m_pen.GetColour().Red() / float(256);
  bcolorv[1] = m_pen.GetColour().Green() / float(256);
  bcolorv[2] = m_pen.GetColour().Blue() / float(256);
  bcolorv[3] = m_pen.GetColour().Alpha() / float(256);

  GLint bcolloc = glGetUniformLocation(program, "border_color");
  glUniform4fv(bcolloc, 1, bcolorv);

  //  Border Width
  GLint borderWidthloc = glGetUniformLocation(program, "border_width");
  glUniform1f(borderWidthloc, m_pen.GetWidth());

  // Perform the actual drawing.
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  //      Enable anti-aliased lines, at best quality
  glDisable(GL_BLEND);
  glUseProgram(0);

#else
  DrawEllipse(x - radius, y - radius, 2 * radius, 2 * radius);
#endif
}

void piDC::DrawDisk(wxCoord x, wxCoord y, wxCoord innerRadius,
                    wxCoord outerRadius) {
  if (dc) {
#if wxUSE_GRAPHICS_CONTEXT
    wxGraphicsContext *wxGC = NULL;
    wxMemoryDC *pmdc = wxDynamicCast(GetDC(), wxMemoryDC);
    if (pmdc)
      wxGC = wxGraphicsContext::Create(*pmdc);
    else {
      wxClientDC *pcdc = wxDynamicCast(GetDC(), wxClientDC);
      if (pcdc) wxGC = wxGraphicsContext::Create(*pcdc);
    }
    if (wxGC) {
      wxGC->SetPen(dc->GetPen());
      wxGC->SetBrush(dc->GetBrush());
      wxGraphicsPath p = wxGC->CreatePath();
      p.AddCircle(x, y, innerRadius);
      p.AddCircle(x, y, outerRadius);
      wxGC->FillPath(p);
    }
#else
    wxDC *wxDC = GetDC();
    float innerSteps = floorf(
        wxMax(sqrtf(sqrtf(((innerRadius * 2) * (innerRadius * 2)) * 2)), 1) *
        M_PI);
    float outerSteps = floorf(
        wxMax(sqrtf(sqrtf(((outerRadius * 2) * (outerRadius * 2)) * 2)), 1) *
        M_PI);
    wxPoint *disk = new wxPoint[(int)innerSteps + (int)outerSteps + 2];
    float a = 0.;
    for (int i = 0; i < (int)innerSteps; i++) {
      disk[i].x = x + innerRadius * sinf(a);
      disk[i].y = y + innerRadius * cosf(a);
      a += 2 * M_PI / innerSteps;
    }
    // a = 0;
    for (int i = 0; i < (int)outerSteps; i++) {
      disk[i + (int)innerSteps].x = x + outerRadius * sinf(a);
      disk[i + (int)innerSteps].y = y + outerRadius * cosf(a);
      a -= 2 * M_PI / outerSteps;
    }
    int npoints[2];
    npoints[0] = (int)innerSteps;
    npoints[1] = (int)outerSteps;
    wxDC->DrawPolyPolygon(2, npoints, disk, 0, 0);
    delete[] disk;
#endif
  }
#ifdef ocpnUSE_GL
  else {

    if (g_textureId >= 0) {
      DrawDiskPattern(x, y, innerRadius, outerRadius, g_textureId,
                      wxSize(g_iTextureWidth, g_iTextureHeight));
      return;
    }

    float innerSteps = floorf(
        wxMax(sqrtf(sqrtf(((innerRadius * 2) * (innerRadius * 2)) * 2)), 1) *
        M_PI);
    float outerSteps = floorf(
        wxMax(sqrtf(sqrtf(((outerRadius * 2) * (outerRadius * 2)) * 2)), 1) *
        M_PI);
    wxPoint *disk = new wxPoint[(int)innerSteps + (int)outerSteps + 2];
    float a = 0.;
    for (int i = 0; i < (int)innerSteps; i++) {
      disk[i].x = x + innerRadius * sinf(a);
      disk[i].y = y + innerRadius * cosf(a);
      a += 2 * M_PI / innerSteps;
    }
    // a = 0;
    for (int i = 0; i < (int)outerSteps; i++) {
      disk[i + (int)innerSteps].x = x + outerRadius * sinf(a);
      disk[i + (int)innerSteps].y = y + outerRadius * cosf(a);
      a -= 2 * M_PI / outerSteps;
    }
    int npoints[2];
    npoints[0] = (int)innerSteps;
    npoints[1] = (int)outerSteps;
    DrawPolygonsTessellated(2, npoints, disk, 0, 0);
    delete[] disk;
  }
#endif  // ocpnUSE_GL
}

void piDC::DrawDiskPattern(wxCoord x, wxCoord y, wxCoord innerRadius,
                           wxCoord outerRadius, GLint textureID,
                           wxSize textureSize) {
  if (dc) {
    DrawDisk(x, y, innerRadius, outerRadius);
  }
#ifdef ocpnUSE_GL
  else {
    // float steps = floorf(wxMax(sqrtf(sqrtf((float)(width*width +
    // height*height))), 1) * M_PI);
    float innerSteps = floorf(
        wxMax(sqrtf(sqrtf(((innerRadius * 2) * (innerRadius * 2)) * 2)), 1) *
        M_PI);
    float outerSteps = floorf(
        wxMax(sqrtf(sqrtf(((outerRadius * 2) * (outerRadius * 2)) * 2)), 1) *
        M_PI);
    wxPoint *disk = new wxPoint[(int)innerSteps + (int)outerSteps + 2];
    float a = 0.;
    for (int i = 0; i < (int)innerSteps; i++) {
      disk[i].x = x + innerRadius * sinf(a);
      disk[i].y = y + innerRadius * cosf(a);
      a += 2 * M_PI / innerSteps;
    }
    // a = 0;
    for (int i = 0; i < (int)outerSteps; i++) {
      disk[i + (int)innerSteps].x = x + outerRadius * sinf(a);
      disk[i + (int)innerSteps].y = y + outerRadius * cosf(a);
      a -= 2 * M_PI / outerSteps;
    }
    int npoints[2];
    npoints[0] = (int)innerSteps;
    npoints[1] = (int)outerSteps;
    DrawPolygonsPattern(2, npoints, disk, textureID, textureSize, 0, 0);
    delete[] disk;
  }
#endif  // ocpnUSE_GL
}

void piDC::StrokeCircle(wxCoord x, wxCoord y, wxCoord radius) {
#if wxUSE_GRAPHICS_CONTEXT
  if (pgc) {
    wxGraphicsPath gpath = pgc->CreatePath();
    gpath.AddCircle(x, y, radius);

    pgc->SetPen(GetPen());
    pgc->SetBrush(GetBrush());
    pgc->DrawPath(gpath);

    // keep dc dirty box up-to-date
    dc->CalcBoundingBox(x + radius + 2, y + radius + 2);
    dc->CalcBoundingBox(x - radius - 2, y - radius - 2);
  } else
#endif
    DrawCircle(x, y, radius);
}

void piDC::DrawEllipse(wxCoord x, wxCoord y, wxCoord width, wxCoord height) {
  if (dc) dc->DrawEllipse(x, y, width, height);
#ifdef ocpnUSE_GL
  else {
    float r1 = width / 2, r2 = height / 2;
    float cx = x + r1, cy = y + r2;

    //      Enable anti-aliased lines, at best quality
    glEnable(GL_BLEND);

    /* formula for variable step count to produce smooth ellipse */
    float steps = floorf(
        wxMax(sqrtf(sqrtf((float)(width * width + height * height))), 1) *
        M_PI);
#ifndef USE_ANDROID_GLES2
    if (ConfigureBrush()) {
      glBegin(GL_TRIANGLE_FAN);
      glVertex2f(cx, cy);
      for (float a = 0; a <= 2 * M_PI + M_PI / steps; a += 2 * M_PI / steps)
        glVertex2f(cx + r1 * sinf(a), cy + r2 * cosf(a));
      glEnd();
    }

    if (ConfigurePen()) {
      glBegin(GL_LINE_STRIP);
      float a;
      for (a = 0; a < 2 * M_PI - M_PI / steps; a += 2 * M_PI / steps)
        glVertex2f(cx + r1 * sinf(a), cy + r2 * cosf(a));
      glVertex2f(
          cx + r1 * sinf(a),
          cy + r2 * cosf(a));  // added to ensure line is closed and no holes
                               // are shown when not using solid line
      glEnd();
    }
#else
#endif
    glDisable(GL_BLEND);
  }
#endif  // ocpnUSE_GL
}

void piDC::DrawPolygon(int n, wxPoint points[], wxCoord xoffset,
                       wxCoord yoffset, float scale, float angle) {
  if (dc) dc->DrawPolygon(n, points, xoffset, yoffset);
#ifdef ocpnUSE_GL
  else {
#ifdef __WXQT__
    SetGLAttrs(false);  // Some QT platforms (Android) have trouble with
                        // GL_BLEND / GL_LINE_SMOOTH
#else
    SetGLAttrs(true);
#endif

    if (g_textureId >= 0) {
      DrawPolygonPattern(n, points, g_textureId,
                         wxSize(g_iTextureWidth, g_iTextureHeight), xoffset,
                         yoffset);
      return;
    }

#ifdef USE_ANDROID_GLES2

    ConfigurePen();

    glEnable(GL_BLEND);
    if (n > 4) {
      if (ConfigureBrush()) {  // Check for transparent brush
        DrawPolygonTessellated(n, points, xoffset, yoffset);
      }

      if (ConfigurePen() && m_pen.GetWidth() != 0) {
      // Draw the ouline
      //  Grow the work buffer as necessary
      if (workBufSize < (size_t)n * 2) {
        workBuf = (float *)realloc(workBuf, (n * 4) * sizeof(float));
        workBufSize = n * 4;
      }

      for (int i = 0; i < n; i++) {
        workBuf[i * 2] = (points[i].x * scale);      // + xoffset;
        workBuf[i * 2 + 1] = (points[i].y * scale);  // + yoffset;
      }

      GLint program = pi_color_tri_shader_program;
      checkGlError("Before glUseProgram", "piDC", __LINE__);
      glUseProgram(program);
      checkGlError("After glUseProgram", "piDC", __LINE__);
      checkGlError("glUseProgram", "piDC", __LINE__);

      // Get pointers to the attributes in the program.
      GLint mPosAttrib = glGetAttribLocation(program, "position");
      checkGlError("aPos", "piDC", __LINE__);

      // Disable VBO's (vertex buffer objects) for attributes.
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

      glVertexAttribPointer(mPosAttrib, 2, GL_FLOAT, GL_FALSE,
                            2 * sizeof(float), workBuf);
      glEnableVertexAttribArray(mPosAttrib);

      //  Border color
      float bcolorv[4];
      bcolorv[0] = m_pen.GetColour().Red() / float(256);
      bcolorv[1] = m_pen.GetColour().Green() / float(256);
      bcolorv[2] = m_pen.GetColour().Blue() / float(256);
      bcolorv[3] = m_pen.GetColour().Alpha() / float(256);

      GLint bcolloc = glGetUniformLocation(program, "color");
      checkGlError("uColour", "piDC", __LINE__);
      glUniform4fv(bcolloc, 1, bcolorv);
      if (bcolloc == -1) wxLogMessage(_("piDC::DrawPolygon: bcolloc -1"));

#if 0
            // Rotate
            mat4x4 I, Q;
            mat4x4_identity(I);
            mat4x4_rotate_Z(Q, I, angle);

            // Translate
            Q[3][0] = xoffset;
            Q[3][1] = yoffset;

            mat4x4 X;
            mat4x4_mul(X, (float (*)[4])gFrame->GetPrimaryCanvas()->GetpVP()->vp_transform, Q);

            GLint matloc = glGetUniformLocation(program,"MVMatrix");
            glUniformMatrix4fv( matloc, 1, GL_FALSE, (const GLfloat*)X );
#endif

      // Perform the actual drawing.
      glDrawArrays(GL_LINE_LOOP, 0, n);

      glUseProgram(0);
      }
    } else {  // n = 3 or 4, most common case for pre-tesselated shapes
#if 0

            //  Grow the work buffer as necessary
            if( workBufSize < (size_t)n*2 ){
                workBuf = (float *)realloc(workBuf, (n*4) * sizeof(float));
                workBufSize = n*4;
            }

            for( int i = 0; i < n; i++ ){
                workBuf[i*2] = (points[i].x * scale); // + xoffset;
                workBuf[i*2 + 1] = (points[i].y * scale); // + yoffset;
            }

            GLint program = pi_texture_2D_shader_program;
            glUseProgram( program );

            // Get pointers to the attributes in the program.
            GLint mPosAttrib = glGetAttribLocation( program, "aPos" );

            // Disable VBO's (vertex buffer objects) for attributes.
            glBindBuffer( GL_ARRAY_BUFFER, 0 );
            glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );

            glVertexAttribPointer( mPosAttrib, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), workBuf );
            glEnableVertexAttribArray( mPosAttrib );


            //  Border color
            float bcolorv[4];
            bcolorv[0] = m_pen.GetColour().Red() / float(256);
            bcolorv[1] = m_pen.GetColour().Green() / float(256);
            bcolorv[2] = m_pen.GetColour().Blue() / float(256);
            bcolorv[3] = m_pen.GetColour().Alpha() / float(256);

            GLint bcolloc = glGetUniformLocation(program, "uColour");
            glUniform4fv(bcolloc, 1, bcolorv);

#if 0
            // Rotate
            mat4x4 I, Q;
            mat4x4_identity(I);
            mat4x4_rotate_Z(Q, I, angle);

            // Translate
            Q[3][0] = xoffset;
            Q[3][1] = yoffset;

            mat4x4 X;
            mat4x4_mul(X, (float (*)[4])gFrame->GetPrimaryCanvas()->GetpVP()->vp_transform, Q);
            GLint matloc = glGetUniformLocation(program, "MVMatrix");
            glUniformMatrix4fv( matloc, 1, GL_FALSE, (const GLfloat*)X );
#endif
            // Perform the actual drawing.
            glDrawArrays(GL_LINE_LOOP, 0, n);

            //  Fill color
            bcolorv[0] = m_brush.GetColour().Red() / float(256);
            bcolorv[1] = m_brush.GetColour().Green() / float(256);
            bcolorv[2] = m_brush.GetColour().Blue() / float(256);
            bcolorv[3] = m_brush.GetColour().Alpha() / float(256);

            glUniform4fv(bcolloc, 1, bcolorv);

            // For the simple common case of a convex rectangle...
            //  swizzle the array points to enable GL_TRIANGLE_STRIP
            if(n == 4){
                float x1 = workBuf[4];
                float y1 = workBuf[5];
                workBuf[4] = workBuf[6];
                workBuf[5] = workBuf[7];
                workBuf[6] = x1;
                workBuf[7] = y1;

                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            }
            else if(n == 3){
                glDrawArrays(GL_TRIANGLES, 0, 3);
            }

            glUseProgram( 0 );
#endif
      GLint program = pi_color_tri_shader_program;
      glUseProgram(program);

      // Get pointers to the attributes in the program.
      GLint mPosAttrib = glGetAttribLocation(program, "position");
      checkGlError("mPosAttrib", "piDC", __LINE__);

      // Disable VBO's (vertex buffer objects) for attributes.
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

      glVertexAttribPointer(mPosAttrib, 2, GL_FLOAT, GL_FALSE,
                            2 * sizeof(float), workBuf);
      glEnableVertexAttribArray(mPosAttrib);

      //  Border color
      float bcolorv[4];
      bcolorv[0] = m_pen.GetColour().Red() / float(256);
      bcolorv[1] = m_pen.GetColour().Green() / float(256);
      bcolorv[2] = m_pen.GetColour().Blue() / float(256);
      bcolorv[3] = m_pen.GetColour().Alpha() / float(256);

      GLint bcolloc = glGetUniformLocation(program, "color");
      checkGlError("uColour", "piDC", __LINE__);
      glUniform4fv(bcolloc, 1, bcolorv);

      // Rotate
      mat4x4 I, Q;
      mat4x4_identity(I);
      mat4x4_rotate_Z(Q, I, angle);

      // Translate
      Q[3][0] = xoffset;
      Q[3][1] = yoffset;

      GLint matloc = glGetUniformLocation(program, "TransformMatrix");
      checkGlError("TransformMatrix", "piDC", __LINE__);
      glUniformMatrix4fv(matloc, 1, GL_FALSE, (const GLfloat *)Q);

      // Perform the actual drawing.
      glDrawArrays(GL_LINE_LOOP, 0, n);

      //  Fill color
      bcolorv[0] = m_brush.GetColour().Red() / float(256);
      bcolorv[1] = m_brush.GetColour().Green() / float(256);
      bcolorv[2] = m_brush.GetColour().Blue() / float(256);
      bcolorv[3] = m_brush.GetColour().Alpha() / float(256);

      glEnableVertexAttribArray(bcolloc);
      glVertexAttribPointer(bcolloc, 4, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                            bcolorv);
      glUniform4fv(bcolloc, 1, bcolorv);

      // For the simple common case of a convex rectangle...
      //  swizzle the array points to enable GL_TRIANGLE_STRIP
      if (n == 4) {
        if( workBufSize < (size_t)8 ){
            workBuf = (float *)realloc(workBuf, 8 * sizeof(float));
            workBufSize = 8;
        }
        float x1 = workBuf[4];
        float y1 = workBuf[5];
        workBuf[4] = workBuf[6];
        workBuf[5] = workBuf[7];
        workBuf[6] = x1;
        workBuf[7] = y1;

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      } else if (n == 3) {
        glDrawArrays(GL_TRIANGLES, 0, 3);
      }

      // Restore the per-object transform to Identity Matrix
      mat4x4 IM;
      mat4x4_identity(IM);
      GLint matlocf = glGetUniformLocation(program, "TransformMatrix");
      glUniformMatrix4fv(matlocf, 1, GL_FALSE, (const GLfloat *)IM);

      glUseProgram(0);
    }

#else

        glPushMatrix();
        glTranslatef(xoffset, yoffset, 0);

        float deg = 180/PI * ( angle  );
        glRotatef(deg, 0, 0, 1);


        if( ConfigureBrush() ) {
            glBegin( GL_POLYGON );
            for( int i = 0; i < n; i++ )
                glVertex2f( (points[i].x * scale) + xoffset, (points[i].y * scale) + yoffset );
            glEnd();
        }


        if( ConfigurePen() && m_pen.GetWidth() != 0) {
//            if( g_GLOptions.m_GLLineSmoothing )
            glEnable( GL_LINE_SMOOTH );
            glBegin( GL_LINE_LOOP );
            for( int i = 0; i < n; i++ )
                glVertex2f( (points[i].x * scale), (points[i].y * scale) );
            glEnd();
            glDisable( GL_LINE_SMOOTH );
        }

        glPopMatrix();

#endif

    SetGLAttrs(false);
  }
#endif  // ocpnUSE_GL
}

void piDC::DrawPolygonPattern(int n, wxPoint points[], int textureID,
                              wxSize textureSize, wxCoord xoffset,
                              wxCoord yoffset, float scale, float angle) {
  if (dc) dc->DrawPolygon(n, points, xoffset, yoffset);
#ifdef ocpnUSE_GL
  else {
#ifdef __WXQT__
    SetGLAttrs(false);  // Some QT platforms (Android) have trouble with
                        // GL_BLEND / GL_LINE_SMOOTH
#else
    SetGLAttrs(true);
#endif

    ConfigurePen();
    glEnable(GL_BLEND);

    if (n > 3) {
      if (ConfigureBrush()) {  // Check for transparent brush
        DrawPolygonTessellatedPattern(n, points, textureID, textureSize,
                                      xoffset, yoffset);
      }

    }
#ifdef USE_ANDROID_GLES2
    else {  // n = 3 or 4, most common case for pre-tesselated shapes
      //  Grow the work buffer as necessary
      if (workBufSize < (size_t)n * 2) {
        workBuf = (float *)realloc(workBuf, (n * 2) * sizeof(float));
        workBufSize = (size_t)n * 2;
      }
      for (int i = 0; i < n; i++) {
        if (i == 0) {
          g_min_x = g_max_x = points[i].x;
          g_min_y = g_max_y = points[i].y;
        }

        workBuf[i * 2] = (points[i].x * scale);      // + xoffset;
        workBuf[i * 2 + 1] = (points[i].y * scale);  // + yoffset;
      }

      checkGlError("Before glUseProgram", "piDC", __LINE__);
      GLint program = pi_texture_2DA_shader_program;
      glUseProgram(program);
      checkGlError("glUseProgram", "piDC", __LINE__);

      // Get pointers to the attributes in the program.
      // Position of vertex(s)
      GLint mPosAttrib = glGetAttribLocation(program, "aPos");
      checkGlError("mPosAttrib", "piDC", __LINE__);
      // Texture coordinates
      GLint mUvAttrib = glGetAttribLocation(program, "aUV");
      checkGlError("mUvAttrib", "piDC", __LINE__);

      // Rotate
      GLint uRotateMatrix = glGetUniformLocation(program, "MVMatrix");
      checkGlError("MVMatrix", "piDC", __LINE__);

      // Transform
      GLint uTranslateMatrix = glGetUniformLocation(program, "TransformMatrix");
      checkGlError("TransformMatrix", "piDC", __LINE__);

      // Set up the texture sampler to texture unit 0
      GLint texUni = glGetUniformLocation(program, "uTex");
      glUniform1i(texUni, 0);
      checkGlError("texUni", "piDC", __LINE__);

      // Disable VBO's (vertex buffer objects) for attributes.
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

      glEnableVertexAttribArray(mPosAttrib);
      checkGlError("glEnableVertexAttribArray:mPosAttrib", "piDC", __LINE__);
      glVertexAttribPointer(mPosAttrib, 2, GL_FLOAT, GL_FALSE,
                            2 * sizeof(GLfloat), workBuf);
      checkGlError("glVertexAttribPointer:mPosAttrib", "piDC", __LINE__);

      //  Pattern color
      float bcolorv[4];
      bcolorv[0] = m_brush.GetColour().Red() / float(256);
      bcolorv[1] = m_brush.GetColour().Green() / float(256);
      bcolorv[2] = m_brush.GetColour().Blue() / float(256);
      bcolorv[3] = m_brush.GetColour().Alpha() / float(256);

      GLint bcolloc = glGetUniformLocation(program, "color");
      checkGlError("bcolloc", "piDC", __LINE__);
      glUniform4fv(bcolloc, 1, bcolorv);
      checkGlError("glUniform4fv(bcolloc", "piDC", __LINE__);

      // Only a triangle can be convex all the time
      if (n == 3) {
        GLfloat UVCoords[] = {
            GLfloat((workBuf[0] - g_min_x) / g_iTextureWidth / 2),
            GLfloat((workBuf[1] - g_min_y) / g_iTextureHeight / 2),
            GLfloat((workBuf[2] - g_min_x) / g_iTextureWidth / 2),
            GLfloat((workBuf[3] - g_min_y) / g_iTextureHeight / 2),
            GLfloat((workBuf[4] - g_min_x) / g_iTextureWidth / 2),
            GLfloat((workBuf[5] - g_min_y) / g_iTextureHeight / 2)};
        glEnableVertexAttribArray(mUvAttrib);
        checkGlError("glEnableVertexAttribArray( mUvAttrib )", "piDC",
                     __LINE__);
        glVertexAttribPointer(mUvAttrib, 2, GL_FLOAT, GL_FALSE,
                              2 * sizeof(GLfloat), UVCoords);
        checkGlError("glVertexAttribPointer", "piDC", __LINE__);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        checkGlError("glDrawArrays(GL_TRIANGLES)", "piDC", __LINE__);
      }
      glUseProgram(0);
      checkGlError("glUseProgram(0)", "piDC", __LINE__);
    }
#else  // USE_ANDROID_GLES2

    {
      if (ConfigureBrush())  // Check for transparent brush
        DrawPolygonTessellatedPattern(n, points, textureID, textureSize,
                                      xoffset, yoffset);
    }
#endif

    SetGLAttrs(false);
  }
#endif  // ocpnUSE_GL
}

#ifdef ocpnUSE_GL

// GL callbacks

typedef union {
  GLdouble data[6];
  struct sGLvertex {
    GLdouble x;
    GLdouble y;
    GLdouble z;
    GLdouble r;
    GLdouble g;
    GLdouble b;
    GLdouble a;
  } info;
} GLvertex;

#ifndef USE_ANDROID_GLES2

#ifdef __WXMAC__
#ifndef APIENTRY
#define APIENTRY
#endif
#endif

void APIENTRY piDCcombineCallback(GLdouble coords[3], GLdouble *vertex_data[4],
                                  GLfloat weight[4], GLdouble **dataOut) {
  GLvertex *vertex;

  vertex = new GLvertex();
  pi_gTesselatorVertices.Add(vertex);

  vertex->info.x = coords[0];
  vertex->info.y = coords[1];
  vertex->info.z = coords[2];

  for (int i = 3; i < 6; i++) {
    vertex->data[i] =
        weight[0] * vertex_data[0][i] + weight[1] * vertex_data[1][i];
  }

  *dataOut = &(vertex->data[0]);
}

void APIENTRY piDCvertexCallback(GLvoid *arg) {
  GLvertex *vertex;
  vertex = (GLvertex *)arg;
  glVertex2f((float)vertex->info.x, (float)vertex->info.y);
}

void APIENTRY piDCerrorCallback(GLenum errorCode) {}

void APIENTRY piDCbeginCallback(GLenum type) { glBegin(type); }

void APIENTRY piDCendCallback() { glEnd(); }
#endif

// GLSL callbacks

#ifdef USE_ANDROID_GLES2

static std::list<double *> odc_combine_work_data;
static void odc_combineCallbackD(GLdouble coords[3], GLdouble *vertex_data[4],
                                 GLfloat weight[4], GLdouble **dataOut,
                                 void *data) {
  //     double *vertex = new double[3];
  //     odc_combine_work_data.push_back(vertex);
  //     memcpy(vertex, coords, 3*(sizeof *coords));
  //     *dataOut = vertex;
  GLvertex *vertex;

  vertex = new GLvertex();
  pi_gTesselatorVertices.Add(vertex);

  vertex->info.x = coords[0];
  vertex->info.y = coords[1];
  vertex->info.z = coords[2];

  for (int i = 3; i < 6; i++) {
    vertex->data[i] =
        weight[0] * vertex_data[0][i] + weight[1] * vertex_data[1][i];
  }

  *dataOut = &(vertex->data[0]);
}

void odc_vertexCallbackD_GLSL(GLvoid *vertex, void *data) {
  piDC *pDC = (piDC *)data;

  // Grow the work buffer if necessary
  if (pDC->s_odc_tess_vertex_idx > pDC->s_odc_tess_buf_len - 8) {
    int new_buf_len = pDC->s_odc_tess_buf_len + 100;
    GLfloat *tmp = pDC->s_odc_tess_work_buf;
    GLfloat *tmp1 = pDC->s_odc_tess_tex_buf;

    pDC->s_odc_tess_work_buf = (GLfloat *)realloc(
        pDC->s_odc_tess_work_buf, new_buf_len * sizeof(GLfloat));
    pDC->s_odc_tess_tex_buf = (GLfloat *)realloc(pDC->s_odc_tess_tex_buf,
                                                 new_buf_len * sizeof(GLfloat));
    if (NULL == pDC->s_odc_tess_work_buf) {
      free(tmp);
      free(tmp1);
      tmp = NULL;
      tmp1 = NULL;
    } else
      pDC->s_odc_tess_buf_len = new_buf_len;
  }

  GLdouble *pointer = (GLdouble *)vertex;

  int i_vertexid = pDC->s_odc_tess_vertex_idx;
  pDC->s_odc_tess_work_buf[pDC->s_odc_tess_vertex_idx++] = (float)pointer[0];
  pDC->s_odc_tess_work_buf[pDC->s_odc_tess_vertex_idx++] = (float)pointer[1];
  pDC->s_odc_tess_tex_buf[i_vertexid++] =
      (float)((pointer[0] - g_min_x) / g_iTextureWidth) / 2;
  pDC->s_odc_tess_tex_buf[i_vertexid] =
      (float)((pointer[1] - g_min_y) / g_iTextureHeight) / 2;

  pDC->s_odc_nvertex++;
}

void odc_beginCallbackD_GLSL(GLenum mode, void *data) {
  piDC *pDC = (piDC *)data;
  pDC->s_odc_tess_vertex_idx_this = pDC->s_odc_tess_vertex_idx;
  pDC->s_odc_tess_mode = mode;
  pDC->s_odc_nvertex = 0;
}

void odc_endCallbackD_GLSL(void *data) {
#if 0
    piDC* pDC = (piDC*)data;
    float *bufPt = &(pDC->s_odc_tess_work_buf[pDC->s_odc_tess_vertex_idx_this]);
    GLint pos = glGetAttribLocation(pDC->s_odc_activeProgram, "aPos");
    glVertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), bufPt);
    glEnableVertexAttribArray(pos);

    glDrawArrays(pDC->s_odc_tess_mode, 0, pDC->s_odc_nvertex);
#else
  piDC *pDC = (piDC *)data;

  GLint program = pDC->s_odc_activeProgram;
  glUseProgram(program);
  checkGlError("glUseProgram", "piDC", __LINE__);

  // Disable VBO's (vertex buffer objects) for attributes.
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  float *bufPt = &(pDC->s_odc_tess_work_buf[pDC->s_odc_tess_vertex_idx_this]);
  GLint pos = glGetAttribLocation(program, "aPos");
  checkGlError("aPos", "piDC", __LINE__);
  glVertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), bufPt);
  glEnableVertexAttribArray(pos);

  //    GLint matloc = glGetUniformLocation(program, "MVMatrix");
  //    glUniformMatrix4fv( matloc, 1, GL_FALSE, (const
  //    GLfloat*)s_tessVP.vp_transform);

  GLint mUvAttrib = glGetAttribLocation(program, "aUV");
  checkGlError("mUvAttrib", "piDC", __LINE__);
  float *bufTex = &(pDC->s_odc_tess_tex_buf[pDC->s_odc_tess_vertex_idx_this]);
  glVertexAttribPointer(mUvAttrib, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat),
                        bufTex);
  checkGlError("glVertexAttribPointer( mUvAttrib)", "piDC", __LINE__);
  /*
      float colorv[4];
      wxColour c = pDC->GetBrush().GetColour();

      colorv[0] = c.Red() / float(256);
      colorv[1] = c.Green() / float(256);
      colorv[2] = c.Blue() / float(256);
      colorv[3] = c.Alpha() / float(256);

      GLint colloc = glGetUniformLocation(program, "uColour");
      glUniform4fv(colloc, 1, colorv);
  */
  glDrawArrays(pDC->s_odc_tess_mode, 0, pDC->s_odc_nvertex);
#endif
}
#endif

#endif  //#ifdef ocpnUSE_GL

void piDC::DrawPolygonTessellated(int n, wxPoint points[], wxCoord xoffset,
                                  wxCoord yoffset) {
  if (dc) dc->DrawPolygon(n, points, xoffset, yoffset);
#ifdef ocpnUSE_GL
  else {
#if !defined(ocpnUSE_GLES) || \
    defined(USE_ANDROID_GLES2)  // tessalator in glues is broken
    if (n < 5)
#endif
    {
      //             DrawPolygon( n, points, xoffset, yoffset );
      //             return;
    }

#ifdef USE_ANDROID_GLES2

    m_tobj = gluNewTess();
    s_odc_tess_vertex_idx = 0;

    gluTessCallback(m_tobj, GLU_TESS_VERTEX_DATA,
                    (_GLUfuncptr)&odc_vertexCallbackD_GLSL);
    gluTessCallback(m_tobj, GLU_TESS_BEGIN_DATA,
                    (_GLUfuncptr)&odc_beginCallbackD_GLSL);
    gluTessCallback(m_tobj, GLU_TESS_END_DATA,
                    (_GLUfuncptr)&odc_endCallbackD_GLSL);
    gluTessCallback(m_tobj, GLU_TESS_COMBINE_DATA,
                    (_GLUfuncptr)&odc_combineCallbackD);
    // s_tessVP = vp;

    gluTessNormal(m_tobj, 0, 0, 1);
    gluTessProperty(m_tobj, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_NONZERO);

    if (ConfigureBrush()) {
      gluTessBeginPolygon(m_tobj, this);
      gluTessBeginContour(m_tobj);

      /*            for( int i = 0; i < n; i++ ) {
                      double *p = new double[6];
                      p[0] = points[i].x, p[1] = points[i].y, p[2] = 0;
                      gluTessVertex(m_tobj, p, p);
                  }
      */
      for (int i = 0; i < n; i++) {
        GLvertex *vertex = new GLvertex();
        pi_gTesselatorVertices.Add(vertex);
        vertex->info.x = (GLdouble)points[i].x;
        vertex->info.y = (GLdouble)points[i].y;
        vertex->info.z = (GLdouble)0.0;
        vertex->info.r = (GLdouble)0.0;
        vertex->info.g = (GLdouble)0.0;
        vertex->info.b = (GLdouble)0.0;
        gluTessVertex(m_tobj, (GLdouble *)vertex, (GLdouble *)vertex);
      }

      gluTessEndContour(m_tobj);
      gluTessEndPolygon(m_tobj);

      GLint program = pi_color_tri_shader_program;
      // GLint program = pi_texture_2D_shader_program;
      glUseProgram(program);

      // Disable VBO's (vertex buffer objects) for attributes.
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

      float *bufPt = &(s_odc_tess_work_buf[s_odc_tess_vertex_idx_this]);
      GLint pos = glGetAttribLocation(program, "position");
      glVertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                            bufPt);
      glEnableVertexAttribArray(pos);

      float colorv[4];
      wxColour c = GetBrush().GetColour();
      colorv[0] = c.Red() / float(256);
      colorv[1] = c.Green() / float(256);
      colorv[2] = c.Blue() / float(256);
      colorv[3] = c.Alpha() / float(256);
      GLint colloc = glGetUniformLocation(program, "color");
      glUniform4fv(colloc, 1, colorv);

      glDrawArrays(s_odc_tess_mode, 0, s_odc_nvertex);
    }

    gluDeleteTess(m_tobj);
    m_tobj = NULL;
    glUseProgram(0);

    for (std::list<double *>::iterator i = odc_combine_work_data.begin();
         i != odc_combine_work_data.end(); ++i)
      delete[] * i;
    odc_combine_work_data.clear();
  }
#else  // USE_ANDROID_GLES2

    m_tobj = gluNewTess();

    gluTessCallback(m_tobj, GLU_TESS_VERTEX, (_GLUfuncptr)&piDCvertexCallback);
    gluTessCallback(m_tobj, GLU_TESS_BEGIN, (_GLUfuncptr)&piDCbeginCallback);
    gluTessCallback(m_tobj, GLU_TESS_END, (_GLUfuncptr)&piDCendCallback);
    gluTessCallback(m_tobj, GLU_TESS_COMBINE,
                    (_GLUfuncptr)&piDCcombineCallback);
    gluTessCallback(m_tobj, GLU_TESS_ERROR, (_GLUfuncptr)&piDCerrorCallback);

    gluTessNormal(m_tobj, 0, 0, 1);
    gluTessProperty(m_tobj, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_NONZERO);

    if (ConfigureBrush()) {
      gluTessBeginPolygon(m_tobj, NULL);
      gluTessBeginContour(m_tobj);

      for (int i = 0; i < n; i++) {
        GLvertex *vertex = new GLvertex();
        pi_gTesselatorVertices.Add(vertex);
        vertex->info.x = (GLdouble)points[i].x;
        vertex->info.y = (GLdouble)points[i].y;
        vertex->info.z = (GLdouble)0.0;
        vertex->info.r = (GLdouble)0.0;
        vertex->info.g = (GLdouble)0.0;
        vertex->info.b = (GLdouble)0.0;
        gluTessVertex(m_tobj, (GLdouble *)vertex, (GLdouble *)vertex);
      }
      gluTessEndContour(m_tobj);
      gluTessEndPolygon(m_tobj);
    }

    for (unsigned int i = 0; i < pi_gTesselatorVertices.Count(); i++)
      delete (GLvertex *)pi_gTesselatorVertices[i];
    pi_gTesselatorVertices.Clear();

    gluDeleteTess(m_tobj);
    m_tobj = NULL;
  }
#endif
#endif  // ocpnUSE_GL
}

#ifdef ocpnUSE_GL
#ifndef __ANDROID__

void __CALL_CONVENTION piDCPatterncombineCallback(GLdouble coords[3],
                                                  GLdouble *vertex_data[4],
                                                  GLfloat weight[4],
                                                  GLdouble **dataOut) {
  GLvertex *vertex;

  vertex = new GLvertex();
  pi_gTesselatorVertices.Add(vertex);

  vertex->info.x = coords[0];
  vertex->info.y = coords[1];
  vertex->info.z = coords[2];

  for (int i = 3; i < 6; i++) {
    vertex->data[i] =
        weight[0] * vertex_data[0][i] + weight[1] * vertex_data[1][i];
  }

  *dataOut = &(vertex->data[0]);
}

void __CALL_CONVENTION piDCPatternvertexCallback(GLvoid *arg) {
  GLvertex *vertex;
  vertex = (GLvertex *)arg;
  glTexCoord2d(vertex->info.x / g_iTextureWidth,
               vertex->info.y / g_iTextureHeight);
  glVertex2d(vertex->info.x, vertex->info.y);
}

void __CALL_CONVENTION piDCPatternerrorCallback(GLenum errorCode) {}

void __CALL_CONVENTION piDCPatternbeginCallback(GLenum type) { glBegin(type); }

void __CALL_CONVENTION piDCPatternendCallback() { glEnd(); }
#endif
#endif  // ocpnUSE_GL

void piDC::DrawPolygonTessellatedPattern(int n, wxPoint points[], int textureID,
                                         wxSize textureSize, wxCoord xoffset,
                                         wxCoord yoffset) {
  if (dc) dc->DrawPolygon(n, points, xoffset, yoffset);
#ifdef ocpnUSE_GL
  else {
    if (n < 3) return;

#ifdef USE_ANDROID_GLES2
      // Pre-configure the GLES program
//
#if 0
        //GLint program = pi_color_tri_shader_program;
        GLint program = pi_texture_2D_shader_program;
        glUseProgram( program );

        // Get pointers to the attributes in the program.
        // Position of vertex(s)
        //GLint mPosAttrib = glGetAttribLocation( program, "aPos" );
        // Texture coordinates
        GLint mUvAttrib  = glGetAttribLocation( program, "aUV" );

        // Set up the texture sampler to texture unit 0
        GLint texUni = glGetUniformLocation( program, "uTexture" );
        glUniform1i( texUni, 0 );

        // Disable VBO's (vertex buffer objects) for attributes.
        glBindBuffer( GL_ARRAY_BUFFER, 0 );
        glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );

//        glVertexAttribPointer( mPosAttrib, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), workBuf );
//        glEnableVertexAttribArray( mPosAttrib );

        GLfloat uvCoords[] = {0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f};
        glVertexAttribPointer( mUvAttrib, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), 0 );
        glEnableVertexAttribArray( mUvAttrib );

        //  Pattern color
        float bcolorv[4];
        bcolorv[0] = m_brush.GetColour().Red() / float(256);
        bcolorv[1] = m_brush.GetColour().Green() / float(256);
        bcolorv[2] = m_brush.GetColour().Blue() / float(256);
        bcolorv[3] = m_brush.GetColour().Alpha() / float(256);
        GLint bcolloc = glGetUniformLocation(program, "uColour");
        glUniform4fv(bcolloc, 1, bcolorv);


//
#else
    // GLint program = pi_colorv_tri_shader_program;
    GLint program = pi_texture_2DA_shader_program;
    s_odc_activeProgram = program;
    glUseProgram(program);
    checkGlError("glUseProgram", "piDC", __LINE__);

    // Disable VBO's (vertex buffer objects) for attributes.
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    checkGlError("glBindBuffer", "piDC", __LINE__);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    checkGlError("glBindBuffer", "piDC", __LINE__);

    GLfloat *bufPt = &(s_odc_tess_work_buf[s_odc_tess_vertex_idx_this]);
    GLint mPosAttrib = glGetAttribLocation(program, "aPos");
    checkGlError("mPosAttrib", "piDC", __LINE__);

    GLint mUvAttrib = glGetAttribLocation(program, "aUV");
    checkGlError("mUvAttrib", "piDC", __LINE__);
    // GLint projection = glGetAttribLocation(program, "MVMatrix");
    // GLint modelView = glGetAttribLocation(program, "TransformMatrix");
    glVertexAttribPointer(mPosAttrib, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                          bufPt);
    checkGlError("glVertexAttribPointer", "piDC", __LINE__);
    glEnableVertexAttribArray(mPosAttrib);
    checkGlError("glEnableVertexAttribArray", "piDC", __LINE__);

    // Bind our texture to the texturing target.
    glBindTexture(GL_TEXTURE_2D, textureID);
    checkGlError("glBindTexture", "piDC", __LINE__);

    // Set up the texture sampler to texture unit 0
    GLint texUni = glGetUniformLocation(program, "uTex");
    checkGlError("texUni", "piDC", __LINE__);
    glUniform1i(texUni, 0);
    checkGlError("texUni", "piDC", __LINE__);

    //        GLint texPOTWidth  = glGetUniformLocation( program, "texPOTWidth"
    //        ); GLint texPOTHeight  = glGetUniformLocation( program,
    //        "texPOTHeight" ); glUniform1f(texPOTWidth, textureSize.x);
    //        glUniform1f(texPOTHeight, textureSize.y);

    //        GLint xo  = glGetUniformLocation( program, "xOff" );
    //        GLint yo  = glGetUniformLocation( program, "yOff" );

    //        glUniform1f(xo, fmod(points[0].x, textureSize.x));
    //        glUniform1f(yo, fmod(points[0].y, textureSize.y));

    //  Pattern color
    float bcolorv[4];
    bcolorv[0] = m_brush.GetColour().Red() / float(256);
    bcolorv[1] = m_brush.GetColour().Green() / float(256);
    bcolorv[2] = m_brush.GetColour().Blue() / float(256);
    bcolorv[3] = m_brush.GetColour().Alpha() / float(256);
    GLint bcolloc = glGetUniformLocation(program, "color");
    checkGlError("bcoloc", "piDC", __LINE__);
    glUniform4fv(bcolloc, 1, bcolorv);
#endif
    // Tesselate
    m_tobj = gluNewTess();
    s_odc_tess_vertex_idx = 0;

    gluTessCallback(m_tobj, GLU_TESS_VERTEX_DATA,
                    (_GLUfuncptr)&odc_vertexCallbackD_GLSL);
    gluTessCallback(m_tobj, GLU_TESS_BEGIN_DATA,
                    (_GLUfuncptr)&odc_beginCallbackD_GLSL);
    gluTessCallback(m_tobj, GLU_TESS_END_DATA,
                    (_GLUfuncptr)&odc_endCallbackD_GLSL);
    gluTessCallback(m_tobj, GLU_TESS_COMBINE_DATA,
                    (_GLUfuncptr)&odc_combineCallbackD);
    // s_tessVP = vp;

    gluTessNormal(m_tobj, 0, 0, 1);
    gluTessProperty(m_tobj, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_NONZERO);

    if (ConfigureBrush()) {
      gluTessBeginPolygon(m_tobj, this);
      gluTessBeginContour(m_tobj);

      for (int i = 0; i < n; i++) {
        GLvertex *vertex = new GLvertex();
        pi_gTesselatorVertices.Add(vertex);
        vertex->info.x = (GLdouble)points[i].x;
        vertex->info.y = (GLdouble)points[i].y;
        vertex->info.z = (GLdouble)0.0;
        vertex->info.r = (GLdouble)0.0;
        vertex->info.g = (GLdouble)0.0;
        vertex->info.b = (GLdouble)0.0;
        gluTessVertex(m_tobj, (GLdouble *)vertex, (GLdouble *)vertex);
      }
      gluTessEndContour(m_tobj);
      gluTessEndPolygon(m_tobj);
    }

#if 0
        //      Render the tesselated results
        GLint program = pi_colorv_tri_shader_program;
        glUseProgram( program );


        // Disable VBO's (vertex buffer objects) for attributes.
        glBindBuffer( GL_ARRAY_BUFFER, 0 );
        glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );


        float *bufPt = &(s_odc_tess_work_buf[s_odc_tess_vertex_idx_this]);
        GLint pos = glGetAttribLocation(program, "position");
        glVertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), bufPt);
        glEnableVertexAttribArray(pos);

        // Bind our texture to the texturing target.
        glBindTexture( GL_TEXTURE_2D, textureID );

        // Set up the texture sampler to texture unit 0
        GLint texUni = glGetUniformLocation( program, "uTexture" );
        glUniform1i( texUni, 0 );

        GLint texPOTWidth  = glGetUniformLocation( program, "texPOTWidth" );
        GLint texPOTHeight  = glGetUniformLocation( program, "texPOTHeight" );
        glUniform1f(texPOTWidth, textureSize.x);
        glUniform1f(texPOTHeight, textureSize.y);

        GLint xo  = glGetUniformLocation( program, "xOff" );
        GLint yo  = glGetUniformLocation( program, "yOff" );

        glUniform1f(xo, fmod(points[0].x, textureSize.x));
        glUniform1f(yo, fmod(points[0].y, textureSize.y));

        //  Pattern color
        float bcolorv[4];
        GLint bcolloc = glGetUniformLocation(program,"color");
        bcolorv[0] = m_brush.GetColour().Red() / float(256);
        bcolorv[1] = m_brush.GetColour().Green() / float(256);
        bcolorv[2] = m_brush.GetColour().Blue() / float(256);
        bcolorv[3] = m_brush.GetColour().Alpha() / float(256);
        glUniform4fv(bcolloc, 1, bcolorv);

        glDrawArrays(s_odc_tess_mode, 0, s_odc_nvertex);
#endif
    glDrawArrays(s_odc_tess_mode, 0, s_odc_nvertex);

    gluDeleteTess(m_tobj);
    m_tobj = NULL;

    for (std::list<double *>::iterator i = odc_combine_work_data.begin();
         i != odc_combine_work_data.end(); i++)
      delete[] * i;
    odc_combine_work_data.clear();

    glUseProgram(0);
  }
#else
#ifndef __ANDROID__
    m_tobj = gluNewTess();

    gluTessCallback(m_tobj, GLU_TESS_VERTEX,
                    (_GLUfuncptr)&piDCPatternvertexCallback);
    gluTessCallback(m_tobj, GLU_TESS_BEGIN,
                    (_GLUfuncptr)&piDCPatternbeginCallback);
    gluTessCallback(m_tobj, GLU_TESS_END, (_GLUfuncptr)&piDCPatternendCallback);
    gluTessCallback(m_tobj, GLU_TESS_COMBINE,
                    (_GLUfuncptr)&piDCPatterncombineCallback);
    gluTessCallback(m_tobj, GLU_TESS_ERROR,
                    (_GLUfuncptr)&piDCPatternerrorCallback);

    gluTessNormal(m_tobj, 0, 0, 1);
    gluTessProperty(m_tobj, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_ODD);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    gluTessProperty(m_tobj, GLU_TESS_BOUNDARY_ONLY, GL_FALSE);

    ConfigurePen();
    if (ConfigureBrush()) {
      gluTessBeginPolygon(m_tobj, NULL);
      gluTessBeginContour(m_tobj);

      for (int i = 0; i < n; i++) {
        GLvertex *vertex = new GLvertex();
        pi_gTesselatorVertices.Add(vertex);
        vertex->info.x = (GLdouble)points[i].x;
        vertex->info.y = (GLdouble)points[i].y;
        vertex->info.z = (GLdouble)0.0;
        vertex->info.r = (GLdouble)0.0;
        vertex->info.g = (GLdouble)0.0;
        vertex->info.b = (GLdouble)0.0;
        gluTessVertex(m_tobj, (GLdouble *)vertex, (GLdouble *)vertex);
      }
      gluTessEndContour(m_tobj);
      gluTessEndPolygon(m_tobj);
    }

    for (unsigned int i = 0; i < pi_gTesselatorVertices.Count(); i++)
      delete (GLvertex *)pi_gTesselatorVertices[i];
    pi_gTesselatorVertices.Clear();

    gluDeleteTess(m_tobj);
    m_tobj = NULL;
  }
#endif
#endif
#endif  // ocpnUSE_GL
}

#if 0

void piDC::DrawPolygon( int n, wxPoint points[], wxCoord xoffset, wxCoord yoffset, float scale )
{
    if( dc )
        dc->DrawPolygon( n, points, xoffset, yoffset );
#ifdef ocpnUSE_GL
    else {
        SetGLAttrs( true );

        if( ConfigureBrush() ) {
            glBegin( GL_POLYGON );
            for( int i = 0; i < n; i++ )
                glVertex2f( (points[i].x * scale) + xoffset, (points[i].y * scale) + yoffset );
            glEnd();
        }

        if( ConfigurePen() ) {
            glBegin( GL_LINE_LOOP );
            for( int i = 0; i < n; i++ )
                glVertex2f( (points[i].x * scale) + xoffset, (points[i].y * scale) + yoffset );
            glEnd();
        }

        SetGLAttrs( false );
    }
#endif

}
#endif

// typedef union {
//     GLdouble data[7];
//     struct sGLvertex {
//         GLdouble x;
//         GLdouble y;
//         GLdouble z;
//         GLdouble r;
//         GLdouble g;
//         GLdouble b;
//         GLdouble a;
//     } info;
//} GLvertex;

#if 0
void piDC::DrawPolygonTessellated( int n, wxPoint points[], wxCoord xoffset, wxCoord yoffset )
{
    if( dc )
        dc->DrawPolygon( n, points, xoffset, yoffset );
#ifdef ocpnUSE_GL
    else {

        GLUtesselator *tobj = gluNewTess();

        gluTessCallback( m_tobj, GLU_TESS_VERTEX, (_GLUfuncptr) &piDCvertexCallback );
        gluTessCallback( m_tobj, GLU_TESS_BEGIN, (_GLUfuncptr) &piDCbeginCallback );
        gluTessCallback( m_tobj, GLU_TESS_END, (_GLUfuncptr) &piDCendCallback );
        gluTessCallback( m_tobj, GLU_TESS_COMBINE, (_GLUfuncptr) &piDCcombineCallback );
        gluTessCallback( m_tobj, GLU_TESS_ERROR, (_GLUfuncptr) &piDCerrorCallback );

        gluTessNormal( m_tobj, 0, 0, 1);
        gluTessProperty(m_tobj, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_ODD);
#ifndef __ANDROID__
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#else
        //glPolygonMode(GL_FRONT_AND_BACK, GL_FILL_NV);
#endif
        gluTessProperty(m_tobj, GLU_TESS_BOUNDARY_ONLY, GL_FALSE);

        if(glIsEnabled(GL_TEXTURE_2D)) g_bTexture2D = true;
        else g_bTexture2D = false;

        ConfigurePen();
        if( ConfigureBrush() ) {
            gluTessBeginPolygon(m_tobj, NULL);
            gluTessBeginContour(m_tobj);
            for( int i = 0; i < n; i++ ) {
                GLvertex* vertex = new GLvertex();
                pi_gTesselatorVertices.Add( vertex );
                vertex->info.x = (GLdouble) points[i].x;
                vertex->info.y = (GLdouble) points[i].y;
                vertex->info.z = (GLdouble) 0.0;
                vertex->info.r = (GLdouble) 0.0;
                vertex->info.g = (GLdouble) 0.0;
                vertex->info.b = (GLdouble) 0.0;
                vertex->info.a = (GLdouble) 0.0;
                gluTessVertex( m_tobj, (GLdouble*)vertex, (GLdouble*)vertex );
            }
            gluTessEndContour( m_tobj );
			gluTessEndPolygon(m_tobj);
		}

		gluDeleteTess(m_tobj);
		for (unsigned int i = 0; i<pi_gTesselatorVertices.Count(); i++)
            delete (GLvertex*)pi_gTesselatorVertices.Item(i);
        pi_gTesselatorVertices.Clear();

    }
#endif
}
#endif

void piDC::DrawPolygonsTessellated(int n, int npoints[], wxPoint points[],
                                   wxCoord xoffset, wxCoord yoffset) {
#if 1
  if (dc) {
    int prev = 0;
    for (int i = 0; i < n; i++) {
      dc->DrawPolygon(npoints[i], &points[i + prev], xoffset, yoffset);
      prev += npoints[i];
    }
  }
#ifdef ocpnUSE_GL
  else {
#ifndef __ANDROID__
    m_tobj = gluNewTess();

    gluTessCallback(m_tobj, GLU_TESS_VERTEX,
                    (_GLUfuncptr)&piDCPatternvertexCallback);
    gluTessCallback(m_tobj, GLU_TESS_BEGIN,
                    (_GLUfuncptr)&piDCPatternbeginCallback);
    gluTessCallback(m_tobj, GLU_TESS_END, (_GLUfuncptr)&piDCPatternendCallback);
    gluTessCallback(m_tobj, GLU_TESS_COMBINE,
                    (_GLUfuncptr)&piDCPatterncombineCallback);
    gluTessCallback(m_tobj, GLU_TESS_ERROR,
                    (_GLUfuncptr)&piDCPatternerrorCallback);

    gluTessNormal(m_tobj, 0, 0, 1);
    gluTessProperty(m_tobj, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_ODD);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    gluTessProperty(m_tobj, GLU_TESS_BOUNDARY_ONLY, GL_FALSE);

    if (glIsEnabled(GL_TEXTURE_2D))
      g_bTexture2D = true;
    else
      g_bTexture2D = false;

    ConfigurePen();
    if (ConfigureBrush()) {
      gluTessBeginPolygon(m_tobj, NULL);
      int prev = 0;
      for (int j = 0; j < n; j++) {
        gluTessBeginContour(m_tobj);
        for (int i = 0; i < npoints[j]; i++) {
          GLvertex *vertex = new GLvertex();
          pi_gTesselatorVertices.Add(vertex);
          vertex->info.x = (GLdouble)points[i + prev].x;
          vertex->info.y = (GLdouble)points[i + prev].y;
          vertex->info.z = (GLdouble)0.0;
          vertex->info.r = (GLdouble)0.0;
          vertex->info.g = (GLdouble)0.0;
          vertex->info.b = (GLdouble)0.0;
          vertex->info.a = (GLdouble)0.0;
          gluTessVertex(m_tobj, (GLdouble *)vertex, (GLdouble *)vertex);
        }
        gluTessEndContour(m_tobj);
        prev += npoints[j];
      }
      gluTessEndPolygon(m_tobj);
    }

    gluDeleteTess(m_tobj);
    for (unsigned int i = 0; i < pi_gTesselatorVertices.Count(); i++)
      delete (GLvertex *)pi_gTesselatorVertices.Item(i);
    pi_gTesselatorVertices.Clear();

#endif  //__ANDROID__
  }
#endif
#endif
}

void piDC::DrawPolygons(int n, int npoint[], wxPoint points[], wxCoord xoffset,
                        wxCoord yoffset) {
  if (g_textureId >= 0)
    DrawPolygonsPattern(n, npoint, points, g_textureId,
                        wxSize(g_iTextureWidth, g_iTextureHeight), xoffset,
                        yoffset);
}

void piDC::DrawPolygonsPattern(int n, int npoint[], wxPoint points[],
                               int textureID, wxSize textureSize,
                               wxCoord xoffset, wxCoord yoffset, float scale,
                               float angle) {
#ifndef __ANDROID__

  DrawPolygonsTessellated(n, npoint, points);

#else
  // Pre-configure the GLES program
  // GLint program = pi_colorv_tri_shader_program;
  //GLint program = pi_texture_2D_shader_program;
  GLint program = pi_texture_2DA_shader_program;
  s_odc_activeProgram = program;

  glUseProgram(program);

  // Disable VBO's (vertex buffer objects) for attributes.
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  float *bufPt = &(s_odc_tess_work_buf[s_odc_tess_vertex_idx_this]);
  GLint pos = glGetAttribLocation(program, "aPos");
  glVertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), bufPt);
  glEnableVertexAttribArray(pos);

  // Bind our texture to the texturing target.
  glBindTexture(GL_TEXTURE_2D, textureID);

  // Set up the texture sampler to texture unit 0
  GLint texUni = glGetUniformLocation(program, "uTex");
  glUniform1i(texUni, 0);

  //        GLint texPOTWidth  = glGetUniformLocation( program, "texPOTWidth" );
  //        GLint texPOTHeight  = glGetUniformLocation( program, "texPOTHeight"
  //        ); glUniform1f(texPOTWidth, textureSize.x);
  //        glUniform1f(texPOTHeight, textureSize.y);

  //        GLint xo  = glGetUniformLocation( program, "xOff" );
  //        GLint yo  = glGetUniformLocation( program, "yOff" );

  //        glUniform1f(xo, fmod(points[0].x, textureSize.x));
  //        glUniform1f(yo, fmod(points[0].y, textureSize.y));

  //  Pattern color
  float bcolorv[4];
  bcolorv[0] = m_brush.GetColour().Red() / float(256);
  bcolorv[1] = m_brush.GetColour().Green() / float(256);
  bcolorv[2] = m_brush.GetColour().Blue() / float(256);
  bcolorv[3] = m_brush.GetColour().Alpha() / float(256);
  GLint bcolloc = glGetUniformLocation(program, "color");
  glUniform4fv(bcolloc, 1, bcolorv);

  // Tesselate
  m_tobj = gluNewTess();
  s_odc_tess_vertex_idx = 0;

  gluTessCallback(m_tobj, GLU_TESS_VERTEX_DATA,
                  (_GLUfuncptr)&odc_vertexCallbackD_GLSL);
  gluTessCallback(m_tobj, GLU_TESS_BEGIN_DATA,
                  (_GLUfuncptr)&odc_beginCallbackD_GLSL);
  gluTessCallback(m_tobj, GLU_TESS_END_DATA,
                  (_GLUfuncptr)&odc_endCallbackD_GLSL);
  gluTessCallback(m_tobj, GLU_TESS_COMBINE_DATA,
                  (_GLUfuncptr)&odc_combineCallbackD);

  gluTessNormal(m_tobj, 0, 0, 1);
  gluTessProperty(m_tobj, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_ODD);
  // glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  gluTessProperty(m_tobj, GLU_TESS_BOUNDARY_ONLY, GL_FALSE);

  if (ConfigureBrush()) {
    gluTessBeginPolygon(m_tobj, this);
    int prev = 0;
    for (int j = 0; j < n; j++) {
      gluTessBeginContour(m_tobj);
      for (int i = 0; i < npoint[j]; i++) {
        double *p = new double[6];
        p[0] = points[i + prev].x, p[1] = points[i + prev].y, p[2] = 0;
        gluTessVertex(m_tobj, p, p);
      }
      gluTessEndContour(m_tobj);
      prev += npoint[j];
    }
    gluTessEndPolygon(m_tobj);
  }

#if 0
        //      Render the tesselated results
        GLint program = pi_colorv_tri_shader_program;
        glUseProgram( program );


        // Disable VBO's (vertex buffer objects) for attributes.
        glBindBuffer( GL_ARRAY_BUFFER, 0 );
        glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );


        float *bufPt = &(s_odc_tess_work_buf[s_odc_tess_vertex_idx_this]);
        GLint pos = glGetAttribLocation(program, "position");
        glVertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), bufPt);
        glEnableVertexAttribArray(pos);

        // Bind our texture to the texturing target.
        glBindTexture( GL_TEXTURE_2D, textureID );

        // Set up the texture sampler to texture unit 0
        GLint texUni = glGetUniformLocation( program, "uTexture" );
        glUniform1i( texUni, 0 );

        GLint texPOTWidth  = glGetUniformLocation( program, "texPOTWidth" );
        GLint texPOTHeight  = glGetUniformLocation( program, "texPOTHeight" );
        glUniform1f(texPOTWidth, textureSize.x);
        glUniform1f(texPOTHeight, textureSize.y);

        GLint xo  = glGetUniformLocation( program, "xOff" );
        GLint yo  = glGetUniformLocation( program, "yOff" );

        glUniform1f(xo, fmod(points[0].x, textureSize.x));
        glUniform1f(yo, fmod(points[0].y, textureSize.y));

        //  Pattern color
        float bcolorv[4];
        GLint bcolloc = glGetUniformLocation(program,"color");
        bcolorv[0] = m_brush.GetColour().Red() / float(256);
        bcolorv[1] = m_brush.GetColour().Green() / float(256);
        bcolorv[2] = m_brush.GetColour().Blue() / float(256);
        bcolorv[3] = m_brush.GetColour().Alpha() / float(256);
        glUniform4fv(bcolloc, 1, bcolorv);

        glDrawArrays(s_odc_tess_mode, 0, s_odc_nvertex);
#endif

  gluDeleteTess(m_tobj);
  m_tobj = NULL;

  glUseProgram(0);
#endif
}

void piDC::StrokePolygon(int n, wxPoint points[], wxCoord xoffset,
                         wxCoord yoffset, float scale) {
#if wxUSE_GRAPHICS_CONTEXT
  if (pgc) {
    wxGraphicsPath gpath = pgc->CreatePath();
    gpath.MoveToPoint(points[0].x + xoffset, points[0].y + yoffset);
    for (int i = 1; i < n; i++)
      gpath.AddLineToPoint(points[i].x + xoffset, points[i].y + yoffset);
    gpath.AddLineToPoint(points[0].x + xoffset, points[0].y + yoffset);

    pgc->SetPen(GetPen());
    pgc->SetBrush(GetBrush());
    pgc->DrawPath(gpath);

    for (int i = 0; i < n; i++)
      dc->CalcBoundingBox(points[i].x + xoffset, points[i].y + yoffset);
  } else
#endif
    DrawPolygon(n, points, xoffset, yoffset, scale);
}

void piDC::DrawBitmap(const wxBitmap &bitmap, wxCoord x, wxCoord y,
                      bool usemask) {
  wxBitmap bmp;
  if (x < 0 || y < 0) {
    int dx = (x < 0 ? -x : 0);
    int dy = (y < 0 ? -y : 0);
    int w = bitmap.GetWidth() - dx;
    int h = bitmap.GetHeight() - dy;
    /* picture is out of viewport */
    if (w <= 0 || h <= 0) return;
    wxBitmap newBitmap = bitmap.GetSubBitmap(wxRect(dx, dy, w, h));
    x += dx;
    y += dy;
    bmp = newBitmap;
  } else {
    bmp = bitmap;
  }
  if (dc) dc->DrawBitmap(bmp, x, y, usemask);
#ifdef ocpnUSE_GL
  else {
#ifdef ocpnUSE_GLES  // Do not attempt to do anything with glDrawPixels if using
                     // opengles
    return;          // this should not be hit anymore ever anyway
#endif

#ifndef USE_ANDROID_GLES2
    wxImage image = bmp.ConvertToImage();
    int w = image.GetWidth(), h = image.GetHeight();

    if (usemask) {
      unsigned char *d = image.GetData();
      unsigned char *a = image.GetAlpha();

      unsigned char mr, mg, mb;
      if (!a && !image.GetOrFindMaskColour(&mr, &mg, &mb))
        printf("trying to use mask to draw a bitmap without alpha or mask\n");

#ifdef __WXOSX__
      if (image.HasMask()) a = 0;
#endif

      unsigned char *e = new unsigned char[4 * w * h];
      if (e && d) {
        for (int y = 0; y < h; y++)
          for (int x = 0; x < w; x++) {
            unsigned char r, g, b;
            int off = (y * w + x);
            r = d[off * 3 + 0];
            g = d[off * 3 + 1];
            b = d[off * 3 + 2];

            e[off * 4 + 0] = r;
            e[off * 4 + 1] = g;
            e[off * 4 + 2] = b;

            e[off * 4 + 3] =
                a ? a[off] : ((r == mr) && (g == mg) && (b == mb) ? 0 : 255);
          }
      }

      glColor4f(1, 1, 1, 1);
      GLDrawBlendData(x, y, w, h, GL_RGBA, e);
      delete[](e);
    } else {
      glRasterPos2i(x, y);
      glPixelZoom(1, -1); /* draw data from top to bottom */
      if (image.GetData())
        glDrawPixels(w, h, GL_RGB, GL_UNSIGNED_BYTE, image.GetData());
      glPixelZoom(1, 1);
    }
#endif  // GLES2
  }
#endif  // ocpnUSE_GL
}

void piDC::DrawText(const wxString &text, wxCoord x, wxCoord y) {
  DrawTextEx(text, x, y, 1.0);
}

void piDC::DrawTextEx(const wxString &text, wxCoord x, wxCoord y,
                      float scaleFactor) {
  if (dc) dc->DrawText(text, x, y);
#ifdef ocpnUSE_GL
  else {
    wxCoord w = 0;
    wxCoord h = 0;

    if (m_buseTex) {
      m_texfont.Build(m_font);  // make sure the font is ready
      m_texfont.GetTextExtent(text, &w, &h);
      m_texfont.SetColor(m_textforegroundcolour);

      if (w && h) {
        if(m_textbackgroundcolour.Alpha() != 0) {
            wxPen p = m_pen;
            wxBrush b = m_brush;
            SetPen(*wxTRANSPARENT_PEN);
            SetBrush(wxBrush(m_textbackgroundcolour));
            DrawRoundedRectangle(x, y, w, h, 3);
            //DrawRectangle(x, y, w, h);
            SetPen(p);
            SetBrush(b);
        }

        glEnable(GL_BLEND);
        glEnable(GL_TEXTURE_2D);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

#ifndef USE_ANDROID_GLES2
        glPushMatrix();
        glTranslatef(x, y, 0);

        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glColor3ub(m_textforegroundcolour.Red(), m_textforegroundcolour.Green(),
                   m_textforegroundcolour.Blue());

        m_texfont.RenderString(text);
        glPopMatrix();
#else
        m_texfont.RenderString(text, x, y);
#endif
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);
      }
    } else {
      wxScreenDC sdc;
      sdc.SetFont(m_font);
      sdc.GetTextExtent(text, &w, &h, NULL, NULL, &m_font);

      /* create bitmap of appropriate size and select it */
      wxBitmap bmp(w, h);
      wxMemoryDC temp_dc;
      temp_dc.SelectObject(bmp);

      /* fill bitmap with black */
      temp_dc.SetBackground(wxBrush(wxColour(0, 0, 0)));
      temp_dc.Clear();

      /* draw the text white */
      temp_dc.SetFont(m_font);
      temp_dc.SetTextForeground(wxColour(255, 255, 255));
      temp_dc.DrawText(text, 0, 0);
      temp_dc.SelectObject(wxNullBitmap);

      /* use the data in the bitmap for alpha channel,
       and set the color to text foreground */
      wxImage image = bmp.ConvertToImage();
      if (x < 0 ||
          y < 0) {  // Allow Drawing text which is offset to start off screen
        int dx = (x < 0 ? -x : 0);
        int dy = (y < 0 ? -y : 0);
        w = bmp.GetWidth() - dx;
        h = bmp.GetHeight() - dy;
        /* picture is out of viewport */
        if (w <= 0 || h <= 0) return;
        image = image.GetSubImage(wxRect(dx, dy, w, h));
        x += dx;
        y += dy;
      }

      unsigned char *data = new unsigned char[w * h * 4];
      unsigned char *im = image.GetData();

      if (im) {
        unsigned int r = m_textforegroundcolour.Red();
        unsigned int g = m_textforegroundcolour.Green();
        unsigned int b = m_textforegroundcolour.Blue();
        for (int i = 0; i < h; i++) {
          for (int j = 0; j < w; j++) {
            unsigned int index = ((i * w) + j) * 4;
            data[index] = r;
            data[index + 1] = g;
            data[index + 2] = b;
            data[index + 3] = im[((i * w) + j) * 3];
          }
        }
      }
#if 0
            glColor4ub( 255, 255, 255, 255 );
            glEnable( GL_BLEND );
            glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
            glRasterPos2i( x, y );
            glPixelZoom( 1, -1 );
            glDrawPixels( w, h, GL_RGBA, GL_UNSIGNED_BYTE, data );
            glPixelZoom( 1, 1 );
            glDisable( GL_BLEND );
#else
      unsigned int texobj;

      if(m_textbackgroundcolour.Alpha() != 0) {
        wxPen p = m_pen;
        wxBrush b = m_brush;
        SetPen(*wxTRANSPARENT_PEN);
        SetBrush(wxBrush(m_textbackgroundcolour));
        DrawRoundedRectangle(x, y, w, h, 3);
        SetPen(p);
        SetBrush(b);
      }

      glGenTextures(1, &texobj);
      glBindTexture(GL_TEXTURE_2D, texobj);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

      int TextureWidth = NextPow2(w);
      int TextureHeight = NextPow2(h);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TextureWidth, TextureHeight, 0,
                   GL_RGBA, GL_UNSIGNED_BYTE, NULL);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE,
                      data);

      glEnable(GL_TEXTURE_2D);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      float u = (float)w / TextureWidth, v = (float)h / TextureHeight;

#ifndef USE_ANDROID_GLES2
      glColor3ub(0, 0, 0);

      glBegin(GL_QUADS);
      glTexCoord2f(0, 0);
      glVertex2f(x, y);
      glTexCoord2f(u, 0);
      glVertex2f(x + w, y);
      glTexCoord2f(u, v);
      glVertex2f(x + w, y + h);
      glTexCoord2f(0, v);
      glVertex2f(x, y + h);
      glEnd();
#else
#endif
      glDisable(GL_BLEND);
      glDisable(GL_TEXTURE_2D);

      glDeleteTextures(1, &texobj);
#endif
      delete[] data;
    }
  }
#endif  // ocpnUSE_GL
}

void piDC::GetTextExtent(const wxString &string, wxCoord *w, wxCoord *h,
                         wxCoord *descent, wxCoord *externalLeading,
                         wxFont *font) {
  //  Give at least reasonable results on failure.
  if (w) *w = 100;
  if (h) *h = 100;

  if (dc)
    dc->GetMultiLineTextExtent(string, w, h, descent, font);
  else {
    wxFont f = m_font;
    if (font) f = *font;

    if (m_buseTex) {
#ifdef ocpnUSE_GL
      m_texfont.Build(f);  // make sure the font is ready
      m_texfont.GetTextExtent(string, w, h);
#else
      wxMemoryDC temp_dc;
      temp_dc.GetMultiLineTextExtent(string, w, h, descent, &f);
#endif
    } else {
      wxMemoryDC temp_dc;
      temp_dc.GetMultiLineTextExtent(string, w, h, descent, &f);
    }
  }

  //  Sometimes GetTextExtent returns really wrong, uninitialized results.
  //  Dunno why....
  if (w && (*w > 2000)) *w = 2000;
  if (h && (*h > 500)) *h = 500;
}

void piDC::ResetBoundingBox() {
  if (dc) dc->ResetBoundingBox();
}

void piDC::CalcBoundingBox(wxCoord x, wxCoord y) {
  if (dc) dc->CalcBoundingBox(x, y);
}

bool piDC::ConfigurePen() {
  wxColour c = wxNullColour;
  int width = 0;

  if (!m_pen.IsOk()) return false;
  if (m_pen == *wxTRANSPARENT_PEN)
    width = 0;
  else {
    c = m_pen.GetColour();
    width = m_pen.GetWidth();
  }
#ifdef ocpnUSE_GL
#ifndef USE_ANDROID_GLES2
  if (c != wxNullColour) glColor4ub(c.Red(), c.Green(), c.Blue(), c.Alpha());
  glLineWidth(wxMax(GLMinSymbolLineWidth, width));
#endif
#endif
  return true;
}

bool piDC::ConfigureBrush() {
  if (m_brush == wxNullBrush || m_brush.GetStyle() == wxBRUSHSTYLE_TRANSPARENT)
    return false;
#ifdef ocpnUSE_GL
#ifndef USE_ANDROID_GLES2
  wxColour c = m_brush.GetColour();
  glColor4ub(c.Red(), c.Green(), c.Blue(), c.Alpha());
#endif
#endif
  return true;
}

void piDC::GLDrawBlendData(wxCoord x, wxCoord y, wxCoord w, wxCoord h,
                           int format, const unsigned char *data) {
#ifdef ocpnUSE_GL
#ifndef USE_ANDROID_GLES2
  glEnable(GL_BLEND);
  glRasterPos2i(x, y);
  glPixelZoom(1, -1);
  glDrawPixels(w, h, format, GL_UNSIGNED_BYTE, data);
  glPixelZoom(1, 1);
  glDisable(GL_BLEND);
#endif
#endif
}

void piDC::SetTextureParms(GLint textureId, int width, int height) {
  g_textureId = textureId;
  g_iTextureWidth = width;
  g_iTextureHeight = height;
}

void piDC::DrawTexture(wxRect texRect, int width, int height, float scaleFactor,
                       wxPoint position, float rotation, wxPoint rPivot) {
  float w = width;
  float h = height;

#ifndef USE_ANDROID_GLES2
#ifndef __ANDROID__
  glColor3f(1, 1, 1);
#endif

  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  float u = (float)width / texRect.width, v = (float)height / texRect.height;

  glBegin(GL_QUADS);
  glTexCoord2f(0, 0);
  glVertex2f(position.x, position.y);
  glTexCoord2f(u, 0);
  glVertex2f(position.x + w, position.y);
  glTexCoord2f(u, v);
  glVertex2f(position.x + w, position.y + h);
  glTexCoord2f(0, v);
  glVertex2f(position.x, position.y + h);
  glEnd();
#else

  float tx1 = texRect.x, ty1 = texRect.y;
  float tx2 = tx1 + w, ty2 = ty1 + h;

  // Normalize values against texture size
  tx1 /= texRect.width, tx2 /= texRect.width;
  ty1 /= texRect.height, ty2 /= texRect.height;

  float uv[8];
  float coords[8];

  // Note swizzle of points to allow TRIANGLE_STRIP drawing
  uv[0] = tx1;
  uv[1] = ty1;
  uv[2] = tx2;
  uv[3] = ty1;
  uv[6] = tx2;
  uv[7] = ty2;
  uv[4] = tx1;
  uv[5] = ty2;

  // pixels
  coords[0] = position.x;
  coords[1] = position.y;
  coords[2] = position.x + w;
  coords[3] = position.y;
  coords[6] = position.x + w;
  coords[7] = position.y + h;
  coords[4] = position.x;
  coords[5] = position.y + h;

  GLint program = pi_texture_2D_shader_program;
  glUseProgram(program);

  // Get pointers to the attributes in the program.
  GLint mPosAttrib = glGetAttribLocation(program, "aPos");
  GLint mUvAttrib = glGetAttribLocation(program, "aUV");

  // Select the active texture unit.
  glActiveTexture(GL_TEXTURE0);

  // Set up the texture sampler to texture unit 0
  GLint texUni = glGetUniformLocation(program, "uTex");
  glUniform1i(texUni, 0);

  // Disable VBO's (vertex buffer objects) for attributes.
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  // Set the attribute mPosAttrib with the vertices in the screen coordinates...
  glVertexAttribPointer(mPosAttrib, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                        coords);
  // ... and enable it.
  glEnableVertexAttribArray(mPosAttrib);

  // Set the attribute mUvAttrib with the vertices in the GL coordinates...
  glVertexAttribPointer(mUvAttrib, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                        uv);
  // ... and enable it.
  glEnableVertexAttribArray(mUvAttrib);

#if 0
            // Rotate
        mat4x4 I, Q;
        mat4x4_identity(I);

        mat4x4_translate_in_place(I, position.x, position.y, 0);
        mat4x4_rotate_Z(Q, I, -rotation);
        mat4x4_translate_in_place(Q, -rPivot.x, -rPivot.y, rotation);

        GLint matloc = glGetUniformLocation(progam,"TransformMatrix");
        glUniformMatrix4fv( matloc, 1, GL_FALSE, (const GLfloat*)Q);
#endif

  // Perform the actual drawing.
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  glUseProgram(0);
#endif
}

void piDC::DrawTextureAlpha(wxRect texRect, int width, int height,
                            float scaleFactor, wxPoint position, float rotation,
                            wxPoint rPivot) {
  float w = width;
  float h = height;

#ifndef USE_ANDROID_GLES2
  ConfigurePen();

  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  float u = (float)width / texRect.width, v = (float)height / texRect.height;

  glBegin(GL_QUADS);
  glTexCoord2f(0, 0);
  glVertex2f(position.x, position.y);
  glTexCoord2f(u, 0);
  glVertex2f(position.x + w, position.y);
  glTexCoord2f(u, v);
  glVertex2f(position.x + w, position.y + h);
  glTexCoord2f(0, v);
  glVertex2f(position.x, position.y + h);
  glEnd();
#else

  float tx1 = texRect.x, ty1 = texRect.y;
  float tx2 = tx1 + w, ty2 = ty1 + h;

  // Normalize values against texture size
  tx1 /= texRect.width, tx2 /= texRect.width;
  ty1 /= texRect.height, ty2 /= texRect.height;

  float uv[8];
  float coords[8];

  // Note swizzle of points to allow TRIANGLE_STRIP drawing
  uv[0] = tx1;
  uv[1] = ty1;
  uv[2] = tx2;
  uv[3] = ty1;
  uv[6] = tx2;
  uv[7] = ty2;
  uv[4] = tx1;
  uv[5] = ty2;

  // pixels
  coords[0] = position.x;
  coords[1] = position.y;
  coords[2] = position.x + w;
  coords[3] = position.y;
  coords[6] = position.x + w;
  coords[7] = position.y + h;
  coords[4] = position.x;
  coords[5] = position.y + h;

  GLint program = pi_texture_2D_shader_program;
  glUseProgram(program);

  // Get pointers to the attributes in the program.
  GLint mPosAttrib = glGetAttribLocation(program, "aPos");
  GLint mUvAttrib = glGetAttribLocation(program, "aUV");

  // Select the active texture unit.
  glActiveTexture(GL_TEXTURE0);

  // Set up the texture sampler to texture unit 0
  GLint texUni = glGetUniformLocation(program, "uTex");
  glUniform1i(texUni, 0);

  // Disable VBO's (vertex buffer objects) for attributes.
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  // Set the attribute mPosAttrib with the vertices in the screen coordinates...
  glVertexAttribPointer(mPosAttrib, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                        coords);
  // ... and enable it.
  glEnableVertexAttribArray(mPosAttrib);

  // Set the attribute mUvAttrib with the vertices in the GL coordinates...
  glVertexAttribPointer(mUvAttrib, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                        uv);
  // ... and enable it.
  glEnableVertexAttribArray(mUvAttrib);

  //  Pattern color
  float bcolorv[4];
  bcolorv[0] = m_pen.GetColour().Red() / float(256);
  bcolorv[1] = m_pen.GetColour().Green() / float(256);
  bcolorv[2] = m_pen.GetColour().Blue() / float(256);
  bcolorv[3] = m_pen.GetColour().Alpha() / float(256);
  GLint bcolloc = glGetUniformLocation(program, "uColour");
  glUniform4fv(bcolloc, 1, bcolorv);

#if 0
            // Rotate
        mat4x4 I, Q;
        mat4x4_identity(I);

        mat4x4_translate_in_place(I, position.x, position.y, 0);
        mat4x4_rotate_Z(Q, I, -rotation);
        mat4x4_translate_in_place(Q, -rPivot.x, -rPivot.y, rotation);

        GLint matloc = glGetUniformLocation(program,"TransformMatrix");
        glUniformMatrix4fv( matloc, 1, GL_FALSE, (const GLfloat*)Q);
#endif

  // Perform the actual drawing.
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  glUseProgram(0);
#endif
}

void piDC::RenderSingleTexture(float *coords, float *uvCoords,
                               PlugIn_ViewPort *vp, float dx, float dy,
                               float angle_rad) {
#ifdef USE_ANDROID_GLES2
  // build_texture_shaders();
  glUseProgram(pi_texture_2D_shader_program);

  // Get pointers to the attributes in the program.
  GLint mPosAttrib = glGetAttribLocation(pi_texture_2D_shader_program, "aPos");
  GLint mUvAttrib = glGetAttribLocation(pi_texture_2D_shader_program, "aUV");

  // Set up the texture sampler to texture unit 0
  GLint texUni = glGetUniformLocation(pi_texture_2D_shader_program, "uTex");
  glUniform1i(texUni, 0);

  // Disable VBO's (vertex buffer objects) for attributes.
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  // Set the attribute mPosAttrib with the vertices in the screen
  // coordinates...
  glVertexAttribPointer(mPosAttrib, 2, GL_FLOAT, GL_FALSE, 0, coords);
  // ... and enable it.
  glEnableVertexAttribArray(mPosAttrib);

  // Set the attribute mUvAttrib with the vertices in the GL coordinates...
  glVertexAttribPointer(mUvAttrib, 2, GL_FLOAT, GL_FALSE, 0, uvCoords);
  // ... and enable it.
  glEnableVertexAttribArray(mUvAttrib);

  // Rotate
  mat4x4 I, Q;
  mat4x4_identity(I);
  mat4x4_rotate_Z(Q, I, angle_rad);

  // Translate
  Q[3][0] = dx;
  Q[3][1] = dy;

  // mat4x4 X;
  // mat4x4_mul(X, (float (*)[4])vp->vp_transform, Q);

  GLint matloc =
      glGetUniformLocation(pi_texture_2D_shader_program, "TransformMatrix");
  glUniformMatrix4fv(matloc, 1, GL_FALSE, (const GLfloat *)Q);

  // Select the active texture unit.
  glActiveTexture(GL_TEXTURE0);

// Bind our texture to the texturing target.
// glBindTexture( GL_TEXTURE_2D, tex );

// Perform the actual drawing.

// For some reason, glDrawElements is busted on Android
// So we do this a hard ugly way, drawing two triangles...
#if 0
    GLushort indices1[] = {0,1,3,2};
    glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_SHORT, indices1);
#else

  float co1[8];
  co1[0] = coords[0];
  co1[1] = coords[1];
  co1[2] = coords[2];
  co1[3] = coords[3];
  co1[4] = coords[6];
  co1[5] = coords[7];
  co1[6] = coords[4];
  co1[7] = coords[5];

  float tco1[8];
  tco1[0] = uvCoords[0];
  tco1[1] = uvCoords[1];
  tco1[2] = uvCoords[2];
  tco1[3] = uvCoords[3];
  tco1[4] = uvCoords[6];
  tco1[5] = uvCoords[7];
  tco1[6] = uvCoords[4];
  tco1[7] = uvCoords[5];

  glVertexAttribPointer(mPosAttrib, 2, GL_FLOAT, GL_FALSE, 0, co1);
  glVertexAttribPointer(mUvAttrib, 2, GL_FLOAT, GL_FALSE, 0, tco1);

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

#endif

#else
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);

  glPushMatrix();
  glTranslatef(dx, dy, 0);
  glRotatef(180 / PI * angle_rad, 0, 0, 1);

  glTexCoordPointer(2, GL_FLOAT, 2 * sizeof(GLfloat), uvCoords);
  glVertexPointer(2, GL_FLOAT, 2 * sizeof(GLfloat), coords);
  glDrawArrays(GL_QUADS, 0, 4);
  glPopMatrix();

#endif

  return;
}

int piDC::ArcSectorPoints(wxPoint *&points, wxCoord xc, wxCoord yc, wxCoord x1,
                          wxCoord y1, wxCoord x2, wxCoord y2, wxCoord x3,
                          wxCoord y3, wxCoord x4, wxCoord y4,
                          bool bHighQuality) {
  double y1yc, x1xc, y4yc, x4xc;
  y1yc = y1 - yc;
  x1xc = x1 - xc;
  y4yc = y4 - yc;
  x4xc = x4 - xc;
  wxDouble l_dFirstAngle;
  l_dFirstAngle = atan2(y1yc, x1xc);

  wxDouble l_dSecondAngle;
  l_dSecondAngle = atan2(y4yc, x4xc);

  wxDouble l_OuterRadius = sqrt(pow((y2 - yc), 2.0) + pow((x2 - xc), 2.0));
  wxDouble l_InnerRadius = sqrt(pow((y1 - yc), 2.0) + pow((x1 - xc), 2.0));
  float innerSteps;
  float outerSteps;
  if (bHighQuality) {
    innerSteps = floorf(
        wxMax(sqrtf(sqrtf(((l_InnerRadius * 2.) * (l_InnerRadius * 2.)) * 2.)),
              1) *
        M_PI);
    outerSteps = floorf(
        wxMax(sqrtf(sqrtf(((l_OuterRadius * 2.) * (l_OuterRadius * 2.)) * 2.)),
              1) *
        M_PI);
  } else {
    innerSteps = 24;
    outerSteps = 24;
  }

  points = new wxPoint[(int)innerSteps + (int)outerSteps + 5];
  double dxc1 = xc - x1;
  double dxc4 = xc - x4;
  double dyc1 = yc - y1;
  double dyc4 = yc - y4;
  double angle = atan2(dxc1 * dyc4 - dyc1 * dxc4, dxc1 * dxc4 + dyc1 * dyc4);
  if (angle < 0) angle += (2 * PI);
  int numpoints_outer = ceil(abs(outerSteps * (angle / (2. * M_PI))));
  int numpoints_inner = ceil(abs(innerSteps * (angle / (2. * M_PI))));
  // if(numpoints_outer == 0) return;
  points[0].x = x1;
  points[0].y = y1;
  float a = l_dFirstAngle;
  for (int i = 0; i < (int)numpoints_outer; i++) {
    points[i + 1].x = xc + l_OuterRadius * cosf(a);
    points[i + 1].y = yc + l_OuterRadius * sinf(a);
    a += 2 * M_PI / outerSteps;
  }
  a = l_dSecondAngle;
  points[(int)numpoints_outer + 1].x = x3;
  points[(int)numpoints_outer + 1].y = y3;
  points[(int)numpoints_outer + 2].x = x4;
  points[(int)numpoints_outer + 2].y = y4;
  for (int i = 0; i < (int)numpoints_inner; i++) {
    points[i + (int)numpoints_outer + 3].x = xc + l_InnerRadius * cosf(a);
    points[i + (int)numpoints_outer + 3].y = yc + l_InnerRadius * sinf(a);
    a -= 2 * M_PI / innerSteps;
  }
  int npoints[1];
  npoints[0] = numpoints_inner + numpoints_outer + 4;
  points[npoints[0] - 1].x = x1;
  points[npoints[0] - 1].y = y1;
  return npoints[0];
}
