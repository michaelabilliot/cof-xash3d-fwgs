// VGuiVertex.h -- a textured vertex of the VGUI polygon drawing
// (members from the Linux hw.so DWARF, public/vgui/VGuiVertex.h)

#ifndef VGUIVERTEX_H
#define VGUIVERTEX_H
#ifdef _WIN32
#pragma once
#endif

namespace vgui2
{

class VGuiVertex
{
public:
	VGuiVertex() {}
	VGuiVertex( int x1, int y1, float u1, float v1 )
	{
		SetVertex( x1, y1, u1, v1 );
	}

	void SetVertex( int x1, int y1 )
	{
		x = x1;
		y = y1;
	}

	void SetVertex( int x1, int y1, float u1, float v1 )
	{
		x = x1;
		y = y1;
		u = u1;
		v = v1;
	}

	int GetX() { return x; }
	int GetY() { return y; }
	float GetU() { return u; }
	float GetV() { return v; }

	void GetVertex( int &x1, int &y1, float &u1, float &v1 )
	{
		x1 = x;
		y1 = y;
		u1 = u;
		v1 = v;
	}

	bool operator==( VGuiVertex &rhs )
	{
		return ( x == rhs.x && y == rhs.y && u == rhs.u && v == rhs.v );
	}

private:
	int x, y;
	float u, v;
};

} // namespace vgui2

#endif // VGUIVERTEX_H
