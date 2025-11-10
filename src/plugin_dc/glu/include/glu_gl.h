/** 
 * \file
 *
 * Parts of the official GL/gl.h which is used by the SGI glu library. They
 * are pulled in on also on  GLES/GLES2 platforms,  and should theoretically
 * not be possible to use on there. However, our bet is that they actually
 * work also there.
 *
 * Also: undef som constants define in both gl2.h and glu.h.
 */
typedef double GLdouble;

#undef GL_ZERO
#undef GL_NONE
#undef GL_ONE
#undef GL_NO_ERROR
#undef GL_FALSE
#undef GL_TRUE

#define GL_LINE_STIPPLE 0x0B24
#define GL_POLYGON_SMOOTH 0x0B41
#define GL_LINE_SMOOTH 0x0B20
#define GL_POLYGON_SMOOTH 0x0B41
#define GL_SMOOTH_LINE_WIDTH_RANGE 0x0B22
