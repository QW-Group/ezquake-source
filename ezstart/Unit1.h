//---------------------------------------------------------------------------

#ifndef Unit1H
#define Unit1H
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ComCtrls.hpp>
#include <ExtCtrls.hpp>
#include <Buttons.hpp>
//---------------------------------------------------------------------------
class TForm1 : public TForm
{
__published:	// IDE-managed Components
        TGroupBox *gs;
        TComboBox *vmode;
        TLabel *Label1;
        TLabel *Label2;
        TComboBox *cmode;
        TComboBox *bpp;
        TGroupBox *ws;
        TGroupBox *us;
        TCheckBox *window;
        TCheckBox *wavonly;
        TLabel *Label4;
        TComboBox *skhz;
        TCheckBox *cdaudio;
        TCheckBox *current;
        TCheckBox *dibonly;
        TGroupBox *ss;
        TCheckBox *ded;
        TLabel *Label6;
        TComboBox *gamedirs;
        TCheckBox *nohwgamma;
        TCheckBox *nomtex;
        TCheckBox *nosound;
        TLabel *Label8;
        TCheckBox *primarysound;
        TLabel *Label10;
        TComboBox *srvcfg;
        TLabel *Label11;
        TComboBox *freq;
        TLabel *Label9;
        TComboBox *ruleset;
        TLabel *Label12;
        TComboBox *clcfg;
        TCheckBox *no24bit;
        TCheckBox *maxtmu2;
        TButton *start;
        TButton *cancel;
        TLabel *Label14;
        TEdit *other;
        TCheckBox *nomp3;
        TCheckBox *snoforceformat;
        TRadioGroup *lang;
        TRadioGroup *ver;
        TComboBox *port;
        TGroupBox *ms;
        TLabel *Label5;
        TLabel *Label7;
        TEdit *mem;
        TEdit *democache;
        TUpDown *UpDown2;
        TUpDown *UpDown1;
        TGroupBox *is;
        TCheckBox *dinput;
        TCheckBox *m_smooth;
        TCheckBox *mw_hook;
        TCheckBox *noacc;
        TCheckBox *nospd;
        TCheckBox *nopar;
        TCheckBox *nohwtimer;
        TCheckBox *nomouse;
        TLabel *Label15;
        TEdit *zone;
        TUpDown *UpDown3;
        TLabel *Label16;
        TEdit *particles;
        TUpDown *UpDown4;
        TComboBox *fbat;
        TButton *load;
        TButton *save;
        TButton *clearb;
        TLabel *Label17;
        TEdit *conbuf;
        TUpDown *UpDown5;
        TCheckBox *condebug;
        TCheckBox *forcetex;
        TCheckBox *noscripts;
        TCheckBox *indphys;
        TCheckBox *noroot;
        TButton *crlnk;
        TComboBox *lnkpath;
        void __fastcall FormCreate(TObject *Sender);
        void __fastcall cancelClick(TObject *Sender);
        void __fastcall gamedirsChange(TObject *Sender);
        void __fastcall m_smoothClick(TObject *Sender);
        void __fastcall dinputClick(TObject *Sender);
        void __fastcall nomtexClick(TObject *Sender);
        void __fastcall maxtmu2Click(TObject *Sender);
        void __fastcall startClick(TObject *Sender);
        void __fastcall nosoundClick(TObject *Sender);
        void __fastcall verClick(TObject *Sender);
        void __fastcall nomouseClick(TObject *Sender);
        void __fastcall noparClick(TObject *Sender);
        void __fastcall windowClick(TObject *Sender);
        void __fastcall currentClick(TObject *Sender);
        void __fastcall noaccClick(TObject *Sender);
        void __fastcall nospdClick(TObject *Sender);
        void __fastcall dedClick(TObject *Sender);
        void __fastcall loadClick(TObject *Sender);
        void __fastcall saveClick(TObject *Sender);
        void __fastcall langClick(TObject *Sender);
        void __fastcall clearbClick(TObject *Sender);
        void __fastcall FormShow(TObject *Sender);
        void __fastcall crlnkClick(TObject *Sender);
        void __fastcall lnkpathChange(TObject *Sender);
        void __fastcall fbatChange(TObject *Sender);
private:	// User declarations
        void __fastcall TForm1::checks();
        void __fastcall TForm1::cmdcalc();
        void __fastcall TForm1::clear();
        void __fastcall TForm1::loadcmd();
        void __fastcall TForm1::CreateShortcut(LPCWSTR,LPCTSTR,LPCTSTR,LPCTSTR,WORD);
public:		// User declarations
        __fastcall TForm1(TComponent* Owner);

};
//---------------------------------------------------------------------------
extern PACKAGE TForm1 *Form1;
//---------------------------------------------------------------------------
#endif
