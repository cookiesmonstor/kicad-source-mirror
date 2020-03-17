///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Oct 26 2018)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#pragma once

#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/intl.h>
#include <wx/string.h>
#include <wx/stattext.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/choice.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/textctrl.h>
#include <wx/checkbox.h>
#include <wx/panel.h>

///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// Class PANEL_SETUP_FORMATTING_BASE
///////////////////////////////////////////////////////////////////////////////
class PANEL_SETUP_FORMATTING_BASE : public wxPanel
{
	private:

	protected:
		wxStaticText* m_staticText26;
		wxChoice* m_choiceSeparatorRefId;
		wxStaticText* m_textSizeLabel;
		wxTextCtrl* m_textSizeCtrl;
		wxStaticText* m_textSizeUnits;
		wxStaticText* m_textOffsetRatioLabel;
		wxTextCtrl* m_textOffsetRatioCtrl;
		wxStaticText* m_offsetRatioUnits;
		wxCheckBox* m_checkSuperSub;
		wxStaticText* m_superSubHint;
		wxStaticText* m_busWidthLabel;
		wxTextCtrl* m_busWidthCtrl;
		wxStaticText* m_busWidthUnits;
		wxStaticText* m_wireWidthLabel;
		wxTextCtrl* m_wireWidthCtrl;
		wxStaticText* m_wireWidthUnits;
		wxStaticText* m_jctSizeLabel;
		wxTextCtrl* m_jctSizeCtrl;
		wxStaticText* m_jctSizeUnits;

	public:

		PANEL_SETUP_FORMATTING_BASE( wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxTAB_TRAVERSAL, const wxString& name = wxEmptyString );
		~PANEL_SETUP_FORMATTING_BASE();

};

