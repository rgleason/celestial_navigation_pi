#ifndef CELESTIAL_NAVIGATION_LUNAR_TOOLS_DIALOG_H
#define CELESTIAL_NAVIGATION_LUNAR_TOOLS_DIALOG_H

#include "LunarSessionEngine.h"
#include "SextantCalibrationEngine.h"

#include <wx/dialog.h>

class CelestialNavigationDialog;
class NavigationAngleCtrl;
class wxButton;
class wxCheckBox;
class wxCheckListBox;
class wxChoice;
class wxDatePickerCtrl;
class wxListCtrl;
class wxPanel;
class wxSpinCtrlDouble;
class wxStaticText;
class wxTextCtrl;
class wxTimePickerCtrl;

class LunarToolsDialog : public wxDialog {
public:
  LunarToolsDialog(CelestialNavigationDialog* parent);
  ~LunarToolsDialog() override;
  void SelectPageForIntegration(unsigned page);

private:
  struct UtcEntryControls {
    wxPanel* dateContainer = nullptr;
    wxPanel* timeContainer = nullptr;
    wxDatePickerCtrl* nativeDate = nullptr;
    wxTimePickerCtrl* nativeTime = nullptr;
    wxTextCtrl* nauticalDate = nullptr;
    wxTextCtrl* nauticalTime = nullptr;
  };

  void BuildSequencePage(wxWindow* parent);
  void BuildPlannerPage(wxWindow* parent);
  void BuildCalibrationPage(wxWindow* parent);
  void PopulateBodies(wxChoice* choice, bool includeMoon);
  void UpdateSequenceSelection(bool refreshAutomaticPosition = true);
  void SelectVisibleSequence(wxCommandEvent& event);
  void ClearSequenceSelection(wxCommandEvent& event);
  void UseEarliestSequencePosition(wxCommandEvent& event);
  void SolveSequence(wxCommandEvent& event);
  void SelectCandidate(wxCommandEvent& event);
  void ApplySequenceCorrection(wxCommandEvent& event);
  void CalculatePlanner(wxCommandEvent& event);
  void PredictCalibrationPair(wxCommandEvent& event);
  void AddCalibrationReading(wxCommandEvent& event);
  void RemoveCalibrationReading(wxCommandEvent& event);
  void SaveCalibrationProfile(wxCommandEvent& event);
  void SelectCalibrationProfile(wxCommandEvent& event);
  void UpdateProfileCorrection();
  void LoadProfiles();
  void PersistProfiles();
  void ShowCandidate(std::size_t index);
  void CreateUtcEntry(wxWindow* parent, UtcEntryControls* controls,
                      const wxDateTime& utc);
  void SetUtcEntry(UtcEntryControls* controls, const wxDateTime& utc);
  wxDateTime ReadUtcEntry(const UtcEntryControls& controls, int format,
                          bool showErrors, const wxString& title) const;
  void ChangeUtcEntryFormat(wxCommandEvent& event);
  void UpdateUtcEntryVisibility();
  wxDateTime CalibrationUtc() const;
  sextant_calibration::BodySample SampleBody(const wxString& body,
                                             const wxDateTime& utc);

  CelestialNavigationDialog* m_parentDialog;
  class wxNotebook* m_notebook;
  wxChoice* m_entryFormat;
  int m_activeEntryFormat;
  double m_defaultLatitude;
  double m_defaultLongitude;

  wxCheckListBox* m_sequenceSights;
  wxChoice* m_sequenceMode;
  NavigationAngleCtrl* m_sequenceLatitude;
  NavigationAngleCtrl* m_sequenceLongitude;
  wxSpinCtrlDouble* m_sequenceSearchHours;
  wxStaticText* m_sequenceReference;
  wxStaticText* m_sequencePositionSource;
  wxCheckBox* m_sequenceRobust;
  wxCheckBox* m_sequenceBias;
  wxCheckBox* m_sequenceMotion;
  wxSpinCtrlDouble* m_sequenceCog;
  wxSpinCtrlDouble* m_sequenceSog;
  wxChoice* m_sequenceCandidate;
  wxStaticText* m_sequenceSummary;
  wxListCtrl* m_sequenceResiduals;
  wxButton* m_applySequence;
  std::vector<std::size_t> m_lunarIndices;
  lunar_session::Result m_sequenceResult;
  bool m_sequencePositionAutomatic;

  NavigationAngleCtrl* m_plannerLatitude;
  NavigationAngleCtrl* m_plannerLongitude;
  UtcEntryControls m_plannerUtc;
  wxListCtrl* m_plannerList;

  NavigationAngleCtrl* m_calLatitude;
  NavigationAngleCtrl* m_calLongitude;
  UtcEntryControls m_calUtc;
  wxChoice* m_calFirstBody;
  wxChoice* m_calSecondBody;
  wxChoice* m_calContact;
  wxSpinCtrlDouble* m_calPressure;
  wxSpinCtrlDouble* m_calTemperature;
  wxStaticText* m_calPrediction;
  NavigationAngleCtrl* m_calObservedAngle;
  wxSpinCtrlDouble* m_calUncertainty;
  wxTextCtrl* m_calNote;
  wxListCtrl* m_calReadings;
  wxTextCtrl* m_profileName;
  wxTextCtrl* m_profileSerial;
  wxChoice* m_profileChoice;
  wxStaticText* m_profileSummary;
  wxStaticText* m_profileCorrection;
  double m_lastPredictionDeg;
  std::vector<sextant_calibration::CheckReading> m_calibrationReadings;
  std::vector<sextant_calibration::Profile> m_profiles;
};

#endif
