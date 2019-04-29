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

class PIANOROLL;
class NOTE;
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

class NOTE
{
public:

	int midi = 0;
	int Selected = 0;
	size_t startmeasure = 0;
	size_t startbeat = 0;
	size_t endmeasure = 0;
	size_t endbeat = 0;
	int startdiv = 0;
	int enddiv = 0;
	int vel = 127;

	D2D1_RECT_F dr;

	bool operator <(const NOTE& n2)
	{
		if (startmeasure < n2.startmeasure)
			return true;
		if (startmeasure > n2.startmeasure)
			return false;
		if (startbeat < n2.startbeat)
			return true;
		if (startbeat > n2.startbeat)
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
			signed int PercPos = 5;
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

	class NOTEPOS
	{
	public:

		int n = 0;
		size_t nm = (size_t)-1;
		size_t nb = (size_t)-1;
	};

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
	CComPtr<ID2D1SolidColorBrush> NoteBrush1;
	CComPtr<ID2D1SolidColorBrush> NoteBrush2;
	CComPtr<ID2D1SolidColorBrush> NoteBrush3;
	CComPtr<ID2D1SolidColorBrush> NoteBrush4;
	unsigned int snapres = 1;
	SIDEDRAW side;
	TOPDRAW top;
	int MaxUndoLevel = 100;
	int BarMoving = false;
	int Selecting = false;
	int PianoClicking = false;
	POINT LastClick;
	int PianoNoteClicked = -1;
	NOTEPOS LastClickN;
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

	bool InRect(D2D1_RECT_F& r, int x, int y)
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


	NOTEPOS MeasureAndBarHitTest(float width)
	{
		NOTEPOS np;
		np.nm = (size_t)-1;
		np.nb = (size_t)-1;

		for (auto& dd : DrawnMeasures)
		{
			if (dd.full.left > width)
				continue;
			if (dd.full.right < width)
				continue;

			// This is it
			np.nm = dd.im;
			for (size_t y = 0; y < dd.Beats.size(); y++)
			{
				auto& b = dd.Beats[y];
				if (b.full.left > width)
					continue;
				if (b.full.right < width)
					continue;
				np.nb = y;
				return np;
			}
		}

		return np;
	}

	NOTEPOS MidiHitTest(float height)
	{
		NOTEPOS np;
		np.n = 0;
		np.nm = (size_t)-1;
		np.nb = (size_t)-1;

		for (size_t i = 0; i < DrawedNotes.size(); i++)
		{
			auto& dd = DrawedNotes[i];
			if (dd.full.top > height)
				continue;
			if (dd.full.bottom < height)
				continue;

			np.n = dd.n;
			return np;
		}
		return np;
	}

	float HeightAtNote(int n)
	{
		if (Heights.size() == 1)
			return Heights[0].nh*HeightScale;

		return 0.0f; //*
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

	size_t NoteAtPos(int x, int y)
	{
		for (size_t i = 0 ; i < notes.size() ; i++)
		{
			auto& n = notes[i];
			if (n.dr.left > x)
				continue;
			if (n.dr.right < x)
				continue;
			if (n.dr.top > y)
				continue;
			if (n.dr.bottom < y)
				continue;
			return i;
		}
		return (size_t)-1;
	}

	void KeyDown(WPARAM ww, LPARAM )
	{
		bool Shift = ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
		bool Control = ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);

		if (ww >= '1' && ww <= '6' )
		{
			if (Control)
			{
				snapres = ww - '1' + 1;
				Redraw();
				return;
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
			if (clip.empty() || LastClickN.nb == -1)
				return;

			NOTE first = clip[0];
			int di = first.midi - LastClickN.n;
			int im = first.startmeasure -= LastClickN.nm;
			PushUndo();
			for (auto& n : clip)
			{
				NOTE n2 = n;
				// Note, and position that changes
				n2.midi -= di;
				n2.startmeasure -= im;
				n2.endmeasure -= im;
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
		int xx = LOWORD(ll);
		int yy = HIWORD(ll);
		if (Selecting)
		{
			D2D1_RECT_F& d3 = SelectRect;
			d3.left = LastClick.x;
			d3.right = xx;
			if (xx < LastClick.x)
			{
				d3.right = LastClick.x;
				d3.left = xx;
			}
			d3.top = LastClick.y;
			d3.bottom = yy;
			if (yy < LastClick.y)
			{
				d3.bottom = LastClick.y;
				d3.top = yy;
			}

			for (auto& n : notes)
			{
				if (InRect(SelectRect, n.dr.right, n.dr.bottom))
					n.Selected = true;
				else
				if (InRect(SelectRect, n.dr.left, n.dr.top))
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
	}

	void LeftUp(WPARAM, LPARAM)
	{
		if (PianoNoteClicked != -1)
		{
			for (auto c : cb)
				c->OnPianoOff(this, PianoNoteClicked);
		}
		Selecting = false;
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
			swprintf_s(re, L"Resolution /1\tCtrl+1");
			AppendMenu(m, MF_STRING, 21, re);
			swprintf_s(re, L"Resolution /2\tCtrl+2");
			AppendMenu(m, MF_STRING, 22, re);
			swprintf_s(re, L"Resolution /3\tCtrl+3");
			AppendMenu(m, MF_STRING, 23, re);
			swprintf_s(re, L"Resolution /4\tCtrl+4");
			AppendMenu(m, MF_STRING, 24, re);
			swprintf_s(re, L"Resolution /5\tCtrl+5");
			AppendMenu(m, MF_STRING, 25, re);
			swprintf_s(re, L"Resolution /6\tCtrl+6");
			AppendMenu(m, MF_STRING, 26, re);
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
				t.atm = hp.nm;
				Times.push_back(t);
				std::sort(Times.begin(), Times.end());
				Redraw();
			}
			if (tcmd >= 21 && tcmd <= 26)
			{
				snapres = tcmd - 20;
				Redraw();
			}
		}


		if (Need)
			Redraw();
	}


	void LeftDown(WPARAM , LPARAM ll)
	{
		Selecting = false;
		LastClickN = MeasureAndBarHitTest(LOWORD(ll));
		LastClickN.n = MidiHitTest(HIWORD(ll)).n;
		LastClick.x = LOWORD(ll);
		LastClick.y = HIWORD(ll);
		// Bar
		if (InRect(top.full, LOWORD(ll), HIWORD(ll)))
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
		auto ni = NoteAtPos(LOWORD(ll), HIWORD(ll));

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
		if (e1.nm != -1 && e2.n > 0)
		{
			// Insert note
			NOTE nx;
			nx.midi = e2.n;
			nx.startbeat = (size_t)e1.nb;
			nx.startmeasure = e1.nm;
			nx.startdiv = 0;
			nx.endbeat = (size_t)e1.nb;
			nx.endmeasure = e1.nm;
			nx.enddiv = 1;
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
		if (top.PercPos == 0)
			return;
		if (top.PercPos > 0)
		{
			top.full.left = (FLOAT)rc.left;
			if (side.PercPos > 0)
				top.full.left = side.full.right;
			top.full.top = (FLOAT)rc.top;
			top.full.right = (FLOAT)rc.right;
			top.full.bottom = (FLOAT)(top.PercPos * rc.bottom) / 100.0f;
		}
		else
		{
			top.full.top = (FLOAT)rc.bottom - (rc.bottom * (-top.PercPos)) / 100;
			top.full.left = (FLOAT)rc.left;
			if (side.PercPos > 0)
				top.full.left = side.full.right;
			top.full.bottom = (FLOAT)rc.bottom;
			top.full.right = (FLOAT)rc.right;
		}

		p->FillRectangle(top.full, SideBrush);

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
			LastMeasureWithNote = notes[notes.size() - 1].endmeasure;
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
			auto msrbegin = DrawnMeasureByIndex(n.startmeasure);
			if (!msrbegin)
				continue;
			auto& msr = *msrbegin;
			if (n.startbeat >= msr.Beats.size())
				continue;
			auto& startbeat = msr.Beats[n.startbeat];
			// Find the note
			if ((n.midi - FirstNote) >= DrawedNotes.size())
				continue;
			auto& dn = DrawedNotes[n.midi - FirstNote];


			D2D1_RECT_F f = msr.full;
			f.left = startbeat.full.left;
			// Adjust
			f.top = dn.full.top;
			f.bottom = dn.full.bottom;

//			size_t ie = n.endmeasure;
			auto msrend = DrawnMeasureByIndex(n.endmeasure);
			if (!msrend)
				msrend = &DrawnMeasures[DrawnMeasures.size() - 1];
			auto& msend = *msrend;
			f.right = msend.full.right;
			if (n.endbeat >= msend.Beats.size())
				continue;
			f.right = msend.Beats[n.endbeat].full.right;


			
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


CComPtr<ID2D1HwndRenderTarget> d;
CComPtr<ID2D1Factory> f;
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
			if (!f)
				D2D1CreateFactory(D2D1_FACTORY_TYPE::D2D1_FACTORY_TYPE_MULTI_THREADED, &f);
			if (!d)
			{
//				D2D1_RENDER_TARGET_PROPERTIES p;
				D2D1_HWND_RENDER_TARGET_PROPERTIES hp;
				hp.hwnd = hh;
				hp.pixelSize.width = rc.right;
				hp.pixelSize.height = rc.bottom;
				d.Release();
				
				f->CreateHwndRenderTarget(D2D1::RenderTargetProperties(), D2D1::HwndRenderTargetProperties(hh, D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)), &d);
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
	wClass.hCursor = LoadCursor(0, IDC_ARROW);
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