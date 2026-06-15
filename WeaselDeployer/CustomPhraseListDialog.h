#pragma once



#include "resource.h"

#include <string>

#include <vector>



struct CustomPhraseEntry {

  std::wstring code;

  std::wstring phrase;

  bool learned = false;

  std::wstring source_file;

};



class CustomPhraseListDialog : public CDialogImpl<CustomPhraseListDialog> {

 public:

  enum { IDD = IDD_CUSTOM_PHRASE_LIST };



  CustomPhraseListDialog();

  ~CustomPhraseListDialog();



 protected:

  BEGIN_MSG_MAP(CustomPhraseListDialog)

  MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)

  MESSAGE_HANDLER(WM_CLOSE, OnClose)

  MESSAGE_HANDLER(WM_DESTROY, OnDestroy)

  MESSAGE_HANDLER(WM_SIZE, OnSize)

  MESSAGE_HANDLER(WM_GETMINMAXINFO, OnGetMinMaxInfo)

  MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)

  MESSAGE_HANDLER(WM_CTLCOLORSTATIC, OnCtlColorStatic)

  COMMAND_HANDLER(IDC_PHRASE_LIST, LBN_SELCHANGE, OnListSelChange)

  COMMAND_ID_HANDLER(IDC_PHRASE_ADD, OnAdd)

  COMMAND_ID_HANDLER(IDC_PHRASE_EDIT, OnEdit)

  COMMAND_ID_HANDLER(IDC_PHRASE_DELETE, OnDelete)

  COMMAND_ID_HANDLER(IDC_PHRASE_EXPORT, OnExport)

  COMMAND_ID_HANDLER(IDC_PHRASE_IMPORT, OnImport)

  COMMAND_ID_HANDLER(IDC_PHRASE_SYNC, OnSyncUserDict)

  COMMAND_ID_HANDLER(IDC_PHRASE_HIDE_SINGLE, OnToggleHideSingle)

  COMMAND_ID_HANDLER(IDC_PHRASE_APPLY_EDIT, OnApplyEdit)

  COMMAND_ID_HANDLER(IDCANCEL, OnCancel)

  END_MSG_MAP()



  LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);

  LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);

  LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);

  LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);

  LRESULT OnGetMinMaxInfo(UINT, WPARAM, LPARAM, BOOL&);

  LRESULT OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&);

  LRESULT OnCtlColorStatic(UINT, WPARAM, LPARAM, BOOL&);

  LRESULT OnListSelChange(WORD, WORD, HWND, BOOL&);

  LRESULT OnAdd(WORD, WORD, HWND, BOOL&);

  LRESULT OnEdit(WORD, WORD, HWND, BOOL&);

  LRESULT OnDelete(WORD, WORD, HWND, BOOL&);

  LRESULT OnExport(WORD, WORD, HWND, BOOL&);

  LRESULT OnImport(WORD, WORD, HWND, BOOL&);

  LRESULT OnSyncUserDict(WORD, WORD, HWND, BOOL&);

  LRESULT OnToggleHideSingle(WORD, WORD, HWND, BOOL&);

  LRESULT OnApplyEdit(WORD, WORD, HWND, BOOL&);

  LRESULT OnCancel(WORD, WORD, HWND, BOOL&);



  void BuildUi();

  void LayoutUi(int cx, int cy);

  void UpdateDetailPanel();

  void ReloadList();

  void ReloadUserDictEntries();

  void PopulateListBox();

  void UpdateStatsLabel();

  bool SaveEntries();

  bool ParsePhraseLine(const std::string& line, CustomPhraseEntry& entry);

  void MergeImportedEntries(const std::wstring& path);

  std::wstring GetPhraseFileName() const;

  int GetSelectedIndex() const;

  bool ShouldShowLearnedEntry(const CustomPhraseEntry& entry) const;



  CListBox list_;

  CStatic subtitle_;

  CStatic stats_;

  CButton hide_single_check_;

  CStatic detail_label_;

  CStatic label_code_;

  CStatic label_phrase_;

  CEdit edit_code_;

  CEdit edit_phrase_;

  CButton btn_apply_;

  CButton btn_add_;

  CButton btn_edit_;

  CButton btn_delete_;

  CButton btn_sync_;

  CButton btn_export_;

  CButton btn_import_;

  CButton btn_close_;

  std::vector<CustomPhraseEntry> custom_entries_;

  std::vector<CustomPhraseEntry> learned_entries_;

  std::vector<CustomPhraseEntry> display_entries_;

  std::wstring phrase_file_;

  bool hide_single_char_learned_ = true;

  bool detail_visible_ = false;

  bool ui_built_ = false;

  HFONT font_normal_ = NULL;

  HFONT font_bold_ = NULL;

  HFONT font_caption_ = NULL;

  HFONT font_title_ = NULL;

  HBRUSH group_brush_ = NULL;

  HBRUSH surface_brush_ = NULL;

  bool IsLearnedSelection(int index) const;

};

