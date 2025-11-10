/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  Layer to use wxDC or opengl
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
 *f
 */

#ifndef __PIDC_H__
#define __PIDC_H__

#ifdef __ANDROID__
#include <qopengl.h>
typedef double GLdouble;
#define GL_GLEXT_LEGACY 1
#include "GLES2/gl2.h"
#include "glu_gl.h"
#include "GL/glu.h"

#elif defined(__WXOSX__)
#include "OpenGL/gl.h"
#include "OpenGL/glu.h"
#include "OpenGL/glext.h"
typedef void (*_GLUfuncptr)();

#else
#include "GL/gl.h"
#include "GL/glu.h"
#include "GL/glext.h"
#endif

#ifdef _MSC_VER
typedef void (__stdcall* _GLUfuncptr)();
#endif

#include <vector>
#include "linmath.h"

#include "TexFont.h"
#include "ocpn_plugin.h"

#ifdef ocpnUSE_GL
#include <wx/glcanvas.h>
#endif


class ViewPort;
class GLUtesselator;

void DrawGLThickLine(float x1, float y1, float x2, float y2, wxPen pen,
                     bool b_hiqual);
void checkGlError(const char* op, const char* filename, int linenumber);

//----------------------------------------------------------------------------
// piDC
//----------------------------------------------------------------------------

class wxGLCanvas;

class piDC {
public:
  piDC(wxGLContext *context);
  piDC(wxDC &pdc);
  piDC();

  ~piDC();

  void Init();

  void SetBackground(const wxBrush &brush);
  void SetPen(const wxPen &pen);
  void SetBrush(const wxBrush &brush);
  void SetTextForeground(const wxColour &colour);
  void SetTextBackground(const wxColour &colour);
  void SetFont(const wxFont &font);
  static void SetGLAttrs(bool highQuality);
  void SetGLStipple() const;
  void SetVP(PlugIn_ViewPort *vp);
  void SetDC(wxDC *dc_in) { dc = dc_in; }

  const wxPen &GetPen() const;
  const wxBrush &GetBrush() const;
  const wxFont &GetFont() const;

  void GetSize(wxCoord *width, wxCoord *height) const;

  void DrawLine(wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2,
                bool b_hiqual = true);
  void DrawLines(int n, wxPoint points[], wxCoord xoffset = 0,
                 wxCoord yoffset = 0, bool b_hiqual = true);

  void StrokeLine(wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2);
  void StrokeLine(wxPoint a, wxPoint b) { StrokeLine(a.x, a.y, b.x, b.y); }
  void StrokeLines(int n, wxPoint *points);

  void DrawArc(wxCoord xc, wxCoord yc, wxCoord x1, wxCoord y1, wxCoord x2,
               wxCoord y2, bool b_hiqual = true);
  void StrokeArc(wxCoord xc, wxCoord yc, wxCoord x1, wxCoord y1, wxCoord x2,
                 wxCoord y2);
  void StrokeSector(wxCoord xc, wxCoord yc, wxCoord x1, wxCoord y1, wxCoord x2,
                    wxCoord y2, wxCoord x3, wxCoord y3, wxCoord x4, wxCoord y4);

  void Clear();
  void DrawRectangle(wxCoord x, wxCoord y, wxCoord w, wxCoord h);
  void DrawRoundedRectangle(wxCoord x, wxCoord y, wxCoord w, wxCoord h,
                            wxCoord rr);
  void DrawCircle(wxCoord x, wxCoord y, wxCoord radius);
  void DrawCircle(const wxPoint &pt, wxCoord radius) {
    DrawCircle(pt.x, pt.y, radius);
  }
  void DrawDisk(wxCoord x, wxCoord y, wxCoord innerRadius, wxCoord outerRadius);
  void DrawDiskPattern(wxCoord x, wxCoord y, wxCoord innerRadius,
                       wxCoord outerRadius, GLint textureID,
                       wxSize textureSize);
  void DrawSector(wxCoord xc, wxCoord yc, wxCoord x1, wxCoord y1, wxCoord x2,
                  wxCoord y2, wxCoord x3, wxCoord y3, wxCoord x4, wxCoord y4);
  void StrokeCircle(wxCoord x, wxCoord y, wxCoord radius);

  void DrawEllipse(wxCoord x, wxCoord y, wxCoord width, wxCoord height);
  void DrawPolygon(int n, wxPoint points[], wxCoord xoffset = 0,
                   wxCoord yoffset = 0, float scale = 1.0, float angle = 0);
  void StrokePolygon(int n, wxPoint points[], wxCoord xoffset = 0,
                     wxCoord yoffset = 0, float scale = 1.0);
  void DrawPolygons(int n, int npoint[], wxPoint points[], wxCoord xoffset = 0,
                    wxCoord yoffset = 0);

  void DrawPolygonPattern(int n, wxPoint points[], int textureID,
                          wxSize textureSize, wxCoord xoffset = 0,
                          wxCoord yoffset = 0, float scale = 1.0,
                          float angle = 0.0);
  void DrawPolygonsPattern(int n, int npoint[], wxPoint points[], int textureID,
                           wxSize textureSize, wxCoord xoffset = 0,
                           wxCoord yoffset = 0, float scale = 1.0,
                           float angle = 0.0);

  // TODO move to Protected
  void DrawPolygonsTessellated(int n, int npoint[], wxPoint points[],
                               wxCoord xoffset = 0, wxCoord yoffset = 0);
  void DrawPolygonTessellated(int n, wxPoint points[], wxCoord xoffset = 0,
                              wxCoord yoffset = 0);

  void DrawBitmap(const wxBitmap &bitmap, wxCoord x, wxCoord y, bool usemask);

  void DrawText(const wxString &text, wxCoord x, wxCoord y);
  void DrawTextEx(const wxString &text, wxCoord x, wxCoord y,
                  float scaleFactor);

  void GetTextExtent(const wxString &string, wxCoord *w, wxCoord *h,
                     wxCoord *descent = NULL, wxCoord *externalLeading = NULL,
                     wxFont *font = NULL);

  void DrawTexture(wxRect texRect, int width, int height, float scaleFactor,
                   wxPoint position, float rotation, wxPoint rPivot);
  void DrawTextureAlpha(wxRect texRect, int width, int height,
                        float scaleFactor, wxPoint position, float rotation,
                        wxPoint rPivot);

  void ResetBoundingBox();
  void CalcBoundingBox(wxCoord x, wxCoord y);

  void DestroyClippingRegion() {}

  wxDC *GetDC() const { return dc; }

  void DrawGLLineArray(int n, float *vertex_array, unsigned char *color_array,
                       bool b_hiqual);
  void DrawGLLineArray(int n, float *vertex_array, float *color_array,
                       bool b_hiqual);

  void SetTextureParms(GLint textureId, int width, int height);

  void RenderSingleTexture(float *coords, float *uvCoords, PlugIn_ViewPort *vp,
                           float dx, float dy, float angle_rad);

  int ArcSectorPoints(wxPoint *&points, wxCoord xc, wxCoord yc, wxCoord x1,
                      wxCoord y1, wxCoord x2, wxCoord y2, wxCoord x3,
                      wxCoord y3, wxCoord x4, wxCoord y4, bool bHighQuality);

#ifdef ocpnUSE_GL
  GLfloat *s_odc_tess_work_buf;
  GLfloat *s_odc_tess_tex_buf;
#endif

#ifdef USE_ANDROID_GLES2
  int s_odc_tess_vertex_idx;
  int s_odc_tess_vertex_idx_this;
  int s_odc_tess_buf_len;
  GLenum s_odc_tess_mode;
  int s_odc_nvertex;
  vec4 s_odc_tess_color;
  ViewPort *s_odc_tessVP;
  GLint s_odc_activeProgram;

#endif
  GLUtesselator *m_tobj;

protected:
  void DrawPolygonTessellatedPattern(int n, wxPoint points[], int textureID,
                                     wxSize textureSize, wxCoord xoffset = 0,
                                     wxCoord yoffset = 0);

  bool ConfigurePen();
  bool ConfigureBrush();

  void GLDrawBlendData(wxCoord x, wxCoord y, wxCoord w, wxCoord h, int format,
                       const unsigned char *data);
  void drawrrhelperGLES2(wxCoord x0, wxCoord y0, wxCoord r, int quadrant,
                         int steps);

  void DrawGLThickLine(float x1, float y1, float x2, float y2, wxPen pen,
                       bool b_hiqual);
  void DrawGLThickLines(int n, wxPoint points[], wxCoord xoffset,
                        wxCoord yoffset, wxPen pen, bool b_hiqual);
  void DrawEndCap(float x1, float y1, float t1, float angle);

  wxGLContext *glcontext;
  wxDC *dc;
  wxPen m_pen;
  wxBrush m_brush;
  wxColour m_textforegroundcolour;
  wxColour m_textbackgroundcolour;
  wxFont m_font;

#ifdef ocpnUSE_GL
  TexFontPI m_texfont;
#endif

  bool m_buseTex;

#if wxUSE_GRAPHICS_CONTEXT
  wxGraphicsContext *pgc;
#endif

  float *workBuf;
  size_t workBufSize;
  unsigned int workBufIndex;

  wxSize m_vpSize;
};

#endif
