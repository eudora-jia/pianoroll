#include "stdafx.h"

HWND MainWindow = 0;
HINSTANCE hAppInstance = 0;
HICON hIcon1 = 0;

// ----
const TCHAR* ttitle = _T("App");
// ----

#include <d2d1.h>
#pragma comment(lib,"d2d1.lib")
#pragma comment(lib,"winmm.lib")
#include <atlbase.h>


HMIDIOUT mIN = 0;


namespace PR
{ 
class PIANOROLL;
class NOTE;
#ifdef _WIN64
typedef long long ssize_t;
#else
typedef long ssize_t;
#endif

// Todo
/*

	Cut
	Join
	Split
	Enlarge
	Reduce
	Enlarge to grid
	Reduce to grid
	Dot
	Quantize

	Layers
	Channels

	Effects
		Cresc/Dim
		Humanizator
		Transposer
		Stack/Leg


*/


class PIANOROLLCALLBACK
{
public:

	virtual HRESULT NoteAdded(PIANOROLL* pr, NOTE*) = 0;
	virtual HRESULT NoteRemoved(PIANOROLL* pr, NOTE*) = 0;
	virtual void RedrawRequest(PIANOROLL* pr,unsigned long long param) = 0;	
	virtual HRESULT OnNoteChange(PIANOROLL* pr, NOTE* oldn, NOTE* newn) = 0;
	virtual HRESULT OnNoteSelect(PIANOROLL* pr, NOTE* oldn, bool) = 0;
	virtual void OnPianoOn(PIANOROLL*, int n) = 0;
	virtual void OnPianoOff(PIANOROLL*, int off) = 0;

};


class FRACTION
{
public:
	mutable ssize_t n = 0;
	mutable ssize_t d = 1;

	static void Om(const FRACTION& f1, const FRACTION& f2)
	{
		if (f1.d == f2.d)
			return;
		f1.simplify();
		f2.simplify();
		auto f1d = f1.d;
		f1.bmul(f2.d);
		f2.bmul(f1d);
		return;
	}


	static ssize_t gcd(ssize_t num1, ssize_t num2)
	{
		if (num1 == 0 || num2 == 0)
			return 0;
		ssize_t remainder = num2 % num1;
		if (remainder != 0)
			return gcd(remainder, num1);
		return num1;
	}



	const FRACTION& simplify() const
	{
		if (n == 0)
		{
			d = 1;
			return *this;
		}
		ssize_t g = gcd(n, d);
		if (g == 0)
			return *this;
		n /= g;
		d /= g;
		return *this;
	}


	float r()
	{
		if (!d)
			return 0; // whops
		return (float)n / (float)d;
	}

	FRACTION(ssize_t nu = 0, ssize_t de = 1)
	{
		n = nu;
		d = de;
	}

	const FRACTION& bmul(ssize_t de) const
	{
		n *= de;
		d *= de;
		return *this;
	}

	FRACTION& operator +=(const FRACTION& a)
	{
		FRACTION::Om(*this, a);
		n += a.n;
		return *this;
	}
	FRACTION& operator -=(const FRACTION& a)
	{
		FRACTION::Om(*this, a);
		n -= a.n;
		return *this;
	}

	bool operator ==(const FRACTION& f)
	{
		Om(*this, f);
		if (n == f.n)
			return true;
		return false;
	}
	bool operator !=(const FRACTION& f)
	{
		return !operator==(f);
	}

	bool operator <(FRACTION& f)
	{
		Om(*this, f);
		if (n < f.n)
			return true;
		return false;
	}
	bool operator <=(FRACTION& f)
	{
		Om(*this, f);
		if (n <= f.n)
			return true;
		return false;
	}
	bool operator >(FRACTION& f)
	{
		return !operator<(f);
	}
	bool operator >=(FRACTION& f)
	{
		return !operator<=(f);
	}
};

FRACTION operator +(const FRACTION& a, const FRACTION& b)
{
	FRACTION::Om(a, b);
	FRACTION f(a.n + b.n, a.d);
	return f;
}
FRACTION operator -(const FRACTION& a, const FRACTION& b)
{
	FRACTION::Om(a, b);
	FRACTION f(a.n - b.n, a.d);
	return f;
}
FRACTION operator /(const FRACTION& a, const FRACTION& b)
{
	FRACTION f(a.n * b.d, a.d * b.n);
	return f.simplify();
}
bool operator <(const FRACTION& a, const FRACTION& b)
{
	FRACTION::Om(a, b);
	if (a.n < b.n)
		return true;
	return false;
}
bool operator >(const FRACTION& a, const FRACTION& b)
{
	return !operator <(a, b);
}



class POSITION
{
	public:
		size_t m = 0;
		FRACTION f;
		int noteht = 0;

		bool operator <(const POSITION& p)
		{
			if (m < p.m)
				return true;
			if (m > p.m)
				return false;
			if (f < p.f)
				return true;
			return false;
		}

		bool operator ==(const POSITION& p)
		{
			if (m != p.m)
				return false;
			if (f != p.f)
				return false;
			return true;
		}

		bool operator !=(const POSITION& p)
		{
			return !operator ==(p);
		}
		bool operator >(const POSITION& p)
		{
			return !operator <(p);
		}
};


class NOTE
{
public:

	int midi = 0;
	int Selected = 0;
	POSITION p;
	FRACTION d;
	int vel = 127;

	D2D1_RECT_F dr;
	
	POSITION EndX() const
	{
		POSITION pp = p;
		pp.f += d;
		return pp;
	}


	bool operator <(const NOTE& n2)
	{
		if (p < n2.p)
			return true;
		if (p > n2.p)
			return false;
		if (midi < n2.midi)
			return true;
		return false;
	}
};


class PIANOROLL
{
private:


	struct ASKTEXT
	{
		const wchar_t* ti;
		const wchar_t* as;
		wchar_t* re;
		int P;
		wstring* re2;
		int mx = 1000;
	};

	static INT_PTR CALLBACK A_DP(HWND hh, UINT mm, WPARAM ww, LPARAM ll)
	{
		static ASKTEXT* as = 0;
		switch (mm)
		{
		case WM_INITDIALOG:
		{
			as = (ASKTEXT*)ll;
			SetWindowText(hh, as->ti);
			if (as->P != 2)
			{
				SetWindowText(GetDlgItem(hh, 101), as->as);
				if (as->re)
					SetWindowText(GetDlgItem(hh, 102), as->re);
				if (as->re2)
					SetWindowText(GetDlgItem(hh, 102), as->re2->c_str());
			}
			else
				SetWindowText(GetDlgItem(hh, 701), as->as);
			if (as->P == 1)
			{
				auto w = GetWindowLongPtr(GetDlgItem(hh, 102), GWL_STYLE);
				w |= ES_PASSWORD;
				SetWindowLongPtr(GetDlgItem(hh, 102), GWL_STYLE, w);
			}
			return true;
		}
		case WM_COMMAND:
		{
			if (LOWORD(ww) == IDOK)
			{
				wchar_t re1[1000] = { 0 };
				wchar_t re2[1000] = { 0 };
				//					MessageBox(0, L"foo", 0, 0);
				if (as->P == 2)
				{
					GetWindowText(GetDlgItem(hh, 101), re1, 1000);
					GetWindowText(GetDlgItem(hh, 102), re2, 1000);
					if (wcscmp(re1, re2) != 0 || wcslen(re1) == 0)
					{
						SetWindowText(GetDlgItem(hh, 101), L"");
						SetWindowText(GetDlgItem(hh, 102), L"");
						SetFocus(GetDlgItem(hh, 101));
						return 0;
					}
					wcscpy_s(as->re, 1000, re1);
					EndDialog(hh, IDOK);
					return 0;
				}

				if (as->re2)
				{
					int lex = GetWindowTextLength(GetDlgItem(hh, 102));
					vector<wchar_t> re(lex + 100);
					GetWindowText(GetDlgItem(hh, 102), re.data(), lex + 100);
					*as->re2 = re.data();
					EndDialog(hh, IDOK);
				}
				else
				{
					GetWindowText(GetDlgItem(hh, 102), as->re, as->mx);
					EndDialog(hh, IDOK);
				}
				return 0;
			}
			if (LOWORD(ww) == IDCANCEL)
			{
				EndDialog(hh, IDCANCEL);
				return 0;
			}
		}
		}
		return 0;
	}

	inline bool AskText(HWND hh, const TCHAR * ti, const TCHAR * as, TCHAR * re, wstring * re2 = 0, int mxt = 1000)
	{
		const char* res = "\xC4\x0A\xCA\x90\x00\x00\x00\x00\x04\x00\x00\x00\x00\x00\x6D\x01\x3E\x00\x00\x00\x00\x00\x00\x00\x0A\x00\x54\x00\x61\x00\x68\x00\x6F\x00\x6D\x00\x61\x00\x00\x00\x01\x00\x00\x50\x00\x00\x00\x00\x0A\x00\x09\x00\x1C\x01\x1A\x00\x65\x00\xFF\xFF\x82\x00\x00\x00\x00\x00\x00\x00\x80\x00\x81\x50\x00\x00\x00\x00\x0A\x00\x29\x00\x1D\x01\x0F\x00\x66\x00\xFF\xFF\x81\x00\x00\x00\x00\x00\x00\x00\x00\x03\x01\x50\x00\x00\x00\x00\x2F\x01\x16\x00\x32\x00\x0E\x00\x01\x00\xFF\xFF\x80\x00\x4F\x00\x4B\x00\x00\x00\x00\x00\x00\x00\x00\x03\x01\x50\x00\x00\x00\x00\x2F\x01\x29\x00\x32\x00\x0E\x00\x02\x00\xFF\xFF\x80\x00\x43\x00\x61\x00\x6E\x00\x63\x00\x65\x00\x6C\x00\x00\x00\x00\x00";
		ASKTEXT a = { ti,as,re,0,re2,mxt };
		return (DialogBoxIndirectParam(GetModuleHandle(0), (LPCDLGTEMPLATEW)res, hh, A_DP, (LPARAM)& a) == IDOK);
	}

	HWND hParent = 0;
	vector<shared_ptr<PIANOROLLCALLBACK>> cb;


	class SIDEDRAW
	{
	public:
		signed int PercPos = 10; // If negative, at right
		D2D1_RECT_F full;

	};


	class TOPDRAW
	{
		public:
			signed int he = 15;
			D2D1_RECT_F full;
	};

	class HEIGHT
	{
	public:
		int AtPos = 0;
		float nh = 20.0;
	};

	class TIME
	{
	public:
		size_t atm = 0;
		int nb = 4;
		bool operator<(const TIME& t)
		{
			if (atm < t.atm)
				return true;
			return false;
		}
	};


	class DRAWNBEAT
	{
	public:
		D2D1_RECT_F full;
	};

	class DRAWNMEASURESANDBEATS
	{
	public:
		size_t im = 0;
		D2D1_RECT_F full;
		vector<DRAWNBEAT> Beats;
	};
	vector<DRAWNMEASURESANDBEATS> DrawnMeasures;

	class DRAWPIANO
	{
	public:
		int m = 0;
		D2D1_RECT_F full;
	};
	vector<DRAWPIANO> DrawnPiano;


	class DRAWNNOTES
	{
	public:
		int n;
		D2D1_RECT_F full;
	};
	vector<DRAWNNOTES> DrawedNotes;

	int DENOM = 4;
	float HeightScale = 1.0;
	float bw = 60.0f;
	float BarStroke = 1.0;
	float MeasureStroke = 2.0;
	D2D1_COLOR_F bg = { 0,0,0.1f,0.3f };
	D2D1_COLOR_F whitecolor = { 1.0f,1.0f,1.0f,1.0f };
	D2D1_COLOR_F blackcolor = { 0.0f,0.0f,0.0f,1.0f };
	D2D1_COLOR_F sidecolor = { 0,0.1f,0.1f,1.0f };
	D2D1_COLOR_F note1color = { 0.8f,0.8f,0.9f,0.5f };
	D2D1_COLOR_F note2color = { 0.8f,0.9f,0.0f,0.9f };
	D2D1_COLOR_F note3color = { 0.1f,0.2f,0.4f,0.3f };
	D2D1_COLOR_F note4color = { 0.1f,0.2f,0.4f,0.9f };
	D2D1_COLOR_F linecolor = { 0.5f,0.7f,0.3f,0.8f };
	D2D1_COLOR_F snaplinecolor = { 0.3f,0.6f,0.6f,0.8f };
	D2D1_COLOR_F scrollcolor = { 1.0f,1.0f,1.0f,0.3f };
	int Direction = 0; // 0 up, 1 down
	int FirstNote = 48;
	int ScrollX = 0;
	int Key = 0;
	int Mode = 0;

	vector<int> Scale;
	bool BelongsToScale(int m, vector<int>& s)
	{
		if (std::find(s.begin(), s.end(), m % 12) == s.end())
			return false;
		return true;
	}
	void CreateScale(int k, int m, vector<int>& midis)
	{
		midis.clear();
		unsigned int fi = 0x48; // C
		if (k > 0)
			fi = (7 * k)%12;

		if (m == 1)
			fi -= 3;

		fi = fi % 12;
		if (m == 1)
		{
			Scale.push_back(fi);
			Scale.push_back(fi + 2);
			Scale.push_back(fi + 3);
			Scale.push_back(fi + 5);
			Scale.push_back(fi + 7);
			Scale.push_back(fi + 8);
			Scale.push_back(fi + 11);
		}
		else
		if (m == 0)
			{
			Scale.push_back(fi);
			Scale.push_back(fi + 2);
			Scale.push_back(fi + 4);
			Scale.push_back(fi + 5);
			Scale.push_back(fi + 7);
			Scale.push_back(fi + 9);
			Scale.push_back(fi + 11);

		}
		for (auto& e : Scale)
			e = e % 12;
		/*
		int c = 0, d = 2, e = 4, f = 5, g = 7, a = 9, b = 11;



		if (k >= 1) f++;
		if (k >= 2) c++;
		if (k >= 3) g++;
		if (k >= 4) d++;
		if (k >= 5) a++;
		if (k >= 6) e++;
		if (k >= 7) b++;

		if (k <= -1) b--;
		if (k <= -2) e--;
		if (k <= -3) a--;
		if (k <= -4) d--;
		if (k <= -5) g--;
		if (k <= -6) c--;
		if (k <= -7) f--;


		midis.push_back(c);
		midis.push_back(d);
		midis.push_back(e);
		midis.push_back(f);
		midis.push_back(g);
		midis.push_back(a);
		midis.push_back(b);
		std::sort(midis.begin(), midis.end());

	//	if (m == 1)
		//	midis[6]++;
	*/}

	vector<NOTE> notes;
	vector<NOTE> clip;
	stack<vector<NOTE>> undo;
	stack<vector<NOTE>> redo;
	RECT rdr;
	size_t TotalWidthForMusic = 0;
	size_t LeftWidthForMusic = 0;
	vector<HEIGHT> Heights = {HEIGHT()};
	vector<TIME> Times = { TIME() };
	CComPtr<ID2D1SolidColorBrush> SideBrush;
	CComPtr<ID2D1SolidColorBrush> LineBrush;
	CComPtr<ID2D1SolidColorBrush> WhiteBrush;
	CComPtr<ID2D1SolidColorBrush> BlackBrush;
	CComPtr<ID2D1SolidColorBrush> SnapLineBrush;
	CComPtr<ID2D1SolidColorBrush> ScrollBrush;
	CComPtr<ID2D1SolidColorBrush> NoteBrush1;
	CComPtr<ID2D1SolidColorBrush> NoteBrush2;
	CComPtr<ID2D1SolidColorBrush> NoteBrush3;
	CComPtr<ID2D1SolidColorBrush> NoteBrush4;
	HCURSOR CursorArrow = 0, CursorResize = 0;
	unsigned int snapres = 1;
	signed int noteres = 1; // bars 
	SIDEDRAW side;
	TOPDRAW top;
	int MaxUndoLevel = 100;
	int BarMoving = false;
	int Selecting = false;
	NOTE* NoteDragging = 0;
	NOTE* NoteResizing = 0;
	NOTE NoteResizingSt;
	bool ResizingRight = 0; // 1 right, 0 left
	int PianoClicking = false;
	POINT LastClick;
	int PianoNoteClicked = -1;
	POSITION LastClickN;
	D2D1_RECT_F SelectRect;

	CComPtr<ID2D1SolidColorBrush> GetD2SolidBrush(ID2D1RenderTarget*p,D2D1_COLOR_F cc)
	{
		CComPtr<ID2D1SolidColorBrush> b = 0;
		p->CreateSolidColorBrush(cc, &b);
		return b;
	}

	void PushUndo()
	{
		undo.push(notes);
		redo = stack<vector<NOTE>>();
	}

	template <typename T = float> bool InRect(D2D1_RECT_F& r, T x, T y)
	{
		if (x >= r.left && x <= r.right && y >= r.top && y <= r.bottom)
			return true;
		return false;
	}

	DRAWNMEASURESANDBEATS* DrawnMeasureByIndex(size_t idx)
	{
		auto e = std::find_if(DrawnMeasures.begin(), DrawnMeasures.end(), [&](const DRAWNMEASURESANDBEATS& m) -> BOOL
			{
				if (m.im == idx)
					return true;
				return false;
			});
		if (e == DrawnMeasures.end())
			return nullptr;
		return &*e;
	}



public:

	PIANOROLL(HWND hp = 0)
	{
		hParent = hp;
		CreateScale(Key, Mode, Scale);
		CursorResize = LoadCursor(0, IDC_SIZEWE);
		CursorArrow = LoadCursor(0, IDC_HAND);
	}

	void SetWindow(HWND hp)
	{
		hParent = hp;
	}
	void Undo()
	{
		if (undo.empty())
			return;
		redo.push(notes);
		notes = undo.top();
		undo.pop();
		Redraw();
	}

	void Redo()
	{
		if (redo.empty())
			return;
		undo.push(notes);
		notes = redo.top();
		redo.pop();
		Redraw();
	}


	void Redraw(unsigned long long p = 0)
	{
		for (auto c : cb)
			c->RedrawRequest(this,p);
	}


	void AddCallback(shared_ptr<PIANOROLLCALLBACK> c)
	{
		cb.push_back(c);
	}


	size_t AbsMeasure(size_t im)
	{
		size_t nb = 0;
		for (size_t i = 0; i < im; i++)
		{
			auto t = TimeAtMeasure(i);
			nb += t.nb;
		}
		return nb;
	}

	FRACTION AbsF(POSITION& p)
	{
		auto nb = AbsMeasure(p.m);
		FRACTION f(nb, DENOM);
		f += p.f;
		return f;
	}

	POSITION MeasureAndBarHitTest(float width,bool Precise = false)
	{
		POSITION np;
		np.m = (size_t)-1;

		for (auto& dd : DrawnMeasures)
		{
			if (dd.full.left > width)
				continue;
			if (dd.full.right < width)
				continue;

			// This is it
			np.m = dd.im;
			for (size_t y = 0; y < dd.Beats.size(); y++)
			{
				auto& b = dd.Beats[y];
				if (b.full.left > width)
					continue;
				if (b.full.right < width)
					continue;
//				np.beatht = y;

				// Calculate Also the fraction
				if (Precise)
				{
					np.f.d = (int)(dd.full.right - dd.full.left);
					np.f.n = (int)(width - dd.full.left);
					break;
				}
				else
				{
					np.f.d = DENOM*snapres;
					float widthpersnap = (dd.full.right - dd.full.left) / np.f.d;
					for (auto nn = 0; nn < np.f.d; nn++)
					{
						if (width >= ((nn * widthpersnap) + dd.full.left))
							np.f.n = nn;
						else
							break;
					}
				}

				return np;
			}
		}

		return np;
	}

	int MidiHitTest(float height)
	{
		for (size_t i = 0; i < DrawedNotes.size(); i++)
		{
			auto& dd = DrawedNotes[i];
			if (dd.full.top > height)
				continue;
			if (dd.full.bottom < height)
				continue;

			return dd.n;
		}
		return -1;
	}

	float HeightAtNote(int n)
	{
		if (Heights.size() == 1)
			return Heights[0].nh*HeightScale;

		return 0.0f; //*
	}

	void ConvertPositionToNewMeasure(POSITION& p)
	{
		auto f1 = AbsF(p);
		size_t im = 0;
		for (; ; im++)
		{
			auto ti = TimeAtMeasure(im);
			FRACTION bt(ti.nb,DENOM);
			if (f1 < bt)
				break;
			f1 -= bt;
		}
		p.m = im;
		p.f = f1;
	}


	TIME TimeAtMeasure(size_t im)
	{
		TIME tt;
		for (auto& t : Times)
		{
			if (t.atm <= im)
				tt = t;
		}
		return tt;
	}

	D2D1_RECT_F NotePos(int n)
	{
		D2D1_RECT_F d;
		
		d.left = (FLOAT)rdr.left;
		d.right = (FLOAT)rdr.right;

		if (Direction == 0)
		{
			float NoteBottomX = (FLOAT)rdr.bottom;
			for (auto j = FirstNote; j < n; j++)
			{
				NoteBottomX -= HeightAtNote(j);
			}
			float NoteTopX = NoteBottomX - HeightAtNote(n);
			d.bottom = NoteBottomX;
			d.top = NoteTopX;
		}

		return d;
	}

	size_t NoteAtPos(int x, int y,bool ResizeEdge = false,bool* Right = 0)
	{
/*		bool Shift = ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
		if (Shift)
			DebugBreak();
*/
		for (size_t i = 0 ; i < notes.size() ; i++)
		{
			auto& n = notes[i];
			if (ResizeEdge)
			{
				if (abs(n.dr.left - x) > 5 && abs(n.dr.right - x) > 5)
					continue;
				if (Right)
				{
					*Right = false;
					if (abs(n.dr.right - x) < 5)
						*Right = true;
				}

			}
			else
			{
				if (n.dr.left > x)
					continue;
				if (n.dr.right < x)
					continue;
			}
			if (n.dr.top > y)
				continue;
			if (n.dr.bottom < y)
				continue;
			return i;
		}
		return (size_t)-1;
	}

	void KeyDown(WPARAM ww, LPARAM,bool S = false,bool C = false,bool A = false)
	{
		bool Shift = ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
		bool Control = ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
		bool Alt = ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0);
		if (S)
			Shift = true;
		if (C)
			Control = true;
		if (A)
			Alt = true;

		if (ww == 191) // /
		{
			bool R = false;
			// Split notes
			for (size_t i = 0; i < notes.size(); i++)
			{
				if (!notes[i].Selected)
					continue;

				auto e = notes[i].EndX();
				ConvertPositionToNewMeasure(e);
				//auto e2 = AbsF(e);
				if (!R)
					PushUndo();
				R = true;
				notes[i].d.n++;

			}
			if (R)
				Redraw();
			return;
		}

		if (ww == 'S')
		{
			vector<NOTE> add;
			bool R = false;
			// Split notes
			for (size_t i = 0; i < notes.size(); i++)
			{
				if (!notes[i].Selected)
					continue;

				// Change duration
				// If there is a beat, cut it there
				// Else half
				if (!R)
					PushUndo();
				R = true;
				auto n2 = notes[i];


			}

			if (add.empty())
				return;
			notes.insert(notes.end(), add.begin(), add.end());
			std::sort(notes.begin(), notes.end());
			Redraw();
			return;
		}

		if (ww == 'J')
		{
			bool R = false;
			// Join notes
			for (size_t i = 0; i < notes.size(); i++)
			{
				if (!notes[i].Selected)
					continue;
				for (size_t y = (i + 1); y < notes.size(); y++)
				{
					if (!notes[y].Selected)
						continue;

					if (notes[i].midi != notes[y].midi)
						continue;

					// Check gap
					if (notes[i].EndX() != notes[y].p)
						continue;

					if (!R)
						PushUndo();
					notes[i].d += notes[y].d;
					notes.erase(notes.begin() + y);
					R = true;
					break;
				}
			}
			if (R)
				Redraw();
			return;
		}


		if (ww >= '1' && ww <= '6' )
		{
			if (Control)
			{
				snapres = (unsigned int)ww - '1' + 1;
				Redraw();
				return;
			}
		}

		if (ww >= '1' && ww <= '4')
		{
			if (!Control)
			{
				if (ww == '1') noteres = -1;
				if (ww == '2') noteres = -2;
				if (ww == '3') noteres = -3;
				if (ww == '4') noteres = -4;
			}
		}

		if (ww >= '1' && ww <= '4')
		{
			if (Shift)
			{
				if (ww == '2') noteres = 2;
				if (ww == '3') noteres = 3;
				if (ww == '4') noteres = 4;
			}
		}


		if (ww == 'A' && Control)
		{
			for (auto& n : notes)
				n.Selected = true;
			Redraw();
			return;
		}
		if (ww == 'V' && Control)
		{
			if (clip.empty() || LastClickN.m == -1)
				return;

			NOTE first = clip[0];
			int di = first.midi - LastClickN.noteht;
			size_t im = first.p.m - LastClickN.m;
			PushUndo();
			for (auto& n : clip)
			{
				NOTE n2 = n;
				// Note, and position that changes
				n2.midi -= di;
				n2.p.m -= im;
				notes.push_back(n2);
			}

			std::sort(notes.begin(), notes.end());
			Redraw();
			return;
		}
		if (ww == 'C' && Control)
		{
			clip.clear();
			for (auto& n : notes)
			{
				if (n.Selected)
				{
					clip.push_back(n);
					n.Selected = false;
				}
			}
			Redraw();
			return;
		}
		if (ww == 'Z' && Control)
		{
			Undo();
			return;
		}
		if (ww == 'Y' && Control)
		{
			Redo();
			return;
		}
		if (ww == VK_RIGHT)
		{
			ScrollX += 10;
			Redraw();
			return;
		}
		if (ww == VK_LEFT)
		{
			if (ScrollX >= 10)
				ScrollX -= 10;
			Redraw();
			return;
		}
		if (ww == VK_UP)
		{
			if (FirstNote >= 110)
				return;
			FirstNote++;
			Redraw();
			return;
		}
		if (ww == VK_DOWN)
		{
			if (FirstNote <= 0)
				return;
			FirstNote--;
			Redraw();
			return;
		}
		if (ww == VK_MULTIPLY)
		{
			HeightScale = 1.0f;
			bw = 60;
			Redraw();
			return;
		}


		if (ww == 188 || ww == 190)
		{
			bool R = false;
			bool U = false;
			for (auto& n : notes)
			{
				if (n.Selected)
				{
					NOTE nn = n;
					if (Control && ww == 188)
						nn.vel = 0;
					if (Control && ww == 190)
						nn.vel = 127;
					if (!Shift && nn.vel >= 5 && ww == 188)
						nn.vel -= 5;
					if (Shift && nn.vel > 0 && ww == 188)
						nn.vel--;
					if (Shift && nn.vel < 127 && ww == 190)
						nn.vel++;
					if (!Shift && nn.vel < 123 && ww == 190)
						nn.vel += 5;
					for (auto c : cb)
					{
						if (FAILED(c->OnNoteChange(this, &n, &nn)))
							return;
					}
					if (!U)
						PushUndo();
					U = true;
					R = true;
					n.vel = nn.vel;
				}
			}
			if (R)
				Redraw();
			return;
		}


		if (ww == VK_ADD || ww == VK_SUBTRACT || ww == VK_OEM_PLUS || ww == VK_OEM_MINUS)
		{
			bool R = false;
			bool U = false;
			for (auto& n : notes)
			{
				if (n.Selected)
				{
					NOTE nn = n;
					if ((ww == VK_ADD || ww == VK_OEM_PLUS))
					{
						if (Shift)
							nn.midi++;
						else
						{
							for (;;)
							{
								nn.midi++;
								if (BelongsToScale(nn.midi, Scale))
									break;
							}
						}
					}
					if ((ww == VK_SUBTRACT || ww == VK_OEM_MINUS))
						if (Shift)
							nn.midi--;
						else
						{
							for (;;)
							{
								nn.midi--;
								if (BelongsToScale(nn.midi, Scale))
									break;
							}
						}
					if (nn.midi < 0)
						nn.midi = 0;
					if (nn.midi > 127)
						nn.midi = 127;
					if (n.midi == nn.midi)
						continue;
					for (auto c : cb)
					{
						if (FAILED(c->OnNoteChange(this, &n,&nn)))
							return;
					}
					if (!U)
						PushUndo();
					U = true;
					R = true;
					n.midi = nn.midi;
				}
			}
			if (!R)
			{
				// No Note selected, so zoom
				if (Shift)
				{
					if (ww == VK_ADD || ww == VK_OEM_PLUS)
						HeightScale += 0.1f;
					if (ww == VK_SUBTRACT || ww == VK_OEM_MINUS)
					{
						if (HeightScale > 0.4f)
							HeightScale -= 0.1f;
					}
				}
				else
				{
					if (ww == VK_ADD || ww == VK_OEM_PLUS)
						bw += 10;
					if (ww == VK_SUBTRACT || ww == VK_OEM_MINUS)
					{
						if (bw > 10)
							bw -= 10;
					}
				}
			}
			Redraw();
			return;

		}
		if (ww == VK_DELETE)
		{
			bool R = false;
			bool U = false;
			for (size_t i = 0 ; i < notes.size() ; i++)
			{
				if (notes[i].Selected)
				{
					NOTE nn = notes[i];
					nn.midi = -1;
					for (auto c : cb)
					{
						if (FAILED(c->OnNoteChange(this,&notes[i], &nn)))
							return;
					}
					R = true;
					if (!U)
						PushUndo();
					U = true;
					notes.erase(notes.begin() + i);
					i = (size_t)-1;
				}
			}
			if (!R)
				return;
			Redraw();
			return;

		}
	}

	void MouseMove(WPARAM, LPARAM ll)
	{
		bool Shift = ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
		bool Control = ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
		int xx = LOWORD(ll);
		int yy = HIWORD(ll);
		if (Selecting)
		{
			D2D1_RECT_F& d3 = SelectRect;
			d3.left = (FLOAT)LastClick.x;
			d3.right = (FLOAT)xx;
			if (xx < LastClick.x)
			{
				d3.right = (FLOAT)LastClick.x;
				d3.left = (FLOAT)xx;
			}
			d3.top = (FLOAT)LastClick.y;
			d3.bottom = (FLOAT)yy;
			if (yy < LastClick.y)
			{
				d3.bottom = (FLOAT)LastClick.y;
				d3.top = (FLOAT)yy;
			}

			for (auto& n : notes)
			{
				if (InRect(SelectRect, n.dr.right, n.dr.bottom))
					n.Selected = true;
				else
				if (InRect(SelectRect,(FLOAT)n.dr.left, (FLOAT)n.dr.top))
					n.Selected = true;
				else
				{
					if (!Shift)
						n.Selected = false;
				}
			}

			Redraw();
			return;
		}
		if (BarMoving)
		{
			int nx = LOWORD(ll);
			ScrollX += nx - LastClick.x;
			if (ScrollX < 0)
				ScrollX = 0;
			LastClick.x = nx;
			Redraw(0);
		}
		// Side
		if (PianoClicking)
		{
			// Find the note, back search
			for (int y = (int)DrawnPiano.size() - 1; y >= 0; y--)
			{
				auto& pd = DrawnPiano[y];
				if (InRect(pd.full, LOWORD(ll), HIWORD(ll)))
				{
					if (PianoNoteClicked != pd.m)
					{
						if (PianoNoteClicked != -1)
						{
							for (auto c : cb)
								c->OnPianoOff(this, PianoNoteClicked);
						}
						PianoNoteClicked = pd.m;
						if (PianoNoteClicked != -1)
						{
							for (auto c : cb)
								c->OnPianoOn(this, PianoNoteClicked);
						}

						Redraw(2);
					}
					break;
				}
			}

			return;
		}

		if (NoteDragging)
		{
			auto hp = MeasureAndBarHitTest((float)xx);
			NOTE nx = *NoteDragging;
			nx.p.m = hp.m;
			nx.p.f = hp.f;

			int np = MidiHitTest((float)yy);
			nx.midi = np;
			for (auto c : cb)
			{
				if (FAILED(c->OnNoteChange(this, NoteDragging,&nx)))
					return;
			}
			*NoteDragging = nx;
			Redraw();
		}

		if (NoteResizing)
		{
			SetCursor(CursorResize);
#ifdef _DEBUG
		///	if (Shift) DebugBreak();
#endif

			auto hp = MeasureAndBarHitTest((float)xx,Shift);
			if (ResizingRight)
			{
#ifdef _DEBUG
				if (Control) DebugBreak();
#endif
				// Enlarge note
				if (!Shift)
					hp.f.n++; // Add a beat
				if (hp.m < NoteResizing->p.m)
					return; // Back Measure
				if (hp.m == NoteResizing->p.m && hp.f < NoteResizing->p.f)
					return; // Back Inside
				NOTE nn = *NoteResizing;
				auto a1 = AbsF(hp);
				auto a2 = AbsF(NoteResizing->p);
				
#ifdef _DEBUG
//				wchar_t a[100];	a1.simplify();	a2.simplify();	swprintf_s(a, 100, L"%u/%u %u/%u \r\n", a1.n, a1.d, a2.n, a2.d);	OutputDebugString(a);
#endif
				nn.d = a1 - a2;
				if (nn.d.n <= 0)
					return;
				for (auto c : cb)
				{
					if (FAILED(c->OnNoteChange(this, NoteResizing, &nn)))
						return;
				}
				nn.d.simplify();
				*NoteResizing = nn;
				Redraw();
			}
			else
			{
				// Change Position
				auto oldp = NoteResizing->p;
				auto endp = AbsF(oldp) + NoteResizing->d;
				auto newd = endp - AbsF(hp);
				if (newd.n == 0)
					return;
				FRACTION mx;
				mx = (AbsF(NoteResizingSt.p) + NoteResizingSt.d);
				if (AbsF(hp) > mx)
					return;
				//DebugBreak();
				NOTE nn = *NoteResizing;
				nn.d  = newd.simplify();
				nn.p = hp;
				for (auto c : cb)
				{
					if (FAILED(c->OnNoteChange(this, NoteResizing, &nn)))
						return;
				}
				NoteResizing->p = hp;
				NoteResizing->d = newd.simplify();
				Redraw();
			}


			return;
		}

		// Cursor pos for resizing
		auto ni = NoteAtPos(xx,yy,true);
		if (ni != -1)
		{
			SetCursor(CursorResize);
		}
		else
			SetCursor(CursorArrow);


	}

	void LeftUp(WPARAM, LPARAM)
	{
		if (PianoNoteClicked != -1)
		{
			for (auto c : cb)
				c->OnPianoOff(this, PianoNoteClicked);
		}
		if (NoteResizing || NoteDragging)
			std::sort(notes.begin(), notes.end());
		NoteResizing = 0;
		NoteDragging = 0;
		Selecting = false;
		SelectRect.left = SelectRect.top = SelectRect.right = SelectRect.bottom = 0;
		PianoNoteClicked = -1;
		PianoClicking = false;
		BarMoving = false;
		Redraw();
	}

	void RightDown(WPARAM, LPARAM ll)
	{
		auto hp = MeasureAndBarHitTest(LOWORD(ll));
		LastClick.x = LOWORD(ll);
		LastClick.y = HIWORD(ll);

		// Selected ?
		bool S = false;
		bool Need = false;
		auto ni = NoteAtPos(LOWORD(ll), HIWORD(ll));
		if (ni != -1)
		{
			if (notes[ni].Selected == false)
				Need = true;
			notes[ni].Selected = true;
			S = true;
		}
		else
		{
			// Other selected?
			for (auto& n : notes)
			{
				if (n.Selected)
				{
					S = true;
					break;
				}
			}
		}


		if (S)
		{
			// Selected menu
			auto m = CreatePopupMenu();
			AppendMenu(m, MF_STRING, 1, L"Delete\tDel");
			AppendMenu(m, MF_SEPARATOR, 0, L"");
			AppendMenu(m, MF_STRING, 3, L"Move Down\t-");
			AppendMenu(m, MF_STRING, 2, L"Move Up\t+");
			AppendMenu(m, MF_SEPARATOR, 0, L"");
			AppendMenu(m, MF_STRING, 11, L"Velocity Down\t<");
			AppendMenu(m, MF_STRING, 12, L"Velocity Up\t>");
			POINT p;
			GetCursorPos(&p);
			int tcmd = TrackPopupMenu(m, TPM_CENTERALIGN | TPM_RETURNCMD, p.x, p.y, 0, hParent , 0);
			DestroyMenu(m);
			if (tcmd == 1)
			{
				KeyDown(VK_DELETE, 0);
			}
			if (tcmd == 2)
			{
				KeyDown(VK_ADD, 0);
			}
			if (tcmd == 3)
			{
				KeyDown(VK_SUBTRACT, 0);
			}
			if (tcmd == 11)
			{
				KeyDown(188, 0);
			}
			if (tcmd == 12)
			{
				KeyDown(190, 0);
			}
		}
		else
		{
			// Selected menu
			auto m = CreatePopupMenu();
			wchar_t re[1000] = { 0 };
			swprintf_s(re, L"Key...(Current: %i)\t", Key);
			AppendMenu(m, MF_STRING, 1, re);
			AppendMenu(m, MF_STRING, 2, L"Mode Major");
			AppendMenu(m, MF_STRING, 3, L"Mode Minor");
			AppendMenu(m, MF_SEPARATOR, 0, L"");
			swprintf_s(re, L"Beats (From now on)\t");
			AppendMenu(m, MF_STRING, 4, re);
			swprintf_s(re, L"Beats (This Measure only)\t" );
			AppendMenu(m, MF_STRING, 5, re);
			AppendMenu(m, MF_SEPARATOR, 0, L"");
			swprintf_s(re, L"Snap Resolution /1\tCtrl+1");
			AppendMenu(m, MF_STRING, 21, re);
			swprintf_s(re, L"Snap Resolution /2\tCtrl+2");
			AppendMenu(m, MF_STRING, 22, re);
			swprintf_s(re, L"Snap Resolution /3\tCtrl+3");
			AppendMenu(m, MF_STRING, 23, re);
			swprintf_s(re, L"Snap Resolution /4\tCtrl+4");
			AppendMenu(m, MF_STRING, 24, re);
			swprintf_s(re, L"Snap Resolution /5\tCtrl+5");
			AppendMenu(m, MF_STRING, 25, re);
			swprintf_s(re, L"Snap Resolution /6\tCtrl+6");
			AppendMenu(m, MF_STRING, 26, re);
			AppendMenu(m, MF_SEPARATOR, 0, L"");
			swprintf_s(re, L"Next Note 1 Beat\t1");
			AppendMenu(m, MF_STRING, 31, re);
			swprintf_s(re, L"Next Note 2 Beats\t2");
			AppendMenu(m, MF_STRING, 32, re);
			swprintf_s(re, L"Next Note 3 Beats\t3");
			AppendMenu(m, MF_STRING, 33, re);
			swprintf_s(re, L"Next Note 4 Beats\t4");
			AppendMenu(m, MF_STRING, 34, re);
			swprintf_s(re, L"Next Note 1/2 Beat\tShift+2");
			AppendMenu(m, MF_STRING, 42, re);
			swprintf_s(re, L"Next Note 1/3 Beat\tShift+3");
			AppendMenu(m, MF_STRING, 43, re);
			swprintf_s(re, L"Next Note 1/4 Beat\tShift+4");
			AppendMenu(m, MF_STRING, 44, re);
			POINT p;
			GetCursorPos(&p);
			int tcmd = TrackPopupMenu(m, TPM_CENTERALIGN | TPM_RETURNCMD, p.x, p.y, 0, hParent, 0);
			DestroyMenu(m);
			if (tcmd == 1)
			{
				swprintf_s(re, L"%i", Key);
				if (!AskText(hParent, L"Key", L"Enter key:", re))
					return;
				Key = _wtoi(re);
				CreateScale(Key, Mode, Scale);
			}
			if (tcmd == 2 || tcmd == 3)
			{
				Mode = (tcmd == 2) ? 0 : 1;
				CreateScale(Key, Mode, Scale);
			}
			if (tcmd == 4)
			{
				swprintf_s(re, L"%i", Key);
				if (!AskText(hParent, L"Beats", L"Enter beats:", re))
					return;
				int nb = _wtoi(re);
				if (!nb)
					return;

				TIME t;
				t.nb = nb;
				t.atm = hp.m;
				Times.push_back(t);
				std::sort(Times.begin(), Times.end());
				Redraw();
			}
			if (tcmd >= 21 && tcmd <= 26)
			{
				snapres = tcmd - 20;
				Redraw();
			}
			if (tcmd == 31) noteres = -1;
			if (tcmd == 32) noteres = -2;
			if (tcmd == 33) noteres = -3;
			if (tcmd == 34) noteres = -4;

			if (tcmd == 42) noteres = 2;
			if (tcmd == 43) noteres = 3;
			if (tcmd == 44) noteres = 4;

		}


		if (Need)
			Redraw();
	}


	void LeftDown(WPARAM , LPARAM ll)
	{
		Selecting = false;
		int xx = LOWORD(ll);
		int yy = HIWORD(ll);
		LastClickN = MeasureAndBarHitTest((FLOAT)xx);
		LastClickN.noteht = MidiHitTest((FLOAT)yy);
		LastClick.x = xx;
		LastClick.y = yy;
		// Bar
		if (InRect(top.full, xx,yy))
		{
			BarMoving = true;
			return;
		}

		// Side
		if (InRect(side.full, LOWORD(ll), HIWORD(ll)))
		{
			PianoClicking = true;

			// Find the note, back search
			for (int y = (int)DrawnPiano.size() - 1; y >= 0; y--)
			{
				auto& pd = DrawnPiano[y];
				if (InRect(pd.full, LOWORD(ll), HIWORD(ll)))
				{
					if (PianoNoteClicked != -1)
					{
						for (auto c : cb)
							c->OnPianoOff(this,PianoNoteClicked);
					}
					PianoNoteClicked = pd.m;
					if (PianoNoteClicked != -1)
					{
						for (auto c : cb)
							c->OnPianoOn(this, PianoNoteClicked);
					}
					break;
				}
			}

			Redraw(2);
			return;
		}

		// Find note there
		bool Need = false;
		auto ni = NoteAtPos(xx, yy);
		auto ni2 = NoteAtPos(xx, yy,true,&ResizingRight);

		if (ni2 != -1)
		{
			NoteResizing = &notes[ni2];
			NoteResizingSt = *NoteResizing;
			SetCursor(CursorResize);
			return;
		}

		// Deselect
		if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == 0)
		{
			for (size_t i = 0; i < notes.size(); i++)
			{
				auto& n = notes[i];
				if (i == ni)
					continue;
				if (n.Selected)
				{
					Need = true;
					n.Selected = false;
					for (auto c : cb)
						c->OnNoteSelect(this, &n, 0);
				}
			}
		}
		if (ni != -1)
		{
			notes[ni].Selected = !notes[ni].Selected;
			Need = true;
			NoteDragging = &notes[ni];
			for (auto c : cb)
				c->OnNoteSelect(this, &notes[ni], notes[ni].Selected);
		}
		else
			Selecting = true;

		if (Need)
		{
			Redraw();
		}
	}

	void LeftDoubleClick(WPARAM, LPARAM ll) 
	{
		bool U = false;
		// Find note there
		auto ni = NoteAtPos(LOWORD(ll), HIWORD(ll));
		if (ni != -1)
		{
			for (auto c : cb)
			{
				if (FAILED(c->NoteRemoved(this,&notes[ni])))
					return;
			}
			if (!U)
				PushUndo();
			U = true;
			notes.erase(notes.begin() + ni);
			Redraw();
			return;
		}

		auto e1 = MeasureAndBarHitTest(LOWORD(ll));
		auto e2 = MidiHitTest(HIWORD(ll));
		if (e1.m != -1 && e2 > 0)
		{
			auto msr = DrawnMeasureByIndex(e1.m);
			if (!msr)
				return;
			// Insert note
			NOTE nx;
			nx.midi = e2;
			nx.p.m = e1.m;
			nx.p.f = e1.f;
			if (noteres < 0)
			{
				nx.d.n = abs(noteres);
				nx.d.d = DENOM;
			}
			else
			{
				nx.d.n = 1;
				if (noteres == 1)
					nx.d.d = DENOM*1;
				if (noteres == 2)
					nx.d.d = DENOM*2;
				if (noteres == 3)
					nx.d.d = DENOM*3;
				if (noteres == 4)
					nx.d.d = DENOM*4;
			}

			for (auto c : cb)
			{
				if (FAILED(c->NoteAdded(this,&nx)))
					return;
			}
			if (!U)
				PushUndo();
			U = true;
			notes.push_back(nx);
			std::sort(notes.begin(), notes.end());
			Redraw();
		}
	}


	void PaintSide(ID2D1RenderTarget* p,RECT rc)
	{
		DrawnPiano.clear();
		// Full
		if (side.PercPos == 0)
			return;
		if (side.PercPos > 0)
		{
			side.full.left = (FLOAT)rc.left;
			side.full.top = (FLOAT)rc.top;
			side.full.bottom = (FLOAT)rc.bottom;
			side.full.right = (side.PercPos * rc.right) / 100.0f;
		}
		else
		{
			side.full.left = (FLOAT)rc.right - (rc.right*(-side.PercPos))/100;
			side.full.top = (FLOAT)rc.top;
			side.full.bottom = (FLOAT)rc.bottom;
			side.full.right = (FLOAT)rc.right;
		}

		if (!SideBrush)
			SideBrush = GetD2SolidBrush(p, sidecolor);

		p->FillRectangle(side.full, SideBrush);

		// White keys
		float HeightPerNote = HeightAtNote(0);
		float TotalOct = HeightPerNote * 12;
		float WhiteSize = TotalOct / 7.0f;
		float LeftWas = 0;
		for (auto& a : DrawedNotes)
		{
			int m = a.n % 12;
			//auto height = a.full.bottom - a.full.top;
			if (m == 0 || m == 2 || m == 4 || m == 5 || m == 7 || m == 9 || m == 11)
			{
				auto d2 = side.full;
				d2.top = a.full.top;
				d2.bottom = a.full.bottom;

				if (m == 0)
				{
					// C
					d2.top = d2.bottom - WhiteSize;
					LeftWas = d2.top;
				}
				else
				{
					d2.bottom = LeftWas;
					d2.top = d2.bottom - WhiteSize;
					LeftWas = d2.top;
				}

				d2.top += 1;
				d2.bottom -= 1;

/*				//if (m > 6)
					//continue;
				if (m == 0) // C 
				{
					d2.top -= 2*height / 3;
					d2.top += 1;
				}
				
				if (m == 2) // D
				{
					d2.top -= height / 3;
					d2.top += 1;
					d2.bottom += height/3;
					d2.bottom -= 1;
				}
				if (m == 4) // E
				{
					d2.top += 1;
					d2.bottom += 2*height / 3;
					d2.bottom -= 1;
				}

				if (m == 5) // F 
				{
					d2.top -= 2 * height / 3;
					d2.top += 1;
					d2.bottom -= 1;
				}
				if (m == 7) // G
				{
					d2.top -= height / 3;
					d2.top += 1;
					d2.bottom += height / 3;
					d2.bottom -= 1;
				}
				if (m == 9) // D
				{
					d2.top -= height / 3;
					d2.top += 1;
					d2.bottom += height / 3;
					d2.bottom -= 1;
				}
				if (m == 11) // E
				{
					d2.top += 2;
					d2.bottom += 2 * height / 3;
					d2.bottom -= 1;
				}
*/

				DRAWPIANO dp;
				dp.full = d2;
				dp.m = a.n;
				DrawnPiano.push_back(dp);
				if (a.n == PianoNoteClicked && PianoClicking)
					p->FillRectangle(d2, LineBrush);
				else
					p->FillRectangle(d2, WhiteBrush);
			}

		}
		for (auto& a : DrawedNotes)
		{
			int m = a.n % 12;
			//auto height = a.full.bottom - a.full.top;
			if (m == 1 || m == 3 || m == 6 || m == 8 || m == 10)
			{
				auto d2 = side.full;
				d2.top = a.full.top;
				d2.bottom = a.full.bottom;
				d2.bottom += 1;
				d2.top -= 1;
				if (side.PercPos > 0)
					d2.right = 2 * (d2.right - d2.left) / 3;
				else
					d2.left += 1 * (d2.right - d2.left) / 3;

				if (a.n == PianoNoteClicked && PianoClicking)
					p->FillRectangle(d2, LineBrush);
				else
					p->FillRectangle(d2, BlackBrush);
				DRAWPIANO dp;
				dp.full = d2;
				dp.m = a.n;
				DrawnPiano.push_back(dp);
			}

		}
	}

	void PaintTop(ID2D1RenderTarget* p, RECT rc)
	{
		// Full
		if (top.he == 0)
			return;
		if (top.he > 0)
		{
			top.full.left = (FLOAT)rc.left;
			if (side.PercPos > 0)
				top.full.left = side.full.right;
			top.full.top = (FLOAT)rc.top;
			top.full.right = (FLOAT)rc.right;
			top.full.bottom = top.full.top + top.he;
		}
		else
		{
			top.full.top = (FLOAT)rc.bottom + top.he;
			top.full.left = (FLOAT)rc.left;
			if (side.PercPos > 0)
				top.full.left = side.full.right;
			top.full.bottom = (FLOAT)rc.bottom;
			top.full.right = (FLOAT)rc.right;
		}

		if (!ScrollBrush)
			ScrollBrush = GetD2SolidBrush(p, scrollcolor);


		p->FillRectangle(top.full, ScrollBrush);

		auto barr = top.full;

		// How many measures are max?

		// Left = the percentage hidden
		barr.left += ScrollX;
		if (TotalWidthForMusic > barr.right)
		{
			float P = (float)barr.right / (float)TotalWidthForMusic;
			barr.right = barr.left + (barr.right - barr.left) * P;
		}

		if (barr.left < top.full.left)
			barr.left = top.full.left;
		p->FillRectangle(barr, LineBrush);
	}

	void Paint(ID2D1RenderTarget* p,RECT rc,unsigned long long param = 0)
	{
		rdr = rc;
		p->BeginDraw();

		if (param == 1) // Move top paint
		{
			PaintTop(p, rc);

			p->EndDraw();
			return;
		}

		if (param == 2)
		{
			PaintSide(p, rc);
			p->EndDraw();
			return;

		}

		if (!WhiteBrush)
			WhiteBrush = GetD2SolidBrush(p, whitecolor);
		if (!BlackBrush)
			BlackBrush = GetD2SolidBrush(p, blackcolor);

		// Background
		p->Clear(bg);


		// Horzs
		if (!LineBrush)
			LineBrush = GetD2SolidBrush(p,linecolor);
		if (!SnapLineBrush)
			SnapLineBrush = GetD2SolidBrush(p, snaplinecolor);

		DrawedNotes.clear();
		for (auto c1 = FirstNote; ; c1++)
		{
			if (c1 > 128)
				break;
			auto e = NotePos(c1);
			if (Direction == 0) // Down to up
			{
				if (e.top < rc.top)
					break;
			}
			else
			{
				if (e.bottom > rc.bottom)
					break;
			}
			// Paint only the up part
			D2D1_POINT_2F p1, p2;
			p1.x = e.left;
			p1.y = e.top;
			p2.x = e.right;
			p2.y = e.top;
			p->DrawLine(p1, p2, LineBrush);

			DRAWNNOTES dn;
			dn.full = e;
			dn.n = c1;
			DrawedNotes.push_back(dn);
		}


		// Bars
		TotalWidthForMusic = 0;
		LeftWidthForMusic = 0;
		float StartX = (float)rc.left;
		if (side.PercPos > 0)
			StartX = rc.left + (side.PercPos * rc.right) / 100.0f;
		DrawnMeasures.clear();
		int End = 0;
		int EndVisible = 0;
		auto FarStartX = StartX;
		StartX -= ScrollX;

		if (side.PercPos > 0)
		{
			D2D1_POINT_2F p1, p2;
		    p1.y = (FLOAT)rc.top;
			p1.x = StartX;
			p2.y = (FLOAT)rc.bottom;
			p2.x = StartX;
			p->DrawLine(p1, p2, LineBrush, MeasureStroke);
		}

		size_t LastMeasureWithNote = 0;
		if (!notes.empty())
		{
			LastMeasureWithNote = notes[notes.size() - 1].p.m;
		}

		for (auto m = 0 ; ; m++)
		{
			auto time = TimeAtMeasure(m);
			DRAWNMEASURESANDBEATS dd;
			dd.im = m;
		//	auto SaveStartX = StartX;
			for (int ib = 0; ib < time.nb; ib++)
			{
				float E = StartX + bw;
				if (E > rc.right && ib == 0)
				{
					EndVisible = 1;
					if (LastMeasureWithNote < m)
						End = 1;
					if (End)
						break;
				}
				D2D1_POINT_2F p1, p2;
				if (ib == 0)
				{
					dd.full.left = StartX;
					dd.full.top = (FLOAT)rc.top;
				}
				if (ib == (time.nb - 1))
				{
					dd.full.right = E;
					dd.full.bottom = (FLOAT)rc.bottom;
				}
				p1.x = E;
				p1.y = (FLOAT)rc.top;
				p2.x = E;
				p2.y = (FLOAT)rc.bottom;
				D2D1_RECT_F b;
				b.left = StartX;
				b.top = (FLOAT)rc.top;
				b.right = p2.x;
				b.bottom = (FLOAT)rc.bottom;

				DRAWNBEAT dbb;
				dbb.full = b;
				dd.Beats.push_back(dbb);
				if (E <= rc.right && p1.x > FarStartX)
				{
					p->DrawLine(p1, p2, LineBrush, ib == (time.nb - 1) ? MeasureStroke : BarStroke);
					if (snapres > 1 && E <= rc.right)
					{
						// From StartX to StartX + time.bw, small lines
						float bbw = bw / (float)snapres;
						auto pp1 = p1;
						auto pp2 = p2;
						pp1.x -= bw;
						pp2.x -= bw;
						for (size_t i = 0; i < (snapres - 1); i++)
						{
							pp1.x += bbw;
							pp2.x += bbw;
							if (pp1.x >= ScrollX)
								p->DrawLine(pp1, pp2, SnapLineBrush, BarStroke);
						}
					}
				}
				StartX += bw;
			}
			if (End)
				break;
			TotalWidthForMusic += (size_t)(dd.full.right - dd.full.left);
			if (EndVisible == 0)
				DrawnMeasures.push_back(dd);
		}


		// Notes
		if (!NoteBrush1)
			NoteBrush1 = GetD2SolidBrush(p, note1color);
		if (!NoteBrush2)
			NoteBrush2 = GetD2SolidBrush(p, note2color);
		if (!NoteBrush3)
			NoteBrush3 = GetD2SolidBrush(p, note3color);
		if (!NoteBrush4)
			NoteBrush4 = GetD2SolidBrush(p, note4color);
		for (auto& n : notes)
		{
			n.dr.right = 0;
			n.dr.bottom = 0;
			auto msrbegin = DrawnMeasureByIndex(n.p.m);
			if (!msrbegin)
				continue;
			auto& msr = *msrbegin;
			D2D1_RECT_F f = msr.full;

			float fwi = (f.right - f.left);
			FRACTION fbr(msr.Beats.size(), 4);
			// in fbr, full len
			// in n.p ?
			f.left += fwi * (n.p.f / fbr).r();
			f.right = f.left + (bw * DENOM * n.d.r());

			// Find the note
			if ((n.midi - FirstNote) >= DrawedNotes.size())
				continue;
			auto& dn = DrawedNotes[n.midi - FirstNote];

			// Adjust
			f.top = dn.full.top;
			f.bottom = dn.full.bottom;

			//f.left = msr.full;
			if (n.Selected)
				p->FillRectangle(f, NoteBrush2);
			else
				p->FillRectangle(f, NoteBrush1);
			n.dr = f;

			// Velocity
			auto f2 = f;
			f2.left += 2;
			f2.right -= 2;
			f2.top += (f2.bottom - f2.top) / 2 + 5;
			f2.bottom = f2.top + 3;
			// in 127, width full
			// in V, ? 
			float wf = (f2.right - f2.left) * n.vel/ 127;
			p->FillRectangle(f2, NoteBrush3);

			f2.right = f2.left + wf;
			p->FillRectangle(f2, NoteBrush4);
		}

		// Side bar
		PaintSide(p, rc);

		// Side bar
		PaintTop(p, rc);

		if (Selecting)
		{
			p->FillRectangle(SelectRect, this->LineBrush);

		}

		p->EndDraw();
	}
};
}
using namespace PR;

CComPtr<ID2D1HwndRenderTarget> d;
CComPtr<ID2D1Factory> fa;
PIANOROLL prx;

HANDLE hEv = CreateEvent(0, 0, 0, 0);
#include ".\\mt\\rw.hpp"
tlock<queue<DWORD>> q;
void PlayThread()
{
	for (;;)
	{
		WaitForSingleObject(hEv, INFINITE);
		DWORD e = 0;

		for (;;)
		{
			q.writelock([&](queue<DWORD> & qq) {
				if (qq.empty())
					return;
				e = qq.front();
				qq.pop();
				});

			if (e == 0)
				break;
			//	e |= (0x7F << 16);
	//	e |= m << 8;
			midiOutShortMsg(mIN, e);
			Sleep(250);
			e &= 0x00FFFF;
			midiOutShortMsg(mIN, e);
		}
	}
}
void Play(int m,int vel = 0x7F)
{
	DWORD e = 0x000090;
	e |= (vel << 16);
	e |= m << 8;
	q.writelock([&](queue<DWORD> & qq) {
		qq.push(e);
		});
	SetEvent(hEv);
}

class M : public PIANOROLLCALLBACK
{
public:

	M()
	{
	}
	virtual HRESULT NoteAdded(PIANOROLL*, NOTE* n)
	{
		Play(n->midi,n->vel);
		return S_OK;
	}
	virtual HRESULT NoteRemoved(PIANOROLL*, NOTE*)
	{
		return S_OK;
	}
	virtual void RedrawRequest(PIANOROLL* p,unsigned long long param)
	{
		RECT rc;
		GetClientRect(MainWindow, &rc);
		p->Paint(d, rc,param);
	}
	virtual HRESULT OnNoteChange(PIANOROLL*, NOTE* n1, NOTE* n2)
	{
		if (n1->midi != n2->midi || n1->vel != n2->vel)
		{
			Play(n2->midi,n2->vel);
		}
		return S_OK;
	}
	virtual HRESULT OnNoteSelect(PIANOROLL*, NOTE* n, bool s)
	{
		if (!s)
			return S_OK;
		Play(n->midi, n->vel);
		return S_OK;
	}
	virtual void OnPianoOn(PIANOROLL* ,int n)
	{
		DWORD e = 0x7F0090;
		e |= n << 8;
		midiOutShortMsg(mIN, e);
	}
	virtual void OnPianoOff(PIANOROLL*, int off)
	{
		DWORD e = 0x000090;
		e |= off << 8;
		midiOutShortMsg(mIN, e);
	}

};
shared_ptr<M> c = make_shared<M>(); 
LRESULT CALLBACK Main_DP(HWND hh, UINT mm, WPARAM ww, LPARAM ll)
	{
	switch (mm)
		{
		case WM_CREATE:
			{
			MainWindow = hh;
			prx.SetWindow(hh);
			prx.AddCallback(c);
			break;
			}

		case WM_COMMAND:
			{
			int LW = LOWORD(ww);
			UNREFERENCED_PARAMETER(LW);


			return 0;
			}

		case WM_CLOSE:
			{
			DestroyWindow(hh);
			return 0;
			}

		case WM_KEYDOWN:
			{
				prx.KeyDown(ww, ll);
				return 0;
			}
		case WM_MOUSEMOVE:
			{
				prx.MouseMove(ww, ll);
				return 0;
			}
		case WM_LBUTTONDOWN:
			{
				prx.LeftDown(ww, ll);
				return 0;
			}
		case WM_RBUTTONDOWN:
			{
				prx.RightDown(ww, ll);
				return 0;
			}
		case WM_LBUTTONUP:
			{
				prx.LeftUp(ww, ll);
				return 0;
			}
		case WM_LBUTTONDBLCLK:
			{
				prx.LeftDoubleClick(ww, ll);
				return 0;
			}
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			BeginPaint(hh, &ps);

			RECT rc;
			GetClientRect(hh, &rc);
			if (!fa)
				D2D1CreateFactory(D2D1_FACTORY_TYPE::D2D1_FACTORY_TYPE_MULTI_THREADED, &fa);
			if (!d)
			{
//				D2D1_RENDER_TARGET_PROPERTIES p;
				D2D1_HWND_RENDER_TARGET_PROPERTIES hp;
				hp.hwnd = hh;
				hp.pixelSize.width = rc.right;
				hp.pixelSize.height = rc.bottom;
				d.Release();
				
				fa->CreateHwndRenderTarget(D2D1::RenderTargetProperties(), D2D1::HwndRenderTargetProperties(hh, D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)), &d);
			}
			prx.Paint(d, rc);
			EndPaint(hh, &ps);
			return 0;
		}

		case WM_SIZE:
		{
			if (!d)
				return 0;

			RECT rc;
			GetClientRect(hh, &rc);
			D2D1_SIZE_U u;
			u.width = rc.right;
			u.height = rc.bottom;
			d->Resize(u);
			return 0;
		}

		case WM_DESTROY:
			{
			PostQuitMessage(0);
			return 0;
			}
		}
	return DefWindowProc(hh, mm, ww, ll);
	}



int __stdcall WinMain(HINSTANCE h, HINSTANCE, LPSTR, int)
	{
	WSADATA wData;
	WSAStartup(MAKEWORD(2, 2), &wData);
	CoInitializeEx(0, COINIT_APARTMENTTHREADED);
	INITCOMMONCONTROLSEX icex = { 0 };
	icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_DATE_CLASSES | ICC_WIN95_CLASSES;
	icex.dwSize = sizeof(icex);
	InitCommonControlsEx(&icex);
	InitCommonControls();
	hIcon1 = LoadIcon(0, IDI_APPLICATION);
	midiOutOpen(&mIN, 0, 0, 0, 0);
	hAppInstance = h;
	std::thread tt(PlayThread);
	tt.detach();

	WNDCLASSEX wClass = { 0 };
	wClass.cbSize = sizeof(wClass);

	wClass.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW | CS_PARENTDC;
	wClass.lpfnWndProc = (WNDPROC)Main_DP;
	wClass.hInstance = h;
	wClass.hIcon = hIcon1;
	wClass.hCursor = 0;// LoadCursor(0, IDC_ARROW);
	wClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wClass.lpszClassName = _T("CLASS");
	wClass.hIconSm = hIcon1;
	RegisterClassEx(&wClass);

	MainWindow = CreateWindowEx(0,
		_T("CLASS"),
		ttitle,
		WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS |
		WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		0, 0, h, 0);

	ShowWindow(MainWindow, SW_SHOW);


	MSG msg;

	while (GetMessage(&msg, 0, 0, 0))
		{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		}

	return 0;
}