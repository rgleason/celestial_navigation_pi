#ifndef CELESTIAL_NAVIGATION_LUNAR_TOOLS_DIALOG_H
#define CELESTIAL_NAVIGATION_LUNAR_TOOLS_DIALOG_H

#include "LunarSessionEngine.h"
#include "SextantCalibrationEngine.h"

#include <wx/dialog.h>

class CelestialNavigationDialog;
class wxButton;
class wxCheckBox;
class wxCheckListBox;
class wxChoice;
class wxDatePickerCtrl;
class wxListCtrl;
class wxSpinCtrlDouble;
class wxStaticText;
class wxTextCtrl;
class wxTimePickerCtrl;

class LunarToolsDialog : public wxDialog {
public:
  LunarToolsDialog(CelestialNavigationDialog* parent);
  void SelectPageForIntegration(unsigned page);

private:
  void BuildSequencePage(wxWindow* parent);
  void BuildPlannerPage(wxWindow* parent);
  void BuildCalibrationPage(wxWindow* parent);
  void PopulateBodies(wxChoice* choice, bool includeMoon);
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
  wxDateTime CalibrationUtc() const;
  sextant_calibration::BodySample SampleBody(const wxString& body,
                                             const wxDateTime& utc);

  CelestialNavigationDialog* m_parentDialog;
  class wxNotebook* m_notebook;

  wxCheckListBox* m_sequenceSights;
  wxChoice* m_sequenceMode;
  wxSpinCtrlDouble* m_sequenceLatitude;
  wxSpinCtrlDouble* m_sequenceLongitude;
  wxSpinCtrlDouble* m_sequenceSearchHours;
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

  wxSpinCtrlDouble* m_plannerLatitude;
  wxSpinCtrlDouble* m_plannerLongitude;
  wxDatePickerCtrl* m_plannerDate;
  wxTimePickerCtrl* m_plannerTime;
  wxListCtrl* m_plannerList;

  wxSpinCtrlDouble* m_calLatitude;
  wxSpinCtrlDouble* m_calLongitude;
  wxDatePickerCtrl* m_calDate;
  wxTimePickerCtrl* m_calTime;
  wxChoice* m_calFirstBody;
  wxChoice* m_calSecondBody;
  wxChoice* m_calContact;
  wxSpinCtrlDouble* m_calPressure;
  wxSpinCtrlDouble* m_calTemperature;
  wxStaticText* m_calPrediction;
  wxSpinCtrlDouble* m_calObservedDegrees;
  wxSpinCtrlDouble* m_calObservedMinutes;
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
