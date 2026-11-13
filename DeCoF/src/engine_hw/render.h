// render.h -- public interface to the refresh functions

#ifndef RENDER_H
#define RENDER_H

// refresh definitions
typedef struct
{
	vrect_t		vrect;				// subwindow in video for refresh
									// FIXME: not need vrect next field here?
	vrect_t		aliasvrect;			// scaled Alias version
	int			vrectright, vrectbottom;	// right & bottom screen coords
	int			aliasvrectright, aliasvrectbottom;	// scaled Alias versions
	float		vrectrightedge;			// rightmost right edge we care about,
										//  for use in edge list
	float		fvrectx, fvrecty;		// for floating-point compares
	float		fvrectx_adj, fvrecty_adj; // left and top edges, for clamping
	int			vrect_x_adj_shift20;	// (vrect.x + 0.5 - epsilon) << 20
	int			vrectright_adj_shift20;	// (vrectright + 0.5 - epsilon) << 20
	float		fvrectright_adj, fvrectbottom_adj;
										// right and bottom edges, for clamping
	float		fvrectright;			// rightmost edge, for Alias clamping
	float		fvrectbottom;			// bottommost edge, for Alias clamping
	float		horizontalFieldOfView;	// at Z = 1.0, this many X is visible
										// 2.0 = 90 degrees
	float		xOrigin;			// should probably allways be 0.5
	float		yOrigin;			// between be around 0.3 to 0.5

	vec3_t		vieworg;
	vec3_t		viewangles;

	color24		ambientlight;
	qboolean	onlyClientDraws;
} refdef_t;

extern	float		r_blend;		// blending factor of the current entity

extern	refdef_t	r_refdef;
extern	vec3_t		r_origin, vpn, vright, vup;

extern	struct texture_s	*r_notexture_mip;

// gl_rlight.c
extern	int		r_dlightchanged;	// which ones changed
extern	int		r_dlightactive;		// which ones are active

void	R_PushDlights( void );
void	R_RenderView( void );
void	R_PolyBlend( void );
void	D_FlushCaches( void );

#endif // RENDER_H
